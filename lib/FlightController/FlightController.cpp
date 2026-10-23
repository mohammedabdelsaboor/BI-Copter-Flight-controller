/**
 * @file   FlightController.cpp
 */
#include "FlightController.hpp"

namespace
{
float clampAbs(float v, float limit)
{
  return (v > limit) ? limit : ((v < -limit) ? -limit : v);
}
}  // namespace

FlightController::FlightController(Mpu6050& imu, AttitudeEstimator& attitude, StickInput& sticks, BiCopterMixer& mixer,
                                   Settings& settings)
  : m_imu(imu), m_attitude(attitude), m_sticks(sticks), m_mixer(mixer), m_settings(settings)
{
}

void FlightController::configure(const FlightConfig& config)
{
  m_config = config;
  m_defaults = config;
  m_pidRoll.configure(config.roll, config.loopHz);
  m_pidPitch.configure(config.pitch, config.loopHz);
  m_pidYaw.configure(config.yaw, config.loopHz);
  for (int i = 0; i < 3; i++)
  {
    m_gyroFilter[i].configure(config.gyroFilterHz[i], config.loopHz);
  }
  m_servoPitchFilter.configure(config.servoFilterHz, config.loopHz);
  m_servoYawFilter.configure(config.servoFilterHz, config.loopHz);
}

void FlightController::start()
{
  // Average a few samples so one noisy reading does not tilt the starting attitude
  float accel[3] = {0.0f, 0.0f, 0.0f};
  int   good = 0;
  for (int n = 0; n < 50; n++)
  {
    if (m_imu.read(m_sample))
    {
      for (int i = 0; i < 3; i++)
      {
        accel[i] += m_sample.accelG[i];
      }
      good++;
    }
    delay(2);
  }
  if (good > 0)
  {
    for (int i = 0; i < 3; i++)
    {
      accel[i] /= static_cast<float>(good);
    }
    // Clone MPU6050 boards often read 1.2-1.5 g on Z when still. The model is still here (gyro calibration
    // just passed), so whatever Z reads beyond gravity is offset. X/Y offsets are taken out by the level trim.
    const float horizontal = accel[0] * accel[0] + accel[1] * accel[1];
    const float expectedZ = (horizontal < 1.0f) ? sqrtf(1.0f - horizontal) : 0.0f;
    m_accelZOffsetG = (accel[2] >= 0.0f) ? (accel[2] - expectedZ) : (accel[2] + expectedZ);
    accel[2] -= m_accelZOffsetG;
    m_attitude.reset(accel);
  }
  // Not calibrated yet stays DISARMED (arming is blocked) and the sketch keeps retrying the calibration
  m_state = FlightState::DISARMED;
  if (!m_imu.isCalibrated())
  {
    setEvent("gyro not calibrated - keep the model still, retrying");
  }
  else
  {
    snprintf(m_eventText, sizeof(m_eventText), "ready - IMU 0x%02X, accel Z offset %+.2f g%s", m_imu.whoAmI(),
             m_accelZOffsetG, (fabsf(m_accelZOffsetG) > 0.6f) ? " (very large: check/replace the IMU)" : "");
    setEvent(m_eventText);
  }
}

void FlightController::update(const RcPwmInput& receiver, float dt, uint32_t nowMs)
{
  // ---- IMU ----
  if (m_imu.read(m_sample))
  {
    m_imuFailStreak = 0;
    m_sample.accelG[2] -= m_accelZOffsetG;
    trackGyroBias(dt);
    for (int i = 0; i < 3; i++)
    {
      m_sample.gyroDps[i] -= m_gyroDrift[i];
    }
  }
  else if (++m_imuFailStreak >= m_config.imuFaultReads)
  {
    if (m_state != FlightState::FAULT)
    {
      disarm(FlightState::FAULT, "IMU lost - reboot required");
    }
    m_imu.recover();  // keep trying so the console can show if wiring comes back
    m_imuFailStreak = 0;
  }
  m_attitude.update(m_sample.gyroDps, m_sample.accelG, dt);

  // ---- Radio ----
  const Sticks& sticks = m_sticks.update(receiver, !isArmed(), nowMs);

  runStateMachine(sticks, nowMs);
  runControl(sticks, dt);
}

void FlightController::trackGyroBias(float dt)
{
  // The gyro zero point creeps as the MPU6050 warms up. While disarmed and truly still, slowly follow it so
  // a fresh zero is in place at every arm. Frozen while armed (the model may be turning on purpose).
  constexpr float STILL_DPS = 1.5f;    // every axis below this (after the current correction)
  constexpr float STILL_G = 0.03f;     // and the accelerometer reads 1 g +- this
  constexpr float STILL_TIME = 1.0f;   // for this long before tracking starts, s
  constexpr float TRACK_TAU_S = 3.0f;  // how fast the correction follows, s

  bool still = (m_state == FlightState::DISARMED);
  for (int i = 0; still && (i < 3); i++)
  {
    still = fabsf(m_sample.gyroDps[i] - m_gyroDrift[i]) < STILL_DPS;
  }
  const float a = sqrtf(m_sample.accelG[0] * m_sample.accelG[0] + m_sample.accelG[1] * m_sample.accelG[1]
                        + m_sample.accelG[2] * m_sample.accelG[2]);
  still = still && (fabsf(a - 1.0f) < STILL_G);

  m_stillTimeS = still ? (m_stillTimeS + dt) : 0.0f;
  if (m_stillTimeS > STILL_TIME)
  {
    const float k = dt / (TRACK_TAU_S + dt);
    for (int i = 0; i < 3; i++)
    {
      m_gyroDrift[i] += k * (m_sample.gyroDps[i] - m_gyroDrift[i]);
    }
  }
}

void FlightController::runStateMachine(const Sticks& sticks, uint32_t nowMs)
{
  const float roll = m_attitude.rollDeg() - m_settings.get().levelRollDeg;
  const float pitch = m_attitude.pitchDeg() - m_settings.get().levelPitchDeg;

  switch (m_state)
  {
    case FlightState::DISARMED:
    {
      if (!sticks.armGesture)
      {
        m_armGestureMs = nowMs;
        m_armGestureReleased = true;
      }
      else if (m_armGestureReleased && ((nowMs - m_armGestureMs) >= m_config.armHoldMs))
      {
        const char* block = armBlockReason(sticks);
        m_armGestureReleased = false;
        if (block == nullptr)
        {
          arm();
        }
        else
        {
          setEvent(block);
        }
      }

      if (!sticks.levelGesture)
      {
        m_levelGestureMs = nowMs;
        m_levelGestureDone = false;
      }
      else if (!m_levelGestureDone && ((nowMs - m_levelGestureMs) >= m_config.levelCalHoldMs))
      {
        m_levelGestureDone = true;
        calibrateLevel();
      }
      break;
    }

    case FlightState::ARMED:
    {
      if (!sticks.valid)
      {
        disarm(FlightState::FAILSAFE, "FAILSAFE - radio lost, motors off");
        break;
      }

      if (!sticks.disarmGesture)
      {
        m_disarmGestureMs = nowMs;
      }
      else if ((nowMs - m_disarmGestureMs) >= m_config.armHoldMs)
      {
        disarm(FlightState::DISARMED, "disarmed (stick)");
        break;
      }

      if (!sticks.throttleLow)
      {
        m_throttleLowSinceMs = nowMs;
      }
      else if ((nowMs - m_throttleLowSinceMs) >= m_config.autoDisarmMs)
      {
        disarm(FlightState::DISARMED, "auto disarm (throttle low)");
        break;
      }

      if ((fabsf(roll) < m_config.crashTiltDeg) && (fabsf(pitch) < m_config.crashTiltDeg))
      {
        m_crashSinceMs = nowMs;
      }
      else if ((nowMs - m_crashSinceMs) >= m_config.crashTimeMs)
      {
        disarm(FlightState::DISARMED, "CRASH - tilt limit, disarmed");
      }
      break;
    }

    case FlightState::FAILSAFE:
    {
      if (sticks.valid && sticks.throttleLow)
      {
        m_state = FlightState::DISARMED;
        m_armGestureReleased = false;
        setEvent("radio back - disarmed");
      }
      break;
    }

    case FlightState::FAULT:
    default: break;
  }

  m_telemetry.armBlock = (m_state == FlightState::DISARMED) ? armBlockReason(sticks) : nullptr;
}

void FlightController::runControl(const Sticks& sticks, float dt)
{
  const bool  armed = isArmed();
  const float roll = m_attitude.rollDeg() - m_settings.get().levelRollDeg;
  const float pitch = m_attitude.pitchDeg() - m_settings.get().levelPitchDeg;

  // Outer loop: angle -> rate demand
  const float rollSet = sticks.roll * m_config.maxAngleDeg;    // stick right -> right side down
  const float pitchSet = sticks.pitch * m_config.maxAngleDeg;  // stick forward -> nose down
  const float rateRoll = clampAbs(m_config.angleProll * (rollSet - roll), m_config.maxRateRollDps);
  const float ratePitch = clampAbs(m_config.anglePpitch * (pitchSet - pitch), m_config.maxRatePitchDps);
  const bool  flying = armed && (sticks.throttle > m_config.throttleIEnable);
  const float gyroRoll = m_gyroFilter[0].apply(m_sample.gyroDps[0]);
  const float gyroPitch = m_gyroFilter[1].apply(m_sample.gyroDps[1]);
  const float gyroYaw = m_gyroFilter[2].apply(m_sample.gyroDps[2]);

  // Yaw: stick commands a turn rate; stick centred holds the heading (the target stays where the stick let go)
  constexpr float MAX_HEADING_ERROR_DEG = 30.0f;
  if (flying && (sticks.yaw == 0.0f) && (m_config.headingP > 0.0f))
  {
    m_headingErrorDeg = clampAbs(m_headingErrorDeg - gyroYaw * dt, MAX_HEADING_ERROR_DEG);
  }
  else
  {
    m_headingErrorDeg = 0.0f;
  }
  const float rateYaw = -sticks.yaw * m_config.maxYawRateDps  // stick right -> nose right = -yaw
                        + clampAbs(m_config.headingP * m_headingErrorDeg, 0.5f * m_config.maxYawRateDps);
  float       cmdRoll = m_pidRoll.update(rateRoll, gyroRoll, dt, flying);
  float       cmdPitch = m_pidPitch.update(ratePitch, gyroPitch, dt, flying);
  float       cmdYaw = m_pidYaw.update(rateYaw, gyroYaw, dt, flying);
  if (m_config.reverseRoll) cmdRoll = -cmdRoll;
  if (m_config.reversePitch) cmdPitch = -cmdPitch;
  if (m_config.reverseYaw) cmdYaw = -cmdYaw;
  cmdPitch = m_servoPitchFilter.apply(cmdPitch);
  cmdYaw = m_servoYawFilter.apply(cmdYaw);

  // On the ground at idle the motors must not react (would lift one side and tip over)
  const float motorRoll = flying ? cmdRoll : 0.0f;
  m_mixer.write({sticks.throttle, motorRoll, cmdPitch, cmdYaw}, armed);

  m_telemetry.state = m_state;
  m_telemetry.rollDeg = roll;
  m_telemetry.pitchDeg = pitch;
  for (int i = 0; i < 3; i++)
  {
    m_telemetry.gyroDps[i] = m_sample.gyroDps[i];
    m_telemetry.accelG[i] = m_sample.accelG[i];
  }
  m_telemetry.rateSetDps[0] = rateRoll;
  m_telemetry.rateSetDps[1] = ratePitch;
  m_telemetry.rateSetDps[2] = rateYaw;
  m_telemetry.command[0] = cmdRoll;
  m_telemetry.command[1] = cmdPitch;
  m_telemetry.command[2] = cmdYaw;
  m_telemetry.throttle = sticks.throttle;
  m_telemetry.yawI = m_pidYaw.i();
  m_telemetry.yawGyroDriftDps = m_gyroDrift[2];
  m_telemetry.preview = m_mixer.mix({0.5f, cmdRoll, cmdPitch, cmdYaw}, true);
  m_telemetry.imuErrors = m_imu.errorCount();
  m_telemetry.imuId = m_imu.whoAmI();
  m_telemetry.accelUsed = m_attitude.accelUsed();
}

const char* FlightController::armBlockReason(const Sticks& sticks) const
{
  if (m_state == FlightState::FAULT) return "no arm: IMU fault";
  if (!m_imu.isCalibrated()) return "no arm: gyro not calibrated";
  if (!sticks.valid) return "no arm: no radio / receiver failsafe";
  if (!sticks.throttleLow) return "no arm: throttle not low";
  if (!m_sticks.centresLocked()) return "no arm: release roll/pitch/yaw sticks 2 s (learning centres)";
  const float roll = m_attitude.rollDeg() - m_settings.get().levelRollDeg;
  const float pitch = m_attitude.pitchDeg() - m_settings.get().levelPitchDeg;
  if ((fabsf(roll) > m_config.armMaxTiltDeg) || (fabsf(pitch) > m_config.armMaxTiltDeg))
  {
    return "no arm: not level";
  }
  return nullptr;
}

bool FlightController::calibrateLevel()
{
  if (m_state != FlightState::DISARMED)
  {
    setEvent("level cal refused: not disarmed");
    return false;
  }
  m_settings.setLevel(m_attitude.rollDeg(), m_attitude.pitchDeg());
  const bool ok = m_settings.save();
  setEvent(ok ? "level calibrated + saved" : "level cal: flash write failed");
  return ok;
}

void FlightController::forceDisarm(const char* reason)
{
  if (m_state == FlightState::ARMED)
  {
    disarm(FlightState::DISARMED, reason);
  }
}

void FlightController::arm()
{
  resetPids();
  m_state = FlightState::ARMED;
  m_disarmGestureMs = millis();
  m_throttleLowSinceMs = millis();
  m_crashSinceMs = millis();
  setEvent("ARMED");
}

void FlightController::disarm(FlightState next, const char* reason)
{
  m_state = next;
  m_armGestureReleased = false;
  resetPids();
  m_mixer.write({0.0f, 0.0f, 0.0f, 0.0f}, false);
  setEvent(reason);
}

void FlightController::resetPids()
{
  m_pidRoll.reset();
  m_pidPitch.reset();
  m_pidYaw.reset();
}

void FlightController::setEvent(const char* text)
{
  m_event = text;
  m_eventCount++;
}

void FlightController::setRateGains(uint8_t axis, float p, float i, float d)
{
  PidGains* gains = (axis == 0) ? &m_config.roll : ((axis == 1) ? &m_config.pitch : &m_config.yaw);
  Pid*      pid = (axis == 0) ? &m_pidRoll : ((axis == 1) ? &m_pidPitch : &m_pidYaw);
  gains->kp = p;
  gains->ki = i;
  gains->kd = d;
  pid->configure(*gains, m_config.loopHz);
}

void FlightController::setAngleP(uint8_t axis, float p)
{
  if (axis == 0)
  {
    m_config.angleProll = p;
  }
  else if (axis == 1)
  {
    m_config.anglePpitch = p;
  }
  else
  {
    m_config.headingP = p;
  }
}

const PidGains& FlightController::rateGains(uint8_t axis) const
{
  return (axis == 0) ? m_config.roll : ((axis == 1) ? m_config.pitch : m_config.yaw);
}

StoredGains FlightController::exportGains() const
{
  StoredGains out = {};
  out.valid = true;
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    out.rateP[axis] = rateGains(axis).kp;
    out.rateI[axis] = rateGains(axis).ki;
    out.rateD[axis] = rateGains(axis).kd;
  }
  out.angleP[0] = m_config.angleProll;
  out.angleP[1] = m_config.anglePpitch;
  out.angleP[2] = m_config.headingP;
  return out;
}

void FlightController::applyGains(const StoredGains& gains)
{
  if (!gains.valid)
  {
    return;
  }
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    setRateGains(axis, gains.rateP[axis], gains.rateI[axis], gains.rateD[axis]);
  }
  setAngleP(0, gains.angleP[0]);
  setAngleP(1, gains.angleP[1]);
  setAngleP(2, gains.angleP[2]);
}

void FlightController::restoreDefaultGains()
{
  m_config = m_defaults;
  configure(m_defaults);
}

const char* FlightController::stateName(FlightState state)
{
  switch (state)
  {
    case FlightState::DISARMED: return "DISARMED";
    case FlightState::ARMED: return "ARMED";
    case FlightState::FAILSAFE: return "FAILSAFE";
    case FlightState::FAULT: return "FAULT";
  }
  return "?";
}

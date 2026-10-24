/**
 * @file   FlightController.hpp
 * @brief  Bicopter flight logic: arming state machine, failsafe, self-levelling cascaded PID.
 *
 *   stick angle --(angle P)--> rate demand --(rate PID vs gyro)--> roll/pitch/yaw command --> mixer
 *
 * Roll is held by the motors, pitch and yaw by the tilt servos (see BiCopterMixer).
 * While disarmed the same control law runs with the I terms off and the motors held OFF, so the
 * servos react to sticks and to tilting the frame by hand: an easy way to check directions.
 */
#pragma once

#include <Arduino.h>
#include "Mpu6050.hpp"
#include "AttitudeEstimator.hpp"
#include "StickInput.hpp"
#include "BiCopterMixer.hpp"
#include "Settings.hpp"
#include "Pid.hpp"

struct FlightConfig
{
  float    loopHz;
  float    maxAngleDeg;
  float    maxYawRateDps;
  float    angleProll;
  float    anglePpitch;
  float    headingP;  // heading hold gain, 0 = off
  float    maxRateRollDps;
  float    maxRatePitchDps;
  PidGains roll;
  PidGains pitch;
  PidGains yaw;
  bool     reverseRoll;
  bool     reversePitch;
  bool     reverseYaw;
  float    armMaxTiltDeg;
  float    crashTiltDeg;
  uint32_t crashTimeMs;
  uint32_t autoDisarmMs;
  uint32_t armHoldMs;
  uint32_t levelCalHoldMs;
  float    throttleIEnable;
  uint32_t imuFaultReads;
  float    gyroFilterHz[3];  // roll, pitch, yaw gyro low pass, 0 = off
  float    servoFilterHz;    // low pass on the pitch/yaw (servo) commands, 0 = off
  // Hover tilt compensation, see FlightController::updateHoverTrim()
  bool     hoverTrimEnabled;
  float    hoverTrimTauS;          // how fast the learned motor tilt follows, s
  float    hoverTrimMaxDeg;        // limit of the body pitch offset
  float    hoverTrimMinThrottle;   // learn only above this throttle (airborne)
  float    servoDegPerCommand;     // motor tilt in degrees for a pitch command of 1.0
  // Tilt torque = thrust x tilt, so the servo (pitch/yaw) commands are scaled by hoverThrottle / throttle
  float    hoverThrottle;          // throttle (thrust) at hover, 0 = no scaling
  float    servoGainMin;
  float    servoGainMax;
};

enum class FlightState : uint8_t
{
  DISARMED,
  ARMED,
  FAILSAFE,  // radio lost while armed: motors off until the radio is back with throttle low
  FAULT,     // IMU failure: motors off until reboot
};

struct FlightTelemetry
{
  FlightState  state;
  const char*  armBlock;  // why arming is refused right now, nullptr if it would arm
  float        rollDeg;   // level corrected
  float        pitchDeg;
  float        gyroDps[3];  // body roll, pitch, yaw rates
  float        accelG[3];
  float        rateSetDps[3];
  float        command[3];  // roll, pitch, yaw outputs to the mixer
  float        throttle;
  float        yawI;             // yaw I term: its build-up is what moves the servos apart
  float        pitchI;           // pitch I term: the steady servo tilt needed to hold the nose
  float        yawGyroDriftDps;  // gyro Z zero-point correction learned while disarmed
  MixerOutputs preview;          // what the outputs would be armed at 50 % throttle
  uint32_t     imuErrors;
  uint8_t      imuId;      // WHO_AM_I
  bool         accelUsed;  // accelerometer accepted by the attitude filter this loop
  float        hoverMotorTiltDeg;  // learned average motor tilt in hover, + = forward
  float        hoverPitchTrimDeg;  // body pitch offset that cancels it, + = nose down
};

class FlightController
{
public:
  FlightController(Mpu6050& imu, AttitudeEstimator& attitude, StickInput& sticks, BiCopterMixer& mixer,
                   Settings& settings);

  void configure(const FlightConfig& config);

  /** Seeds the attitude and measures the accel Z offset. Call after gyro calibration, model still. */
  void start();

  /** One control step. */
  void update(const RcPwmInput& receiver, float dt, uint32_t nowMs);

  void forceDisarm(const char* reason);

  /** Level calibration from the console: stores the current attitude as level (disarmed only). */
  bool calibrateLevel();

  /** Forgets the learned hover tilt and saves that (disarmed only). */
  bool resetHoverTrim();

  bool isArmed() const { return m_state == FlightState::ARMED; }
  FlightState state() const { return m_state; }
  const FlightTelemetry& telemetry() const { return m_telemetry; }

  /** Latest event text and a counter that increments on each new event (for the console). */
  const char* lastEvent() const { return m_event; }
  uint32_t eventCount() const { return m_eventCount; }
  float accelZOffsetG() const { return m_accelZOffsetG; }

  static const char* stateName(FlightState state);

  // ---- Live tuning (axis 0 roll, 1 pitch, 2 yaw) ----
  void setRateGains(uint8_t axis, float p, float i, float d);
  void setAngleP(uint8_t axis, float p);  // roll / pitch self level, yaw heading hold
  const PidGains& rateGains(uint8_t axis) const;
  float angleP(uint8_t axis) const
  {
    return (axis == 0) ? m_config.angleProll : ((axis == 1) ? m_config.anglePpitch : m_config.headingP);
  }
  StoredGains exportGains() const;
  void applyGains(const StoredGains& gains);
  void restoreDefaultGains();

private:
  void runStateMachine(const Sticks& sticks, uint32_t nowMs);
  void runControl(const Sticks& sticks, float dt);
  const char* armBlockReason(const Sticks& sticks) const;
  void arm();
  void disarm(FlightState next, const char* reason);
  void setEvent(const char* text);
  void resetPids();
  void trackGyroBias(float dt);
  /** @param steady  pitch close to its target and not rotating: only then is the servo tilt a CG offset */
  void updateHoverTrim(float servoPitchCommand, float throttle, bool steady, float dt);
  void saveHoverTrim();

  Mpu6050&           m_imu;
  AttitudeEstimator& m_attitude;
  StickInput&        m_sticks;
  BiCopterMixer&     m_mixer;
  Settings&          m_settings;

  FlightConfig    m_config = {};
  FlightConfig    m_defaults = {};  // as configured at boot, for 'defaults'
  Pid             m_pidRoll;
  Pid             m_pidPitch;
  Pid             m_pidYaw;
  Pt1Filter       m_gyroFilter[3];
  Pt1Filter       m_servoPitchFilter;
  Pt1Filter       m_servoYawFilter;
  ImuSample       m_sample = {};
  FlightTelemetry m_telemetry = {};

  FlightState m_state = FlightState::DISARMED;
  uint32_t    m_imuFailStreak = 0;
  uint32_t    m_armGestureMs = 0;
  uint32_t    m_disarmGestureMs = 0;
  uint32_t    m_levelGestureMs = 0;
  uint32_t    m_throttleLowSinceMs = 0;
  uint32_t    m_crashSinceMs = 0;
  bool        m_armGestureReleased = false;  // gesture must be let go once before it arms
  bool        m_levelGestureDone = false;

  float       m_accelZOffsetG = 0.0f;               // measured at start(), see there
  float       m_gyroDrift[3] = {0.0f, 0.0f, 0.0f};  // deg/s, see trackGyroBias()
  float       m_stillTimeS = 0.0f;
  float       m_headingErrorDeg = 0.0f;  // heading hold: target minus actual heading
  float       m_hoverMotorTiltDeg = 0.0f;  // learned in flight, saved to flash after each flight
  float       m_hoverPitchTrimDeg = 0.0f;
  float       m_hoverSavedDeg = 0.0f;      // value last written to flash
  float       m_hoverLearnS = 0.0f;        // time spent learning this flight
  char        m_eventText[96] = {};
  const char* m_event = "boot";
  uint32_t    m_eventCount = 0;
};

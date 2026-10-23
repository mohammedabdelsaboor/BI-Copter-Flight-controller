/**
 * @file   SerialConsole.cpp
 */
#include "SerialConsole.hpp"
#include <stdarg.h>

namespace
{
constexpr uint32_t STREAM_PERIOD_MS = 100;
constexpr uint32_t STATUS_PERIOD_MS = 1000;
}  // namespace

SerialConsole::SerialConsole(FlightController& flight, const RcPwmInput& receiver, const StickInput& sticks,
                             BiCopterMixer& mixer, Settings& settings, LoopStats& stats)
  : m_flight(flight), m_receiver(receiver), m_sticks(sticks), m_mixer(mixer), m_settings(settings), m_stats(stats)
{
}

void SerialConsole::begin(const char* banner)
{
  printLine("");
  printLine(banner);
  printLine("type h + Enter for help");
}

void SerialConsole::update(uint32_t nowMs)
{
  while (Serial.available() > 0)
  {
    const char c = static_cast<char>(Serial.read());
    if ((c == '\n') || (c == '\r'))
    {
      if (m_lineLength > 0)
      {
        m_line[m_lineLength] = '\0';
        handleCommand(m_line);
        m_lineLength = 0;
      }
    }
    else if (m_lineLength < sizeof(m_line) - 1)
    {
      m_line[m_lineLength++] = c;
    }
  }

  if (m_flight.eventCount() != m_seenEvents)
  {
    m_seenEvents = m_flight.eventCount();
    printf(">> %s", m_flight.lastEvent());
  }

  if ((m_stream != Stream::NONE) && ((nowMs - m_lastStreamMs) >= STREAM_PERIOD_MS))
  {
    m_lastStreamMs = nowMs;
    printStream();
  }

  if (m_status && ((nowMs - m_lastStatusMs) >= STATUS_PERIOD_MS))
  {
    m_lastStatusMs = nowMs;
    printStatus();
  }

  drainOutput();
}

bool SerialConsole::takeGyroCalibrationRequest()
{
  const bool request = m_gyroCalRequest;
  m_gyroCalRequest = false;
  return request;
}

void SerialConsole::handleCommand(char* line)
{
  const bool disarmed = !m_flight.isArmed();
  char       side = 0;
  int        value = 0;
  float      p = 0.0f;
  float      i = 0.0f;
  float      d = 0.0f;

  if (strcmp(line, "h") == 0)
  {
    printHelp();
  }
  else if (strcmp(line, "s") == 0)
  {
    m_status = !m_status;
  }
  else if (strcmp(line, "i") == 0)
  {
    m_stream = Stream::IMU;
  }
  else if (strcmp(line, "r") == 0)
  {
    m_stream = Stream::RC;
  }
  else if (strcmp(line, "o") == 0)
  {
    m_stream = Stream::OUTPUTS;
  }
  else if (strcmp(line, "f") == 0)
  {
    m_stream = Stream::FLIGHT;
  }
  else if (strcmp(line, "x") == 0)
  {
    m_stream = Stream::NONE;
    m_status = false;
  }
  else if (strcmp(line, "pid") == 0)
  {
    printGains();
  }
  else if (!disarmed)
  {
    printLine("disarm first");
  }
  else if (strcmp(line, "g") == 0)
  {
    m_gyroCalRequest = true;
  }
  else if (strcmp(line, "l") == 0)
  {
    m_flight.calibrateLevel();
  }
  else if (sscanf(line, "trim %c %d", &side, &value) == 2)
  {
    const StoredSettings& s = m_settings.get();
    value = constrain(value, -300, 300);
    if (side == 'l')
      m_settings.setServoTrims(static_cast<int16_t>(value), s.servoTrimRightUs);
    else if (side == 'r')
      m_settings.setServoTrims(s.servoTrimLeftUs, static_cast<int16_t>(value));
    m_mixer.setServoTrims(m_settings.get().servoTrimLeftUs, m_settings.get().servoTrimRightUs);
    printf("servo trims L=%d R=%d us (type 'save' to keep)", m_settings.get().servoTrimLeftUs,
           m_settings.get().servoTrimRightUs);
  }
  else if (sscanf(line, "pid %c %f %f %f", &side, &p, &i, &d) == 4)
  {
    const int axis = axisIndex(side);
    if ((axis < 0) || (p < 0.0f) || (i < 0.0f) || (d < 0.0f) || (p > 0.05f) || (i > 0.05f) || (d > 0.01f))
    {
      printLine("usage: pid r|p|y P I D   e.g. pid p 0.0025 0.004 0   (P,I <= 0.05, D <= 0.01)");
    }
    else
    {
      m_flight.setRateGains(static_cast<uint8_t>(axis), p, i, d);
      printGains();
    }
  }
  else if (sscanf(line, "ang %c %f", &side, &p) == 2)
  {
    const int axis = axisIndex(side);
    if ((axis < 0) || (p < 0.0f) || (p > 15.0f) || ((axis < 2) && (p == 0.0f)))
    {
      printLine("usage: ang r|p|y P   e.g. ang r 3.0   (0 < P <= 15, ang y 0 = heading hold off)");
    }
    else
    {
      m_flight.setAngleP(static_cast<uint8_t>(axis), p);
      printGains();
    }
  }
  else if (strcmp(line, "save") == 0)
  {
    m_settings.setGains(m_flight.exportGains());
    printLine(m_settings.save() ? "saved trims + level + gains" : "save FAILED");
  }
  else if (strcmp(line, "defaults") == 0)
  {
    m_settings.restoreDefaults();
    m_mixer.setServoTrims(0, 0);
    m_flight.restoreDefaultGains();
    printLine("level, trims and gains back to Config.hpp defaults (type 'save' to keep)");
  }
  else if (strcmp(line, "reboot") == 0)
  {
    Serial.flush();
    ESP.restart();
  }
  else
  {
    printf("unknown '%s', h for help", line);
  }
}

void SerialConsole::printHelp()
{
  printLine("---- bicopter console ----");
  printLine(" s           status line on/off (1 Hz)");
  printLine(" i / r / o   stream IMU / radio / outputs (10 Hz)");
  printLine(" f           flight log: angles, rates, commands, outputs (10 Hz)");
  printLine(" x           stop all streams");
  printLine(" g           re-calibrate gyro (keep still)        [disarmed]");
  printLine(" l           save current attitude as level        [disarmed]");
  printLine(" trim l|r N  servo centre trim in us, e.g. trim l 20 [disarmed]");
  printLine(" pid         show gains");
  printLine(" pid r|p|y P I D   set rate gains, e.g. pid y 0.0025 0.003 0 [disarmed]");
  printLine(" ang r|p|y P self-level (r,p) / heading-hold (y) gain       [disarmed]");
  printLine(" save        write trims + gains to flash          [disarmed]");
  printLine(" defaults    level, trims, gains back to Config.hpp [disarmed]");
  printLine(" reboot");
  printLine("Arm: throttle low + yaw RIGHT 1 s. Disarm: throttle low + yaw LEFT 1 s.");
  printLine("Level cal: disarmed, throttle low, pitch full fwd/back 3 s.");
}

int SerialConsole::axisIndex(char axis)
{
  switch (axis)
  {
    case 'r': return 0;
    case 'p': return 1;
    case 'y': return 2;
    default: return -1;
  }
}

void SerialConsole::printGains()
{
  static const char* const names[3] = {"roll ", "pitch", "yaw  "};
  for (uint8_t axis = 0; axis < 3; axis++)
  {
    const PidGains& g = m_flight.rateGains(axis);
    if (axis < 2)
    {
      printf("%s rate P %.5f I %.5f D %.5f | angle P %.2f", names[axis], g.kp, g.ki, g.kd, m_flight.angleP(axis));
    }
    else
    {
      printf("%s rate P %.5f I %.5f D %.5f | heading P %.2f", names[axis], g.kp, g.ki, g.kd, m_flight.angleP(axis));
    }
  }
}

void SerialConsole::printStatus()
{
  const FlightTelemetry& t = m_flight.telemetry();
  printf("[%s] roll %+5.1f pitch %+5.1f | rx %s%s | loop %lu Hz max %lu us ovr %lu | imu err %lu%s%s",
         FlightController::stateName(t.state), t.rollDeg, t.pitchDeg, m_receiver.signalLost() ? "LOST" : "ok",
         m_receiver.usingHardwareCapture() ? "" : "(irq)", static_cast<unsigned long>(m_stats.loopHz),
         static_cast<unsigned long>(m_stats.maxLoopUs), static_cast<unsigned long>(m_stats.overruns),
         static_cast<unsigned long>(t.imuErrors), t.armBlock ? " | " : "", t.armBlock ? t.armBlock : "");
  m_stats.maxLoopUs = 0;
}

void SerialConsole::printStream()
{
  const FlightTelemetry& t = m_flight.telemetry();
  switch (m_stream)
  {
    case Stream::IMU:
    {
      const float g = sqrtf(t.accelG[0] * t.accelG[0] + t.accelG[1] * t.accelG[1] + t.accelG[2] * t.accelG[2]);
      printf("roll %+6.1f pitch %+6.1f | gyro r %+6.1f p %+6.1f y %+6.1f dps | acc %+5.2f %+5.2f %+5.2f = %4.2f g %s | "
             "id 0x%02X zoff %+.2f",
             t.rollDeg, t.pitchDeg, t.gyroDps[0], t.gyroDps[1], t.gyroDps[2], t.accelG[0], t.accelG[1], t.accelG[2], g,
             t.accelUsed ? "used" : "IGNORED", t.imuId, m_flight.accelZOffsetG());
    }
    break;

    case Stream::RC:
    {
      const Sticks& s = m_sticks.sticks();
      printf("us %4u %4u %4u %4u | ctr %4u %4u %4u%s | roll %+5.2f pitch %+5.2f thr %4.2f yaw %+5.2f | %s%s%s%s",
             s.rawUs[0], s.rawUs[1], s.rawUs[2], s.rawUs[3], m_sticks.centreUs(0), m_sticks.centreUs(1),
             m_sticks.centreUs(3), m_sticks.centresLocked() ? "" : " (learning)", s.roll, s.pitch, s.throttle, s.yaw,
             s.valid ? "valid" : "FAILSAFE", s.armGesture ? " ARM-gesture" : "",
             s.disarmGesture ? " DISARM-gesture" : "", s.levelGesture ? " LEVEL-gesture" : "");
      break;
    }

    case Stream::OUTPUTS:
    {
      const MixerOutputs& o = m_mixer.outputs();
      printf("cmd r %+5.2f p %+5.2f y %+5.2f | ESC L %4u R %4u | servo L %4u R %4u | @50%%thr ESC L %4u R %4u",
             t.command[0], t.command[1], t.command[2], o.escLeftUs, o.escRightUs, o.servoLeftUs, o.servoRightUs,
             t.preview.escLeftUs, t.preview.escRightUs);
      break;
    }

    case Stream::FLIGHT:
    {
      const MixerOutputs& o = m_mixer.outputs();
      printf("%s thr %.2f | R %+5.1f P %+5.1f | rate r %+6.1f/%+6.1f p %+6.1f/%+6.1f y %+6.1f/%+6.1f | cmd %+5.2f "
             "%+5.2f %+5.2f yI %+5.2f | ESC %4u %4u | srv %4u %4u diff %+4d | zdrift %+.2f | hover tilt %+.1f",
             FlightController::stateName(t.state), t.throttle, t.rollDeg, t.pitchDeg, t.gyroDps[0], t.rateSetDps[0],
             t.gyroDps[1], t.rateSetDps[1], t.gyroDps[2], t.rateSetDps[2], t.command[0], t.command[1], t.command[2],
             t.yawI, o.escLeftUs, o.escRightUs, o.servoLeftUs, o.servoRightUs,
             static_cast<int>(o.servoLeftUs) + static_cast<int>(o.servoRightUs) - 3000, t.yawGyroDriftDps,
             t.hoverMotorTiltDeg);
      break;
    }

    case Stream::NONE:
    default: break;
  }
}

void SerialConsole::printLine(const char* text)
{
  printf("%s", text);
}

void SerialConsole::printf(const char* format, ...)
{
  char    buffer[256];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(buffer, sizeof(buffer) - 2, format, args);
  va_end(args);
  if (length < 0)
  {
    return;
  }
  length = min<int>(length, sizeof(buffer) - 3);
  buffer[length++] = '\r';
  buffer[length++] = '\n';

  // Queue the whole line or drop it, never half a line
  const uint16_t used = static_cast<uint16_t>((m_txHead - m_txTail + TX_QUEUE_SIZE) % TX_QUEUE_SIZE);
  if (length > (TX_QUEUE_SIZE - 1 - used))
  {
    return;
  }
  for (int i = 0; i < length; i++)
  {
    m_txQueue[m_txHead] = buffer[i];
    m_txHead = (m_txHead + 1) % TX_QUEUE_SIZE;
  }
  drainOutput();
}

void SerialConsole::drainOutput()
{
  if (!Serial)
  {
    m_txTail = m_txHead;  // no terminal: discard instead of sending stale lines later
    return;
  }
  while (m_txTail != m_txHead)
  {
    const int space = Serial.availableForWrite();
    if (space <= 0)
    {
      return;
    }
    // Contiguous run up to the end of the ring or the head
    const uint16_t end = (m_txHead > m_txTail) ? m_txHead : TX_QUEUE_SIZE;
    const uint16_t chunk = static_cast<uint16_t>(min<int>(end - m_txTail, space));
    const size_t   sent = Serial.write(reinterpret_cast<const uint8_t*>(&m_txQueue[m_txTail]), chunk);
    if (sent == 0)
    {
      return;
    }
    m_txTail = static_cast<uint16_t>((m_txTail + sent) % TX_QUEUE_SIZE);
  }
}

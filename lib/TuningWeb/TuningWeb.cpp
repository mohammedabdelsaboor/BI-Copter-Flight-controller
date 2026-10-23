/**
 * @file   TuningWeb.cpp
 */
#include "TuningWeb.hpp"
#include <WiFi.h>
#include <WebServer.h>
#include "TuningPage.hpp"

namespace
{
constexpr uint32_t PUBLISH_PERIOD_MS = 50;   // snapshot rate, the page polls slower than this
constexpr uint32_t APPLY_TIMEOUT_MS = 1500;  // the control loop may be busy (gyro calibration, flash write)

// Same limits as the console
constexpr float MAX_RATE_PI = 0.05f;
constexpr float MAX_RATE_D = 0.01f;
constexpr float MAX_ANGLE_P = 15.0f;
constexpr float MAX_LEVEL_DEG = 30.0f;
constexpr int   MAX_TRIM_US = 300;

/** Copies text into a JSON string body, double quotes and backslashes become single quotes. */
void copyJsonSafe(char* out, size_t size, const char* text)
{
  size_t n = 0;
  for (; (text != nullptr) && (text[n] != '\0') && (n < size - 1); n++)
  {
    out[n] = ((text[n] == '"') || (text[n] == '\\') || (text[n] < ' ')) ? '\'' : text[n];
  }
  out[n] = '\0';
}
}  // namespace

TuningWeb::TuningWeb(FlightController& flight, BiCopterMixer& mixer, Settings& settings)
  : m_flight(flight), m_mixer(mixer), m_settings(settings)
{
}

bool TuningWeb::begin(const TuningWebConfig& config)
{
  m_config = config;
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(config.ssid, config.password, config.channel))
  {
    return false;
  }
  WiFi.setTxPower(static_cast<wifi_power_t>(config.txPowerQdbm));

  m_server = new WebServer(80);
  setupRoutes();
  m_server->begin();
  publish(0);
  m_running = true;
  // Core 0, next to the WiFi stack. The control loop owns core 1.
  return xTaskCreatePinnedToCore(serverTask, "tuningWeb", 8192, this, 1, nullptr, 0) == pdPASS;
}

void TuningWeb::serverTask(void* arg)
{
  TuningWeb* self = static_cast<TuningWeb*>(arg);
  for (;;)
  {
    self->m_server->handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void TuningWeb::setupRoutes()
{
  m_server->on("/", HTTP_GET, [this]() { m_server->send(200, "text/html", TUNING_PAGE); });
  m_server->on("/api/state", HTTP_GET, [this]() { handleState(); });
  m_server->on("/api/log", HTTP_GET, [this]() { handleLog(); });
  m_server->on("/api/gains", HTTP_POST, [this]() { handleGains(); });
  m_server->on("/api/level", HTTP_POST, [this]() { handleLevel(); });
  m_server->on("/api/trims", HTTP_POST, [this]() { handleTrims(); });
  m_server->on("/api/action", HTTP_POST, [this]() { handleAction(); });
  m_server->onNotFound(
      [this]()
      {
        m_server->sendHeader("Location", "/");
        m_server->send(302, "text/plain", "");
      });
}

// ---------------------------------------------------------------------------------------------
//  Control loop side
// ---------------------------------------------------------------------------------------------
void TuningWeb::update(uint32_t nowMs, uint32_t loopHz)
{
  if (!m_running)
  {
    return;
  }

  Request  request = {};
  uint32_t id = 0;
  portENTER_CRITICAL(&m_lock);
  const bool pending = (m_requestId != m_doneId);
  if (pending)
  {
    request = m_request;
    id = m_requestId;
  }
  portEXIT_CRITICAL(&m_lock);

  if (pending)
  {
    apply(request);  // writes m_reply under the lock and marks the request done
    portENTER_CRITICAL(&m_lock);
    m_doneId = id;
    portEXIT_CRITICAL(&m_lock);
    publish(loopHz);  // the page reloads right after a change, give it fresh values
  }
  else if ((nowMs - m_lastPublishMs) >= PUBLISH_PERIOD_MS)
  {
    m_lastPublishMs = nowMs;
    publish(loopHz);
  }

  if ((nowMs - m_lastLogMs) >= (1000 / LOG_HZ))
  {
    m_lastLogMs = nowMs;
    recordSample(nowMs);
  }
}

void TuningWeb::recordSample(uint32_t nowMs)
{
  const FlightTelemetry& t = m_flight.telemetry();
  const MixerOutputs&    o = m_mixer.outputs();
  LogSample              s = {};
  s.ms = nowMs;
  s.state = static_cast<uint8_t>(m_flight.state());
  s.accelUsed = t.accelUsed;
  s.throttle = t.throttle;
  s.roll = t.rollDeg;
  s.pitch = t.pitchDeg;
  for (int i = 0; i < 3; i++)
  {
    s.gyro[i] = t.gyroDps[i];
    s.rateSet[i] = t.rateSetDps[i];
    s.command[i] = t.command[i];
  }
  s.yawI = t.yawI;
  s.hoverTilt = t.hoverMotorTiltDeg;
  s.hoverTrim = t.hoverPitchTrimDeg;
  s.servoUs[0] = o.servoLeftUs;
  s.servoUs[1] = o.servoRightUs;
  s.escUs[0] = o.escLeftUs;
  s.escUs[1] = o.escRightUs;

  portENTER_CRITICAL(&m_lock);
  s.seq = ++m_logSeq;
  m_log[s.seq % LOG_SIZE] = s;
  portEXIT_CRITICAL(&m_lock);
}

bool TuningWeb::takeGyroCalibrationRequest()
{
  const bool request = m_gyroCalRequest;
  m_gyroCalRequest = false;
  return request;
}

void TuningWeb::apply(const Request& request)
{
  char reply[sizeof(m_reply)] = {};

  if (m_flight.isArmed())
  {
    snprintf(reply, sizeof(reply), "refused: disarm first");
  }
  else
  {
    switch (request.action)
    {
      case Action::SET_GAINS:
        m_flight.applyGains(request.gains);
        m_unsaved = true;
        snprintf(reply, sizeof(reply), "gains applied - press Save to keep them");
        break;

      case Action::SET_LEVEL:
        m_settings.setLevel(request.level[0], request.level[1]);
        m_unsaved = true;
        snprintf(reply, sizeof(reply), "level offsets applied - press Save to keep them");
        break;

      case Action::LEVEL_NOW:
        snprintf(reply, sizeof(reply), "%s",
                 m_flight.calibrateLevel() ? "level calibrated and saved" : "level calibration failed");
        break;

      case Action::SET_TRIMS:
        m_settings.setServoTrims(request.trimUs[0], request.trimUs[1]);
        m_mixer.setServoTrims(request.trimUs[0], request.trimUs[1]);
        m_unsaved = true;
        snprintf(reply, sizeof(reply), "servo trims L %d R %d us applied", request.trimUs[0], request.trimUs[1]);
        break;

      case Action::SAVE:
      {
        m_settings.setGains(m_flight.exportGains());
        const bool ok = m_settings.save();
        m_unsaved = m_unsaved && !ok;
        snprintf(reply, sizeof(reply), ok ? "saved level + trims + gains to flash" : "save FAILED");
        break;
      }

      case Action::DEFAULTS:
        m_settings.restoreDefaults();
        m_mixer.setServoTrims(0, 0);
        m_flight.restoreDefaultGains();
        m_unsaved = true;
        snprintf(reply, sizeof(reply), "back to Config.hpp defaults - press Save to keep them");
        break;

      case Action::GYRO_CAL:
        m_gyroCalRequest = true;
        snprintf(reply, sizeof(reply), "gyro calibration started - keep the model still");
        break;

      case Action::NONE:
      default: snprintf(reply, sizeof(reply), "nothing to do"); break;
    }
  }

  portENTER_CRITICAL(&m_lock);
  memcpy(m_reply, reply, sizeof(m_reply));
  portEXIT_CRITICAL(&m_lock);
}

void TuningWeb::publish(uint32_t loopHz)
{
  const FlightTelemetry& t = m_flight.telemetry();
  const MixerOutputs&    o = m_mixer.outputs();
  const StoredSettings&  s = m_settings.get();

  Snapshot snap = {};
  copyJsonSafe(snap.state, sizeof(snap.state), FlightController::stateName(m_flight.state()));
  copyJsonSafe(snap.armBlock, sizeof(snap.armBlock), t.armBlock);
  copyJsonSafe(snap.event, sizeof(snap.event), m_flight.lastEvent());
  snap.eventCount = m_flight.eventCount();
  snap.loopHz = loopHz;
  snap.armed = m_flight.isArmed();
  snap.accelUsed = t.accelUsed;
  snap.unsaved = m_unsaved;
  snap.rollDeg = t.rollDeg;
  snap.pitchDeg = t.pitchDeg;
  for (int i = 0; i < 3; i++)
  {
    snap.gyroDps[i] = t.gyroDps[i];
  }
  snap.throttle = t.throttle;
  snap.hoverTiltDeg = t.hoverMotorTiltDeg;
  snap.hoverTrimDeg = t.hoverPitchTrimDeg;
  snap.servoUs[0] = o.servoLeftUs;
  snap.servoUs[1] = o.servoRightUs;
  snap.escUs[0] = o.escLeftUs;
  snap.escUs[1] = o.escRightUs;
  snap.level[0] = s.levelRollDeg;
  snap.level[1] = s.levelPitchDeg;
  snap.trimUs[0] = s.servoTrimLeftUs;
  snap.trimUs[1] = s.servoTrimRightUs;
  snap.gains = m_flight.exportGains();

  portENTER_CRITICAL(&m_lock);
  m_snapshot = snap;
  portEXIT_CRITICAL(&m_lock);
}

// ---------------------------------------------------------------------------------------------
//  Web task side
// ---------------------------------------------------------------------------------------------
void TuningWeb::submit(const Request& request)
{
  portENTER_CRITICAL(&m_lock);
  m_request = request;
  const uint32_t id = m_requestId + 1;
  m_requestId = id;
  portEXIT_CRITICAL(&m_lock);

  const uint32_t startMs = millis();
  while ((m_doneId != id) && ((millis() - startMs) < APPLY_TIMEOUT_MS))
  {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  if (m_doneId != id)
  {
    m_server->send(503, "text/plain", "flight controller busy, try again");
    return;
  }
  char reply[sizeof(m_reply)];
  portENTER_CRITICAL(&m_lock);
  memcpy(reply, m_reply, sizeof(reply));
  portEXIT_CRITICAL(&m_lock);
  m_server->send(strncmp(reply, "refused", 7) == 0 ? 409 : 200, "text/plain", reply);
}

float TuningWeb::argFloat(const char* name, float fallback) const
{
  const String text = m_server->arg(name);
  if (text.length() == 0)
  {
    return fallback;
  }
  const float value = text.toFloat();
  return isfinite(value) ? value : fallback;
}

void TuningWeb::handleState()
{
  Snapshot snap;
  portENTER_CRITICAL(&m_lock);
  snap = m_snapshot;
  portEXIT_CRITICAL(&m_lock);

  // Motor tilt from the servo pulse, positive = motor tilted forward
  float tiltDeg[2];
  float trimDeg[2];
  for (int i = 0; i < 2; i++)
  {
    const bool  reversed = (i == 0) ? m_config.servoLeftReversed : m_config.servoRightReversed;
    const float direction = reversed ? -1.0f : 1.0f;
    tiltDeg[i] = direction * (static_cast<float>(snap.servoUs[i]) - m_config.servoCenterUs) / m_config.servoUsPerDeg;
    trimDeg[i] = direction * static_cast<float>(snap.trimUs[i]) / m_config.servoUsPerDeg;
  }
  const StoredGains& g = snap.gains;

  char json[1400];
  snprintf(json, sizeof(json),
           "{\"state\":\"%s\",\"armBlock\":\"%s\",\"event\":\"%s\",\"eventCount\":%lu,\"loopHz\":%lu,"
           "\"armed\":%s,\"accelUsed\":%s,\"unsaved\":%s,"
           "\"roll\":%.2f,\"pitch\":%.2f,\"rawRoll\":%.2f,\"rawPitch\":%.2f,\"gyro\":[%.1f,%.1f,%.1f],"
           "\"throttle\":%.2f,\"hoverTilt\":%.1f,\"hoverTrim\":%.1f,\"servoUs\":[%u,%u],\"escUs\":[%u,%u],\"tiltDeg\":[%.1f,%.1f],"
           "\"level\":[%.2f,%.2f],\"trimUs\":[%d,%d],\"trimDeg\":[%.1f,%.1f],\"usPerDeg\":%.2f,"
           "\"rateP\":[%.5f,%.5f,%.5f],\"rateI\":[%.5f,%.5f,%.5f],\"rateD\":[%.5f,%.5f,%.5f],"
           "\"angleP\":[%.2f,%.2f,%.2f]}",
           snap.state, snap.armBlock, snap.event, static_cast<unsigned long>(snap.eventCount),
           static_cast<unsigned long>(snap.loopHz), snap.armed ? "true" : "false", snap.accelUsed ? "true" : "false",
           snap.unsaved ? "true" : "false", snap.rollDeg, snap.pitchDeg, snap.rollDeg + snap.level[0],
           snap.pitchDeg + snap.level[1], snap.gyroDps[0], snap.gyroDps[1], snap.gyroDps[2], snap.throttle,
           snap.hoverTiltDeg, snap.hoverTrimDeg, snap.servoUs[0], snap.servoUs[1], snap.escUs[0], snap.escUs[1], tiltDeg[0], tiltDeg[1], snap.level[0],
           snap.level[1], snap.trimUs[0], snap.trimUs[1], trimDeg[0], trimDeg[1], m_config.servoUsPerDeg, g.rateP[0],
           g.rateP[1], g.rateP[2], g.rateI[0], g.rateI[1], g.rateI[2], g.rateD[0], g.rateD[1], g.rateD[2], g.angleP[0],
           g.angleP[1], g.angleP[2]);
  m_server->sendHeader("Cache-Control", "no-store");
  m_server->send(200, "application/json", json);
}

void TuningWeb::handleLog()
{
  // CSV lines for every sample newer than ?since=<seq>. Copied one at a time to keep the lock short.
  const uint32_t since = static_cast<uint32_t>(m_server->arg("since").toInt());
  portENTER_CRITICAL(&m_lock);
  const uint32_t newest = m_logSeq;
  portEXIT_CRITICAL(&m_lock);
  uint32_t first = since + 1;
  if ((newest >= LOG_SIZE) && (first <= newest - LOG_SIZE))
  {
    first = newest - LOG_SIZE + 1;  // the page fell behind, older samples are gone
  }

  String out;
  out.reserve(160 * LOG_SIZE);
  char line[200];
  for (uint32_t seq = first; seq <= newest; seq++)
  {
    LogSample s;
    portENTER_CRITICAL(&m_lock);
    s = m_log[seq % LOG_SIZE];
    portEXIT_CRITICAL(&m_lock);
    if (s.seq != seq)
    {
      continue;  // overwritten while we were reading
    }
    snprintf(line, sizeof(line),
             "%lu,%lu,%s,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%u,%u,%u,%u,%d\n",
             static_cast<unsigned long>(s.seq), static_cast<unsigned long>(s.ms),
             FlightController::stateName(static_cast<FlightState>(s.state)), s.throttle, s.roll, s.pitch, s.gyro[0],
             s.gyro[1], s.gyro[2], s.rateSet[0], s.rateSet[1], s.rateSet[2], s.command[0], s.command[1], s.command[2],
             s.yawI, s.hoverTilt, s.hoverTrim, s.servoUs[0], s.servoUs[1], s.escUs[0], s.escUs[1], s.accelUsed ? 1 : 0);
    out += line;
  }
  m_server->sendHeader("Cache-Control", "no-store");
  m_server->send(200, "text/plain", out);
}

void TuningWeb::handleGains()
{
  Snapshot snap;
  portENTER_CRITICAL(&m_lock);
  snap = m_snapshot;
  portEXIT_CRITICAL(&m_lock);

  // Fields left out keep their current value
  Request request = {};
  request.action = Action::SET_GAINS;
  request.gains = snap.gains;
  request.gains.valid = true;
  char name[4] = {};
  for (int axis = 0; axis < 3; axis++)
  {
    StoredGains& g = request.gains;
    snprintf(name, sizeof(name), "p%d", axis);
    g.rateP[axis] = argFloat(name, g.rateP[axis]);
    snprintf(name, sizeof(name), "i%d", axis);
    g.rateI[axis] = argFloat(name, g.rateI[axis]);
    snprintf(name, sizeof(name), "d%d", axis);
    g.rateD[axis] = argFloat(name, g.rateD[axis]);
    snprintf(name, sizeof(name), "a%d", axis);
    g.angleP[axis] = argFloat(name, g.angleP[axis]);

    const bool rateOk = (g.rateP[axis] >= 0.0f) && (g.rateP[axis] <= MAX_RATE_PI) && (g.rateI[axis] >= 0.0f)
                        && (g.rateI[axis] <= MAX_RATE_PI) && (g.rateD[axis] >= 0.0f) && (g.rateD[axis] <= MAX_RATE_D);
    // Roll / pitch self level needs some P, yaw heading hold may be 0 (off)
    const bool angleOk =
        (g.angleP[axis] >= 0.0f) && (g.angleP[axis] <= MAX_ANGLE_P) && ((axis == 2) || (g.angleP[axis] > 0.0f));
    if (!rateOk || !angleOk)
    {
      m_server->send(400, "text/plain",
                     "out of range: rate P, I 0..0.05, D 0..0.01, angle P 0..15 (roll/pitch above 0)");
      return;
    }
  }
  submit(request);
}

void TuningWeb::handleLevel()
{
  Request request = {};
  request.action = Action::SET_LEVEL;
  request.level[0] = argFloat("roll", NAN);
  request.level[1] = argFloat("pitch", NAN);
  if (!(fabsf(request.level[0]) <= MAX_LEVEL_DEG) || !(fabsf(request.level[1]) <= MAX_LEVEL_DEG))
  {
    m_server->send(400, "text/plain", "level offsets must be within +-30 deg");
    return;
  }
  submit(request);
}

void TuningWeb::handleTrims()
{
  Request request = {};
  request.action = Action::SET_TRIMS;
  const float left = argFloat("left", NAN);
  const float right = argFloat("right", NAN);
  if (!(fabsf(left) <= MAX_TRIM_US) || !(fabsf(right) <= MAX_TRIM_US))
  {
    m_server->send(400, "text/plain", "servo trims must be within +-300 us");
    return;
  }
  request.trimUs[0] = static_cast<int16_t>(lroundf(left));
  request.trimUs[1] = static_cast<int16_t>(lroundf(right));
  submit(request);
}

void TuningWeb::handleAction()
{
  const String name = m_server->arg("do");
  Request      request = {};
  if (name == "levelNow")
    request.action = Action::LEVEL_NOW;
  else if (name == "save")
    request.action = Action::SAVE;
  else if (name == "defaults")
    request.action = Action::DEFAULTS;
  else if (name == "gyro")
    request.action = Action::GYRO_CAL;
  else
  {
    m_server->send(400, "text/plain", "unknown action");
    return;
  }
  submit(request);
}

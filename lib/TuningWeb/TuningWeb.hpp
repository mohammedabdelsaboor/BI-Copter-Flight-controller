/**
 * @file   TuningWeb.hpp
 * @brief  WiFi access point + web page for bench tuning: live attitude, level calibration, servo tilt
 *         trims and PID gains. Open http://192.168.4.1 after joining the access point.
 *
 * The web server runs in its own task on core 0, away from the control loop on core 1. It never
 * touches the flight objects itself:
 *   - update() (control loop) copies a telemetry snapshot that the web task reads,
 *   - a change from the page is queued as a request that update() applies between two control steps.
 * So the page cannot stall or race the control loop. Changes are refused while armed.
 */
#pragma once

#include <Arduino.h>
#include "FlightController.hpp"
#include "BiCopterMixer.hpp"
#include "Settings.hpp"

class WebServer;

struct TuningWebConfig
{
  const char* ssid;
  const char* password;
  uint8_t     channel;
  int8_t      txPowerQdbm;  // quarter dBm
  uint16_t    servoCenterUs;
  float       servoUsPerDeg;
  bool        servoLeftReversed;
  bool        servoRightReversed;
};

class TuningWeb
{
public:
  TuningWeb(FlightController& flight, BiCopterMixer& mixer, Settings& settings);

  /** Starts the access point and the web server task. */
  bool begin(const TuningWebConfig& config);

  /** Publishes telemetry and applies a queued change. Call every loop, from the control loop. */
  void update(uint32_t nowMs, uint32_t loopHz);

  /** True once after "recalibrate gyro" on the page. The sketch runs the (blocking) calibration. */
  bool takeGyroCalibrationRequest();

private:
  enum class Action : uint8_t
  {
    NONE,
    SET_GAINS,
    SET_LEVEL,
    LEVEL_NOW,
    SET_TRIMS,
    SAVE,
    DEFAULTS,
    GYRO_CAL,
    HOVER_RESET
  };

  struct Request
  {
    Action      action;
    StoredGains gains;
    float       level[2];  // roll, pitch, deg
    int16_t     trimUs[2];  // left, right
  };

  struct Snapshot
  {
    char     state[12];
    char     armBlock[72];
    char     event[96];
    uint32_t eventCount;
    uint32_t loopHz;
    bool     armed;
    bool     accelUsed;
    bool     unsaved;
    float    rollDeg;  // level corrected, what the controller holds
    float    pitchDeg;
    float    gyroDps[3];
    float    throttle;
    float    hoverTiltDeg;  // learned motor tilt in hover, + = forward
    float    hoverTrimDeg;  // body pitch offset applied for it
    uint16_t servoUs[2];
    uint16_t escUs[2];
    float    level[2];
    int16_t  trimUs[2];
    StoredGains gains;
  };

  /** One flight recorder sample, taken by the control loop at LOG_HZ. */
  struct LogSample
  {
    uint32_t seq;
    uint32_t ms;
    uint8_t  state;
    bool     accelUsed;
    float    throttle;
    float    roll;
    float    pitch;
    float    gyro[3];
    float    rateSet[3];
    float    command[3];
    float    yawI;
    float    hoverTilt;
    float    hoverTrim;
    uint16_t servoUs[2];
    uint16_t escUs[2];
  };
  static constexpr uint32_t LOG_HZ = 25;
  static constexpr uint16_t LOG_SIZE = 64;  // 2.5 s of samples, the page fetches every 200 ms

  static void serverTask(void* arg);
  void handleLog();
  void recordSample(uint32_t nowMs);
  void setupRoutes();
  void handleState();
  void handleGains();
  void handleLevel();
  void handleTrims();
  void handleAction();
  /** Web task: queues the request and waits for the control loop to apply it. */
  void submit(const Request& request);
  void apply(const Request& request);
  void publish(uint32_t loopHz);
  float argFloat(const char* name, float fallback) const;

  FlightController& m_flight;
  BiCopterMixer&    m_mixer;
  Settings&         m_settings;
  TuningWebConfig   m_config = {};
  WebServer*        m_server = nullptr;

  portMUX_TYPE      m_lock = portMUX_INITIALIZER_UNLOCKED;
  Snapshot          m_snapshot = {};  // guarded by m_lock
  Request           m_request = {};   // guarded by m_lock
  volatile uint32_t m_requestId = 0;  // web task increments when it queues a request
  volatile uint32_t m_doneId = 0;     // control loop sets it to m_requestId when applied
  char              m_reply[96] = {};  // written by the control loop before m_doneId
  bool              m_running = false;
  bool              m_unsaved = false;
  bool              m_gyroCalRequest = false;
  uint32_t          m_lastPublishMs = 0;
  uint32_t          m_lastLogMs = 0;
  LogSample         m_log[LOG_SIZE] = {};  // ring, guarded by m_lock
  uint32_t          m_logSeq = 0;          // sequence number of the newest sample, guarded by m_lock
};

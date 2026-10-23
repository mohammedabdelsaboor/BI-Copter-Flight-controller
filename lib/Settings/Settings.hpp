/**
 * @file   Settings.hpp
 * @brief  Values that are calibrated on the model and kept in flash (NVS) across power cycles.
 */
#pragma once

#include <Arduino.h>

struct StoredSettings
{
  float   levelRollDeg;  // attitude that counts as level (IMU not mounted perfectly flat)
  float   levelPitchDeg;
  int16_t servoTrimLeftUs;  // tilt servo centre corrections
  int16_t servoTrimRightUs;
};

/** PID gains tuned from the console. Axes: 0 roll, 1 pitch, 2 yaw. */
struct StoredGains
{
  bool  valid;  // false = use the defaults from Config.hpp
  float rateP[3];
  float rateI[3];
  float rateD[3];
  float angleP[3];  // roll, pitch (self level), yaw (heading hold)
};

class Settings
{
public:
  /** Loads from flash, or defaults if nothing valid is stored. */
  bool begin();
  bool save();
  void restoreDefaults();

  const StoredSettings& get() const { return m_data; }
  void setLevel(float rollDeg, float pitchDeg)
  {
    m_data.levelRollDeg = rollDeg;
    m_data.levelPitchDeg = pitchDeg;
  }
  void setServoTrims(int16_t leftUs, int16_t rightUs)
  {
    m_data.servoTrimLeftUs = leftUs;
    m_data.servoTrimRightUs = rightUs;
  }

  const StoredGains& gains() const { return m_gains; }
  void setGains(const StoredGains& gains)
  {
    m_gains = gains;
    m_gains.valid = true;
  }
  void clearGains() { m_gains = {}; }

  /** Motor tilt learned in hover (FlightController hover compensation), + = forward. */
  float hoverTiltDeg() const { return m_hoverTiltDeg; }
  void setHoverTilt(float deg) { m_hoverTiltDeg = deg; }

private:
  static constexpr const char* NAMESPACE = "bicopter";
  static constexpr const char* KEY = "settings";
  static constexpr uint32_t    VERSION = 1;  // bump when StoredSettings changes
  static constexpr const char* GAINS_KEY = "gains";
  static constexpr uint32_t    GAINS_VERSION = 2;  // bump when StoredGains changes
  static constexpr const char* HOVER_KEY = "hoverTilt";  // own key, so older saves keep level/trims/gains

  struct Blob
  {
    uint32_t       version;
    StoredSettings data;
  };

  struct GainsBlob
  {
    uint32_t    version;
    StoredGains gains;
  };

  StoredSettings m_data = {0.0f, 0.0f, 0, 0};
  StoredGains    m_gains = {};
  float          m_hoverTiltDeg = 0.0f;
};

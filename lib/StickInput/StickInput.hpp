/**
 * @file   StickInput.hpp
 * @brief  Converts the 4 receiver pulses into normalised stick commands and stick gestures.
 *
 * - Learns where each stick really rests, once after boot, then locks it. Radios with trims rarely centre
 *   at exactly 1500 us, and that offset would otherwise look like a constant demand that the I term winds up on.
 *   Trims moved later still work, because the centre no longer follows them.
 * - Detects the receiver failsafe (throttle pulse below the normal range) as well as missing pulses.
 * - Uses a wider deadband on yaw: on a Mode 2 radio the throttle and yaw share one stick.
 */
#pragma once

#include <Arduino.h>
#include "RcPwmInput.hpp"

struct StickConfig
{
  uint8_t  chRoll;
  uint8_t  chPitch;
  uint8_t  chThrottle;
  uint8_t  chYaw;
  bool     reverseRoll;
  bool     reversePitch;
  bool     reverseYaw;
  uint16_t stickMinUs;
  uint16_t stickMaxUs;
  uint16_t deadbandUs;
  uint16_t yawDeadbandUs;
  bool     learnCentres;  // false keeps 1500 us, so transmitter trims act as trims
  uint16_t throttleLowUs;
  uint16_t throttleMaxUs;
  uint16_t failsafeThrottleUs;
  uint16_t yawArmUs;
  uint16_t yawDisarmUs;
  uint16_t pitchGestureUs;
};

struct Sticks
{
  float    roll;      // -1..1, +1 = stick right
  float    pitch;     // -1..1, +1 = stick forward
  float    yaw;       // -1..1, +1 = stick right
  float    throttle;  // 0..1
  bool     throttleLow;
  bool     valid;          // false = signal lost or receiver failsafe
  bool     armGesture;     // throttle low + yaw fully right
  bool     disarmGesture;  // throttle low + yaw fully left
  bool     levelGesture;   // throttle low + pitch fully forward or back
  uint16_t rawUs[RcPwmInput::CHANNELS];
};

class StickInput
{
public:
  void configure(const StickConfig& config);

  /** @param learnCentres  true while disarmed, so centres can follow the trims. */
  const Sticks& update(const RcPwmInput& receiver, bool learnCentres, uint32_t nowMs);

  const Sticks& sticks() const { return m_sticks; }
  uint16_t centreUs(uint8_t channel) const { return m_centreUs[channel]; }
  bool centresLocked() const;

private:
  static constexpr uint16_t MID_US = 1500;
  static constexpr uint16_t CENTRE_TOLERANCE_US = 100;  // a "centre" further than this from 1500 is a held stick
  static constexpr uint16_t STILL_US = 5;
  static constexpr uint32_t STILL_TIME_MS = 2000;

  float stickToUnit(uint16_t pulseUs, uint8_t channel, bool reverse, uint16_t deadbandUs) const;
  void learnCentres(uint32_t nowMs);

  StickConfig m_config = {};
  Sticks      m_sticks = {};
  uint16_t    m_centreUs[RcPwmInput::CHANNELS] = {MID_US, MID_US, MID_US, MID_US};
  uint16_t    m_stillRefUs[RcPwmInput::CHANNELS] = {};
  uint32_t    m_stillSinceMs[RcPwmInput::CHANNELS] = {};
  bool        m_centreLocked[RcPwmInput::CHANNELS] = {};  // learned once after boot, then fixed
};

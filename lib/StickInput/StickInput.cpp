/**
 * @file   StickInput.cpp
 */
#include "StickInput.hpp"

void StickInput::configure(const StickConfig& config)
{
  m_config = config;
  if (!config.learnYawCentre)
  {
    // Sloppy yaw gimbals rest anywhere within +-35 us, so a learned yaw centre is as likely to be off
    // as 1500. A fixed centre plus the wider yaw deadband is the more predictable choice.
    m_centreUs[config.chYaw] = MID_US;
    m_centreLocked[config.chYaw] = true;
  }
}

const Sticks& StickInput::update(const RcPwmInput& receiver, bool learn, uint32_t nowMs)
{
  for (uint8_t i = 0; i < RcPwmInput::CHANNELS; i++)
  {
    m_sticks.rawUs[i] = receiver.pulseUs(i);
  }

  const uint16_t throttleUs = m_sticks.rawUs[m_config.chThrottle];
  const uint16_t yawUs = m_sticks.rawUs[m_config.chYaw];
  const uint16_t pitchUs = m_sticks.rawUs[m_config.chPitch];

  m_sticks.valid = !receiver.signalLost() && (throttleUs >= m_config.failsafeThrottleUs);
  if (!m_sticks.valid)
  {
    // Neutral commands, no gestures
    m_sticks.roll = m_sticks.pitch = m_sticks.yaw = m_sticks.throttle = 0.0f;
    m_sticks.throttleLow = true;
    m_sticks.armGesture = m_sticks.disarmGesture = m_sticks.levelGesture = false;
    return m_sticks;
  }

  if (learn && m_config.learnCentres)
  {
    learnCentres(nowMs);
  }

  m_sticks.roll =
      stickToUnit(m_sticks.rawUs[m_config.chRoll], m_config.chRoll, m_config.reverseRoll, m_config.deadbandUs);
  m_sticks.pitch = stickToUnit(pitchUs, m_config.chPitch, m_config.reversePitch, m_config.deadbandUs);
  m_sticks.yaw = stickToUnit(yawUs, m_config.chYaw, m_config.reverseYaw, m_config.yawDeadbandUs);

  m_sticks.throttleLow = throttleUs < m_config.throttleLowUs;
  const float span = static_cast<float>(m_config.throttleMaxUs - m_config.throttleLowUs);
  const float t = static_cast<float>(static_cast<int32_t>(throttleUs) - m_config.throttleLowUs) / span;
  m_sticks.throttle = m_sticks.throttleLow ? 0.0f : constrain(t, 0.0f, 1.0f);

  // Gestures use raw pulses so a stick that does not reach +-1.0 after centring still works
  const bool yawRight = m_config.reverseYaw ? (yawUs < (3000 - m_config.yawArmUs)) : (yawUs > m_config.yawArmUs);
  const bool yawLeft = m_config.reverseYaw ? (yawUs > (3000 - m_config.yawDisarmUs)) : (yawUs < m_config.yawDisarmUs);
  const int32_t pitchOffset = abs(static_cast<int32_t>(pitchUs) - static_cast<int32_t>(m_centreUs[m_config.chPitch]));
  m_sticks.armGesture = m_sticks.throttleLow && yawRight;
  m_sticks.disarmGesture = m_sticks.throttleLow && yawLeft;
  m_sticks.levelGesture = m_sticks.throttleLow && (pitchOffset > m_config.pitchGestureUs);
  return m_sticks;
}

float StickInput::stickToUnit(uint16_t pulseUs, uint8_t channel, bool reverse, uint16_t deadbandUs) const
{
  const int32_t centre = m_centreUs[channel];
  const int32_t us = constrain(static_cast<int32_t>(pulseUs), static_cast<int32_t>(m_config.stickMinUs),
                               static_cast<int32_t>(m_config.stickMaxUs));
  const int32_t offset = us - centre;
  const int32_t deadband = deadbandUs;
  float         value = 0.0f;
  if (offset > deadband)
  {
    value = static_cast<float>(offset - deadband) / static_cast<float>(m_config.stickMaxUs - centre - deadband);
  }
  else if (offset < -deadband)
  {
    value = static_cast<float>(offset + deadband) / static_cast<float>(centre - m_config.stickMinUs - deadband);
  }
  value = constrain(value, -1.0f, 1.0f);
  return reverse ? -value : value;
}

bool StickInput::centresLocked() const
{
  if (!m_config.learnCentres)
  {
    return true;
  }
  for (uint8_t i = 0; i < RcPwmInput::CHANNELS; i++)
  {
    if ((i != m_config.chThrottle) && !m_centreLocked[i])
    {
      return false;
    }
  }
  return true;
}

void StickInput::learnCentres(uint32_t nowMs)
{
  for (uint8_t i = 0; i < RcPwmInput::CHANNELS; i++)
  {
    if ((i == m_config.chThrottle) || m_centreLocked[i])
    {
      continue;
    }
    const int32_t pulse = m_sticks.rawUs[i];
    if (abs(pulse - static_cast<int32_t>(m_stillRefUs[i])) > STILL_US)
    {
      m_stillRefUs[i] = static_cast<uint16_t>(pulse);
      m_stillSinceMs[i] = nowMs;
    }
    else if (((nowMs - m_stillSinceMs[i]) >= STILL_TIME_MS)
             && (abs(pulse - static_cast<int32_t>(MID_US)) <= CENTRE_TOLERANCE_US))
    {
      m_centreUs[i] = static_cast<uint16_t>(pulse);
      m_centreLocked[i] = true;
    }
  }
}

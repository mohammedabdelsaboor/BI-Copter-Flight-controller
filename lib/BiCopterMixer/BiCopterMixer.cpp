/**
 * @file   BiCopterMixer.cpp
 */
#include "BiCopterMixer.hpp"

namespace
{
float clamp01(float v)
{
  return (v < 0.0f) ? 0.0f : ((v > 1.0f) ? 1.0f : v);
}
float clamp11(float v)
{
  return (v < -1.0f) ? -1.0f : ((v > 1.0f) ? 1.0f : v);
}
}  // namespace

bool BiCopterMixer::begin(const MixerConfig& config)
{
  m_config = config;
  bool ok = true;
  // ESCs first so they see a valid OFF pulse as early as possible
  ok &= m_escLeft.begin(config.escLeftPin, config.escHz, config.escOffUs, config.escMaxUs, config.escOffUs);
  ok &= m_escRight.begin(config.escRightPin, config.escHz, config.escOffUs, config.escMaxUs, config.escOffUs);
  ok &= m_servoLeft.begin(config.servoLeftPin, config.servoHz, config.servoMinUs, config.servoMaxUs,
                          config.servoCenterUs);
  ok &= m_servoRight.begin(config.servoRightPin, config.servoHz, config.servoMinUs, config.servoMaxUs,
                           config.servoCenterUs);
  safe();
  return ok;
}

void BiCopterMixer::setServoTrims(int16_t leftUs, int16_t rightUs)
{
  m_trimLeftUs = leftUs;
  m_trimRightUs = rightUs;
}

MixerOutputs BiCopterMixer::mix(const MixerCommand& command, bool motorsArmed) const
{
  MixerOutputs out = {};

  // ---- Motors: roll by differential thrust, keep the roll difference when a motor saturates ----
  if (motorsArmed)
  {
    const float throttle = clamp01(command.throttle);
    const float roll = clamp11(command.roll);
    float       left = throttle + roll;
    float       right = throttle - roll;

    const float high = max(left, right);
    if (high > 1.0f)
    {
      left -= high - 1.0f;  // lose a little lift rather than roll authority
      right -= high - 1.0f;
    }
    const float low = min(left, right);
    if (low < 0.0f)
    {
      left -= low;  // raise both so the low side still has thrust to correct with
      right -= low;
    }
    out.escLeftUs = thrustToEscUs(left);
    out.escRightUs = thrustToEscUs(right);
  }
  else
  {
    out.escLeftUs = m_config.escOffUs;
    out.escRightUs = m_config.escOffUs;
  }

  // ---- Servos: pitch tilts both forward, yaw tilts them in opposite directions ----
  const float tiltLeft = clamp11(command.pitch - command.yaw);
  const float tiltRight = clamp11(command.pitch + command.yaw);
  out.servoLeftUs = tiltToServoUs(tiltLeft, m_config.servoLeftReversed, m_trimLeftUs);
  out.servoRightUs = tiltToServoUs(tiltRight, m_config.servoRightReversed, m_trimRightUs);
  return out;
}

void BiCopterMixer::write(const MixerCommand& command, bool motorsArmed)
{
  apply(mix(command, motorsArmed));
}

void BiCopterMixer::writeEscRaw(uint16_t pulseUs)
{
  MixerOutputs out = m_outputs;
  out.escLeftUs = pulseUs;
  out.escRightUs = pulseUs;
  apply(out);
}

void BiCopterMixer::safe()
{
  apply(mix({0.0f, 0.0f, 0.0f, 0.0f}, false));
}

uint16_t BiCopterMixer::tiltToServoUs(float tilt, bool reversed, int16_t trimUs) const
{
  const float direction = reversed ? -1.0f : 1.0f;
  const float us = static_cast<float>(m_config.servoCenterUs + trimUs)
                   + direction * tilt * static_cast<float>(m_config.servoTravelUs);
  return static_cast<uint16_t>(constrain(lroundf(us), m_config.servoMinUs, m_config.servoMaxUs));
}

uint16_t BiCopterMixer::thrustToEscUs(float thrust) const
{
  const float span = static_cast<float>(m_config.escMaxUs - m_config.escIdleUs);
  return static_cast<uint16_t>(m_config.escIdleUs + lroundf(clamp01(thrust) * span));
}

void BiCopterMixer::apply(const MixerOutputs& outputs)
{
  m_escLeft.writeUs(outputs.escLeftUs);
  m_escRight.writeUs(outputs.escRightUs);
  m_servoLeft.writeUs(outputs.servoLeftUs);
  m_servoRight.writeUs(outputs.servoRightUs);
  m_outputs = {m_escLeft.lastUs(), m_escRight.lastUs(), m_servoLeft.lastUs(), m_servoRight.lastUs()};
}

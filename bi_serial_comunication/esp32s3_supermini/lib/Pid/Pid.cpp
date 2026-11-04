/**
 * @file   Pid.cpp
 */
#include "Pid.hpp"
#include <math.h>

namespace
{
float clampf(float v, float limit)
{
  return (v > limit) ? limit : ((v < -limit) ? -limit : v);
}
}  // namespace

void Pid::configure(const PidGains& gains, float sampleHz)
{
  m_gains = gains;
  m_dFilter.configure(gains.dFilterHz, sampleHz);
  reset();
}

float Pid::update(float setpoint, float measurement, float dt, bool integrate)
{
  const float error = setpoint - measurement;
  m_p = m_gains.kp * error;

  if (!integrate)
  {
    m_i = 0.0f;
  }
  else
  {
    // Conditional integration: stop winding up when the output is already saturated the same way
    const bool saturatedHigh = (m_output >= m_gains.outLimit) && (error > 0.0f);
    const bool saturatedLow = (m_output <= -m_gains.outLimit) && (error < 0.0f);
    if (!saturatedHigh && !saturatedLow)
    {
      m_i = clampf(m_i + m_gains.ki * error * dt, m_gains.iLimit);
    }
  }

  // D on measurement: no kick when the stick moves quickly
  if (m_first || (dt <= 0.0f))
  {
    m_lastMeasurement = measurement;
    m_first = false;
  }
  const float rate = (dt > 0.0f) ? ((measurement - m_lastMeasurement) / dt) : 0.0f;
  m_lastMeasurement = measurement;
  m_d = -m_gains.kd * m_dFilter.apply(rate);

  // Slew limiter, after ArduPilot's SlewLimiter: an oscillation shows up as P+D swinging quickly back and
  // forth. Track how fast it moves (25 Hz filtered, peak held with a 0.3 s decay) and, above slewMax,
  // scale P and D down so a slow servo is not driven into a limit cycle. Normal corrections stay untouched.
  m_slewMod = 1.0f;
  if ((m_gains.slewMax > 0.0f) && (dt > 0.0f))
  {
    constexpr float FILTER_HZ = 25.0f;
    constexpr float DECAY_TAU_S = 0.3f;
    constexpr float MODIFIER_GAIN = 1.5f;
    constexpr float MIN_MODIFIER = 0.1f;
    const float     pd = m_p + m_d;
    const float     alpha = dt / (dt + 1.0f / (6.28318531f * FILTER_HZ));
    if (isnan(m_lastPd))
    {
      m_lastPd = pd;  // first sample after a reset: no slew yet
    }
    m_slewRate += alpha * ((pd - m_lastPd) / dt - m_slewRate);
    m_lastPd = pd;
    m_slewAmplitude = fmaxf(fabsf(m_slewRate), m_slewAmplitude * expf(-dt / DECAY_TAU_S));
    if (m_slewAmplitude > m_gains.slewMax)
    {
      m_slewMod = m_gains.slewMax / (m_gains.slewMax + MODIFIER_GAIN * (m_slewAmplitude - m_gains.slewMax));
      m_slewMod = fmaxf(m_slewMod, MIN_MODIFIER);
    }
    m_p *= m_slewMod;
    m_d *= m_slewMod;
  }

  m_output = clampf(m_p + m_i + m_d, m_gains.outLimit);
  return m_output;
}

void Pid::reset()
{
  m_i = 0.0f;
  m_p = 0.0f;
  m_d = 0.0f;
  m_output = 0.0f;
  m_first = true;
  m_lastPd = NAN;
  m_slewRate = 0.0f;
  m_slewAmplitude = 0.0f;
  m_slewMod = 1.0f;
  m_dFilter.reset();
}

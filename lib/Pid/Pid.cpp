/**
 * @file   Pid.cpp
 */
#include "Pid.hpp"

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
  m_dFilter.reset();
}

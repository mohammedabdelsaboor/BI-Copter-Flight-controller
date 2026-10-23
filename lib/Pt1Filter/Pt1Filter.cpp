/**
 * @file   Pt1Filter.cpp
 */
#include "Pt1Filter.hpp"

namespace
{
constexpr float TWO_PI_F = 6.28318530718f;
}  // namespace

void Pt1Filter::configure(float cutoffHz, float sampleHz)
{
  if ((cutoffHz <= 0.0f) || (sampleHz <= 0.0f))
  {
    m_gain = 1.0f;
    return;
  }
  const float rc = 1.0f / (TWO_PI_F * cutoffHz);
  const float dt = 1.0f / sampleHz;
  m_gain = dt / (rc + dt);
}

float Pt1Filter::apply(float input)
{
  m_state += m_gain * (input - m_state);
  return m_state;
}

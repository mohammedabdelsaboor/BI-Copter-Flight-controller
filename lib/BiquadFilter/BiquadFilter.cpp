/**
 * @file   BiquadFilter.cpp
 */
#include "BiquadFilter.hpp"
#include <math.h>

void BiquadFilter::configure(float cutoffHz, float sampleHz)
{
  m_passThrough = (cutoffHz <= 0.0f) || (sampleHz <= 0.0f) || (cutoffHz >= 0.5f * sampleHz);
  if (m_passThrough)
  {
    return;
  }

  // RBJ audio cookbook low pass, Q = 1/sqrt(2) (Butterworth: flat pass band, no overshoot bump)
  constexpr float Q = 0.70710678f;
  const float     omega = 6.28318530718f * cutoffHz / sampleHz;
  const float     sn = sinf(omega);
  const float     cs = cosf(omega);
  const float     alpha = sn / (2.0f * Q);
  const float     a0 = 1.0f + alpha;

  m_b0 = ((1.0f - cs) * 0.5f) / a0;
  m_b1 = (1.0f - cs) / a0;
  m_b2 = m_b0;
  m_a1 = (-2.0f * cs) / a0;
  m_a2 = (1.0f - alpha) / a0;
  reset(0.0f);
}

float BiquadFilter::apply(float input)
{
  if (m_passThrough)
  {
    return input;
  }
  const float output = m_b0 * input + m_z1;
  m_z1 = m_b1 * input - m_a1 * output + m_z2;
  m_z2 = m_b2 * input - m_a2 * output;
  return output;
}

void BiquadFilter::reset(float value)
{
  // Steady state for a constant input: output == input == value
  m_z1 = value * (1.0f - m_b0);
  m_z2 = value * (m_b2 - m_a2);
}

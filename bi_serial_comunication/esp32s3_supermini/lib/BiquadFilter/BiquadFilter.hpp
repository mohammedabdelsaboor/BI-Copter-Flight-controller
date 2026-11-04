/**
 * @file   BiquadFilter.hpp
 * @brief  Second order (Butterworth) low pass filter.
 *
 * Twice the roll-off of a PT1: at 3x the cutoff it already removes ~90 % of the signal, so motor
 * vibration on the accelerometer is cleaned up without slowing the part of the signal that matters.
 */
#pragma once

class BiquadFilter
{
public:
  /** cutoffHz <= 0, or at/above half the sample rate, turns the filter into a pass-through. */
  void configure(float cutoffHz, float sampleHz);
  float apply(float input);

  /** Starts the filter as if it had been fed this value for a long time (no start-up ramp). */
  void reset(float value = 0.0f);

private:
  bool  m_passThrough = true;
  float m_b0 = 1.0f;
  float m_b1 = 0.0f;
  float m_b2 = 0.0f;
  float m_a1 = 0.0f;
  float m_a2 = 0.0f;
  float m_z1 = 0.0f;  // transposed direct form II state
  float m_z2 = 0.0f;
};

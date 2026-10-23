/**
 * @file   Pt1Filter.hpp
 * @brief  First order low pass filter.
 */
#pragma once

class Pt1Filter
{
public:
  /** cutoffHz <= 0 turns the filter into a pass-through. */
  void configure(float cutoffHz, float sampleHz);
  float apply(float input);
  void reset(float value = 0.0f) { m_state = value; }
  float value() const { return m_state; }

private:
  float m_gain = 1.0f;
  float m_state = 0.0f;
};

/**
 * @file   LeadCompensator.hpp
 * @brief  Phase lead for a slow actuator: output = input + lead time x (filtered rate of change).
 *
 * A hobby servo moves slowly towards a new position (its own internal loop slows down as it gets close).
 * When the command starts changing, this briefly asks for a bit more than the target, so the servo starts
 * moving harder and arrives sooner. Once the command settles the extra part fades back to zero.
 */
#pragma once

#include "Pt1Filter.hpp"

class LeadCompensator
{
public:
  /**
   * @param leadS      how far ahead to push, seconds (0 = off, pass-through)
   * @param filterHz   low pass on the rate of change, keeps noise from being amplified
   * @param maxBoost   largest extra output, same units as the input
   */
  void configure(float leadS, float filterHz, float maxBoost, float sampleHz);
  float apply(float input, float dt);
  void reset();

private:
  Pt1Filter m_rateFilter;
  float     m_leadS = 0.0f;
  float     m_maxBoost = 0.0f;
  float     m_last = 0.0f;
  bool      m_first = true;
};

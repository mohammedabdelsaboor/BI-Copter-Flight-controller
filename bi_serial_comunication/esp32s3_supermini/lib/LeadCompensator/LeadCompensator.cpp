/**
 * @file   LeadCompensator.cpp
 */
#include "LeadCompensator.hpp"

void LeadCompensator::configure(float leadS, float filterHz, float maxBoost, float sampleHz)
{
  m_leadS = leadS;
  m_maxBoost = maxBoost;
  m_rateFilter.configure(filterHz, sampleHz);
  reset();
}

float LeadCompensator::apply(float input, float dt)
{
  if ((m_leadS <= 0.0f) || (dt <= 0.0f))
  {
    return input;
  }
  if (m_first)
  {
    m_last = input;  // no jump on the first sample
    m_first = false;
  }
  const float rate = m_rateFilter.apply((input - m_last) / dt);
  m_last = input;

  float boost = m_leadS * rate;
  if (boost > m_maxBoost) boost = m_maxBoost;
  if (boost < -m_maxBoost) boost = -m_maxBoost;
  return input + boost;
}

void LeadCompensator::reset()
{
  m_rateFilter.reset();
  m_last = 0.0f;
  m_first = true;
}

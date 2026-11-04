/**
 * @file   PwmOutput.cpp
 */
#include "PwmOutput.hpp"

bool PwmOutput::begin(uint8_t pin, uint32_t frequencyHz, uint16_t minUs, uint16_t maxUs, uint16_t initialUs)
{
  m_pin = pin;
  m_periodUs = 1000000UL / frequencyHz;
  m_minUs = minUs;
  m_maxUs = min<uint32_t>(maxUs, m_periodUs - 50);  // keep a low gap so the receiver sees a pulse
  m_attached = ledcAttach(pin, frequencyHz, RESOLUTION_BITS);
  if (m_attached)
  {
    writeUs(initialUs);
  }
  return m_attached;
}

void PwmOutput::writeUs(uint16_t pulseUs)
{
  const uint16_t us = constrain(pulseUs, m_minUs, m_maxUs);
  m_lastUs = us;
  if (m_attached)
  {
    const uint32_t duty = (static_cast<uint32_t>(us) << RESOLUTION_BITS) / m_periodUs;
    ledcWrite(m_pin, duty);
  }
}

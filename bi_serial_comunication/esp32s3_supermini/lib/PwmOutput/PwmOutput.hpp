/**
 * @file   PwmOutput.hpp
 * @brief  One servo/ESC pulse output on an ESP32 LEDC channel, commanded in microseconds.
 */
#pragma once

#include <Arduino.h>

class PwmOutput
{
public:
  /**
   * @param frequencyHz  50 for analogue servos (MG996R), 50-490 for PWM ESCs.
   * @param minUs/maxUs  every write is clamped to this range.
   * @param initialUs    output from the moment the pin is attached (ESC off / servo centre).
   */
  bool begin(uint8_t pin, uint32_t frequencyHz, uint16_t minUs, uint16_t maxUs, uint16_t initialUs);

  void writeUs(uint16_t pulseUs);
  uint16_t lastUs() const { return m_lastUs; }

private:
  static constexpr uint8_t RESOLUTION_BITS = 14;  // ESP32-S3 LEDC maximum, 0.15 us steps at 400 Hz

  uint8_t  m_pin = 0;
  uint32_t m_periodUs = 20000;
  uint16_t m_minUs = 1000;
  uint16_t m_maxUs = 2000;
  uint16_t m_lastUs = 0;
  bool     m_attached = false;
};

/**
 * @file   RcPwmInput.hpp
 * @brief  Measures 4 RC PWM channels (one wire per channel) on the ESP32-S3.
 *
 * Pulse edges are timestamped by the MCPWM capture hardware, so the measurement does not depend on
 * interrupt latency (GPIO interrupts + micros() give +-50 us jitter when WiFi/USB/I2C interrupts run).
 * Falls back to GPIO interrupts if the capture units cannot be allocated.
 * A median of the last 3 pulses removes single glitches from motor noise or loose wires.
 */
#pragma once

#include <Arduino.h>
#include "driver/mcpwm_cap.h"

class RcPwmInput
{
public:
  static constexpr uint8_t CHANNELS = 4;

  /** @param timeoutUs  a channel that has not pulsed for this long marks the signal as lost. */
  bool begin(const uint8_t (&pins)[CHANNELS], uint32_t timeoutUs);

  /** Collects new pulses from the interrupts. Call once per control loop. */
  void update();

  /** Filtered pulse width in us (0 until the first pulse arrived). */
  uint16_t pulseUs(uint8_t channel) const { return m_pulseUs[channel]; }

  /** Unfiltered width of the last edge pair, even if it was rejected. For diagnostics. */
  uint16_t rawUs(uint8_t channel) const { return static_cast<uint16_t>(m_capture[channel].rawWidthUs); }

  bool signalLost() const { return m_signalLost; }
  bool usingHardwareCapture() const { return m_hardwareCapture; }
  uint32_t framesReceived() const { return m_frames; }

private:
  static constexpr uint32_t MIN_VALID_US = 800;
  static constexpr uint32_t MAX_VALID_US = 2200;
  static constexpr uint32_t CAPTURE_CHANNELS_PER_GROUP = 3;  // ESP32-S3: 2 MCPWM groups x 3 capture inputs

  struct Capture
  {
    uint8_t           pin;
    uint32_t          ticksPerUs;
    volatile uint32_t riseStamp;  // capture ticks or micros
    volatile uint32_t widthUs;
    volatile uint32_t rawWidthUs;
    volatile uint32_t lastPulseUs;
    volatile bool     fresh;
  };

  bool beginHardwareCapture();
  void beginGpioInterrupts();
  static bool IRAM_ATTR onCapture(mcpwm_cap_channel_handle_t channel, const mcpwm_capture_event_data_t* event,
                                  void* arg);
  static void IRAM_ATTR onEdge(void* arg);
  static void IRAM_ATTR storeWidth(Capture* capture, uint32_t widthUs);
  static uint16_t median3(uint16_t a, uint16_t b, uint16_t c);

  Capture  m_capture[CHANNELS] = {};
  uint16_t m_history[CHANNELS][3] = {};
  uint8_t  m_historyIndex[CHANNELS] = {};
  uint16_t m_pulseUs[CHANNELS] = {};
  uint32_t m_timeoutUs = 250000;
  uint32_t m_frames = 0;
  bool     m_signalLost = true;
  bool     m_hardwareCapture = false;
};

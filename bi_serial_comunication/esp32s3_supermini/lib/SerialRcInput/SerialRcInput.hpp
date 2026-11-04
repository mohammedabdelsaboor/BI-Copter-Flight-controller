/**
 * @file   SerialRcInput.hpp
 * @brief  Receives the 4 RC channels from the receiver bridge board over UART (RcLink frames).
 *
 * Drop-in replacement for RcPwmInput: the rest of the firmware only uses pulseUs(), signalLost() and CHANNELS.
 * The signal counts as lost when no valid frame arrived for the timeout, or when the bridge reports that it
 * lost the receiver itself.
 */
#pragma once

#include <Arduino.h>
#include "RcLink.hpp"

class SerialRcInput
{
public:
  static constexpr uint8_t CHANNELS = rclink::CHANNELS;

  /** @param txPin  -1 if the ESP32 does not send anything back. */
  void begin(HardwareSerial& uart, int8_t rxPin, int8_t txPin, uint32_t baud, uint32_t timeoutUs);

  /** Reads the bytes waiting in the UART buffer. Call once per control loop. */
  void update();

  /** Pulse width in us from the last valid frame (0 until the first frame). */
  uint16_t pulseUs(uint8_t channel) const { return m_pulseUs[channel]; }

  bool     signalLost() const { return m_signalLost; }
  uint32_t framesReceived() const { return m_frames; }
  uint32_t badFrames() const { return m_badFrames; }      // failed CRC
  uint32_t bytesReceived() const { return m_bytes; }      // every byte, valid or not (wiring check)
  uint32_t missedFrames() const { return m_missedFrames; } // sequence gaps
  bool     remoteLost() const { return m_remoteLost; }     // bridge reports the receiver lost

private:
  static constexpr uint16_t MIN_VALID_US = 800;
  static constexpr uint16_t MAX_VALID_US = 2200;

  HardwareSerial*  m_uart = nullptr;
  rclink::Parser   m_parser;
  uint16_t         m_pulseUs[CHANNELS] = {};
  uint32_t         m_timeoutUs = 100000;
  uint32_t         m_lastFrameUs = 0;
  uint32_t         m_frames = 0;
  uint32_t         m_badFrames = 0;
  uint32_t         m_bytes = 0;
  uint32_t         m_missedFrames = 0;
  uint8_t          m_lastSeq = 0;
  bool             m_remoteLost = true;
  bool             m_signalLost = true;
};

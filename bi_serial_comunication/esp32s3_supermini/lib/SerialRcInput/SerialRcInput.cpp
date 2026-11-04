#include "SerialRcInput.hpp"
#include "driver/gpio.h"

void SerialRcInput::begin(HardwareSerial& uart, int8_t rxPin, int8_t txPin, uint32_t baud, uint32_t timeoutUs)
{
  m_uart = &uart;
  m_timeoutUs = timeoutUs;
  m_uart->setRxBufferSize(256);  // ~19 frames: enough even if a loop is late by several ms
  // GPIO43/44 start out as UART0 TX/RX of the boot console. A pin still muxed to U0TXD keeps driving its output
  // and the link never sees the other board's signal, so hand the pin back to plain GPIO first.
  if (rxPin >= 0)
  {
    gpio_reset_pin(static_cast<gpio_num_t>(rxPin));
  }
  m_uart->begin(baud, SERIAL_8N1, rxPin, txPin);
}

void SerialRcInput::update()
{
  if (m_uart == nullptr)
  {
    return;
  }
  int budget = 128;  // bounded work per loop
  while ((m_uart->available() > 0) && (budget-- > 0))
  {
    m_bytes++;
    switch (m_parser.feed(static_cast<uint8_t>(m_uart->read())))
    {
      case rclink::Parser::Result::FRAME:
      {
        const rclink::Frame& f = m_parser.frame();
        if (m_frames > 0)
        {
          m_missedFrames += static_cast<uint8_t>(f.seq - m_lastSeq - 1);
        }
        m_lastSeq = f.seq;
        for (uint8_t i = 0; i < CHANNELS; i++)
        {
          const uint16_t us = f.channelUs[i];
          if ((us >= MIN_VALID_US) && (us <= MAX_VALID_US))
          {
            m_pulseUs[i] = us;
          }
        }
        m_remoteLost = (f.flags & rclink::FLAG_RX_LOST) != 0;
        m_lastFrameUs = micros();
        m_frames++;
        break;
      }
      case rclink::Parser::Result::BAD_CRC: m_badFrames++; break;
      case rclink::Parser::Result::NONE:
      default: break;
    }
  }
  const bool stale = (m_frames == 0) || ((micros() - m_lastFrameUs) > m_timeoutUs);
  m_signalLost = stale || m_remoteLost;
}

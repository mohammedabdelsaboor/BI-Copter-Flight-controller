/**
 * @file   RcLink.hpp
 * @brief  Serial frame that carries the 4 receiver channels from the PowerFeather bridge to the SuperMini.
 *
 * Used by both projects (powerfeather_rc_bridge sends, esp32s3_supermini receives), so the two sides can never
 * disagree on the format. Header only, no Arduino dependency.
 *
 * Frame, 13 bytes, little endian:
 *
 *   0xA5 0x5A | flags | seq | ch1 lo hi | ch2 lo hi | ch3 lo hi | ch4 lo hi | crc8
 *
 *   flags  bit 0: the bridge lost the receiver (a channel stopped pulsing)
 *   seq    counts up by one per frame, so the receiver can count lost frames
 *   chN    pulse width in microseconds (0 = no pulse seen yet)
 *   crc8   CRC-8 (polynomial 0x07, init 0) over flags, seq and the channels
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace rclink
{
inline constexpr uint32_t BAUD = 115200;
inline constexpr uint8_t  CHANNELS = 4;
inline constexpr uint8_t  SYNC0 = 0xA5;
inline constexpr uint8_t  SYNC1 = 0x5A;
inline constexpr uint8_t  FLAG_RX_LOST = 0x01;
inline constexpr size_t   PAYLOAD_BYTES = 2 + 2 * CHANNELS;  // flags, seq, channels
inline constexpr size_t   FRAME_BYTES = 2 + PAYLOAD_BYTES + 1;
inline constexpr uint32_t SEND_PERIOD_MS = 10;  // 100 frames per second; one frame takes ~1.1 ms at 115200

struct Frame
{
  uint8_t  flags = 0;
  uint8_t  seq = 0;
  uint16_t channelUs[CHANNELS] = {};
};

inline uint8_t crc8(const uint8_t* data, size_t length)
{
  uint8_t crc = 0;
  for (size_t i = 0; i < length; i++)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

/** Writes one frame into out (FRAME_BYTES long). */
inline void encode(const Frame& frame, uint8_t (&out)[FRAME_BYTES])
{
  out[0] = SYNC0;
  out[1] = SYNC1;
  out[2] = frame.flags;
  out[3] = frame.seq;
  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    out[4 + 2 * i] = static_cast<uint8_t>(frame.channelUs[i] & 0xFF);
    out[5 + 2 * i] = static_cast<uint8_t>(frame.channelUs[i] >> 8);
  }
  out[FRAME_BYTES - 1] = crc8(&out[2], PAYLOAD_BYTES);
}

/** Byte-by-byte decoder. Resynchronises on the sync bytes after any error. */
class Parser
{
public:
  enum class Result : uint8_t
  {
    NONE,     // byte taken, frame not complete
    FRAME,    // a valid frame is in frame()
    BAD_CRC,  // a complete frame failed its check
  };

  Result feed(uint8_t byte)
  {
    switch (m_index)
    {
      case 0:
        m_index = (byte == SYNC0) ? 1 : 0;
        return Result::NONE;
      case 1:
        if (byte == SYNC1)
        {
          m_index = 2;
        }
        else
        {
          m_index = (byte == SYNC0) ? 1 : 0;
        }
        return Result::NONE;
      default:
        break;
    }
    m_payload[m_index - 2] = byte;
    m_index++;
    if (m_index < FRAME_BYTES)
    {
      return Result::NONE;
    }
    m_index = 0;
    if (crc8(m_payload, PAYLOAD_BYTES) != m_payload[PAYLOAD_BYTES])
    {
      return Result::BAD_CRC;
    }
    m_frame.flags = m_payload[0];
    m_frame.seq = m_payload[1];
    for (uint8_t i = 0; i < CHANNELS; i++)
    {
      m_frame.channelUs[i] = static_cast<uint16_t>(m_payload[2 + 2 * i] | (m_payload[3 + 2 * i] << 8));
    }
    return Result::FRAME;
  }

  const Frame& frame() const { return m_frame; }

private:
  uint8_t m_payload[PAYLOAD_BYTES + 1] = {};
  size_t  m_index = 0;
  Frame   m_frame;
};
}  // namespace rclink

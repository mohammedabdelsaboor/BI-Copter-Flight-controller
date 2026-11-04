/**
 * @file   main.cpp
 * @brief  ESP32-S3 PowerFeather receiver bridge: measures the 4 PWM channels of a FlySky FS-iA6B and sends
 *         them to the ESP32-S3 SuperMini flight controller over UART (frame in ../common/RcLink/RcLink.hpp).
 *
 *   Receiver CH1..CH4  ->  GPIO36, GPIO37, GPIO38, GPIO39   (3.3 V signals)
 *   UART TX  GPIO44    ->  SuperMini GPIO44 (RX)   the PowerFeather pin marked TX is GPIO44
 *   UART RX  GPIO42    <-  SuperMini GPIO43 (TX)   marked RX, not used yet
 *   GND                --  SuperMini GND and receiver GND
 *
 * Every 10 ms one frame goes out with the latest pulse widths. If the receiver stops pulsing, the frame
 * carries FLAG_RX_LOST and the flight controller goes to failsafe.
 *
 * USB console: the four channels are printed 5 times a second.
 * LED (GPIO46): slow blink = receiver ok, fast blink = receiver lost.
 */
#include <Arduino.h>
#include "RcLink.hpp"
#include "RcPwmInput.hpp"

namespace
{
constexpr uint8_t  RC_PINS[RcPwmInput::CHANNELS] = {36, 37, 38, 39};
constexpr int8_t   PIN_LINK_TX = 44;  // marked TX on the PowerFeather
constexpr int8_t   PIN_LINK_RX = 42;  // marked RX
constexpr uint8_t  PIN_LED = 46;
constexpr uint8_t  PIN_EN_3V3 = 4;            // PowerFeather 3V3 header rail (receiver may be powered from it)
constexpr uint32_t RC_TIMEOUT_US = 100000;    // FS-iA6B pulses every ~20 ms
constexpr uint32_t PRINT_PERIOD_MS = 200;

RcPwmInput receiver;
}  // namespace

void setup()
{
  pinMode(PIN_EN_3V3, OUTPUT);
  digitalWrite(PIN_EN_3V3, HIGH);
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never block when no USB terminal is connected
  Serial1.begin(rclink::BAUD, SERIAL_8N1, PIN_LINK_RX, PIN_LINK_TX);
  const bool capture = receiver.begin(RC_PINS, RC_TIMEOUT_US);
  delay(500);
  Serial.printf("RC bridge: CH1-4 on GPIO %u %u %u %u (%s), link TX GPIO%d at %lu baud\n", RC_PINS[0], RC_PINS[1],
                RC_PINS[2], RC_PINS[3], capture ? "hardware capture" : "pin interrupts", PIN_LINK_TX,
                static_cast<unsigned long>(rclink::BAUD));
}

void loop()
{
  static uint32_t nextSendMs = millis();
  static uint32_t nextPrintMs = 0;
  static uint8_t  seq = 0;
  const uint32_t  nowMs = millis();
  receiver.update();
  if (static_cast<int32_t>(nowMs - nextSendMs) < 0)
  {
    return;
  }
  nextSendMs += rclink::SEND_PERIOD_MS;

  rclink::Frame frame;
  frame.seq = seq++;
  for (uint8_t i = 0; i < rclink::CHANNELS; i++)
  {
    frame.channelUs[i] = receiver.pulseUs(i);
  }
  const bool lost = receiver.signalLost();
  frame.flags = lost ? rclink::FLAG_RX_LOST : 0;
  uint8_t bytes[rclink::FRAME_BYTES];
  rclink::encode(frame, bytes);
  Serial1.write(bytes, sizeof(bytes));

  const uint32_t blinkMs = lost ? 100 : 500;
  digitalWrite(PIN_LED, ((nowMs / blinkMs) % 2) ? HIGH : LOW);

  if (static_cast<int32_t>(nowMs - nextPrintMs) >= 0)
  {
    nextPrintMs = nowMs + PRINT_PERIOD_MS;
    Serial.printf("CH1 %4u  CH2 %4u  CH3 %4u  CH4 %4u  %s  (raw %4u %4u %4u %4u)\n", frame.channelUs[0],
                  frame.channelUs[1], frame.channelUs[2], frame.channelUs[3], lost ? "LOST" : "ok",
                  receiver.rawUs(0), receiver.rawUs(1), receiver.rawUs(2), receiver.rawUs(3));
  }
}

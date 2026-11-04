# bi_serial_comunication

The bicopter flight controller on an **ESP32-S3 SuperMini** (S3FH4R2), with the radio receiver read by a
separate **ESP32-S3 PowerFeather** that sends the stick values over UART.

This folder is a self-contained copy of the firmware in the repository root, so the original PowerFeather
flight build is not affected. Flight logic, gains, filters, console and web tuning page are the same; only the
pins and the receiver input are different.

```
FS-iA6B ──4 x PWM──> ESP32-S3 PowerFeather ──UART 115200──> ESP32-S3 SuperMini ──> 2 ESC, 2 servos
                      powerfeather_rc_bridge                  esp32s3_supermini      MPU6050 on I2C
```

| Folder | Contents |
|---|---|
| [esp32s3_supermini/](esp32s3_supermini/) | flight controller (PlatformIO), receiver input replaced by `SerialRcInput` |
| [powerfeather_rc_bridge/](powerfeather_rc_bridge/) | receiver bridge: measures CH1-CH4 with hardware capture and sends them every 10 ms |
| [common/RcLink/](common/RcLink/) | the frame format, one header used by both projects |

## Wiring

**ESP32-S3 SuperMini (flight controller)**

| Signal | GPIO |
|---|---|
| ESC left / right | 5 / 6 |
| Servo left / right | 3 / 4 |
| MPU6050 SDA / SCL | 8 / 9 (sensor on 3V3 and GND) |
| Link input (from PowerFeather TX) | 43, marked TX (`PIN_LINK_RX` in Config.hpp) |
| Status LED | 48, on-board RGB LED |

**ESP32-S3 PowerFeather (receiver bridge)**

| Signal | GPIO |
|---|---|
| Receiver CH1 (roll), CH2 (pitch), CH3 (throttle), CH4 (yaw) | 36, 37, 38, 39 |
| UART TX to SuperMini GPIO43 | 44, marked TX |
| UART RX (not used yet) | 42, marked RX |
| Status LED | 46, on-board |

Connect the grounds of both boards, the receiver, the ESCs and the servo supply. Both boards use 3.3 V logic,
so the link wire connects directly: **PowerFeather TX (GPIO44) to SuperMini GPIO43**. On the PowerFeather the
pin marked TX is GPIO44 and RX is GPIO42. The SuperMini reads the link on GPIO43 (its pin marked TX) because that
is where the wire went; the ESP32-S3 can route a UART to any pin, so this only needs `PIN_LINK_RX` in Config.hpp. The ESP32-S3 inputs are not 5 V
tolerant; the FS-iA6B outputs 3.3 V signals even when powered from 5 V. Do not power the servos from either
board; use the UBEC as before.

## Link protocol

A 13-byte frame every 10 ms (100 Hz), 115200 baud, 8N1:

```
0xA5 0x5A | flags | seq | CH1 | CH2 | CH3 | CH4 | CRC-8
                          (each channel: pulse width in µs, uint16 little endian)
```

- `flags` bit 0 is set when the bridge lost the receiver (no pulses for 100 ms).
- `seq` counts frames, so the flight controller can count lost frames.
- The CRC-8 (polynomial 0x07) covers `flags` to `CH4`. Frames that fail the CRC are dropped.

The SuperMini goes to failsafe (motors off) when no valid frame arrives for 100 ms, or when a frame carries the
"receiver lost" flag. The receiver failsafe on the FS-i6 must still be set, as in
[../docs/SETUP_AND_TUNING.md](../docs/SETUP_AND_TUNING.md), because the FS-iA6B keeps pulsing after the radio
link drops.

## Build and flash

PowerFeather (receiver bridge):

```bash
cd powerfeather_rc_bridge
pio run -t upload
pio device monitor        # prints CH1-CH4 five times a second
```

SuperMini (flight controller; if the upload does not start, hold BOOT, press RST, release BOOT):

```bash
cd esp32s3_supermini
pio run -t upload
pio device monitor        # 'h' for help, 's' status, 'r' sticks
```

## First checks, propellers off

1. **Receiver.** On the PowerFeather's console each stick moves only its own channel, about 1000-2000 µs, and
   the line ends with `ok`. `LOST` means no pulses: check the signal wires on GPIO36-39 and the receiver GND.
2. **Link.** On the SuperMini, `s` switches the status line on. It shows `rx ok (N frames, 0 bad)`, and the
   frame count rises by about 100 per second. `rx LOST` with 0 frames means no data: check PowerFeather TX (GPIO44)
   to SuperMini GPIO43 and the common GND. A rising `bad` count points to noise or a wrong baud rate.
3. **Sticks.** `r` on the SuperMini prints the four channels. Each stick must move only its own channel, in the
   directions given in [../docs/SETUP_AND_TUNING.md](../docs/SETUP_AND_TUNING.md).
4. **Failsafe.** Disarmed, unplugging the UART wire shows `rx LOST` and arming is refused. Armed (propellers
   off), unplugging it or switching off the transmitter with its failsafe set stops the motors within 0.1 s:
   the status shows `FAILSAFE` and the LED blinks blue. Throttle low with the link back returns to disarmed.
5. **Sensor, servos and motors:** the same bench checks as for the original model (Appendix B of the report).

**LED colours (SuperMini):** green, slow blink = disarmed; red, solid = armed; blue, fast blink = failsafe (no
link or receiver lost); white, very fast = fault or gyro calibration. **PowerFeather:** slow blink = receiver
ok, fast blink = receiver lost.

## Notes

- The SuperMini has 4 MB flash; the firmware uses about 88 % of its 1.25 MB application partition.
- The MPU6050 module on this build answers with `WHO_AM_I 0x70` (an MPU6500-type chip); the firmware accepts it.
- GPIO43 of the SuperMini is its boot-message pin: for about 0.1 s after reset the chip drives it while the
  bridge drives it too. This is brief and current-limited in practice, but moving the wire to GPIO44 (and setting
  `PIN_LINK_RX = 44`) avoids it.
- Adding channels later (e.g. an arm switch on CH5) only needs `rclink::CHANNELS` raised in
  [common/RcLink/RcLink.hpp](common/RcLink/RcLink.hpp), one more pin on the bridge, and the stick code on the
  flight controller.

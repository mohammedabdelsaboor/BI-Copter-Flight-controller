# Appendix A: Hardware Wiring

This appendix belongs to the project report in the [README](../README.md). It lists the pin assignment of the
ESP32-S3 PowerFeather, the power distribution and the mounting of the inertial sensor.

## A.1 Pin assignment

| Signal | GPIO | Label on the PowerFeather | Goes to |
|---|---|---|---|
| MPU6050 SDA | 35 | SDA | GY-521 SDA |
| MPU6050 SCL | 36 | SCL | GY-521 SCL |
| Receiver CH1, roll | 39 | SCK | receiver CH1 signal |
| Receiver CH2, pitch | 40 | MO | receiver CH2 signal |
| Receiver CH3, throttle | 41 | MI | receiver CH3 signal |
| Receiver CH4, yaw | 42 | RX | receiver CH4 signal |
| Left tilt servo | 15 | D5 | servo signal (orange) |
| Right tilt servo | 16 | D6 | servo signal (orange) |
| Left ESC | 6 | D8 | ESC signal (white or yellow) |
| Right ESC | 9 | A1 | ESC signal (white or yellow) |
| Status LED | 46 | on the board | |

The firmware switches on GPIO 4 (the 3V3 header rail, which powers the MPU6050) and GPIO 14 (the STEMMA-QT rail)
during start-up. You don't need to wire anything for that.

## A.2 Power distribution

```
 Battery ──┬── left ESC ── left motor
           ├── right ESC ── right motor
           └── 5-6 V UBEC (5 A or more) ──┬── left servo  (+ / -)
                                          ├── right servo (+ / -)
                                          └── receiver    (+ / -)

 PowerFeather: from USB, or its own LiPo / VS input.
 MPU6050 VCC -> PowerFeather 3V3.

 Every ground is joined: battery -, UBEC -, ESC signal grounds, receiver -, PowerFeather GND.
```

A few things that cause trouble if you get them wrong:

- Don't run the servos from the PowerFeather. An MG996R can pull 1–2 A when it is pushing hard, and two of
  them will brown out the board. Give them their own UBEC, and put a 470–1000 µF capacitor across the servo
  supply.
- If the ESCs have built-in BECs, connect the red wire of one of them at most, or none if you use a UBEC.
- The receiver outputs 3.3 V-friendly signals but needs 5 V on its power pins.

## A.3 Mounting the MPU6050

Put it near the centre of the frame on a piece of soft foam or double-sided foam tape, chip facing up, with the
wires loose enough that they don't pull on it. The default setting in `Config.hpp` expects the
**X arrow on the board pointing to the nose and the Y arrow pointing to the left motor**.

If your board sits another way, change `IMU_AXIS_MAP` in `include/Config.hpp`. The setup guide has a table of
the common mountings and a test to confirm it. The build stops with an error if the map you enter is a mirror
image, which is the usual mistake.

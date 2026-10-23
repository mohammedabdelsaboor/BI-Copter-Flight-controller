# Setup and tuning

Do everything up to "First hover" with the **propellers off**.

Open the console with `pio device monitor` (or any serial terminal) and type a command followed by Enter.
Once a second the board prints a status line: its state, the roll and pitch angles, whether the radio is OK,
the loop rate, and, when arming would be refused, the reason.

## 1. Power-up

Leave the model still for the first couple of seconds after power-on. The gyro is calibrated then, and if the
frame moves the board just tries again until it's still. The status line shows `DISARMED` when it's done.

For the first 2 seconds after that, keep your hands off the roll, pitch and yaw sticks. The firmware takes
the position they rest at as their centre. If a stick was trimmed off-centre, that would otherwise look like a
constant command. Arming waits until this is finished.

## 2. Radio

Type `r`. Move each stick and watch the numbers:

- roll stick right gives `roll +`
- pitch stick forward gives `pitch +`
- yaw stick right gives `yaw +`
- throttle goes from `thr 0.00` at the bottom to `thr 1.00` at the top

If one goes the wrong way, set its `RC_REVERSE_...` value in `include/Config.hpp`, or reverse the channel on
the transmitter. The `ctr` values show the learned stick centres.

With the throttle down, yaw fully right should show `ARM-gesture` and fully left `DISARM-gesture`.

### Receiver failsafe

This step matters more than any other. When the FS-iA6B loses the transmitter it keeps sending the last
pulses it had, so a model flying away at half throttle would carry on at half throttle. The fix is to make the
receiver send a throttle value lower than the stick can normally reach:

1. On the FS-i6, open *Functions setup → End points* and set the CH3 low end to 120 %.
2. Open *Functions setup → Failsafe → Channel 3*, turn it on, hold the throttle fully down and long-press OK.
3. Set the CH3 end point back to 100 %.
4. Test it. With `r` running, switch the transmitter off. Within a second the line must show `FAILSAFE`.

Don't fly until that test passes.

## 3. Sensor direction

Type `i` and move the frame by hand:

| Move | Should read |
|---|---|
| right motor down | `roll +` |
| nose down | `pitch +` |
| nose turning left, seen from above | yaw rate (`y`) `+` |

If something is wrong, the board is mounted differently from the default. Pick the matching line for
`IMU_AXIS_MAP` in `Config.hpp`:

| How the board sits (chip facing up) | `IMU_AXIS_MAP` |
|---|---|
| X arrow to the nose, Y to the left motor (default) | `{{0, 1, 2}, {+1, +1, +1}}` |
| X arrow to the tail, Y to the right motor | `{{0, 1, 2}, {-1, -1, +1}}` |
| Y arrow to the nose, X to the right motor | `{{1, 0, 2}, {+1, -1, +1}}` |
| Y arrow to the tail, X to the left motor | `{{1, 0, 2}, {-1, +1, +1}}` |
| X arrow to the nose, chip facing **down** | `{{0, 1, 2}, {+1, -1, -1}}` |

The `i` line also shows the total acceleration. It should read close to `1.00 g` and say `used`. Many
GY-521 boards read too high on the Z axis; the firmware measures that offset at start-up (the `zoff` value)
and removes it. An offset bigger than about 0.6 g means the sensor is bad.

### Level

Stand the model on a flat surface with a bubble level or phone level app on the motor arm, and pack under
the legs until it reads level both ways. Then type `l`, or hold the pitch stick fully forward for 3 seconds
with the throttle down. The status line should now show roll and pitch close to zero.

Skipping this is the most common cause of drift. If the board thinks "level" is tilted by 8°, the model will
fly tilted by 8° and slide that way.

## 4. Servos

1. With the servo horns off, power up. Both servos go to centre. Fit the horns so both motors point straight up.
2. Fine-tune with `trim l 20` or `trim r -15` (microseconds), then `save`.
3. **Hand test.** Still disarmed, tilt the nose down by hand. Both motors should tilt back, as if trying to lift
   the nose. If they tilt forward, swap `SERVO_LEFT_REVERSED` and `SERVO_RIGHT_REVERSED` in `Config.hpp`.
4. **Twist test.** Turn the nose left by hand. The left motor should tilt forward and the right one back.
5. **Stick test.** Pitch stick forward should tilt both motors forward. Yaw stick right tilts the left motor
   back and the right one forward. If the hand tests are right but a stick is backwards, the stick needs
   reversing (step 2), not the servos.

`SERVO_TRAVEL_US` sets how far the servos move at full command. Lower `SERVO_MIN_US` and `SERVO_MAX_US` if the
mechanism hits its stops.

## 5. Motors and ESCs

1. Calibrate the ESCs once: `pio run -e esc_calibration -t upload`. Throttle up, plug in the battery, wait for
   the beeps, throttle down, wait for the confirmation beeps, unplug. Then flash the normal firmware again.
2. The two propellers must spin in opposite directions. Swap any two motor wires to reverse one.
3. Roll test without arming: type `o` and tip the frame right side down. In the `@50%thr` values the right ESC
   should be higher than the left.
4. Arm. Both motors should idle smoothly. Raise `ESC_IDLE_US` if one stutters.

## 6. First hover

Pick an open space, and use a light tether if you can. Raise the throttle slowly until the model is light on
its legs, then lift off to about half a metre. If it starts to oscillate, land and lower the P gain of that
axis.

A healthy setup hovers somewhere between 40 % and 60 % throttle. If it needs 80 % or more, there isn't enough
spare thrust for the controller to correct with. Make the model lighter, or fit bigger props or stronger motors.

## 7. Tuning

The gains can be changed from the console without re-flashing. Changes only work while disarmed, take effect
straight away, and survive a reboot once you type `save`.

```
pid                         show the current gains
pid r 0.0016 0.003 0.00004  roll rate P, I, D
pid p 0.0025 0.004 0        pitch rate P, I, D
pid y 0.0025 0.003 0        yaw rate P, I, D
ang r 3.0                   roll self-level strength
ang p 3.0                   pitch self-level strength
ang y 1.5                   heading hold strength (0 switches it off)
save                        keep them
defaults                    back to the values in Config.hpp
```

Work on one axis at a time, roll first, then pitch, then yaw:

1. Raise P in steps of about 15 % until that axis starts to wobble, then go back to about 70 % of that value.
2. Raise I until the model stops slowly leaning or drifting back. If it starts swinging slowly, back off.
3. On roll only, a little more D (0.00004 to 0.00006) calms the overshoot after a stick movement. Leave D at 0
   on pitch and yaw. The servos are too slow for it to help.

To see what the controller is doing, record a flight log while you test:

```bash
~/.platformio/penv/bin/python tools/flight_log.py        # 60 s of the 'f' stream into logs/
```

Each line shows, per axis, the measured rate next to the rate the controller wanted (`r gyro/demand`), the
commands sent to the mixer, the yaw I term (`yI`), all four outputs and the difference between the two servos
(`diff`). Once a set of gains works, copy them into `Config.hpp` so a fresh flash starts with them.

| What you see | What to change |
|---|---|
| fast wobble left/right | roll P down 20 %, or roll D up a little |
| slow rocking left/right | roll self-level (`ang r`) down, roll I up |
| nose bobbing, servos buzzing | pitch P down; check the linkage for free play |
| nose creeps forward or back | redo the level calibration, then pitch I up |
| slow turn | servo trims first, then yaw I up; check the props spin in opposite directions |
| yaw twitching | yaw P down |

## Troubleshooting

These all came up while this firmware was being tested on the real model.

**The console shows nothing.** The PowerFeather's USB buffer is tiny (64 bytes). The firmware queues console
output and sends it a bit at a time, so make sure you're on the current version. Type `s` to switch the
status line back on if it was turned off with `x`.

**Upload fails on the first attempt.** The board reconnects as a different USB device when it enters the
bootloader. `platformio.ini` uses the 1200-baud reset for this, so it should work first time. If it still
fails, hold BOOT, press RESET, release BOOT and upload again.

**The angle drifts slowly even when the model is still.** Check the `i` stream. If the total acceleration is
far from 1.00 g or shows `IGNORED`, the accelerometer is being rejected. That was the 1.42 g reading on this
build, now corrected automatically at start-up.

**The servos slowly move apart while the throttle is up.** Watch `yI` and `diff` in the flight log. If they
grow together, the yaw I term is building up. On this build the cause was the yaw stick resting at 1438 µs,
because of a transmitter trim. Centre learning and the wider yaw deadband now take care of that. On a test
stand that stops the model turning, some build-up is normal.

**The model keeps spinning faster.** The yaw correction is reversed, or there's a constant twisting force it
can't overcome. Do the twist test from step 4. If the servos resist correctly, the cause is mechanical:
motors not upright, mismatched props, or one motor working much harder because the weight is off-centre.

**FAULT on the status line.** The sensor stopped answering. Check the four IMU wires, then reboot.

## Console commands

```
s                status line on/off           g           recalibrate the gyro (keep still)
i / r / o / f    IMU / radio / outputs / log  l           save the current attitude as level
x                stop all output              trim l|r N  servo trim in µs
pid, pid a P I D show / set rate gains        ang a P     self-level or heading-hold gain
save             store trims, level, gains    defaults    back to Config.hpp values
reboot
```

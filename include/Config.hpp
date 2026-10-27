/**
 * @file   Config.hpp
 * @brief  Every build-time setting of the bicopter in one place.
 *
 * Only the sketch (src/bi_copter_firmware.ino) includes this file. It copies these values into
 * the config structs of each library, so the libraries in lib/ stay independent of this project.
 *
 * Body frame used everywhere (right handed, FLU):
 *   +X forward (nose)   -> ROLL  axis  -> corrected by differential THRUST (ESCs)
 *   +Y left             -> PITCH axis  -> corrected by tilting BOTH servos together
 *   +Z up               -> YAW   axis  -> corrected by tilting the servos in OPPOSITE directions
 *   +roll = right side down, +pitch = nose down, +yaw = nose turns left
 */
#pragma once

#include <cstdint>
#include "Mpu6050.hpp"
#include "Pid.hpp"

// The tables below are aligned by hand, keep clang-format away from them.
// clang-format off
namespace cfg
{
// ============================================================================
//  Pins - ESP32-S3 PowerFeather
// ============================================================================
inline constexpr uint8_t PIN_IMU_SDA     = 35;
inline constexpr uint8_t PIN_IMU_SCL     = 36;

// Receiver signal wires, in channel order CH1, CH2, CH3, CH4
inline constexpr uint8_t PIN_RC[4]       = {39, 40, 41, 42};

inline constexpr uint8_t PIN_SERVO_LEFT  = 15;
inline constexpr uint8_t PIN_SERVO_RIGHT = 16;
inline constexpr uint8_t PIN_ESC_LEFT    = 6;
inline constexpr uint8_t PIN_ESC_RIGHT   = 9;

inline constexpr uint8_t PIN_LED         = 46;   // PowerFeather user LED
inline constexpr uint8_t PIN_EN_3V3      = 4;    // PowerFeather 3V3 header rail enable (powers the MPU6050)
inline constexpr uint8_t PIN_EN_SQT      = 14;   // PowerFeather STEMMA-QT rail enable

// ============================================================================
//  Loop timing
// ============================================================================
inline constexpr uint32_t LOOP_HZ        = 500;  // PID loop rate. MPU6050 delivers 1 kHz, I2C read takes ~0.4 ms
inline constexpr uint32_t LOOP_PERIOD_US = 1000000UL / LOOP_HZ;
inline constexpr uint32_t WATCHDOG_MS    = 300;  // Board resets (outputs go low) if the loop stalls this long

// ============================================================================
//  IMU - MPU6050
// ============================================================================
inline constexpr uint8_t  IMU_I2C_ADDRESS = 0x68;   // AD0 low. 0x69 if AD0 is tied high
inline constexpr uint32_t IMU_I2C_HZ      = 400000;
inline constexpr uint8_t  IMU_DLPF_CFG    = 3;      // 0=260Hz 1=184 2=94 3=44 4=21 5=10 6=5. 3 rejects most prop vibration

// How the sensor is mounted. body_axis[i] = sign[i] * sensor_axis[from[i]]  (0=X, 1=Y, 2=Z)
// Current: the sensor's X arrow points to the NOSE and the Y arrow to the LEFT motor, chip on top.
//   -> tilting the nose up/down (servo axis) turns about sensor Y, tilting left/right (ESC axis) about sensor X.
// If roll AND pitch both read the wrong sign (X arrow to the tail, Y arrow to the right motor) use
//   {{0, 1, 2}, {-1, -1, +1}}
// The map must be a proper rotation (no mirror), this is checked at compile time.
inline constexpr ImuAxisMap IMU_AXIS_MAP  = {{0, 1, 2}, {+1, +1, +1}};
static_assert(isProperRotation(IMU_AXIS_MAP), "IMU_AXIS_MAP is a mirror image - flip the sign of one axis");

inline constexpr uint16_t GYRO_CAL_SAMPLES    = 1000;  // ~1 s at boot, keep the model still
inline constexpr float    GYRO_CAL_MAX_MOTION = 4.0f;  // deg/s peak-to-peak allowed during calibration

// Attitude filter: how fast the accelerometer pulls the gyro angle back to level
// Slow on purpose: on a bicopter the accelerometer sees the thrust direction, which tilts with the servos.
// Trusting it fast (0.8 s) let every servo movement shift the level estimate and the model rocked and drifted.
// The gyro holds the angle in between; its zero is re-learned every time the model sits still disarmed.
inline constexpr float ATTITUDE_ACCEL_TAU_S = 3.0f;
inline constexpr float ATTITUDE_ACCEL_MIN_G = 0.80f;   // accel trust fades from full at 1 g to none at these limits
inline constexpr float ATTITUDE_ACCEL_MAX_G = 1.20f;
// Mahony integral term: learns the gyro zero error in flight. With the slow 3 s correction above, a 0.5 deg/s
// gyro error alone would otherwise leave the angle 1.5 deg off. 0 switches the integral term off.
inline constexpr float ATTITUDE_BIAS_TAU_S    = 15.0f;
inline constexpr float ATTITUDE_BIAS_LIMIT_DPS = 5.0f;
// Second order low pass on the accelerometer before it is fused (prop vibration). The gyro path is not delayed.
inline constexpr float ATTITUDE_ACCEL_LPF_HZ  = 10.0f;

// ============================================================================
//  Radio - FlySky FS-i6 + FS-iA6/iA6B, 4 channels PWM
// ============================================================================
// Channel numbers are 0 based. FS-i6 Mode 2 default: CH1 roll(aileron), CH2 pitch(elevator), CH3 throttle, CH4 yaw(rudder)
inline constexpr uint8_t RC_CH_ROLL     = 0;
inline constexpr uint8_t RC_CH_PITCH    = 1;
inline constexpr uint8_t RC_CH_THROTTLE = 2;
inline constexpr uint8_t RC_CH_YAW      = 3;

// Stick direction. After reversing: roll stick RIGHT, pitch stick FORWARD and yaw stick RIGHT must give
// positive values. Check with the 'r' console command and flip what is wrong.
inline constexpr bool RC_REVERSE_ROLL  = false;
inline constexpr bool RC_REVERSE_PITCH = false;
inline constexpr bool RC_REVERSE_YAW   = false;

inline constexpr uint32_t RC_TIMEOUT_US        = 250000; // no pulses this long on any channel = signal lost
inline constexpr uint16_t RC_STICK_MIN_US      = 1000;
inline constexpr uint16_t RC_STICK_MAX_US      = 2000;
inline constexpr uint16_t RC_DEADBAND_US       = 6;      // roll/pitch, around the stick centre
// Yaw shares the left stick with throttle (Mode 2): pushing throttle up nudges yaw by 20-40 us, which the
// yaw I term would integrate into a slowly growing servo difference. A larger yaw deadband ignores it.
// Measured on this FS-i6: the yaw stick rests anywhere from 1465 to 1500 us, and full throttle pulls yaw up by
// another 35-50 us. 75 us around a fixed 1500 us centre covers both.
inline constexpr uint16_t RC_YAW_DEADBAND_US   = 75;
// true:  after boot, each stick's resting position (within +-100 us of 1500) is learned ONCE and then locked.
//        Removes a transmitter trim/subtrim offset that would otherwise command a constant yaw/pitch/roll,
//        while trims changed later (in flight) still work.
// false: centre is fixed at 1500 us.
inline constexpr bool     RC_LEARN_CENTRES     = true;
inline constexpr bool     RC_LEARN_YAW_CENTRE  = false;  // yaw keeps 1500 us (see RC_YAW_DEADBAND_US)
inline constexpr uint16_t RC_THROTTLE_LOW_US   = 1100;   // below this the throttle counts as "low"
inline constexpr uint16_t RC_THROTTLE_MAX_US   = 1950;   // full throttle
// The FS-iA6 keeps sending the LAST pulses when the radio link drops, unless a failsafe is set on the
// transmitter. Set the FS-i6 throttle failsafe below normal minimum (docs/SETUP_AND_TUNING.md) and the firmware
// treats a throttle pulse below this value as "signal lost".
inline constexpr uint16_t RC_FAILSAFE_THROTTLE_US = 975;

// Stick gestures (only 4 channels, so no arm switch). All need throttle LOW.
inline constexpr uint16_t RC_YAW_ARM_US        = 1850;   // yaw fully RIGHT held -> ARM
inline constexpr uint16_t RC_YAW_DISARM_US     = 1150;   // yaw fully LEFT  held -> DISARM
inline constexpr uint16_t RC_PITCH_GESTURE_US  = 350;    // pitch more than this from centre (disarmed) -> level calibration
inline constexpr uint32_t ARM_HOLD_MS          = 1000;
inline constexpr uint32_t LEVEL_CAL_HOLD_MS    = 3000;

// ============================================================================
//  Outputs
// ============================================================================
// 100 Hz: a new position every 10 ms instead of 20 ms, and the servo pushes twice as often, so it reacts
// faster. Most MG996R (including clones) take it. If a servo gets hot to the touch after a minute, buzzes or
// jitters at rest, go back to 50. Fast digital servos (0.06-0.10 s/60 deg) can run 200-333 Hz.
inline constexpr uint32_t SERVO_HZ             = 100;
inline constexpr uint16_t SERVO_CENTER_US      = 1500;
inline constexpr uint16_t SERVO_TRAVEL_US      = 400;    // +-400 us around centre = full tilt command (~+-40 deg on MG996R)
inline constexpr uint16_t SERVO_MIN_US         = 1000;   // hard limits, protect the tilt mechanism
inline constexpr uint16_t SERVO_MAX_US         = 2000;
// Set so that a POSITIVE tilt command tilts that motor FORWARD. Mirrored servos usually need one reversed.
inline constexpr bool     SERVO_LEFT_REVERSED  = true;
inline constexpr bool     SERVO_RIGHT_REVERSED = false;

// Standard PWM ESCs (BLHeli_S / BLHeli_32 / SimonK) accept 400 Hz. Use 50 for old or unknown ESCs.
inline constexpr uint32_t ESC_HZ               = 400;
inline constexpr uint16_t ESC_OFF_US           = 1000;   // disarmed
inline constexpr uint16_t ESC_IDLE_US          = 1100;   // armed with throttle low, motors spin slowly
inline constexpr uint16_t ESC_MAX_US           = 1950;
// Thrust linearisation (ArduPilot MOT_THST_EXPO). 0 = off. With 0.65 the throttle stick is thrust, so the
// hover point moves lower on the stick (roughly 60 % before -> 45 % now).
inline constexpr float    ESC_THRUST_EXPO      = 0.65f;

// ============================================================================
//  Flight control (self-levelling "angle" mode, cascaded angle -> rate PID)
// ============================================================================
inline constexpr float MAX_ANGLE_DEG       = 25.0f;   // full stick roll/pitch angle
inline constexpr float MAX_YAW_RATE_DPS    = 120.0f;  // full stick yaw rate

// Outer loop: angle error (deg) * P = rate demand (deg/s)
inline constexpr float ANGLE_P_ROLL        = 3.0f;
inline constexpr float ANGLE_P_PITCH       = 3.0f;
inline constexpr float MAX_RATE_ROLL_DPS   = 150.0f;
inline constexpr float MAX_RATE_PITCH_DPS  = 90.0f;   // servos are slow, do not ask for more
// Square-root angle controller (ArduPilot ATC_ACCEL_R_MAX / ATC_ACCEL_P_MAX), deg/s^2. 0 = plain angle P.
// Full angle P within a few degrees of level, softer beyond so a big tilt does not slam the servos.
inline constexpr float ANGLE_ACCEL_MAX_ROLL  = 720.0f;  // motors are fast
inline constexpr float ANGLE_ACCEL_MAX_PITCH = 0.0f;    // plain angle P on pitch, like the simple MG996R builds that fly well
// Heading hold: with the yaw stick centred the nose is held pointing where it was (deg error * P = deg/s).
// Stops the slow yaw creep that a rate-only yaw loop allows. 0 = off (yaw rate mode only).
inline constexpr float HEADING_HOLD_P      = 1.5f;

// Inner loop: rate error (deg/s) -> normalised output (-1..1)
// Pitch I limit 0.50: in a logged throttle sweep this model needed ~0.3-0.45 of steady pitch command (motors
// ~10-15 deg forward) from 30 % throttle up. With the old 0.25 limit the I term saturated and the model hung
// 20-25 deg nose-up and backed away.
// The last column is the slew limit of P+D (ArduPilot SMAX), output units per second (1.0 = 35 deg of servo):
// 2.5/s = ~90 deg/s of servo. Faster P+D swings (an oscillation the MG996R cannot follow) get P and D scaled
// down automatically, so P can be set for a firm response without the servo limit-cycling. 0 = off.
//                                   P        I        D        I limit  out limit  D filter Hz  slew max
inline constexpr PidGains PID_ROLL  = {0.0016f, 0.0030f, 0.00004f, 0.15f,   0.35f,     30.0f,       0.0f};  // motors
// Slew max raised 2.5 -> 8.0 per second (~280 deg/s of servo). The MG996R moves ~350-430 deg/s; at 2.5 the
// limiter cut P to a fraction whenever a fast correction was needed, so the servos reached the set point late
// and the model fell over while still "balancing". 8.0 only steps in for a real servo limit cycle.
// Out limit 1.0 = the full +-40 deg of servo travel (was 0.70 of 350 us = +-24 deg, too little to catch a fall).
// Simple baseline (slew limiter off, last column 0): working MG996R bicopters run a plain PID with a small I.
inline constexpr PidGains PID_PITCH = {0.0030f, 0.0015f, 0.0f,     0.50f,   1.00f,     30.0f,       0.0f};  // servos together
inline constexpr PidGains PID_YAW   = {0.0025f, 0.0030f, 0.0f,     0.30f,   1.00f,     30.0f,       0.0f};  // servos opposite

// Filters. MG996R servos cannot follow anything above ~10 Hz; feeding them gyro noise / motor vibration
// makes them buzz in their gear backlash. Roll is driven by the fast motors so it keeps a higher cutoff.
inline constexpr float GYRO_LPF_ROLL_HZ    = 80.0f;
inline constexpr float GYRO_LPF_PITCH_HZ   = 40.0f;   // was 25: less delay in the pitch loop
inline constexpr float GYRO_LPF_YAW_HZ     = 25.0f;
// Was 10 Hz: that filter alone delayed a 3 Hz wobble by ~17 deg and made the pitch loop swing at any useful P.
// Phase lead on the pitch/yaw servo commands. When a correction starts, the servo is briefly asked for a
// little more than the target so it starts moving harder. Simulated on an MG996R-like servo: a 5-10 deg
// correction reaches 90 % in ~75-80 ms instead of ~120 ms. 0 = off. If the servos buzz or twitch, halve it.
// 0.04 doubled the servo-loop gain at 3-8 Hz and with pitch P 0.004 the model vibrated. Try live with "lead 0.03".
// Off in the simple baseline: a fast servo start makes the heavy motor mount kick the body the wrong way
// for a moment (reaction torque), which the lead made worse. Re-enable live with "lead 0.02" to compare.
inline constexpr float SERVO_LEAD_S        = 0.0f;    // seconds
inline constexpr float SERVO_LEAD_FILTER_HZ = 8.0f;   // smoothing of the rate of change: 8 Hz passes balancing moves, not prop vibration
inline constexpr float SERVO_LEAD_MAX      = 0.30f;   // largest extra command (0.3 = 12 deg of servo)
inline constexpr float SERVO_CMD_LPF_HZ    = 30.0f;   // smoothing of the pitch/yaw servo commands (was 20: ~3 ms less delay)

// Hover tilt compensation. A CG a few mm ahead of/behind the rotor axis makes the servos hold the motors
// tilted in hover, which pushes the model forward/back all the time. The firmware learns that tilt and pitches
// the body the opposite way so the thrust points straight up. Kept across arms, cleared at power-off.
// false: the tilt is still learned and shown on the web page ("Hover balance", a CG meter) but the body is not
// tilted. On this model the learned value (+5..7 deg) came from flights that were still swinging and the
// correction pushed it backwards: balance the CG first (until the meter reads under ~2 deg), then try true.
inline constexpr bool  HOVER_TRIM_ENABLED      = false;
// true: every take-off starts with the servos at the tilt learned in the last steady hover (frozen per flight,
// limited to HOVER_TRIM_MAX_DEG). The I term then only corrects the rest instead of building it up for 2-3 s.
// Off until a clean hover has been learned: the stored value (+6.3 deg) was learned on the ground with the I
// term wound up, and pre-tilting the servos by it made the model push forward at every take-off.
inline constexpr bool  SERVO_HOVER_FF_ENABLED  = false;
inline constexpr float HOVER_TRIM_TAU_S        = 3.0f;    // seconds to follow a change
inline constexpr float HOVER_TRIM_MAX_DEG      = 6.0f;    // more than this: fix the CG instead
inline constexpr float HOVER_TRIM_MIN_THROTTLE = 0.30f;   // learn only when airborne

// Servo gain scaling with thrust: gains are tuned at HOVER_THROTTLE and scaled by HOVER_THROTTLE / throttle.
// Set HOVER_THROTTLE to the throttle the model hovers at (shown on the web page). 0 switches scaling off.
inline constexpr float HOVER_THROTTLE          = 0.0f;    // 0 = no servo gain scaling (simple baseline)
// 1.0: never weaken the servos above hover. More throttle also loads the servo/bracket more and the motors
// get pushed back (measured: the model tilts back as throttle rises), so full correction is needed there.
inline constexpr float SERVO_GAIN_MIN          = 1.00f;
// 1.0: no boost below hover either. With 1.5 the servo gains were 1.5x stronger at the 30 % throttle the
// logged tests ran at, right where the pitch axis was already swinging at ~2 Hz. Raise only after tuning.
inline constexpr float SERVO_GAIN_MAX          = 1.00f;

// Flip a correction if that axis fights instead of resisting (docs/SETUP_AND_TUNING.md, bench checks)
inline constexpr bool REVERSE_ROLL_CORRECTION  = false;
inline constexpr bool REVERSE_PITCH_CORRECTION = false;
inline constexpr bool REVERSE_YAW_CORRECTION   = true; 

inline constexpr float    ARM_MAX_TILT_DEG     = 25.0f;   // refuse to arm when tilted more than this
inline constexpr float    CRASH_TILT_DEG       = 60.0f;   // armed and tilted more than this ...
inline constexpr uint32_t CRASH_TIME_MS        = 500;     // ... for this long -> disarm
inline constexpr uint32_t AUTO_DISARM_MS       = 15000;   // armed with throttle low this long -> disarm
inline constexpr float    THROTTLE_I_ENABLE    = 0.10f;   // below this the model counts as landed again (I terms reset)
// Throttle where the model leaves the ground. Until it is reached, I terms, heading hold, roll on the motors
// and hover learning stay off, so nothing winds up against the ground. Set it a little below the throttle
// where your model actually lifts off (the web page shows the hover throttle).
inline constexpr float    LIFTOFF_THROTTLE     = 0.35f;
inline constexpr uint32_t IMU_FAULT_READS      = 25;      // consecutive failed IMU reads (50 ms) -> fault, disarm

// ============================================================================
//  Web tuning page (WiFi access point)
// ============================================================================
// Join the WiFi network below with a phone or laptop and open http://192.168.4.1
// Live attitude, level calibration, servo tilt trims and PID gains. Changes are refused while ARMED.
inline constexpr bool     WEB_TUNING_ENABLED = true;
inline constexpr char     WIFI_AP_SSID[]     = "BiCopter";
inline constexpr char     WIFI_AP_PASSWORD[] = "bicopter123";  // at least 8 characters
inline constexpr uint8_t  WIFI_AP_CHANNEL    = 6;
// Transmit power in quarter dBm, 78 = 19.5 dBm (full). On the PowerFeather 34 (8.5 dBm) was too low:
// the access point started but no phone could see it.
inline constexpr int8_t   WIFI_TX_POWER_QDBM = 78;
// Converts servo pulses to the motor tilt angle shown on the page (MG996R: ~350 us = ~35 deg)
inline constexpr float    SERVO_US_PER_DEG   = 10.0f;

}  // namespace cfg
// clang-format on

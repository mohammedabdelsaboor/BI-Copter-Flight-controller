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
inline constexpr float ATTITUDE_ACCEL_TAU_S = 0.8f;
inline constexpr float ATTITUDE_ACCEL_MIN_G = 0.80f;   // accel ignored outside this window (manoeuvres, bumps)
inline constexpr float ATTITUDE_ACCEL_MAX_G = 1.20f;

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
inline constexpr uint16_t RC_YAW_DEADBAND_US   = 45;
// true:  after boot, each stick's resting position (within +-100 us of 1500) is learned ONCE and then locked.
//        Removes a transmitter trim/subtrim offset that would otherwise command a constant yaw/pitch/roll,
//        while trims changed later (in flight) still work.
// false: centre is fixed at 1500 us.
inline constexpr bool     RC_LEARN_CENTRES     = true;
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
// MG996R is an analogue-style servo: keep it at 50 Hz. Higher rates make it hot and jittery.
inline constexpr uint32_t SERVO_HZ             = 50;
inline constexpr uint16_t SERVO_CENTER_US      = 1500;
inline constexpr uint16_t SERVO_TRAVEL_US      = 350;    // +-350 us around centre = full tilt command (~+-35 deg on MG996R)
inline constexpr uint16_t SERVO_MIN_US         = 1000;   // hard limits, protect the tilt mechanism
inline constexpr uint16_t SERVO_MAX_US         = 2000;
// Set so that a POSITIVE tilt command tilts that motor FORWARD. Mirrored servos usually need one reversed.
inline constexpr bool     SERVO_LEFT_REVERSED  = false;
inline constexpr bool     SERVO_RIGHT_REVERSED = true;

// Standard PWM ESCs (BLHeli_S / BLHeli_32 / SimonK) accept 400 Hz. Use 50 for old or unknown ESCs.
inline constexpr uint32_t ESC_HZ               = 400;
inline constexpr uint16_t ESC_OFF_US           = 1000;   // disarmed
inline constexpr uint16_t ESC_IDLE_US          = 1100;   // armed with throttle low, motors spin slowly
inline constexpr uint16_t ESC_MAX_US           = 1950;

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
// Heading hold: with the yaw stick centred the nose is held pointing where it was (deg error * P = deg/s).
// Stops the slow yaw creep that a rate-only yaw loop allows. 0 = off (yaw rate mode only).
inline constexpr float HEADING_HOLD_P      = 1.5f;

// Inner loop: rate error (deg/s) -> normalised output (-1..1)
//                                   P        I        D        I limit  out limit  D filter Hz
inline constexpr PidGains PID_ROLL  = {0.0016f, 0.0030f, 0.00004f, 0.15f,   0.35f,     30.0f};  // motors
inline constexpr PidGains PID_PITCH = {0.0025f, 0.0040f, 0.0f,     0.25f,   0.60f,     30.0f};  // servos together
inline constexpr PidGains PID_YAW   = {0.0025f, 0.0030f, 0.0f,     0.30f,   0.60f,     30.0f};  // servos opposite

// Filters. MG996R servos cannot follow anything above ~10 Hz; feeding them gyro noise / motor vibration
// makes them buzz in their gear backlash. Roll is driven by the fast motors so it keeps a higher cutoff.
inline constexpr float GYRO_LPF_ROLL_HZ    = 80.0f;
inline constexpr float GYRO_LPF_PITCH_HZ   = 25.0f;
inline constexpr float GYRO_LPF_YAW_HZ     = 25.0f;
inline constexpr float SERVO_CMD_LPF_HZ    = 10.0f;   // smoothing of the pitch/yaw servo commands

// Flip a correction if that axis fights instead of resisting (docs/SETUP_AND_TUNING.md, bench checks)
inline constexpr bool REVERSE_ROLL_CORRECTION  = false;
inline constexpr bool REVERSE_PITCH_CORRECTION = false;
inline constexpr bool REVERSE_YAW_CORRECTION   = false;

inline constexpr float    ARM_MAX_TILT_DEG     = 25.0f;   // refuse to arm when tilted more than this
inline constexpr float    CRASH_TILT_DEG       = 70.0f;   // armed and tilted more than this ...
inline constexpr uint32_t CRASH_TIME_MS        = 500;     // ... for this long -> disarm
inline constexpr uint32_t AUTO_DISARM_MS       = 15000;   // armed with throttle low this long -> disarm
inline constexpr float    THROTTLE_I_ENABLE    = 0.10f;   // I terms held at zero below this throttle (on the ground)
inline constexpr uint32_t IMU_FAULT_READS      = 25;      // consecutive failed IMU reads (50 ms) -> fault, disarm

}  // namespace cfg
// clang-format on

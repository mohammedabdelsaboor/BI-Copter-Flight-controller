/**
 * @file   AttitudeEstimator.hpp
 * @brief  Roll/pitch estimate from gyro + accelerometer (complementary filter with Euler kinematics).
 *
 * Inputs are in the body frame FLU (X forward, Y left, Z up). Level and still the accelerometer reads
 * (0, 0, +1 g). +roll = right side down, +pitch = nose down.
 */
#pragma once

struct AttitudeConfig
{
  float accelTauS;  // time constant of the accel correction, larger = trust the gyro longer
  float accelMinG;  // accel ignored when |a| is outside [min, max] (accelerating, bumps)
  float accelMaxG;
};

class AttitudeEstimator
{
public:
  void configure(const AttitudeConfig& config) { m_config = config; }

  /** Sets the angles directly from the accelerometer (call when still, after gyro calibration). */
  void reset(const float accelG[3]);

  void update(const float gyroDps[3], const float accelG[3], float dt);

  float rollDeg() const { return m_rollRad * RAD_TO_DEG_F; }
  float pitchDeg() const { return m_pitchRad * RAD_TO_DEG_F; }
  bool accelUsed() const { return m_accelUsed; }

private:
  static constexpr float RAD_TO_DEG_F = 57.2957795f;
  static constexpr float DEG_TO_RAD_F = 0.0174532925f;

  static bool accelAngles(const float accelG[3], float& roll, float& pitch);

  AttitudeConfig m_config = {0.8f, 0.8f, 1.2f};
  float          m_rollRad = 0.0f;
  float          m_pitchRad = 0.0f;
  bool           m_accelUsed = false;
};

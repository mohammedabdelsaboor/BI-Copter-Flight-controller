/**
 * @file   AttitudeEstimator.hpp
 * @brief  Roll/pitch estimate from gyro + accelerometer: Mahony filter on a quaternion.
 *
 * The gyro is integrated as the main source. The accelerometer, after a low pass to remove vibration,
 * gives the direction of "down" and slowly steers the estimate towards it (proportional term). An integral
 * term learns the gyro's zero error while flying, so a slow correction no longer leaves a steady angle error.
 * How much the accelerometer is trusted fades smoothly as its reading moves away from 1 g.
 *
 * Inputs are in the body frame FLU (X forward, Y left, Z up). Level and still the accelerometer reads
 * (0, 0, +1 g). +roll = right side down, +pitch = nose down.
 */
#pragma once

#include "BiquadFilter.hpp"

struct AttitudeConfig
{
  float accelTauS;  // time constant of the accel correction, larger = trust the gyro longer
  float accelMinG;  // accel weight fades to zero at these magnitudes (accelerating, bumps)
  float accelMaxG;
  float biasTauS;      // how fast the gyro zero error is learned, 0 = no integral term
  float biasLimitDps;  // largest gyro zero error the integral term may learn
  float accelLpfHz;    // low pass on the accelerometer before fusion, 0 = off
  float sampleHz;      // update() rate, for the low pass
};

class AttitudeEstimator
{
public:
  void configure(const AttitudeConfig& config);

  /** Sets the angles directly from the accelerometer (call when still, after gyro calibration). */
  void reset(const float accelG[3]);

  void update(const float gyroDps[3], const float accelG[3], float dt);

  float rollDeg() const { return m_rollRad * RAD_TO_DEG_F; }
  float pitchDeg() const { return m_pitchRad * RAD_TO_DEG_F; }

  /** True when the accelerometer contributed this update. */
  bool accelUsed() const { return m_accelWeight > 0.0f; }

  /** 0..1, how much the accelerometer was trusted this update. */
  float accelWeight() const { return m_accelWeight; }

  /** Gyro zero error learned by the integral term, body roll/pitch/yaw, deg/s. */
  float gyroBiasDps(int axis) const { return m_biasRad[axis] * RAD_TO_DEG_F; }

private:
  static constexpr float RAD_TO_DEG_F = 57.2957795f;
  static constexpr float DEG_TO_RAD_F = 0.0174532925f;

  static bool accelAngles(const float accelG[3], float& roll, float& pitch);
  void setFromEuler(float roll, float pitch);
  void updateEuler();

  AttitudeConfig m_config = {3.0f, 0.8f, 1.2f, 15.0f, 5.0f, 10.0f, 500.0f};
  BiquadFilter   m_accelFilter[3];
  float          m_q[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // body -> world, w x y z
  float          m_biasRad[3] = {0.0f, 0.0f, 0.0f};  // rad/s, added to the gyro (so it is minus the zero error)
  float          m_rollRad = 0.0f;
  float          m_pitchRad = 0.0f;
  float          m_accelWeight = 0.0f;
};

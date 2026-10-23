/**
 * @file   AttitudeEstimator.cpp
 */
#include "AttitudeEstimator.hpp"
#include <math.h>

bool AttitudeEstimator::accelAngles(const float accelG[3], float& roll, float& pitch)
{
  const float ax = accelG[0];
  const float ay = accelG[1];
  const float az = accelG[2];
  const float yz = sqrtf(ay * ay + az * az);
  if (yz < 1e-3f)
  {
    return false;
  }
  roll = atan2f(ay, az);
  pitch = atan2f(-ax, yz);
  return true;
}

void AttitudeEstimator::reset(const float accelG[3])
{
  float roll = 0.0f;
  float pitch = 0.0f;
  if (accelAngles(accelG, roll, pitch))
  {
    m_rollRad = roll;
    m_pitchRad = pitch;
  }
}

void AttitudeEstimator::update(const float gyroDps[3], const float accelG[3], float dt)
{
  const float p = gyroDps[0] * DEG_TO_RAD_F;
  const float q = gyroDps[1] * DEG_TO_RAD_F;
  const float r = gyroDps[2] * DEG_TO_RAD_F;

  // Body rates -> Euler angle rates (keeps the angles right when yawing while tilted)
  constexpr float PITCH_LIMIT = 1.48f;  // ~85 deg, keeps tan() finite

  const float sinRoll = sinf(m_rollRad);
  const float cosRoll = cosf(m_rollRad);
  const float pitchClamped = fmaxf(-PITCH_LIMIT, fminf(PITCH_LIMIT, m_pitchRad));
  const float rollRate = p + tanf(pitchClamped) * (q * sinRoll + r * cosRoll);
  const float pitchRate = q * cosRoll - r * sinRoll;

  m_rollRad += rollRate * dt;
  m_pitchRad += pitchRate * dt;

  // Pull slowly towards the accelerometer angles when it only measures gravity
  const float magnitude = sqrtf(accelG[0] * accelG[0] + accelG[1] * accelG[1] + accelG[2] * accelG[2]);
  float       accRoll = 0.0f;
  float       accPitch = 0.0f;
  m_accelUsed =
      (magnitude > m_config.accelMinG) && (magnitude < m_config.accelMaxG) && accelAngles(accelG, accRoll, accPitch);
  if (m_accelUsed)
  {
    const float k = dt / (m_config.accelTauS + dt);
    float       rollError = accRoll - m_rollRad;
    // Shortest way round at the +-180 deg roll wrap
    if (rollError > 3.14159265f) rollError -= 6.28318531f;
    if (rollError < -3.14159265f) rollError += 6.28318531f;
    m_rollRad += k * rollError;
    m_pitchRad += k * (accPitch - m_pitchRad);
  }

  if (m_rollRad > 3.14159265f) m_rollRad -= 6.28318531f;
  if (m_rollRad < -3.14159265f) m_rollRad += 6.28318531f;
}

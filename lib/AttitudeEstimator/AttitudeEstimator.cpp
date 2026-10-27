/**
 * @file   AttitudeEstimator.cpp
 */
#include "AttitudeEstimator.hpp"
#include <math.h>

void AttitudeEstimator::configure(const AttitudeConfig& config)
{
  m_config = config;
  for (int i = 0; i < 3; i++)
  {
    m_accelFilter[i].configure(config.accelLpfHz, config.sampleHz);
  }
}

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
    setFromEuler(roll, pitch);
  }
  for (int i = 0; i < 3; i++)
  {
    m_accelFilter[i].reset(accelG[i]);
    m_biasRad[i] = 0.0f;
  }
}

void AttitudeEstimator::setFromEuler(float roll, float pitch)
{
  // Yaw is unknown without a compass and does not matter here, so it starts at zero
  const float cr = cosf(roll * 0.5f);
  const float sr = sinf(roll * 0.5f);
  const float cp = cosf(pitch * 0.5f);
  const float sp = sinf(pitch * 0.5f);
  m_q[0] = cr * cp;
  m_q[1] = sr * cp;
  m_q[2] = cr * sp;
  m_q[3] = -sr * sp;
  updateEuler();
}

void AttitudeEstimator::update(const float gyroDps[3], const float accelG[3], float dt)
{
  float omega[3] = {gyroDps[0] * DEG_TO_RAD_F, gyroDps[1] * DEG_TO_RAD_F, gyroDps[2] * DEG_TO_RAD_F};

  // ---- Accelerometer: low pass, then decide how much it can be trusted ----
  float a[3];
  for (int i = 0; i < 3; i++)
  {
    a[i] = m_accelFilter[i].apply(accelG[i]);
  }
  const float norm = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);

  // Full trust at exactly 1 g, falling linearly to nothing at the min/max limits
  const float halfWindow = (norm >= 1.0f) ? (m_config.accelMaxG - 1.0f) : (1.0f - m_config.accelMinG);
  m_accelWeight = (halfWindow > 0.0f) ? (1.0f - fabsf(norm - 1.0f) / halfWindow) : 0.0f;
  if ((m_accelWeight < 0.0f) || (norm < 1e-3f))
  {
    m_accelWeight = 0.0f;
  }

  if (m_accelWeight > 0.0f)
  {
    for (int i = 0; i < 3; i++)
    {
      a[i] /= norm;
    }

    // Where the current estimate thinks "up" is, seen from the body
    const float* q = m_q;
    const float  vx = 2.0f * (q[1] * q[3] - q[0] * q[2]);
    const float  vy = 2.0f * (q[0] * q[1] + q[2] * q[3]);
    const float  vz = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];

    // The cross product of measured and estimated "up" is the rotation that would line them up
    const float ex = a[1] * vz - a[2] * vy;
    const float ey = a[2] * vx - a[0] * vz;
    const float ez = a[0] * vy - a[1] * vx;
    const float error[3] = {ex, ey, ez};

    const float kp = (m_config.accelTauS > 0.0f) ? (1.0f / m_config.accelTauS) : 0.0f;
    // Integral gain from the bias time constant: ki = kp / tau keeps the loop well damped
    const float ki = ((m_config.biasTauS > 0.0f) && (kp > 0.0f)) ? (kp / m_config.biasTauS) : 0.0f;
    const float biasLimit = m_config.biasLimitDps * DEG_TO_RAD_F;

    for (int i = 0; i < 3; i++)
    {
      if (ki > 0.0f)
      {
        m_biasRad[i] += ki * m_accelWeight * error[i] * dt;
        m_biasRad[i] = fmaxf(-biasLimit, fminf(biasLimit, m_biasRad[i]));
      }
      omega[i] += kp * m_accelWeight * error[i];
    }
  }

  for (int i = 0; i < 3; i++)
  {
    omega[i] += m_biasRad[i];
  }

  // ---- Integrate: q_dot = 0.5 * q * (0, omega) ----
  float*      q = m_q;
  const float hx = 0.5f * omega[0] * dt;
  const float hy = 0.5f * omega[1] * dt;
  const float hz = 0.5f * omega[2] * dt;
  const float w = q[0] - hx * q[1] - hy * q[2] - hz * q[3];
  const float x = q[1] + hx * q[0] + hz * q[2] - hy * q[3];
  const float y = q[2] + hy * q[0] - hz * q[1] + hx * q[3];
  const float z = q[3] + hz * q[0] + hy * q[1] - hx * q[2];
  const float n = sqrtf(w * w + x * x + y * y + z * z);
  if (n > 1e-6f)
  {
    q[0] = w / n;
    q[1] = x / n;
    q[2] = y / n;
    q[3] = z / n;
  }
  updateEuler();
}

void AttitudeEstimator::updateEuler()
{
  const float* q = m_q;
  m_rollRad = atan2f(2.0f * (q[0] * q[1] + q[2] * q[3]), 1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
  const float s = 2.0f * (q[0] * q[2] - q[3] * q[1]);
  m_pitchRad = asinf(fmaxf(-1.0f, fminf(1.0f, s)));
}

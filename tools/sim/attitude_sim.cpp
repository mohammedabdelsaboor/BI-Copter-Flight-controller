/**
 * @file   attitude_sim.cpp
 * @brief  Runs the firmware's AttitudeEstimator (Mahony) next to the complementary filter it replaced,
 *         on the same simulated hover, and prints both estimates as CSV for tools/make_figures.py.
 *
 * The hover: slow pitch/roll motion, a gyro zero error of +0.6 / -0.4 deg/s left after calibration,
 * gyro noise, broadband vibration plus a 120 Hz motor tone on the accelerometer, and the small
 * horizontal accelerations that the servo corrections cause.
 *
 * Build and run (tools/make_figures.py does this for you):
 *   g++ -O2 -std=c++17 -Ilib/AttitudeEstimator -Ilib/BiquadFilter tools/sim/attitude_sim.cpp \
 *       lib/AttitudeEstimator/AttitudeEstimator.cpp lib/BiquadFilter/BiquadFilter.cpp -o attitude_sim
 */
#include "AttitudeEstimator.hpp"
#include <cmath>
#include <cstdio>
#include <random>

namespace
{
constexpr float DEG = 0.0174532925f;
constexpr float PI_F = 3.14159265f;

/** The complementary filter the firmware used before the Mahony filter (3 s time constant). */
class ComplementaryFilter
{
public:
  void update(const float gyroDps[3], const float a[3], float dt)
  {
    const float p = gyroDps[0] * DEG, q = gyroDps[1] * DEG, r = gyroDps[2] * DEG;
    const float sr = sinf(m_roll), cr = cosf(m_roll);
    m_roll += (p + tanf(m_pitch) * (q * sr + r * cr)) * dt;
    m_pitch += (q * cr - r * sr) * dt;
    const float g = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if ((g > 0.8f) && (g < 1.2f))
    {
      const float k = dt / (3.0f + dt);
      m_roll += k * (atan2f(a[1], a[2]) - m_roll);
      m_pitch += k * (atan2f(-a[0], sqrtf(a[1] * a[1] + a[2] * a[2])) - m_pitch);
    }
  }
  float rollDeg() const { return m_roll / DEG; }
  float pitchDeg() const { return m_pitch / DEG; }

private:
  float m_roll = 0.0f;
  float m_pitch = 0.0f;
};

void gravity(float roll, float pitch, float a[3])
{
  a[0] = -sinf(pitch);
  a[1] = sinf(roll) * cosf(pitch);
  a[2] = cosf(roll) * cosf(pitch);
}
}  // namespace

int main()
{
  const float     dt = 0.002f;  // 500 Hz, as in the firmware
  std::mt19937    rng(1);
  std::normal_distribution<float> noise(0.0f, 1.0f);

  AttitudeEstimator mahony;
  mahony.configure({3.0f, 0.8f, 1.2f, 15.0f, 5.0f, 10.0f, 500.0f});
  const float level[3] = {0.0f, 0.0f, 1.0f};
  mahony.reset(level);
  ComplementaryFilter complementary;

  std::printf("t,true_pitch,complementary_pitch,mahony_pitch,true_roll,complementary_roll,mahony_roll\n");
  for (int n = 0; n < 100 * 500; n++)
  {
    const float t = n * dt;
    const float roll = 3.0f * DEG * sinf(2 * PI_F * 0.5f * t);
    const float pitch = 2.0f * DEG * sinf(2 * PI_F * 0.35f * t + 1.0f);
    const float rollRate = 3.0f * 2 * PI_F * 0.5f * cosf(2 * PI_F * 0.5f * t);
    const float pitchRate = 2.0f * 2 * PI_F * 0.35f * cosf(2 * PI_F * 0.35f * t + 1.0f);

    const float gyro[3] = {rollRate + 0.6f + 0.1f * noise(rng), pitchRate - 0.4f + 0.1f * noise(rng),
                           0.1f * noise(rng)};
    float accel[3];
    gravity(roll, pitch, accel);
    for (int i = 0; i < 3; i++)
    {
      accel[i] += 0.08f * noise(rng) + 0.10f * sinf(2 * PI_F * 120.0f * t + i);
    }
    accel[0] += 0.10f * sinf(2 * PI_F * 2.0f * t);
    accel[1] += 0.06f * sinf(2 * PI_F * 2.3f * t + 0.5f);

    mahony.update(gyro, accel, dt);
    complementary.update(gyro, accel, dt);

    if (n % 25 == 0)  // 20 Hz is plenty for the plot
    {
      std::printf("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", t, pitch / DEG, complementary.pitchDeg(),
                  mahony.pitchDeg(), roll / DEG, complementary.rollDeg(), mahony.rollDeg());
    }
  }
  return 0;
}

/**
 * @file   Mpu6050.hpp
 * @brief  Minimal, fast MPU6050 driver: one 14 byte burst read per loop, gyro bias calibration and
 *         a configurable sensor -> body axis remap.
 *
 * Also accepts the common MPU6500/MPU9250 clones sold on GY-521 boards (WHO_AM_I 0x70/0x71/0x73).
 */
#pragma once

#include <Arduino.h>
#include <Wire.h>

/** body_axis[i] = sign[i] * sensor_axis[from[i]], axes 0=X 1=Y 2=Z. */
struct ImuAxisMap
{
  uint8_t from[3];
  int8_t  sign[3];
};

/** True when the map is a rotation (determinant +1). A mirror image breaks the attitude maths. */
constexpr bool isProperRotation(const ImuAxisMap& m)
{
  // Build the 3x3 matrix row by row and take its determinant
  int r[3][3] = {};
  for (int i = 0; i < 3; i++)
  {
    r[i][m.from[i]] = m.sign[i];
  }
  const int det = r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) - r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0])
                  + r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
  return det == 1;
}

struct Mpu6050Config
{
  uint8_t    sdaPin;
  uint8_t    sclPin;
  uint32_t   i2cHz;
  uint8_t    address;
  uint8_t    dlpfCfg;  // 0..6, see register 0x1A in the datasheet
  ImuAxisMap axisMap;
};

/** One sample in the BODY frame. */
struct ImuSample
{
  float gyroDps[3];  // deg/s, bias removed
  float accelG[3];   // g
  float temperatureC;
};

class Mpu6050
{
public:
  bool begin(const Mpu6050Config& config);

  /** Burst reads gyro + accel + temperature. Returns false on an I2C error (sample left unchanged). */
  bool read(ImuSample& sample);

  /**
   * Averages the gyro while the model is still. Blocking, takes samples x 1 ms.
   * @return false if the model moved more than maxMotionDps, keep the old bias and try again.
   */
  bool calibrateGyro(uint16_t samples, float maxMotionDps);

  /** Re-initialises the I2C bus and the sensor after errors. */
  bool recover();

  uint8_t whoAmI() const { return m_whoAmI; }
  uint32_t errorCount() const { return m_errorCount; }
  bool isCalibrated() const { return m_calibrated; }

private:
  static constexpr uint8_t REG_SMPLRT_DIV = 0x19;
  static constexpr uint8_t REG_CONFIG = 0x1A;
  static constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
  static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
  static constexpr uint8_t REG_ACCEL_CONFIG2 = 0x1D;  // MPU6500 family only
  static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
  static constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
  static constexpr uint8_t REG_WHO_AM_I = 0x75;

  static constexpr uint8_t GYRO_FS_500DPS = 0x08;
  static constexpr uint8_t ACCEL_FS_8G = 0x10;
  static constexpr float   GYRO_LSB_PER_DPS = 65.5f;
  static constexpr float   ACCEL_LSB_PER_G = 4096.0f;

  bool configureSensor();
  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length);
  bool readRaw(int16_t accel[3], int16_t gyro[3], int16_t& temperature);

  Mpu6050Config m_config = {};
  float         m_gyroBias[3] = {0.0f, 0.0f, 0.0f};  // raw LSB, sensor frame
  uint8_t       m_whoAmI = 0;
  uint32_t      m_errorCount = 0;
  bool          m_calibrated = false;
};

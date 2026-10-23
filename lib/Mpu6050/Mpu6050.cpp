/**
 * @file   Mpu6050.cpp
 */
#include "Mpu6050.hpp"

bool Mpu6050::begin(const Mpu6050Config& config)
{
  m_config = config;
  Wire.begin(m_config.sdaPin, m_config.sclPin, m_config.i2cHz);
  Wire.setTimeOut(2);  // ms, a stuck bus must not stall the control loop
  return configureSensor();
}

bool Mpu6050::configureSensor()
{
  writeRegister(REG_PWR_MGMT_1, 0x80);  // device reset
  delay(100);

  uint8_t id = 0;
  if (!readRegisters(REG_WHO_AM_I, &id, 1))
  {
    return false;
  }
  m_whoAmI = id;

  // 0x68 = MPU6050, 0x70/0x71/0x73 = MPU6500/9250/9255 clones with a compatible register map
  const bool known = (id == 0x68) || (id == 0x70) || (id == 0x71) || (id == 0x73) || (id == 0x72);
  if (!known)
  {
    return false;
  }

  bool ok = true;
  ok &= writeRegister(REG_PWR_MGMT_1, 0x01);  // wake up, clock from gyro X PLL (more stable)
  delay(10);
  ok &= writeRegister(REG_SMPLRT_DIV, 0x00);  // 1 kHz output rate with DLPF on
  ok &= writeRegister(REG_CONFIG, m_config.dlpfCfg & 0x07);
  ok &= writeRegister(REG_GYRO_CONFIG, GYRO_FS_500DPS);
  ok &= writeRegister(REG_ACCEL_CONFIG, ACCEL_FS_8G);
  if (id != 0x68)
  {
    ok &= writeRegister(REG_ACCEL_CONFIG2, m_config.dlpfCfg & 0x07);  // clones filter the accel separately
  }
  delay(20);
  return ok;
}

bool Mpu6050::recover()
{
  Wire.end();
  delay(2);
  Wire.begin(m_config.sdaPin, m_config.sclPin, m_config.i2cHz);
  Wire.setTimeOut(2);
  return configureSensor();
}

bool Mpu6050::read(ImuSample& sample)
{
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
  if (!readRaw(accel, gyro, temperature))
  {
    m_errorCount++;
    return false;
  }

  float gyroSensor[3];
  float accelSensor[3];
  for (int i = 0; i < 3; i++)
  {
    gyroSensor[i] = (static_cast<float>(gyro[i]) - m_gyroBias[i]) / GYRO_LSB_PER_DPS;
    accelSensor[i] = static_cast<float>(accel[i]) / ACCEL_LSB_PER_G;
  }

  for (int i = 0; i < 3; i++)
  {
    const uint8_t from = m_config.axisMap.from[i];
    const float   sign = static_cast<float>(m_config.axisMap.sign[i]);
    sample.gyroDps[i] = sign * gyroSensor[from];
    sample.accelG[i] = sign * accelSensor[from];
  }
  sample.temperatureC = (m_whoAmI == 0x68) ? (static_cast<float>(temperature) / 340.0f + 36.53f)
                                           : (static_cast<float>(temperature) / 333.87f + 21.0f);
  return true;
}

bool Mpu6050::calibrateGyro(uint16_t samples, float maxMotionDps)
{
  float    sum[3] = {0.0f, 0.0f, 0.0f};
  int16_t  minRaw[3] = {INT16_MAX, INT16_MAX, INT16_MAX};
  int16_t  maxRaw[3] = {INT16_MIN, INT16_MIN, INT16_MIN};
  uint16_t good = 0;

  for (uint16_t n = 0; n < samples; n++)
  {
    int16_t accel[3];
    int16_t gyro[3];
    int16_t temperature;
    if (readRaw(accel, gyro, temperature))
    {
      for (int i = 0; i < 3; i++)
      {
        sum[i] += gyro[i];
        minRaw[i] = min(minRaw[i], gyro[i]);
        maxRaw[i] = max(maxRaw[i], gyro[i]);
      }
      good++;
    }
    delay(1);
  }

  if (good < (samples * 9U) / 10U)
  {
    return false;  // too many I2C errors
  }

  const float maxMotionLsb = maxMotionDps * GYRO_LSB_PER_DPS;
  for (int i = 0; i < 3; i++)
  {
    if (static_cast<float>(maxRaw[i] - minRaw[i]) > maxMotionLsb)
    {
      return false;  // model was moved
    }
  }

  for (int i = 0; i < 3; i++)
  {
    m_gyroBias[i] = sum[i] / static_cast<float>(good);
  }
  m_calibrated = true;
  return true;
}

bool Mpu6050::readRaw(int16_t accel[3], int16_t gyro[3], int16_t& temperature)
{
  uint8_t b[14];
  if (!readRegisters(REG_ACCEL_XOUT_H, b, sizeof(b)))
  {
    return false;
  }
  accel[0] = static_cast<int16_t>((b[0] << 8) | b[1]);
  accel[1] = static_cast<int16_t>((b[2] << 8) | b[3]);
  accel[2] = static_cast<int16_t>((b[4] << 8) | b[5]);
  temperature = static_cast<int16_t>((b[6] << 8) | b[7]);
  gyro[0] = static_cast<int16_t>((b[8] << 8) | b[9]);
  gyro[1] = static_cast<int16_t>((b[10] << 8) | b[11]);
  gyro[2] = static_cast<int16_t>((b[12] << 8) | b[13]);
  return true;
}

bool Mpu6050::writeRegister(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(m_config.address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool Mpu6050::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length)
{
  Wire.beginTransmission(m_config.address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  if (Wire.requestFrom(m_config.address, length, static_cast<uint8_t>(true)) != length)
  {
    return false;
  }
  for (uint8_t i = 0; i < length; i++)
  {
    buffer[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

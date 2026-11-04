/**
 * @file   bi_serial_comunication.ino
 * @brief  BiCopter flight controller - ESP32-S3 SuperMini, MPU6050, 2 x MG996R tilt servos, 2 x ESC.
 *         The FlySky FS-iA6B is read by an ESP32-S3 PowerFeather, which sends the 4 channels over UART
 *         (../powerfeather_rc_bridge, frame format in ../common/RcLink/RcLink.hpp).
 *
 * This file only wires the libraries in lib/ together using the values in include/Config.hpp,
 * and runs the fixed rate control loop. Flight logic lives in lib/FlightController.
 *
 *   Arm      : throttle low + yaw fully RIGHT for 1 s
 *   Disarm   : throttle low + yaw fully LEFT  for 1 s
 *   Level cal: disarmed, model level, throttle low, pitch fully forward or back for 3 s
 *
 * Tuning  : join the "BiCopter" WiFi network and open http://192.168.4.1 (see Config.hpp)
 *
 * LED: slow blink = disarmed, solid = ARMED, fast blink = failsafe, very fast = fault/calibrating.
 *      RGB LED colours: green = disarmed, red = armed, blue = failsafe (no link), white = fault/calibrating.
 */
#include <Arduino.h>
#include <WiFi.h>
#include "esp_task_wdt.h"

#include "Config.hpp"
#include "Mpu6050.hpp"
#include "SerialRcInput.hpp"
#include "StickInput.hpp"
#include "AttitudeEstimator.hpp"
#include "BiCopterMixer.hpp"
#include "Settings.hpp"
#include "FlightController.hpp"
#include "SerialConsole.hpp"
#include "TuningWeb.hpp"

#ifndef BICOPTER_ESC_CALIBRATION
#define BICOPTER_ESC_CALIBRATION 0
#endif

static constexpr char FIRMWARE_BANNER[] = "BiCopter FC v1.1 - ESP32-S3 SuperMini, receiver over UART";

// ---------------------------------------------------------------------------------------------
//  Objects
// ---------------------------------------------------------------------------------------------
static Mpu6050           imu;
static SerialRcInput     receiver;
static StickInput        sticks;
static AttitudeEstimator attitude;
static BiCopterMixer     mixer;
static Settings          settings;
static LoopStats         loopStats = {};
static FlightController  flight(imu, attitude, sticks, mixer, settings);
static SerialConsole     console(flight, receiver, sticks, mixer, settings, loopStats);
static TuningWeb         web(flight, mixer, settings);

// ---------------------------------------------------------------------------------------------
//  Config -> library structs
// ---------------------------------------------------------------------------------------------
static MixerConfig mixerConfig()
{
  return {cfg::PIN_SERVO_LEFT, cfg::PIN_SERVO_RIGHT, cfg::PIN_ESC_LEFT,        cfg::PIN_ESC_RIGHT,
          cfg::SERVO_HZ,       cfg::ESC_HZ,          cfg::SERVO_CENTER_US,     cfg::SERVO_TRAVEL_US,
          cfg::SERVO_MIN_US,   cfg::SERVO_MAX_US,    cfg::SERVO_LEFT_REVERSED, cfg::SERVO_RIGHT_REVERSED,
          cfg::ESC_OFF_US,     cfg::ESC_IDLE_US,     cfg::ESC_MAX_US,          cfg::ESC_THRUST_EXPO};
}

static StickConfig stickConfig()
{
  return {cfg::RC_CH_ROLL,         cfg::RC_CH_PITCH,         cfg::RC_CH_THROTTLE,          cfg::RC_CH_YAW,
          cfg::RC_REVERSE_ROLL,    cfg::RC_REVERSE_PITCH,    cfg::RC_REVERSE_YAW,          cfg::RC_STICK_MIN_US,
          cfg::RC_STICK_MAX_US,    cfg::RC_DEADBAND_US,      cfg::RC_YAW_DEADBAND_US,      cfg::RC_LEARN_CENTRES,
          cfg::RC_THROTTLE_LOW_US, cfg::RC_THROTTLE_MAX_US,  cfg::RC_FAILSAFE_THROTTLE_US, cfg::RC_YAW_ARM_US,
          cfg::RC_YAW_DISARM_US,   cfg::RC_PITCH_GESTURE_US, cfg::RC_LEARN_YAW_CENTRE};
}

static FlightConfig flightConfig()
{
  return {static_cast<float>(cfg::LOOP_HZ),
          cfg::MAX_ANGLE_DEG,
          cfg::MAX_YAW_RATE_DPS,
          cfg::ANGLE_P_ROLL,
          cfg::ANGLE_P_PITCH,
          cfg::HEADING_HOLD_P,
          cfg::MAX_RATE_ROLL_DPS,
          cfg::MAX_RATE_PITCH_DPS,
          cfg::ANGLE_ACCEL_MAX_ROLL,
          cfg::ANGLE_ACCEL_MAX_PITCH,
          cfg::PID_ROLL,
          cfg::PID_PITCH,
          cfg::PID_YAW,
          cfg::REVERSE_ROLL_CORRECTION,
          cfg::REVERSE_PITCH_CORRECTION,
          cfg::REVERSE_YAW_CORRECTION,
          cfg::ARM_MAX_TILT_DEG,
          cfg::CRASH_TILT_DEG,
          cfg::CRASH_TIME_MS,
          cfg::AUTO_DISARM_MS,
          cfg::ARM_HOLD_MS,
          cfg::LEVEL_CAL_HOLD_MS,
          cfg::THROTTLE_I_ENABLE,
          cfg::IMU_FAULT_READS,
          {cfg::GYRO_LPF_ROLL_HZ, cfg::GYRO_LPF_PITCH_HZ, cfg::GYRO_LPF_YAW_HZ},
          cfg::SERVO_CMD_LPF_HZ,
          cfg::HOVER_TRIM_ENABLED,
          cfg::SERVO_HOVER_FF_ENABLED,
          cfg::HOVER_TRIM_TAU_S,
          cfg::HOVER_TRIM_MAX_DEG,
          cfg::HOVER_TRIM_MIN_THROTTLE,
          static_cast<float>(cfg::SERVO_TRAVEL_US) / cfg::SERVO_US_PER_DEG,
          cfg::HOVER_THROTTLE,
          cfg::SERVO_GAIN_MIN,
          cfg::SERVO_GAIN_MAX,
          cfg::LIFTOFF_THROTTLE,
          cfg::SERVO_LEAD_S,
          cfg::SERVO_LEAD_FILTER_HZ,
          cfg::SERVO_LEAD_MAX};
}

// ---------------------------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------------------------
static void writeLed(bool on, uint8_t r, uint8_t g, uint8_t b)
{
  static int32_t last = -1;  // the RGB LED takes ~30 us per write, only write on a change
  const int32_t  now = on ? ((r << 16) | (g << 8) | b) : 0;
  if (now == last)
  {
    return;
  }
  last = now;
  if (cfg::LED_IS_RGB)
  {
    constexpr uint8_t DIM = 16;  // the WS2812 is very bright at full scale
    rgbLedWrite(cfg::PIN_LED, on ? r / DIM : 0, on ? g / DIM : 0, on ? b / DIM : 0);
  }
  else
  {
    digitalWrite(cfg::PIN_LED, on ? HIGH : LOW);
  }
}

static void updateLed(uint32_t nowMs, bool calibrating)
{
  uint32_t periodMs = 1000;
  uint8_t  r = 0, g = 255, b = 0;
  switch (flight.state())
  {
    case FlightState::ARMED: writeLed(true, 255, 0, 0); return;
    case FlightState::FAILSAFE: periodMs = 200; r = 0; g = 0; b = 255; break;
    case FlightState::FAULT: periodMs = 100; r = g = b = 255; break;
    case FlightState::DISARMED:
    default: periodMs = 1000; break;
  }
  if (calibrating)
  {
    periodMs = 100;
    r = g = b = 255;
  }
  writeLed((nowMs % periodMs) < (periodMs / 2), r, g, b);
}

/** Blocking, model must be still. Returns false after the attempts run out. */
static bool calibrateGyro(uint8_t attempts)
{
  for (uint8_t n = 0; n < attempts; n++)
  {
    updateLed(millis(), true);
    if (imu.calibrateGyro(cfg::GYRO_CAL_SAMPLES, cfg::GYRO_CAL_MAX_MOTION))
    {
      console.printLine("gyro calibrated");
      return true;
    }
    console.printLine("gyro calibration: model moving or IMU error, retrying - keep it still");
  }
  return false;
}

static void startWatchdog()
{
  esp_task_wdt_config_t wdt = {};
  wdt.timeout_ms = cfg::WATCHDOG_MS;
  wdt.idle_core_mask = 0;    // only watch the control loop task
  wdt.trigger_panic = true;  // reset the chip -> all outputs go low -> ESCs stop
  if (esp_task_wdt_reconfigure(&wdt) != ESP_OK)
  {
    esp_task_wdt_init(&wdt);
  }
  esp_task_wdt_add(nullptr);
}

// ---------------------------------------------------------------------------------------------
//  setup / loop
// ---------------------------------------------------------------------------------------------
void setup()
{
  // 1. Outputs first: ESCs see OFF (1000 us), servos centre, before anything slow happens
  mixer.begin(mixerConfig());

  // 2. Status LED
  if (!cfg::LED_IS_RGB)
  {
    pinMode(cfg::PIN_LED, OUTPUT);
  }

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never block when no USB terminal is connected
  delay(150);                // IMU power-up
  console.begin(FIRMWARE_BANNER);

  // 3. Stored trims
  if (!settings.begin())
  {
    console.printLine("settings: none stored, using defaults");
  }
  mixer.setServoTrims(settings.get().servoTrimLeftUs, settings.get().servoTrimRightUs);

  // 4. Receiver link from the PowerFeather bridge (UART1)
  receiver.begin(Serial1, cfg::PIN_LINK_RX, cfg::PIN_LINK_TX, rclink::BAUD, cfg::RC_TIMEOUT_US);
  {
    char text[64];
    snprintf(text, sizeof(text), "receiver link: UART RX GPIO%d, %lu baud", cfg::PIN_LINK_RX,
             static_cast<unsigned long>(rclink::BAUD));
    console.printLine(text);
  }
  sticks.configure(stickConfig());

#if BICOPTER_ESC_CALIBRATION
  console.printLine("*** ESC CALIBRATION MODE - PROPELLERS OFF ***");
  console.printLine("throttle HIGH -> connect battery -> beeps -> throttle LOW -> beeps -> re-flash normal firmware");
  startWatchdog();
  return;
#endif

  // 5. IMU
  const Mpu6050Config imuConfig = {cfg::PIN_IMU_SDA,     cfg::PIN_IMU_SCL,  cfg::IMU_I2C_HZ,
                                   cfg::IMU_I2C_ADDRESS, cfg::IMU_DLPF_CFG, cfg::IMU_AXIS_MAP};
  bool                imuOk = false;
  for (int attempt = 0; (attempt < 5) && !imuOk; attempt++)
  {
    imuOk = imu.begin(imuConfig);
    if (!imuOk)
    {
      delay(200);
    }
  }
  if (!imuOk)
  {
    console.printLine("IMU NOT FOUND - check SDA=8 SCL=9, 3V3, GND, address 0x68");
  }
  else
  {
    char text[48];
    snprintf(text, sizeof(text), "IMU ok, WHO_AM_I 0x%02X", imu.whoAmI());
    console.printLine(text);
    calibrateGyro(20);
  }

  // 6. Flight controller
  attitude.configure({cfg::ATTITUDE_ACCEL_TAU_S, cfg::ATTITUDE_ACCEL_MIN_G, cfg::ATTITUDE_ACCEL_MAX_G,
                      cfg::ATTITUDE_BIAS_TAU_S, cfg::ATTITUDE_BIAS_LIMIT_DPS, cfg::ATTITUDE_ACCEL_LPF_HZ,
                      static_cast<float>(cfg::LOOP_HZ)});
  flight.configure(flightConfig());
  if (settings.gains().valid)
  {
    flight.applyGains(settings.gains());  // gains tuned from the console override Config.hpp
    console.printLine("using PID gains saved from the console ('defaults' to go back to Config.hpp)");
  }
  flight.start();

  // 7. Web tuning page (WiFi access point)
  if (cfg::WEB_TUNING_ENABLED)
  {
    const TuningWebConfig webConfig = {cfg::WIFI_AP_SSID,        cfg::WIFI_AP_PASSWORD,    cfg::WIFI_AP_CHANNEL,
                                       cfg::WIFI_TX_POWER_QDBM,  cfg::SERVO_CENTER_US,     cfg::SERVO_US_PER_DEG,
                                       cfg::SERVO_LEFT_REVERSED, cfg::SERVO_RIGHT_REVERSED};
    web.setRadio(&receiver);
    if (web.begin(webConfig))
    {
      char text[80];
      snprintf(text, sizeof(text), "web tuning: join WiFi '%s', open http://%s", cfg::WIFI_AP_SSID,
               WiFi.softAPIP().toString().c_str());
      console.printLine(text);
    }
    else
    {
      console.printLine("web tuning: WiFi access point failed to start");
    }
  }

  startWatchdog();
}

void loop()
{
  static uint32_t nextTickUs = micros();
  static uint32_t lastTickUs = micros();
  static uint32_t loopCount = 0;
  static uint32_t rateWindowMs = millis();

  // Fixed rate: wait for the next tick
  while (static_cast<int32_t>(micros() - nextTickUs) < 0)
  {
  }
  const uint32_t nowUs = micros();
  nextTickUs += cfg::LOOP_PERIOD_US;
  if (static_cast<int32_t>(nowUs - nextTickUs) > static_cast<int32_t>(cfg::LOOP_PERIOD_US))
  {
    nextTickUs = nowUs + cfg::LOOP_PERIOD_US;  // fell far behind, re-sync instead of bursting
    loopStats.overruns++;
  }
  float dt = static_cast<float>(nowUs - lastTickUs) * 1e-6f;
  dt = constrain(dt, 0.0005f, 0.02f);
  lastTickUs = nowUs;
  const uint32_t nowMs = millis();

  receiver.update();

#if BICOPTER_ESC_CALIBRATION
  {
    const uint16_t throttleUs = receiver.pulseUs(cfg::RC_CH_THROTTLE);
    uint16_t       escUs = cfg::ESC_OFF_US;
    if (!receiver.signalLost())
    {
      escUs = (throttleUs > 1800) ? cfg::ESC_MAX_US : ((throttleUs < 1200) ? cfg::ESC_OFF_US : throttleUs);
    }
    mixer.writeEscRaw(escUs);
  }
#else
  flight.update(receiver, dt, nowMs);

  web.update(nowMs, loopStats.loopHz);

  // 'g' from the console or the web page, or the boot calibration has not succeeded yet (model was moving)
  const bool needCalibration = !imu.isCalibrated() && (flight.state() == FlightState::DISARMED);
  const bool consoleRequest = console.takeGyroCalibrationRequest();
  const bool webRequest = web.takeGyroCalibrationRequest();
  if (consoleRequest || webRequest || needCalibration)
  {
    esp_task_wdt_delete(nullptr);  // calibration blocks ~1 s, disarmed only
    mixer.safe();
    if (calibrateGyro(needCalibration ? 1 : 3))
    {
      flight.start();
    }
    esp_task_wdt_add(nullptr);
    nextTickUs = micros();
    lastTickUs = micros();
  }
#endif

  console.update(nowMs);
  updateLed(nowMs, false);

  // Loop statistics
  loopCount++;
  if ((nowMs - rateWindowMs) >= 1000)
  {
    loopStats.loopHz = loopCount;
    loopCount = 0;
    rateWindowMs = nowMs;
  }
  const uint32_t bodyUs = micros() - nowUs;
  if (bodyUs > loopStats.maxLoopUs)
  {
    loopStats.maxLoopUs = bodyUs;
  }

  esp_task_wdt_reset();
}

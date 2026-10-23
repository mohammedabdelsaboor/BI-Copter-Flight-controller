/**
 * @file   BiCopterMixer.hpp
 * @brief  Turns throttle + roll/pitch/yaw commands into 2 ESC and 2 tilt servo pulses.
 *
 *   left motor  = throttle + roll          tilt left  = pitch - yaw
 *   right motor = throttle - roll          tilt right = pitch + yaw
 *
 * Signs (body frame FLU): +roll raises the left side (rolls right), +pitch tilts both motors forward
 * (nose down), +yaw tilts the left motor back and the right motor forward (nose turns left).
 * A positive tilt command must move that motor FORWARD - set the servo reverse flags so it does.
 */
#pragma once

#include <Arduino.h>
#include "PwmOutput.hpp"

struct MixerConfig
{
  uint8_t  servoLeftPin;
  uint8_t  servoRightPin;
  uint8_t  escLeftPin;
  uint8_t  escRightPin;
  uint32_t servoHz;
  uint32_t escHz;
  uint16_t servoCenterUs;
  uint16_t servoTravelUs;  // pulse change for a tilt command of 1.0
  uint16_t servoMinUs;
  uint16_t servoMaxUs;
  bool     servoLeftReversed;
  bool     servoRightReversed;
  uint16_t escOffUs;
  uint16_t escIdleUs;
  uint16_t escMaxUs;
};

/** throttle 0..1, roll/pitch/yaw -1..1 */
struct MixerCommand
{
  float throttle;
  float roll;
  float pitch;
  float yaw;
};

struct MixerOutputs
{
  uint16_t escLeftUs;
  uint16_t escRightUs;
  uint16_t servoLeftUs;
  uint16_t servoRightUs;
};

class BiCopterMixer
{
public:
  /** Attaches all outputs: ESCs at OFF, servos at centre + trim. Call first thing in setup(). */
  bool begin(const MixerConfig& config);

  void setServoTrims(int16_t leftUs, int16_t rightUs);

  /** Computes the pulses without writing them (used to show outputs while disarmed). */
  MixerOutputs mix(const MixerCommand& command, bool motorsArmed) const;

  /** Mixes and writes. With motorsArmed false the ESCs are held at OFF and only the servos move. */
  void write(const MixerCommand& command, bool motorsArmed);

  /** ESC calibration only: both ESCs get this pulse directly. */
  void writeEscRaw(uint16_t pulseUs);

  /** Motors OFF, servos centred. */
  void safe();

  const MixerOutputs& outputs() const { return m_outputs; }

private:
  uint16_t tiltToServoUs(float tilt, bool reversed, int16_t trimUs) const;
  uint16_t thrustToEscUs(float thrust) const;
  void apply(const MixerOutputs& outputs);

  MixerConfig  m_config = {};
  PwmOutput    m_servoLeft;
  PwmOutput    m_servoRight;
  PwmOutput    m_escLeft;
  PwmOutput    m_escRight;
  MixerOutputs m_outputs = {};
  int16_t      m_trimLeftUs = 0;
  int16_t      m_trimRightUs = 0;
};

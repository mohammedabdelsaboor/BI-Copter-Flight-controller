/**
 * @file   Pid.hpp
 * @brief  Rate PID with derivative on measurement, filtered D and anti-windup.
 */
#pragma once

#include <math.h>
#include "Pt1Filter.hpp"

struct PidGains
{
  float kp;
  float ki;         // per second
  float kd;         // seconds
  float iLimit;     // |I term| maximum
  float outLimit;   // |output| maximum
  float dFilterHz;  // low pass on the D term, 0 = off
  // Slew rate limit of the P+D output in output units per second (ArduPilot ATC_RAT_xxx_SMAX), 0 = off.
  // When P+D moves faster than this (an oscillation), P and D are scaled down until it stops.
  float slewMax = 0.0f;
};

class Pid
{
public:
  void configure(const PidGains& gains, float sampleHz);

  /**
   * @param integrate  false holds the I term at zero (model on the ground).
   * @return           output clamped to +-outLimit
   */
  float update(float setpoint, float measurement, float dt, bool integrate);
  void reset();

  float p() const { return m_p; }
  /** P and D scale from the slew limiter: 1 = full gain, lower = oscillation being damped. */
  float slewModifier() const { return m_slewMod; }
  float i() const { return m_i; }
  float d() const { return m_d; }

private:
  PidGains  m_gains = {};
  Pt1Filter m_dFilter;
  float     m_lastMeasurement = 0.0f;
  float     m_lastPd = NAN;
  float     m_slewRate = 0.0f;       // filtered slew of the raw P+D output, per second
  float     m_slewAmplitude = 0.0f;  // decaying peak of |m_slewRate|
  float     m_slewMod = 1.0f;
  float     m_p = 0.0f;
  float     m_i = 0.0f;
  float     m_d = 0.0f;
  float     m_output = 0.0f;
  bool      m_first = true;
};

/**
 * @file   Pid.hpp
 * @brief  Rate PID with derivative on measurement, filtered D and anti-windup.
 */
#pragma once

#include "Pt1Filter.hpp"

struct PidGains
{
  float kp;
  float ki;         // per second
  float kd;         // seconds
  float iLimit;     // |I term| maximum
  float outLimit;   // |output| maximum
  float dFilterHz;  // low pass on the D term, 0 = off
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
  float i() const { return m_i; }
  float d() const { return m_d; }

private:
  PidGains  m_gains = {};
  Pt1Filter m_dFilter;
  float     m_lastMeasurement = 0.0f;
  float     m_p = 0.0f;
  float     m_i = 0.0f;
  float     m_d = 0.0f;
  float     m_output = 0.0f;
  bool      m_first = true;
};

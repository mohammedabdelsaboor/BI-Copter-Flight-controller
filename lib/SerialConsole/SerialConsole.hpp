/**
 * @file   SerialConsole.hpp
 * @brief  USB serial commands and live data streams for bench setup. Type 'h' + Enter for help.
 *
 * Writing never blocks: when no terminal is connected (or the USB buffer is full) output is dropped,
 * so the console cannot slow down the control loop.
 */
#pragma once

#include <Arduino.h>
#include "FlightController.hpp"
#include "RcPwmInput.hpp"
#include "StickInput.hpp"
#include "BiCopterMixer.hpp"
#include "Settings.hpp"

struct LoopStats
{
  uint32_t loopHz;
  uint32_t maxLoopUs;  // longest loop body since last status print
  uint32_t overruns;   // loops that started late
};

class SerialConsole
{
public:
  SerialConsole(FlightController& flight, const RcPwmInput& receiver, const StickInput& sticks, BiCopterMixer& mixer,
                Settings& settings, LoopStats& stats);

  void begin(const char* banner);

  /** Reads commands and prints streams. Call every loop. */
  void update(uint32_t nowMs);

  /** True once after the user typed 'g'. The sketch runs the (blocking) calibration. */
  bool takeGyroCalibrationRequest();

  /** Queues a line for sending. If the queue is full the line is dropped, it never blocks. */
  void printLine(const char* text);

private:
  enum class Stream : uint8_t
  {
    NONE,
    IMU,
    RC,
    OUTPUTS,
    FLIGHT
  };

  void handleCommand(char* line);
  void printHelp();
  void printStatus();
  void printGains();
  static int axisIndex(char axis);
  void printStream();
  void printf(const char* format, ...) __attribute__((format(printf, 2, 3)));
  void drainOutput();

  // Lines are queued here and sent as USB space frees up. The PowerFeather's USB CDC transmit
  // buffer is only 64 bytes, so a whole status line never fits at once.
  static constexpr uint16_t TX_QUEUE_SIZE = 2048;
  char                      m_txQueue[TX_QUEUE_SIZE] = {};
  uint16_t                  m_txHead = 0;  // next byte to write into
  uint16_t                  m_txTail = 0;  // next byte to send

  FlightController& m_flight;
  const RcPwmInput& m_receiver;
  const StickInput& m_sticks;
  BiCopterMixer&    m_mixer;
  Settings&         m_settings;
  LoopStats&        m_stats;

  char     m_line[48] = {};
  uint8_t  m_lineLength = 0;
  Stream   m_stream = Stream::NONE;
  bool     m_status = true;
  bool     m_gyroCalRequest = false;
  uint32_t m_lastStreamMs = 0;
  uint32_t m_lastStatusMs = 0;
  uint32_t m_seenEvents = 0;
};

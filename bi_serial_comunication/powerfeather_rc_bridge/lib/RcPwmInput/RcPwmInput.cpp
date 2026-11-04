/**
 * @file   RcPwmInput.cpp
 */
#include "RcPwmInput.hpp"
#include "esp_timer.h"

bool RcPwmInput::begin(const uint8_t (&pins)[CHANNELS], uint32_t timeoutUs)
{
  m_timeoutUs = timeoutUs;
  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    m_capture[i].pin = pins[i];
    pinMode(pins[i], INPUT_PULLDOWN);  // unplugged wire reads "no pulses" instead of noise
  }

  m_hardwareCapture = beginHardwareCapture();
  if (!m_hardwareCapture)
  {
    beginGpioInterrupts();
  }
  return m_hardwareCapture;
}

bool RcPwmInput::beginHardwareCapture()
{
  static_assert(CHANNELS <= 2 * CAPTURE_CHANNELS_PER_GROUP, "ESP32-S3 has 6 capture inputs");
  mcpwm_cap_timer_handle_t timers[2] = {nullptr, nullptr};

  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    const uint32_t group = i / CAPTURE_CHANNELS_PER_GROUP;
    if (timers[group] == nullptr)
    {
      mcpwm_capture_timer_config_t timerConfig = {};
      timerConfig.group_id = static_cast<int>(group);
      timerConfig.clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT;
      if ((mcpwm_new_capture_timer(&timerConfig, &timers[group]) != ESP_OK)
          || (mcpwm_capture_timer_enable(timers[group]) != ESP_OK)
          || (mcpwm_capture_timer_start(timers[group]) != ESP_OK))
      {
        return false;
      }
    }

    uint32_t resolutionHz = 0;
    mcpwm_capture_timer_get_resolution(timers[group], &resolutionHz);
    m_capture[i].ticksPerUs = resolutionHz / 1000000UL;
    if (m_capture[i].ticksPerUs == 0)
    {
      return false;
    }

    mcpwm_capture_channel_config_t channelConfig = {};
    channelConfig.gpio_num = m_capture[i].pin;
    channelConfig.prescale = 1;
    channelConfig.flags.pos_edge = 1;
    channelConfig.flags.neg_edge = 1;

    mcpwm_cap_channel_handle_t      channel = nullptr;
    mcpwm_capture_event_callbacks_t callbacks = {};
    callbacks.on_cap = onCapture;
    if ((mcpwm_new_capture_channel(timers[group], &channelConfig, &channel) != ESP_OK)
        || (mcpwm_capture_channel_register_event_callbacks(channel, &callbacks, &m_capture[i]) != ESP_OK)
        || (mcpwm_capture_channel_enable(channel) != ESP_OK))
    {
      return false;
    }
  }
  return true;
}

void RcPwmInput::beginGpioInterrupts()
{
  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    attachInterruptArg(m_capture[i].pin, onEdge, &m_capture[i], CHANGE);
  }
}

bool IRAM_ATTR RcPwmInput::onCapture(mcpwm_cap_channel_handle_t, const mcpwm_capture_event_data_t* event, void* arg)
{
  Capture* const capture = static_cast<Capture*>(arg);
  if (event->cap_edge == MCPWM_CAP_EDGE_POS)
  {
    capture->riseStamp = event->cap_value;
  }
  else
  {
    storeWidth(capture, (event->cap_value - capture->riseStamp) / capture->ticksPerUs);
  }
  return false;
}

void IRAM_ATTR RcPwmInput::onEdge(void* arg)
{
  Capture* const capture = static_cast<Capture*>(arg);
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time());
  if (digitalRead(capture->pin))
  {
    capture->riseStamp = now;
  }
  else
  {
    storeWidth(capture, now - capture->riseStamp);
  }
}

void IRAM_ATTR RcPwmInput::storeWidth(Capture* capture, uint32_t widthUs)
{
  capture->rawWidthUs = widthUs;
  if ((widthUs >= MIN_VALID_US) && (widthUs <= MAX_VALID_US))
  {
    capture->widthUs = widthUs;
    capture->lastPulseUs = static_cast<uint32_t>(esp_timer_get_time());
    capture->fresh = true;
  }
}

void RcPwmInput::update()
{
  const uint32_t now = static_cast<uint32_t>(esp_timer_get_time());
  bool           lost = false;
  bool           anyFresh = false;

  for (uint8_t i = 0; i < CHANNELS; i++)
  {
    Capture& c = m_capture[i];
    if (c.fresh)
    {
      c.fresh = false;
      m_history[i][m_historyIndex[i]] = static_cast<uint16_t>(c.widthUs);
      m_historyIndex[i] = (m_historyIndex[i] + 1) % 3;
      // Until 3 pulses arrived the history holds zeros, so fill it to avoid a slow start
      if (m_pulseUs[i] == 0)
      {
        m_history[i][0] = m_history[i][1] = m_history[i][2] = static_cast<uint16_t>(c.widthUs);
      }
      m_pulseUs[i] = median3(m_history[i][0], m_history[i][1], m_history[i][2]);
      anyFresh = true;
    }
    // Signed compare: the ISR may have stamped a pulse after 'now' was read
    if ((m_pulseUs[i] == 0) || (static_cast<int32_t>(now - c.lastPulseUs) > static_cast<int32_t>(m_timeoutUs)))
    {
      lost = true;
    }
  }

  if (anyFresh)
  {
    m_frames++;
  }
  m_signalLost = lost;
}

uint16_t RcPwmInput::median3(uint16_t a, uint16_t b, uint16_t c)
{
  return max(min(a, b), min(max(a, b), c));
}

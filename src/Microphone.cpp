// SPDX-License-Identifier: GPL-3.0-or-later
#include "Microphone.h"
#include "SampleFormat.h"
#include <cmath>

bool Microphone::begin() {
  repeated_ = 0;
#if SPL_MOCK_INPUT
  nextTick_ = xTaskGetTickCount();
  installed_ = true;
  return true;
#else
  // Install on the audio task's core (0), apart from Protomatter's core-1 ISR.
  i2s_config_t cfg = {};
  cfg.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = config::sampleRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = config::dmaBuffers;
  cfg.dma_buf_len = config::blockFrames;
  cfg.use_apll = false; // ESP32-S3 has no audio PLL; use 160 MHz fractional divider
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 64, &events_) != ESP_OK) {
    ++errors_;
    return false;
  }
  installed_ = true;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE; // ICS-43434 needs no MCLK
  pins.bck_io_num = config::bclk;
  pins.ws_io_num = config::lrclk;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = config::din;
  pinMode(config::din, INPUT_PULLDOWN);
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    ++errors_;
    end();
    return false;
  }
  return true;
#endif
}

void Microphone::end() {
#if !SPL_MOCK_INPUT
  if (installed_) i2s_driver_uninstall(I2S_NUM_0);
#endif
  installed_ = false;
  events_ = nullptr;
}

float Microphone::sampleRate() const {
#if SPL_MOCK_INPUT
  return config::sampleRate;
#else
  return installed_ ? i2s_get_clk(I2S_NUM_0) : 0;
#endif
}

bool Microphone::drainEvents() {
  bool ok = true;
  i2s_event_t ev;
  while (events_ && xQueueReceive(events_, &ev, 0) == pdTRUE) {
    if (ev.type == I2S_EVENT_RX_Q_OVF) { ++overruns_; ok = false; }
    if (ev.type == I2S_EVENT_DMA_ERROR) { ++errors_; ok = false; }
  }
  return ok;
}

bool Microphone::read(float* samples) {
  if (!installed_) return false;
#if SPL_MOCK_INPUT
  // Realtime 1 kHz sine, varying between 80 and 104 nominal dBA every 5 s.
  vTaskDelayUntil(&nextTick_, pdMS_TO_TICKS(10));
  const float db = ((mockFrame_ / (5 * config::sampleRate)) % 2) ? 104 : 80;
  const float amplitude = powf(10, (db - 120) / 20);
  for (size_t i = 0; i < config::blockFrames; ++i, ++mockFrame_) {
    const float x = amplitude * sinf(6.28318530718f * (mockFrame_ % 48) / 48);
    const int32_t word = lroundf(x * 8388608.0f);
    samples[i] = normalized24(uint32_t(word) << 8);
  }
  return true;
#else
  if (!drainEvents()) return false;
  size_t bytes = 0;
  const auto error = i2s_read(I2S_NUM_0, slots_, sizeof(slots_), &bytes, pdMS_TO_TICKS(100));
  const bool eventsOk = drainEvents();
  if (error != ESP_OK || bytes != sizeof(slots_)) { ++errors_; return false; }
  if (!eventsOk) return false;
  // In 32-bit stereo standard I2S DMA order is left, right; ignore inactive right.
  for (size_t i = 0; i < config::blockFrames; ++i) {
    const int32_t word = decode24(slots_[2 * i]);
    if (word == previous_) {
      if (repeated_ < config::sampleRate) ++repeated_;
    } else repeated_ = 0;
    previous_ = word;
    samples[i] = float(word) / 8388608.0f;
  }
  if (stuck()) { ++errors_; return false; }
  return true;
#endif
}

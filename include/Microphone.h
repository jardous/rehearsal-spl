// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Config.h"
#include <Arduino.h>
#include <driver/i2s.h>

class Microphone {
 public:
  bool begin();
  void end();
  bool read(float* samples);
  uint32_t overruns() const { return overruns_; }
  uint32_t errors() const { return errors_; }
  bool stuck() const { return repeated_ >= config::sampleRate / 10; }
  float sampleRate() const;
 private:
  bool drainEvents();
  bool installed_ = false;
  QueueHandle_t events_ = nullptr;
  uint32_t slots_[config::blockFrames * 2] = {};
  uint32_t overruns_ = 0, errors_ = 0, repeated_ = 0;
  int32_t previous_ = 0;
#if SPL_MOCK_INPUT
  TickType_t nextTick_ = 0;
  uint64_t mockFrame_ = 0;
#endif
};

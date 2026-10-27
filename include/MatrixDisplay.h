// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Adafruit_Protomatter.h>
#include "Settings.h"

class MatrixDisplay {
 public:
  MatrixDisplay();
  bool begin();
  void render(const AudioSnapshot& audio, const Settings& settings);
  uint32_t refreshFrames();
 private:
  void digit(int x, int value, uint16_t color);
  uint16_t color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
  Adafruit_Protomatter matrix_;
  bool ready_ = false;
  uint16_t previous_[64 * 32] = {};
};

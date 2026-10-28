// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Adafruit_Protomatter.h>
#include "Settings.h"
#include "Visualization.h"

class MatrixDisplay {
 public:
  MatrixDisplay();
  bool begin();
  void render(const AudioSnapshot& audio, const Settings& settings, ViewMode view,
              const VisualFrame& visual, uint32_t now);
  uint32_t refreshFrames();
 private:
  void digit(int x, int value, uint16_t color);
  uint16_t color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
  uint16_t palette(uint8_t position, uint8_t brightness);
  void renderVisual(ViewMode view, const VisualFrame& visual, const Settings& settings,
                    uint32_t now);
  Adafruit_Protomatter matrix_;
  bool ready_ = false;
  uint16_t previous_[64 * 32] = {};
  float peaks_[visual::bands] = {};
  uint8_t waterfall_[32][visual::bands] = {};
  uint32_t visualAt_ = 0;
  ViewMode previousView_ = ViewMode::Meter;
};

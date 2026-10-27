// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SPLMeter.h"

struct Settings {
  uint32_t magic = 0x534c4d31;
  float calibration = 0;
  float yellow = 85, red = 95;
  uint8_t brightness = 64;
  DisplayMode mode = DisplayMode::Fast;
  bool load();
  bool save() const;
  bool valid() const;
};

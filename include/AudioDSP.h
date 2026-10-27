// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include "AWeighting.h"

struct DSPBlock {
  double rawEnergy = 0;       // normalized, before DC removal
  double acEnergy = 0;        // after DC removal
  double weightedEnergy = 0;
  float peak = 0;
  unsigned clippedSamples = 0;
  bool finite = true;
};

class AudioDSP {
 public:
  void reset();
  float filter(float sample);
  DSPBlock process(const float* input, float* weighted, size_t count);
 private:
  float previousInput_ = 0, previousDC_ = 0;
  float z1_[3] = {}, z2_[3] = {};
};

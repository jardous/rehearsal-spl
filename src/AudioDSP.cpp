// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioDSP.h"
#include <cmath>

void AudioDSP::reset() { *this = AudioDSP(); }

float AudioDSP::filter(float x) {
  // 3.8 Hz high-pass; unity high-frequency gain, not microphone equalization.
  const float dc = 0.99975f * (x - previousInput_) + 0.9995f * previousDC_;
  previousInput_ = x;
  previousDC_ = dc;
  x = dc * aWeightGain;
  for (unsigned i = 0; i < 3; ++i) {
    const auto& c = aWeightSOS[i];
    const float y = x + z1_[i];
    z1_[i] = c.b1 * x - c.a1 * y + z2_[i];
    z2_[i] = c.b2 * x - c.a2 * y;
    x = y;
  }
  return x;
}

DSPBlock AudioDSP::process(const float* input, float* weighted, size_t count) {
  DSPBlock result;
  for (size_t i = 0; i < count; ++i) {
    const float x = input[i];
    result.rawEnergy += double(x) * x;
    result.peak = fmaxf(result.peak, fabsf(x));
    if (fabsf(x) >= 0.999f) ++result.clippedSamples;
    weighted[i] = filter(x);
    result.acEnergy += double(previousDC_) * previousDC_;
    result.weightedEnergy += double(weighted[i]) * weighted[i];
    result.finite = result.finite && std::isfinite(x) && std::isfinite(weighted[i]);
  }
  return result;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Config.h"
#include <cstddef>

enum class ViewMode : uint8_t { Meter, Spectrum, Waterfall, Waveform, Reactive, Count };
const char* viewName(ViewMode view);

namespace visual {
constexpr size_t fftSize = 2048;
constexpr size_t bands = 16;
constexpr size_t columns = 64;
constexpr float lowHz = 60, highHz = 16000;
}

struct VisualInput {
  float samples[visual::fftSize] = {};
  uint32_t timestampMs = 0;
};

// Audio task only. A completed frame is copied to a one-element overwrite queue;
// analysis runs on the UI core and can never back-pressure acquisition.
class VisualCapture {
 public:
  bool push(float sample) {
    frame.samples[position_++] = sample;
    if (position_ != visual::fftSize) return false;
    position_ = 0;
    return true;
  }
  void reset() { position_ = 0; }
  VisualInput frame;
 private:
  size_t position_ = 0;
};

struct VisualFrame {
  float bandDB[visual::bands] = {};
  float waveMin[visual::columns] = {}, waveMax[visual::columns] = {};
  float referenceDB = -45, strength = 0;
  uint32_t timestampMs = 0;
  bool valid = false;
};

// Unweighted, DC-removed Hann-windowed FFT. Bands are relative digital energy,
// NOT per-band dBA. All storage is fixed; no allocation during analysis.
class SpectrumAnalyzer {
 public:
  void process(const VisualInput& input, VisualFrame& output);
  static int bandForFrequency(float hz);
 private:
  void prepare();
  void fft();
  bool prepared_ = false;
  float real_[visual::fftSize] = {}, imag_[visual::fftSize] = {};
  float twiddleReal_[visual::fftSize / 2] = {}, twiddleImag_[visual::fftSize / 2] = {};
  int8_t binBand_[visual::fftSize / 2 + 1] = {};
  float referenceDB_ = -45;
};

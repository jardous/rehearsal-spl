// SPDX-License-Identifier: GPL-3.0-or-later
#include "Visualization.h"
#include <algorithm>
#include <cmath>

const char* viewName(ViewMode view) {
  switch (view) {
    case ViewMode::Meter: return "SPL";
    case ViewMode::Spectrum: return "SPECTRUM";
    case ViewMode::Waterfall: return "WATERFALL";
    case ViewMode::Waveform: return "WAVEFORM";
    case ViewMode::Reactive: return "REACTIVE";
    default: return "SPL";
  }
}

int SpectrumAnalyzer::bandForFrequency(float hz) {
  if (!std::isfinite(hz) || hz < visual::lowHz || hz >= visual::highHz) return -1;
  const int band = int(visual::bands * std::log(hz / visual::lowHz)
                       / std::log(visual::highHz / visual::lowHz));
  return std::min(int(visual::bands) - 1, band);
}

void SpectrumAnalyzer::prepare() {
  constexpr float pi = 3.14159265358979323846f;
  for (size_t i = 0; i < visual::fftSize / 2; ++i) {
    const float angle = 2 * pi * i / visual::fftSize;
    twiddleReal_[i] = std::cos(angle);
    twiddleImag_[i] = -std::sin(angle);
  }
  for (size_t i = 0; i <= visual::fftSize / 2; ++i)
    binBand_[i] = bandForFrequency(float(i * config::sampleRate) / visual::fftSize);
  prepared_ = true;
}

void SpectrumAnalyzer::fft() {
  for (size_t i = 1, j = 0; i < visual::fftSize; ++i) {
    size_t bit = visual::fftSize >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { std::swap(real_[i], real_[j]); std::swap(imag_[i], imag_[j]); }
  }
  for (size_t length = 2; length <= visual::fftSize; length <<= 1) {
    const size_t half = length / 2, stride = visual::fftSize / length;
    for (size_t base = 0; base < visual::fftSize; base += length) {
      for (size_t j = 0; j < half; ++j) {
        const size_t a = base + j, b = a + half, t = j * stride;
        const float re = real_[b] * twiddleReal_[t] - imag_[b] * twiddleImag_[t];
        const float im = real_[b] * twiddleImag_[t] + imag_[b] * twiddleReal_[t];
        real_[b] = real_[a] - re; imag_[b] = imag_[a] - im;
        real_[a] += re; imag_[a] += im;
      }
    }
  }
}

void SpectrumAnalyzer::process(const VisualInput& input, VisualFrame& output) {
  output = {};
  output.timestampMs = input.timestampMs;
  if (!prepared_) prepare();
  double mean = 0;
  for (float x : input.samples) {
    if (!std::isfinite(x) || std::fabs(x) >= 0.999f) return;
    mean += x;
  }
  mean /= visual::fftSize;
  float peak = 0;
  double energy = 0;
  for (size_t i = 0; i < visual::fftSize; ++i) {
    const float x = input.samples[i] - float(mean);
    peak = std::max(peak, std::fabs(x));
    energy += double(x) * x;
    // Periodic Hann, reusing the FFT twiddle cosine table.
    const float cosine = i < visual::fftSize / 2 ? twiddleReal_[i]
      : -twiddleReal_[i - visual::fftSize / 2];
    real_[i] = x * (0.5f - 0.5f * cosine);
    imag_[i] = 0;
  }
  const float scale = std::max(peak, 0.0001f); // waveform auto-gain only
  for (size_t column = 0; column < visual::columns; ++column) {
    float low = 1, high = -1;
    for (size_t j = 0; j < visual::fftSize / visual::columns; ++j) {
      const size_t i = column * (visual::fftSize / visual::columns) + j;
      const float x = (input.samples[i] - float(mean)) / scale;
      low = std::min(low, x); high = std::max(high, x);
    }
    output.waveMin[column] = low; output.waveMax[column] = high;
  }
  fft();
  double bandEnergy[visual::bands] = {};
  // One-sided power, corrected for the Hann window's mean-square gain (3/8).
  constexpr double normalization = 2.0 / (visual::fftSize * visual::fftSize * 0.375);
  for (size_t i = 1; i < visual::fftSize / 2; ++i) {
    const int band = binBand_[i];
    if (band >= 0)
      bandEnergy[band] += (double(real_[i]) * real_[i] + double(imag_[i]) * imag_[i]) * normalization;
  }
  float loudest = -120;
  for (size_t band = 0; band < visual::bands; ++band) {
    output.bandDB[band] = 10 * std::log10(std::max(1e-12, bandEnergy[band]));
    loudest = std::max(loudest, output.bandDB[band]);
  }
  // Visual auto-range only. No gain or correction enters SPL measurements.
  referenceDB_ = std::max(-45.0f, std::max(loudest, referenceDB_ - 1));
  output.referenceDB = referenceDB_;
  const float rawDB = 10 * std::log10(std::max(1e-12, energy / visual::fftSize));
  output.strength = std::max(0.0f, std::min(1.0f, (rawDB + 85) / 60));
  output.valid = true;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

namespace config {
constexpr uint32_t sampleRate = 48000;
constexpr size_t blockFrames = 480; // 10 ms, stereo DMA block = 3840 bytes
constexpr int dmaBuffers = 12;      // 120 ms nominal buffering
constexpr uint32_t staleMs = 250;
constexpr float nominalOffset = 123.01029995664f;
// RMS normalized to signed full-scale peak: full-scale sine RMS is -3.0103 dB.
// SPL = 94 - (-26) + 3.0103 + 20 log10(normalized RMS) + calibration.
constexpr int bclk = 9, lrclk = 10, din = 11;
constexpr int buttonUp = 6, buttonDown = 7;
}

#ifndef SPL_MOCK_INPUT
#define SPL_MOCK_INPUT 0
#endif
#ifndef SPL_ENABLE_ROLLING
#define SPL_ENABLE_ROLLING 1
#endif

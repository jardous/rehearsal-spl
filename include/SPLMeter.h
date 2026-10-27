// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Config.h"
#include <cmath>

enum class DisplayMode : uint8_t { Fast, Slow, Leq };
enum class AudioStatus : uint8_t { Warming, Ok, MicError, Overrun, Clipping, DSPError, Stale };
const char* statusName(AudioStatus status);

struct Levels {
  float fast = NAN, slow = NAN, leq1s = NAN, rolling = NAN, maximum = NAN;
  uint16_t rollingSeconds = 0;
  bool valid = false;
};

class SPLMeter {
 public:
  SPLMeter();
  void resetContinuity();       // clears integrations/history, retains maximum
  void resetMaximum();
  void setCalibration(float db); // new calibration clears all statistics
  void process(const float* weighted, size_t count);
  Levels levels() const;
  static float energyDB(double meanEnergy, float calibration = 0);
 private:
  float calibration_ = 0;
  float fastEnergy_ = 0, slowEnergy_ = 0;
  float fastAlpha_, slowAlpha_;
  double secondSum_ = 0;
  uint32_t secondSamples_ = 0, validSamples_ = 0;
  double lastSecond_ = NAN, maxEnergy_ = NAN;
  bool skipCurrentMaximum_ = false;
#if SPL_ENABLE_ROLLING
  float seconds_[900] = {};
  double rollingSum_ = 0;
  uint16_t ringPos_ = 0, ringCount_ = 0;
#endif
};

struct AudioSnapshot {
  Levels levels;
  AudioStatus status = AudioStatus::Warming;
  uint32_t timestampMs = 0;
  uint64_t frames = 0, clippedSamples = 0;
  uint32_t overruns = 0, errors = 0, deadlineMisses = 0;
  float rawRms = NAN, rawDbfs = NAN, weightedDB = NAN, peak = NAN;
  float calibration = 0, sampleRate = 0;
  uint32_t processingUs = 0, maxProcessingUs = 0, stackFree = 0;
};

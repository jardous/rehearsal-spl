// SPDX-License-Identifier: GPL-3.0-or-later
#include "SPLMeter.h"
#include <algorithm>

const char* statusName(AudioStatus s) {
  switch (s) {
    case AudioStatus::Warming: return "WARMUP";
    case AudioStatus::Ok: return "OK";
    case AudioStatus::MicError: return "MIC ERROR";
    case AudioStatus::Overrun: return "OVERRUN";
    case AudioStatus::Clipping: return "CLIP";
    case AudioStatus::DSPError: return "DSP ERROR";
    case AudioStatus::Stale: return "STALE";
  }
  return "ERROR";
}

AudioStatus integrationStatus(bool valid, AudioStatus previous) {
  if (valid) return AudioStatus::Ok;
  return previous == AudioStatus::Clipping ? AudioStatus::Clipping : AudioStatus::Warming;
}

SPLMeter::SPLMeter()
  : fastAlpha_(-std::expm1(-1.0f / (config::sampleRate * 0.125f))),
    slowAlpha_(-std::expm1(-1.0f / config::sampleRate)) {}

void SPLMeter::resetContinuity() {
  fastEnergy_ = slowEnergy_ = 0;
  secondSum_ = 0;
  secondSamples_ = validSamples_ = 0;
  lastSecond_ = NAN;
  skipCurrentMaximum_ = false;
#if SPL_ENABLE_ROLLING
  ringPos_ = ringCount_ = 0;
  rollingSum_ = 0;
#endif
}
void SPLMeter::resetMaximum() {
  maxEnergy_ = NAN;
  // Exclude a partially pre-reset window from MAX without disrupting LAeq or
  // creating a gap in the rolling window. Next full post-reset bin qualifies.
  skipCurrentMaximum_ = secondSamples_ != 0;
}
void SPLMeter::setCalibration(float db) {
  calibration_ = db;
  resetContinuity();
  resetMaximum();
}

float SPLMeter::energyDB(double e, float correction) {
  return e > 0 && std::isfinite(e)
    ? config::nominalOffset + correction + 10.0 * std::log10(e) : NAN;
}

void SPLMeter::process(const float* weighted, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const float e = weighted[i] * weighted[i];
    fastEnergy_ += fastAlpha_ * (e - fastEnergy_);
    slowEnergy_ += slowAlpha_ * (e - slowEnergy_);
    secondSum_ += e;
    if (validSamples_ < config::sampleRate) ++validSamples_;
    if (++secondSamples_ == config::sampleRate) {
      lastSecond_ = secondSum_ / config::sampleRate;
      if (!skipCurrentMaximum_ && (!std::isfinite(maxEnergy_) || lastSecond_ > maxEnergy_))
        maxEnergy_ = lastSecond_;
      skipCurrentMaximum_ = false;
#if SPL_ENABLE_ROLLING
      if (ringCount_ == 900) rollingSum_ -= seconds_[ringPos_];
      else ++ringCount_;
      seconds_[ringPos_] = lastSecond_;
      rollingSum_ += seconds_[ringPos_];
      ringPos_ = (ringPos_ + 1) % 900;
#endif
      secondSum_ = 0;
      secondSamples_ = 0;
    }
  }
}

Levels SPLMeter::levels() const {
  Levels l;
  l.fast = energyDB(fastEnergy_, calibration_);
  l.slow = energyDB(slowEnergy_, calibration_);
  l.leq1s = energyDB(lastSecond_, calibration_);
  l.maximum = energyDB(maxEnergy_, calibration_);
#if SPL_ENABLE_ROLLING
  l.rollingSeconds = ringCount_;
  if (ringCount_) l.rolling = energyDB(rollingSum_ / ringCount_, calibration_);
#endif
  l.valid = validSamples_ >= config::sampleRate && std::isfinite(l.leq1s)
      && std::isfinite(l.fast) && std::isfinite(l.slow);
  return l;
}

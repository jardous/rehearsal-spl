// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioDSP.h"
#include "Button.h"
#include "SPLMeter.h"
#include "SampleFormat.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static int checks = 0;
static void check(bool ok, const char* description) {
  ++checks;
  if (!ok) { fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
static bool near(double a, double b, double tolerance) {
  return std::isfinite(a) && fabs(a - b) <= tolerance;
}
static float amplitude(float spl) { return powf(10, (spl - 120) / 20); }
static float constantForLevel(float spl) {
  return powf(10, (spl - config::nominalOffset) / 20);
}

struct ToneResult { double rms, weightedRms; Levels levels; };
static ToneResult tone(double hz, float spl, double dc = 0, float correction = 0) {
  AudioDSP dsp;
  SPLMeter meter;
  meter.setCalibration(correction);
  float in[config::blockFrames], out[config::blockFrames];
  double rawSum = 0, weightedSum = 0;
  // One second filter settling, then six seconds of meter integration.
  for (unsigned block = 0; block < 700; ++block) {
    for (unsigned i = 0; i < config::blockFrames; ++i) {
      const uint64_t n = block * config::blockFrames + i;
      const double x = dc + amplitude(spl) * sin(2 * M_PI * hz * n / config::sampleRate);
      // Exercise actual microphone format and quantization, not ideal floats.
      const int32_t word = int32_t(llround(x * 8388608));
      in[i] = normalized24(uint32_t(word) << 8);
    }
    const auto metrics = dsp.process(in, out, config::blockFrames);
    check(metrics.finite && !metrics.clippedSamples, "finite, unclipped sine");
    if (block >= 100) {
      meter.process(out, config::blockFrames);
      rawSum += metrics.rawEnergy;
      weightedSum += metrics.weightedEnergy;
    }
  }
  return {sqrt(rawSum / (6 * config::sampleRate)),
          sqrt(weightedSum / (6 * config::sampleRate)), meter.levels()};
}

int main(int argc, char** argv) {
  if (argc == 3) {
    const auto r = tone(atof(argv[1]), atof(argv[2]));
    printf("%.10g,%.10g,%.6f,%.6f,%.6f\n", r.rms, r.weightedRms,
      r.levels.fast, r.levels.slow, r.levels.leq1s);
    return 0;
  }
  check(decode24(0) == 0, "decode zero");
  check(decode24(0x7fffffff) == 8388607, "positive full scale / discard low byte");
  check(decode24(0x800000ff) == -8388608, "negative full scale / sign extension");
  check(decode24(0xffffff00) == -1, "negative one");
  check(normalized24(0x80000000) == -1, "normalized negative rail");
  for (int32_t n : {-8000000, -12345, 0, 12345, 8000000})
    check(decode24(uint32_t(n) << 8) == n, "signed transport round trip");

  const auto reference = tone(1000, 94);
  check(near(20 * log10(reference.rms), -29.0103, 0.002), "94 SPL raw RMS = -29.0103 peak-referenced dBFS");
  check(reference.levels.valid && near(reference.levels.leq1s, 94, 0.12), "nominal 94 dBA at 1 kHz");
  check(near(reference.levels.slow, 94, 0.15), "slow converges");
  check(near(tone(1000, 80).levels.leq1s, 80, 0.12), "80 dBA reference");
  check(near(tone(1000, 94, 0, 2).levels.leq1s, 96, 0.12), "+2 dB calibration");
  check(near(tone(1000, 94, 0.1).levels.leq1s, reference.levels.leq1s, 0.02), "DC offset rejected");

  AudioDSP dcDSP;
  float dcOut = 0;
  for (unsigned i = 0; i < config::sampleRate * 4; ++i) dcOut = dcDSP.filter(0.2f);
  check(fabsf(dcOut) < 1e-5f, "DC steady-state suppression");
  dcDSP.reset();
  float input[] = {1, -1, 0.9995f, 0.1f}, output[4];
  const auto clipping = dcDSP.process(input, output, 4);
  check(clipping.clippedSamples == 3, "clip counter");
  input[0] = std::numeric_limits<float>::quiet_NaN();
  check(!dcDSP.process(input, output, 4).finite, "non-finite input flagged");

  SPLMeter step;
  std::vector<float> constant(config::sampleRate, constantForLevel(94));
  step.process(constant.data(), 6000);
  check(near(step.levels().fast, 94 + 10 * log10(1 - exp(-1)), 0.01), "Fast 125 ms energy step");
  check(!step.levels().valid, "no partial-second LAeq presented");
  step.process(constant.data(), 42000);
  check(near(step.levels().slow, 94 + 10 * log10(1 - exp(-1)), 0.01), "Slow 1 s energy step");
  check(step.levels().valid && near(step.levels().leq1s, 94, 0.001), "exactly 48000 samples per second");

  SPLMeter energy;
  std::fill(constant.begin(), constant.end(), constantForLevel(80));
  energy.process(constant.data(), constant.size());
  check(near(energy.levels().maximum, 80, 0.001), "first maximum");
  std::fill(constant.begin(), constant.end(), constantForLevel(100));
  energy.process(constant.data(), constant.size());
#if SPL_ENABLE_ROLLING
  check(near(energy.levels().rolling, 97.0329138, 0.001), "energy average, NOT mean dB");
#endif
  check(near(energy.levels().maximum, 100, 0.001), "maximum is max LAeq1s");
  energy.resetContinuity();
  check(!energy.levels().valid && energy.levels().rollingSeconds == 0, "gap discards history and partial bins");
  check(near(energy.levels().maximum, 100, 0.001), "maximum retained across gap");
  energy.resetMaximum();
  check(!std::isfinite(energy.levels().maximum), "maximum reset");
  energy.process(constant.data(), constant.size() - 1);
  check(!energy.levels().valid, "47999 samples not a second");
  energy.process(constant.data(), 1);
  check(energy.levels().valid, "single final sample closes second");
  energy.setCalibration(-3);
  check(!energy.levels().valid && !std::isfinite(energy.levels().maximum), "calibration resets all statistics");

  SPLMeter resetMidSecond;
  resetMidSecond.process(constant.data(), config::sampleRate / 2);
  resetMidSecond.resetMaximum();
  resetMidSecond.process(constant.data(), config::sampleRate / 2);
  check(resetMidSecond.levels().valid, "MAX reset does not disrupt LAeq");
  check(!std::isfinite(resetMidSecond.levels().maximum), "MAX excludes partial pre-reset bin");
#if SPL_ENABLE_ROLLING
  check(resetMidSecond.levels().rollingSeconds == 1, "MAX reset does not lose rolling energy");
#endif
  resetMidSecond.process(constant.data(), constant.size());
  check(near(resetMidSecond.levels().maximum, 100, 0.001), "first full post-reset MAX bin");

  SPLMeter mixedSecond;
  std::fill(constant.begin(), constant.end(), constantForLevel(80));
  mixedSecond.process(constant.data(), constant.size() / 2);
  std::fill(constant.begin(), constant.end(), constantForLevel(100));
  mixedSecond.process(constant.data(), constant.size() / 2);
  check(near(mixedSecond.levels().leq1s, 97.0329138, 0.001), "mixed-level LAeq1s averages energy");

#if SPL_ENABLE_ROLLING
  SPLMeter rolling;
  std::fill(constant.begin(), constant.end(), constantForLevel(80));
  for (unsigned s = 0; s < 900; ++s) rolling.process(constant.data(), constant.size());
  check(rolling.levels().rollingSeconds == 900, "full 15-minute window");
  std::fill(constant.begin(), constant.end(), constantForLevel(100));
  rolling.process(constant.data(), constant.size());
  check(rolling.levels().rollingSeconds == 900, "rolling ring wraps");
  check(near(rolling.levels().rolling, 80 + 10 * log10(999.0 / 900), 0.001), "oldest second evicted in energy domain");
#endif
  Button button;
  check(button.update(true, 0) == ButtonEvent::None, "press debounce");
  check(button.update(false, 5) == ButtonEvent::None, "bounce ignored");
  button.update(true, 10);
  button.update(true, 40);
  button.update(false, 100);
  check(button.update(false, 130) == ButtonEvent::Short, "short on debounced release");
  button.update(true, 200);
  button.update(true, 230);
  check(button.update(true, 1030) == ButtonEvent::Long, "long at 800 ms");
  check(button.update(true, 1200) == ButtonEvent::None, "long not repeated");
  button.update(false, 1210);
  check(button.update(false, 1240) == ButtonEvent::None, "no short after long");
  printf("PASS: %d checks (includes generated-tone blocks)\n", checks);
}

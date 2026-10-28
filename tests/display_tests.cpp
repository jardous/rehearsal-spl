// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixDisplay.h"
#include <cassert>
#include <cstdio>

int main() {
  MatrixDisplay display;
  assert(display.begin());
  auto& panel = *Adafruit_Protomatter::instance;
  Settings settings;
  AudioSnapshot audio;
  audio.status = AudioStatus::Ok;
  audio.levels.valid = true;
  audio.levels.fast = audio.levels.slow = audio.levels.leq1s = 80;
  audio.levels.maximum = 100;
  VisualFrame frame;
  frame.valid = true;
  frame.timestampMs = 100;
  frame.strength = 0.75f;
  for (float& band : frame.bandDB) band = frame.referenceDB;
  for (unsigned i = 0; i < visual::columns; ++i) {
    frame.waveMin[i] = -1; frame.waveMax[i] = 1;
  }
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.text == "dBAFM100 A80");
  const auto shows = panel.shows;
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.shows == shows); // identical display is not resent
  display.render(audio, settings, ViewMode::Spectrum, frame, 100);
  assert(panel.text.empty());
  assert(panel.buffer[31 * 64] != 0 && panel.buffer[31 * 64 + 3] == 0);
  assert(panel.buffer[0] != 0); // full-height peak remains within panel
  display.render(audio, settings, ViewMode::Waterfall, frame, 100);
  assert(panel.text.empty());
  assert(panel.buffer[0] != 0 && panel.buffer[64] == 0);
  display.render(audio, settings, ViewMode::Waterfall, frame, 125);
  assert(panel.buffer[64] == 0); // repeated input does not advance history
  frame.timestampMs = 225;
  for (float& band : frame.bandDB) band = -120;
  display.render(audio, settings, ViewMode::Waterfall, frame, 225);
  assert(panel.buffer[0] == 0 && panel.buffer[64] != 0);
  display.render(audio, settings, ViewMode::Waveform, frame, 225);
  assert(panel.text.empty() && panel.buffer[31 * 64] != 0 && panel.buffer[64] != 0);
  display.render(audio, settings, ViewMode::Reactive, frame, 225);
  assert(panel.text.empty());
  unsigned lit = 0;
  for (unsigned i = 0; i < 32 * 64; ++i) if (panel.buffer[i]) ++lit;
  assert(lit > 0 && lit < 64 * 32 / 2); // sparse pattern, not a solid screen
  display.render(audio, settings, ViewMode::Spectrum, frame, 1000);
  assert(panel.text == "WAIT AUDIO"); // stale visual frame never displayed
  frame.timestampMs = 1100;
  audio.status = AudioStatus::Clipping;
  audio.levels.valid = false;
  for (unsigned i = 0; i < unsigned(ViewMode::Count); ++i) {
    display.render(audio, settings, ViewMode(i), frame, 1100);
    assert(panel.text == "CLIP---"); // errors override every visual view
  }
  audio.status = AudioStatus::Ok;
  audio.levels.valid = true;
  audio.levels.fast = 30;
  display.render(audio, settings, ViewMode::Waterfall, frame, 1100);
  assert(panel.text.empty() && panel.buffer[64] == 0);
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "--");
  audio.levels.fast = 34.99f;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "--");
  audio.levels.fast = 35;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text.find("dBAF") == 0);
  audio.levels.fast = 116.01f;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "OVER RANGE---");
  audio.levels.fast = 100;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text.find("dBAF") == 0); // 3-digit drawing stays within bounds
  puts("PASS: host framebuffer bounds, five views, fault/stale priority and history reset");
}

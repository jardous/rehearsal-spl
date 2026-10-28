// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixDisplay.h"
#include <algorithm>
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
  const auto marker = [&](const char* letters) {
    const uint8_t f[] = {7, 4, 6, 4, 4}, s[] = {7, 4, 7, 1, 7};
    const uint8_t e[] = {7, 4, 6, 4, 7}, q[] = {7, 5, 5, 7, 1};
    const int count = letters[1] ? 2 : 1;
    const int start = 63 - (count * 4 - 1);
    for (int row = 0; row < 5; ++row) {
      for (int x = 56; x < 64; ++x) {
        bool expected = false;
        for (int letter = 0; letter < count; ++letter) {
          const int col = x - (start + 4 * letter);
          const uint8_t* rows = letters[letter] == 'F' ? f : letters[letter] == 'S' ? s
            : letters[letter] == 'E' ? e : q;
          if (col >= 0 && col < 3) expected = expected || (rows[row] & (4 >> col));
        }
        assert(bool(panel.buffer[(24 + row) * 64 + x]) == expected);
      }
    }
    for (int x = 56; x < 64; ++x) assert(panel.buffer[29 * 64 + x] == 0);
    for (int y = 24; y <= 29; ++y) assert(panel.buffer[y * 64 + 63] == 0);
    for (int y = 0; y < 13; ++y)
      for (int x = 46; x < 64; ++x) assert(panel.buffer[y * 64 + x] == 0);
    assert(panel.buffer[13 * 64 + 46] != 0 && panel.buffer[19 * 64 + 46] != 0);
    assert(panel.buffer[20 * 64 + 46] == 0); // dBA baseline matches big digits at row 19
  };
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.text == "dBAM100 A80");
  marker("F");
  const auto shows = panel.shows;
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.shows == shows); // identical display is not resent
  settings.mode = DisplayMode::Slow;
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.text == "dBAM100 A80");
  marker("S");
  settings.mode = DisplayMode::Leq;
  audio.levels.maximum = 116;
  audio.levels.rolling = 116;
  display.render(audio, settings, ViewMode::Meter, frame, 100);
  assert(panel.text == "dBAM116 A116");
  marker("EQ"); // two characters fit beside maximum-length MAX/AVG statistics
  settings.mode = DisplayMode::Fast;
  audio.levels.maximum = 100;
  audio.levels.rolling = NAN;
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
  assert(panel.text == "--dBAM100 A80");
  marker("F");
  assert(panel.buffer[30 * 64] != 0); // low-range bar remains, clamped to empty
  audio.levels.fast = 34.99f;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "--dBAM100 A80");
  uint16_t lowFrame[64 * 32];
  std::copy(panel.buffer, panel.buffer + 64 * 32, lowFrame);
  audio.levels.fast = 35;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "dBAM100 A80");
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      if (y >= 20 || x >= 46) assert(panel.buffer[y * 64 + x] == lowFrame[y * 64 + x]);
  audio.levels.slow = audio.levels.leq1s = 30;
  audio.levels.rolling = 80;
  settings.mode = DisplayMode::Slow;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "--dBAM100 A80");
  marker("S");
  settings.mode = DisplayMode::Leq;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "--dBAM100 A80");
  marker("EQ");
  settings.mode = DisplayMode::Fast;
  audio.levels.fast = 116.01f;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "OVER RANGE---");
  audio.levels.fast = 100;
  display.render(audio, settings, ViewMode::Meter, frame, 1100);
  assert(panel.text == "dBAM100 A80"); // 3-digit drawing stays within bounds
  puts("PASS: framebuffer bounds, five views, fault priority, low-range details and compact mode markers");
}

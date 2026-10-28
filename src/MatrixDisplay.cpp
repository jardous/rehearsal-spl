// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixDisplay.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

static uint8_t rgbPins[] = {42, 41, 40, 38, 39, 37};
static uint8_t addressPins[] = {45, 36, 48, 35};
MatrixDisplay::MatrixDisplay()
  : matrix_(64, 6, 1, rgbPins, 4, addressPins, 2, 47, 14, true) {}

bool MatrixDisplay::begin() {
  ready_ = matrix_.begin() == PROTOMATTER_OK;
  if (ready_) {
    matrix_.setTextWrap(false);
    matrix_.fillScreen(0);
    matrix_.show();
  }
  return ready_;
}
uint32_t MatrixDisplay::refreshFrames() { return ready_ ? matrix_.getFrameCount() : 0; }

uint16_t MatrixDisplay::color(uint8_t r, uint8_t g, uint8_t b, uint8_t v) {
  // Protomatter has no setBrightness(): scale RGB values before quantization.
  return matrix_.color565(uint16_t(r) * v / 255, uint16_t(g) * v / 255,
                          uint16_t(b) * v / 255);
}

void MatrixDisplay::digit(int x, int v, uint16_t c) {
  // Seven-segment digits: 14 x 20 pixels, three fit in x=0..45.
  static const uint8_t masks[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
  const uint8_t m = masks[v];
  if (m & 1) matrix_.fillRect(x + 2, 0, 10, 3, c);
  if (m & 2) matrix_.fillRect(x + 11, 2, 3, 7, c);
  if (m & 4) matrix_.fillRect(x + 11, 11, 3, 7, c);
  if (m & 8) matrix_.fillRect(x + 2, 17, 10, 3, c);
  if (m & 16) matrix_.fillRect(x, 11, 3, 7, c);
  if (m & 32) matrix_.fillRect(x, 2, 3, 7, c);
  if (m & 64) matrix_.fillRect(x + 2, 8, 10, 3, c);
}

void MatrixDisplay::modeMarker(const char* text, uint16_t c) {
  // A 3x5 font with one blank pixel at the right and above the level bar.
  int x = 63 - (int(strlen(text)) * 4 - 1);
  for (; *text; ++text, x += 4) {
    const uint8_t* rows = nullptr;
    static const uint8_t f[] = {7, 4, 6, 4, 4}, s[] = {7, 4, 7, 1, 7};
    static const uint8_t e[] = {7, 4, 6, 4, 7}, q[] = {7, 5, 5, 7, 1};
    static const uint8_t d[] = {6, 5, 5, 5, 6}, m[] = {5, 7, 7, 5, 5};
    switch (*text) {
      case 'F': rows = f; break;
      case 'S': rows = s; break;
      case 'E': rows = e; break;
      case 'Q': rows = q; break;
      case 'D': rows = d; break;
      case 'M': rows = m; break;
      default: continue;
    }
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 3; ++col)
        if (rows[row] & (4 >> col)) matrix_.drawPixel(x + col, 24 + row, c);
  }
}

uint16_t MatrixDisplay::palette(uint8_t position, uint8_t brightness) {
  if (position < 85) return color(255 - position * 3, position * 3, 0, brightness);
  if (position < 170) {
    position -= 85;
    return color(0, 255 - position * 3, position * 3, brightness);
  }
  position -= 170;
  return color(position * 3, 0, 255 - position * 3, brightness);
}

void MatrixDisplay::renderVisual(ViewMode view, const VisualFrame& v, const Settings& s,
                                 uint32_t now) {
  const bool fresh = v.timestampMs != visualAt_;
  visualAt_ = v.timestampMs;
  if (view == ViewMode::Spectrum || view == ViewMode::Waterfall) {
    uint8_t levels[visual::bands];
    for (size_t band = 0; band < visual::bands; ++band) {
      const float level = std::max(0.0f, std::min(1.0f,
        (v.bandDB[band] - (v.referenceDB - 48)) / 48));
      levels[band] = uint8_t(lroundf(level * 255));
    }
    if (view == ViewMode::Spectrum) {
      for (size_t band = 0; band < visual::bands; ++band) {
        const int height = (int(levels[band]) * 32 + 127) / 255;
        if (fresh) peaks_[band] = std::max(float(height), peaks_[band] - 0.6f);
        for (int row = 0; row < height; ++row)
          matrix_.fillRect(band * 4, 31 - row, 3, 1,
            row < 20 ? color(0, 220, 100, s.brightness)
            : row < 28 ? color(255, 180, 0, s.brightness) : color(255, 0, 40, s.brightness));
        if (peaks_[band] >= 1)
          matrix_.fillRect(band * 4, 32 - int(peaks_[band]), 3, 1,
            color(180, 180, 255, s.brightness));
      }
    } else {
      if (fresh) {
        memmove(waterfall_[1], waterfall_[0], sizeof(waterfall_) - sizeof(waterfall_[0]));
        memcpy(waterfall_[0], levels, sizeof(levels));
      }
      for (int row = 0; row < 32; ++row) {
        for (size_t band = 0; band < visual::bands; ++band) {
          const uint8_t level = waterfall_[row][band];
          if (level > 8)
            matrix_.fillRect(band * 4, row, 4, 1,
              palette(uint8_t(170 + int(level) * 85 / 255),
                      uint16_t(s.brightness) * level / 255));
        }
      }
    }
  } else if (view == ViewMode::Waveform) {
    matrix_.drawFastHLine(0, 16, 64, color(12, 18, 24, s.brightness));
    for (size_t column = 0; column < visual::columns; ++column) {
      const int top = std::max(0, std::min(31, int(lroundf(16 - 15 * v.waveMax[column]))));
      const int bottom = std::max(top, std::min(31, int(lroundf(16 - 15 * v.waveMin[column]))));
      matrix_.drawFastVLine(column, top, bottom - top + 1, color(0, 230, 255, s.brightness));
    }
  } else {
    // Sparse moving diamond rings, not full-screen white. Brightness is still
    // not a current limiter: use an adequate panel supply for every view.
    const unsigned phase = now / 40;
    const uint8_t brightness = uint16_t(s.brightness) * uint8_t(v.strength * 220) / 255;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 64; ++x) {
        const unsigned distance = std::abs(x - 31) + 2 * std::abs(y - 16);
        if ((distance + phase) % 10 < 2)
          matrix_.drawPixel(x, y, palette(uint8_t(distance * 4 + phase * 3), brightness));
      }
    }
  }
}

void MatrixDisplay::render(const AudioSnapshot& a, const Settings& s, ViewMode view,
                           const VisualFrame& visualFrame, uint32_t now) {
  if (!ready_) return;
  matrix_.fillScreen(0); // draw offscreen; show only when pixels change
  const auto white = color(255,255,255,s.brightness);
  const auto red = color(255,0,0,s.brightness);
  matrix_.setTextSize(1);
  matrix_.setTextColor(white);
  if (view != previousView_) {
    memset(waterfall_, 0, sizeof(waterfall_));
    memset(peaks_, 0, sizeof(peaks_));
    visualAt_ = 0;
    previousView_ = view;
  }
  const float db = s.mode == DisplayMode::Fast ? a.levels.fast
    : s.mode == DisplayMode::Slow ? a.levels.slow : a.levels.leq1s;
  const bool valid = a.status == AudioStatus::Ok && a.levels.valid && std::isfinite(db);
  if (!valid) {
    // Never carry a visual history or peak marker across an audio discontinuity.
    memset(waterfall_, 0, sizeof(waterfall_));
    memset(peaks_, 0, sizeof(peaks_));
    visualAt_ = 0;
    matrix_.setTextColor(red);
    matrix_.setCursor(0, 1);
    matrix_.print(statusName(a.status));
    matrix_.setTextSize(2);
    matrix_.setCursor(12, 12);
    matrix_.print("---");
  } else if (view != ViewMode::Meter) {
    if (visualFrame.valid && uint32_t(now - visualFrame.timestampMs) <= config::staleMs) {
      renderVisual(view, visualFrame, s, now);
    } else {
      matrix_.setCursor(2, 12);
      matrix_.print("WAIT AUDIO");
    }
  } else if (db > 116) {
    // Outside the conservative useful range, don't imply accurate numbers.
    matrix_.setCursor(0, 1);
    matrix_.print("OVER RANGE");
    matrix_.setTextSize(2);
    matrix_.setCursor(12, 12);
    matrix_.print("---");
  } else {
    const auto levelColor = db < s.yellow ? color(0,255,0,s.brightness)
      : db <= s.red ? color(255,200,0,s.brightness) : red;
    if (db < 35) {
      matrix_.setTextSize(2);
      matrix_.setCursor(20, 6);
      matrix_.print("--");
      matrix_.setTextSize(1);
    } else {
      const int value = int(lroundf(db));
      if (value >= 100) digit(0, value / 100, levelColor);
      digit(16, (value / 10) % 10, levelColor);
      digit(32, value % 10, levelColor);
    }
    matrix_.setCursor(46, 13); // 7-pixel label shares the large digits' bottom row (19)
    matrix_.print("dBA");
    matrix_.setCursor(0, 22);
    char text[16];
    char maxText[5] = "---", avgText[5] = "---";
    if (std::isfinite(a.levels.maximum)) snprintf(maxText, sizeof(maxText), "%d", int(lroundf(a.levels.maximum)));
    const float avg = std::isfinite(a.levels.rolling) ? a.levels.rolling : a.levels.leq1s;
    if (std::isfinite(avg)) snprintf(avgText, sizeof(avgText), "%d", int(lroundf(avg)));
    snprintf(text, sizeof(text), "M%s A%s", maxText, avgText);
    matrix_.print(text); // maximum 9 glyphs = 54 pixels
    modeMarker(SPL_MOCK_INPUT ? "DM" : s.mode == DisplayMode::Fast ? "F"
      : s.mode == DisplayMode::Slow ? "S" : "EQ", white);
    const int length = std::max(0, std::min(64, int((db - 40) * 64 / 70)));
    matrix_.fillRect(0, 30, 64, 2, color(32,32,32,s.brightness));
    matrix_.fillRect(0, 30, length, 2, levelColor);
  }
  if (memcmp(previous_, matrix_.getBuffer(), sizeof(previous_)) != 0) {
    memcpy(previous_, matrix_.getBuffer(), sizeof(previous_));
    matrix_.show(); // double-buffered swap, no visible clear
  }
}

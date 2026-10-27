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

void MatrixDisplay::render(const AudioSnapshot& a, const Settings& s) {
  if (!ready_) return;
  matrix_.fillScreen(0); // draw offscreen; show only when pixels change
  const auto white = color(255,255,255,s.brightness);
  const auto red = color(255,0,0,s.brightness);
  matrix_.setTextSize(1);
  matrix_.setTextColor(white);
  const float db = s.mode == DisplayMode::Fast ? a.levels.fast
    : s.mode == DisplayMode::Slow ? a.levels.slow : a.levels.leq1s;
  const bool valid = a.status == AudioStatus::Ok && a.levels.valid && std::isfinite(db);
  if (!valid) {
    matrix_.setTextColor(red);
    matrix_.setCursor(0, 1);
    matrix_.print(statusName(a.status));
    matrix_.setTextSize(2);
    matrix_.setCursor(12, 12);
    matrix_.print("---");
  } else if (db < 35 || db > 116) {
    // Outside the conservative useful range, don't imply accurate numbers.
    matrix_.setCursor(0, 1);
    matrix_.print(db < 35 ? "LOW RANGE" : "OVER RANGE");
    matrix_.setTextSize(2);
    matrix_.setCursor(12, 12);
    matrix_.print("---");
  } else {
    const auto levelColor = db < s.yellow ? color(0,255,0,s.brightness)
      : db <= s.red ? color(255,200,0,s.brightness) : red;
    const int value = int(lroundf(db));
    if (value >= 100) digit(0, value / 100, levelColor);
    digit(16, (value / 10) % 10, levelColor);
    digit(32, value % 10, levelColor);
    matrix_.setCursor(46, 0);
    matrix_.print("dBA");
    matrix_.setCursor(48, 10);
    matrix_.print(SPL_MOCK_INPUT ? "DM" : s.mode == DisplayMode::Fast ? "F"
      : s.mode == DisplayMode::Slow ? "S" : "EQ");
    matrix_.setCursor(0, 22);
    char text[16];
    char maxText[5] = "---", avgText[5] = "---";
    if (std::isfinite(a.levels.maximum)) snprintf(maxText, sizeof(maxText), "%d", int(lroundf(a.levels.maximum)));
    const float avg = std::isfinite(a.levels.rolling) ? a.levels.rolling : a.levels.leq1s;
    if (std::isfinite(avg)) snprintf(avgText, sizeof(avgText), "%d", int(lroundf(avg)));
    snprintf(text, sizeof(text), "M%s A%s", maxText, avgText);
    matrix_.print(text); // maximum 9 glyphs = 54 pixels
    const int length = std::max(0, std::min(64, int((db - 40) * 64 / 70)));
    matrix_.fillRect(0, 30, 64, 2, color(32,32,32,s.brightness));
    matrix_.fillRect(0, 30, length, 2, levelColor);
  }
  if (memcmp(previous_, matrix_.getBuffer(), sizeof(previous_)) != 0) {
    memcpy(previous_, matrix_.getBuffer(), sizeof(previous_));
    matrix_.show(); // double-buffered swap, no visible clear
  }
}

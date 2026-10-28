// SPDX-License-Identifier: GPL-3.0-or-later
// Host-only framebuffer double: checks drawing bounds and records text/actions.
// It does not emulate panel DMA, refresh timing or the Adafruit font rasterizer.
#pragma once
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <string>

constexpr int PROTOMATTER_OK = 0;
class Adafruit_Protomatter {
 public:
  Adafruit_Protomatter(int, int, int, uint8_t*, int, uint8_t*, int, int, int, bool) {
    instance = this;
  }
  int begin() { return PROTOMATTER_OK; }
  void setTextWrap(bool) {}
  void fillScreen(uint16_t color) {
    std::fill(buffer, buffer + 64 * 32, color);
    text.clear();
  }
  void show() { ++shows; }
  uint32_t getFrameCount() { return shows; }
  uint16_t* getBuffer() { return buffer; }
  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
  }
  void drawPixel(int x, int y, uint16_t color) {
    assert(x >= 0 && x < 64 && y >= 0 && y < 32);
    buffer[y * 64 + x] = color;
  }
  void fillRect(int x, int y, int width, int height, uint16_t color) {
    assert(width >= 0 && height >= 0 && x >= 0 && y >= 0
      && x + width <= 64 && y + height <= 32);
    for (int row = y; row < y + height; ++row)
      for (int col = x; col < x + width; ++col) drawPixel(col, row, color);
  }
  void drawFastHLine(int x, int y, int width, uint16_t color) { fillRect(x, y, width, 1, color); }
  void drawFastVLine(int x, int y, int height, uint16_t color) { fillRect(x, y, 1, height, color); }
  void setTextSize(int size) { size_ = size; }
  void setTextColor(uint16_t color) { color_ = color; }
  void setCursor(int x, int y) { x_ = x; y_ = y; }
  void print(const char* value) {
    text += value;
    for (; *value; ++value) {
      if (*value != ' ') fillRect(x_, y_, 5 * size_, 7 * size_, color_);
      x_ += 6 * size_;
    }
  }
  inline static Adafruit_Protomatter* instance = nullptr;
  std::string text;
  uint32_t shows = 0;
  uint16_t buffer[64 * 32] = {};
 private:
  int x_ = 0, y_ = 0, size_ = 1;
  uint16_t color_ = 0;
};

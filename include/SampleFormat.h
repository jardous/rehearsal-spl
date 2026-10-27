// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

// Philips I2S: signed 24-bit word is MSB aligned in a 32-bit DMA slot.
// Explicit sign extension avoids implementation-defined signed right shifts.
inline int32_t decode24(uint32_t slot) {
  uint32_t word = slot >> 8;
  return (word & 0x800000u) ? int32_t(word) - 0x1000000 : int32_t(word);
}
inline float normalized24(uint32_t slot) {
  return static_cast<float>(decode24(slot)) / 8388608.0f;
}

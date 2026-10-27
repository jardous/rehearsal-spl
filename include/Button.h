// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

enum class ButtonEvent { None, Short, Long };
class Button {
 public:
  ButtonEvent update(bool pressed, uint32_t now) {
    if (pressed != raw_) { raw_ = pressed; changed_ = now; }
    if (raw_ != stable_ && uint32_t(now - changed_) >= 25) {
      stable_ = raw_;
      if (stable_) { started_ = now; longSent_ = false; }
      else if (!longSent_) return ButtonEvent::Short;
    }
    if (stable_ && !longSent_ && uint32_t(now - started_) >= 800) {
      longSent_ = true;
      return ButtonEvent::Long;
    }
    return ButtonEvent::None;
  }
 private:
  bool raw_ = false, stable_ = false, longSent_ = false;
  uint32_t changed_ = 0, started_ = 0;
};

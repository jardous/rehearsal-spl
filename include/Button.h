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

struct ButtonEvents {
  ButtonEvent up = ButtonEvent::None, down = ButtonEvent::None;
  bool nextView = false;
};

class ButtonPair {
 public:
  ButtonEvents update(bool upPressed, bool downPressed, uint32_t now) {
    ButtonEvents events;
    events.up = up_.update(upPressed, now);
    events.down = down_.update(downPressed, now);
    const bool both = upPressed && downPressed;
    if (both && !together_) started_ = now;
    together_ = both;
    if (!chord_ && both) { chord_ = true; sent_ = false; started_ = now; }
    if (chord_) {
      events = {}; // a chord never also changes brightness/mode or resets MAX
      if (!both) started_ = now;
      if (both && !sent_ && uint32_t(now - started_) >= 825) {
        sent_ = true;
        events.nextView = true;
      }
      if (!upPressed && !downPressed) {
        chord_ = false;
        up_ = Button{}; down_ = Button{};
      }
    }
    return events;
  }
 private:
  Button up_, down_;
  bool chord_ = false, sent_ = false, together_ = false;
  uint32_t started_ = 0;
};

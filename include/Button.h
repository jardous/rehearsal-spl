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
  bool pressed() const { return stable_; }
 private:
  bool raw_ = false, stable_ = false, longSent_ = false;
  uint32_t changed_ = 0, started_ = 0;
};

struct ButtonEvents {
  ButtonEvent up = ButtonEvent::None, down = ButtonEvent::None;
  int8_t brightnessStep = 0; // +1 brighter, -1 dimmer; never a single-button action
};

class ButtonPair {
 public:
  ButtonEvents update(bool upPressed, bool downPressed, uint32_t now) {
    ButtonEvents events;
    events.up = up_.update(upPressed, now);
    events.down = down_.update(downPressed, now);
    const bool both = upPressed && downPressed;
    if (both && !(wasUp_ && wasDown_)) {
      chord_ = true;
      // Use observed press order, even when the second press is within the
      // first button's debounce window. Truly simultaneous presses are neutral.
      direction_ = wasUp_ ? 1 : wasDown_ ? -1 : 0;
      started_ = now;
      stepped_ = false;
    }
    if (chord_) {
      events = {}; // no program/mode/MAX action, including staggered releases
      if (both && !stepped_ && uint32_t(now - started_) >= 25) {
        stepped_ = true;
        repeated_ = now;
        events.brightnessStep = direction_;
      } else if (both && stepped_ && uint32_t(now - repeated_) >= 250) {
        repeated_ = now; // no catch-up burst after a slow UI frame
        events.brightnessStep = direction_;
      }
      if (!upPressed && !downPressed && !up_.pressed() && !down_.pressed()) {
        chord_ = false;
        up_ = Button{}; down_ = Button{};
      }
    }
    wasUp_ = upPressed;
    wasDown_ = downPressed;
    return events;
  }
 private:
  Button up_, down_;
  bool chord_ = false, stepped_ = false, wasUp_ = false, wasDown_ = false;
  int8_t direction_ = 0;
  uint32_t started_ = 0, repeated_ = 0;
};

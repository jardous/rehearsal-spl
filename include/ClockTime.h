// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

enum class ClockLink : uint8_t { Starting, Advertising, Pairing, Discovering, Ready, NoCTS, Error };
const char* clockLinkName(ClockLink link);

struct ClockDateTime {
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  uint8_t weekday = 0; // Monday=1 .. Sunday=7
};

struct ClockSnapshot {
  ClockDateTime local;
  ClockLink link = ClockLink::Starting;
  bool valid = false, connected = false;
  uint64_t syncAgeSeconds = 0;
};

// CTS carries LOCAL wall time, not UTC. Keep it separately from the system clock:
// no host timezone, time_t/2038 limit, or double application of a UTC/DST offset.
// Uses the 64-bit monotonic ESP timer, not wrapping 32-bit millis().
class ClockTime {
 public:
  bool sync(const uint8_t* currentTime, size_t length, uint64_t monotonicUs);
  ClockSnapshot snapshot(uint64_t monotonicUs) const;
 private:
  uint64_t localUs_ = 0, syncedUs_ = 0;
  bool valid_ = false;
};

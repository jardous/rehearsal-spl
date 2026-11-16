// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockTime.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using Packet = std::array<uint8_t, 10>;
Packet packet(unsigned year, unsigned month, unsigned day, unsigned hour = 0,
              unsigned minute = 0, unsigned second = 0) {
  return {{uint8_t(year), uint8_t(year >> 8), uint8_t(month), uint8_t(day),
           uint8_t(hour), uint8_t(minute), uint8_t(second), 0, 0, 0}};
}
void expect(const ClockSnapshot& s, unsigned year, unsigned month, unsigned day,
            unsigned hour = 0, unsigned minute = 0, unsigned second = 0) {
  assert(s.valid);
  assert(s.local.year == year && s.local.month == month && s.local.day == day);
  assert(s.local.hour == hour && s.local.minute == minute && s.local.second == second);
}
int main() {
  ClockTime clock;
  assert(!clock.snapshot(0).valid && !clock.snapshot(UINT64_MAX).valid);
  const uint64_t anchor = 9876543210000ULL; // comfortably beyond millis() wrap
  auto p = packet(2026, 11, 15, 23, 59, 59);
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor), 2026, 11, 15, 23, 59, 59);
  assert(clock.snapshot(anchor).local.weekday == 7);
  expect(clock.snapshot(anchor + 999999), 2026, 11, 15, 23, 59, 59);
  expect(clock.snapshot(anchor + 1000000), 2026, 11, 16);
  assert(clock.snapshot(anchor + 123000000).syncAgeSeconds == 123);
  assert(!clock.snapshot(anchor - 1).valid);

  p = packet(2026, 12, 31, 23, 59, 59);
  p[8] = 128; // Exact Time 256 half-second fraction
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor + 499999), 2026, 12, 31, 23, 59, 59);
  expect(clock.snapshot(anchor + 500000), 2027, 1, 1);
  p[8] = 255;
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor + 3906), 2026, 12, 31, 23, 59, 59);
  expect(clock.snapshot(anchor + 3907), 2027, 1, 1);

  // Both Gregorian century exceptions and dates beyond signed 32-bit time_t.
  for (unsigned year : {2000U, 2024U, 2100U, 2400U}) {
    p = packet(year, 2, 28, 23, 59, 59);
    assert(clock.sync(p.data(), p.size(), anchor));
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    expect(clock.snapshot(anchor + 1000000), year, leap ? 2 : 3, leap ? 29 : 1);
  }
  p = packet(2038, 1, 19, 3, 14, 7);
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor + 1000000), 2038, 1, 19, 3, 14, 8);
  p = packet(1582, 1, 1);
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor), 1582, 1, 1);
  p = packet(9999, 12, 31, 23, 59, 59);
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor + 999999), 9999, 12, 31, 23, 59, 59);
  assert(!clock.snapshot(anchor + 1000000).valid);
  assert(!clock.snapshot(UINT64_MAX).valid);

  p = packet(2026, 11, 15, 12, 34, 56);
  assert(clock.sync(p.data(), p.size(), anchor));
  for (const auto& bad : {
      packet(0, 11, 15), packet(1581, 11, 15), packet(10000, 1, 1),
      packet(2026, 0, 15), packet(2026, 13, 15), packet(2026, 11, 0),
      packet(2026, 11, 31), packet(2026, 2, 29), packet(2100, 2, 29),
      packet(2026, 1, 1, 24), packet(2026, 1, 1, 0, 60),
      packet(2026, 1, 1, 0, 0, 60)}) {
    assert(!clock.sync(bad.data(), bad.size(), anchor + 1000000));
    expect(clock.snapshot(anchor), 2026, 11, 15, 12, 34, 56);
  }
  assert(!clock.sync(nullptr, 10, anchor));
  for (size_t length = 0; length < 10; ++length) assert(!clock.sync(p.data(), length, anchor));
  assert(!clock.sync(p.data(), 11, anchor));
  p[7] = 8;
  assert(!clock.sync(p.data(), p.size(), anchor));
  p[7] = 0; // unknown weekday is legal; calculated from the date instead
  p[9] = 0x80;
  assert(!clock.sync(p.data(), p.size(), anchor));
  p[9] = 0x0f; // all defined adjust-reason bits
  assert(clock.sync(p.data(), p.size(), anchor));
  expect(clock.snapshot(anchor + (uint64_t(UINT32_MAX) + 1000) * 1000),
         2027, 1, 4, 5, 37, 44); // >49 days of uninterrupted holdover
  // Phone timezone/DST changes set LOCAL time directly, including backwards.
  p = packet(2026, 10, 25, 2, 0, 0);
  assert(clock.sync(p.data(), p.size(), anchor + 1));
  expect(clock.snapshot(anchor + 1), 2026, 10, 25, 2, 0, 0);
  // Exhaustively cover a full Gregorian cycle with an independent calendar.
  p = packet(2000, 1, 1);
  assert(clock.sync(p.data(), p.size(), anchor));
  uint64_t dayIndex = 0;
  const unsigned monthLengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  for (unsigned year = 2000; year < 2400; ++year) {
    for (unsigned month = 1; month <= 12; ++month) {
      const unsigned count = monthLengths[month - 1]
        + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
      for (unsigned day = 1; day <= count; ++day, ++dayIndex) {
        const auto s = clock.snapshot(anchor + dayIndex * 86400000000ULL);
        expect(s, year, month, day);
        assert(s.local.weekday == (5 + dayIndex) % 7 + 1); // 2000-01-01 Saturday
      }
    }
  }
  for (unsigned i = 0; i <= unsigned(ClockLink::Error); ++i)
    assert(strlen(clockLinkName(ClockLink(i))) > 0);
  puts("PASS: CTS validation, fractions, 400-year calendar, holdover, resync and 64-bit uptime");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "ClockTime.h"

namespace {
bool leap(unsigned year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
unsigned monthDays(unsigned year, unsigned month) {
  static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return days[month - 1] + (month == 2 && leap(year));
}
uint64_t yearDays(unsigned year) {
  const uint64_t y = year - 1;
  return 365 * y + y / 4 - y / 100 + y / 400;
}
constexpr uint64_t secondUs = 1000000, daySeconds = 86400;
}

const char* clockLinkName(ClockLink link) {
  switch (link) {
    case ClockLink::Starting: return "STARTING";
    case ClockLink::Advertising: return "ADVERTISING";
    case ClockLink::Pairing: return "PAIRING";
    case ClockLink::Discovering: return "DISCOVERING";
    case ClockLink::Ready: return "READY";
    case ClockLink::NoCTS: return "NO_CTS";
    default: return "ERROR";
  }
}

bool ClockTime::sync(const uint8_t* p, size_t length, uint64_t now) {
  if (!p || length != 10) return false;
  const unsigned year = unsigned(p[0]) | (unsigned(p[1]) << 8);
  if (year < 1582 || year > 9999 || p[2] < 1 || p[2] > 12 || p[3] < 1
      || p[3] > monthDays(year, p[2]) || p[4] > 23 || p[5] > 59 || p[6] > 59
      || p[7] > 7 || (p[9] & 0xf0)) return false;
  uint64_t days = yearDays(year) + p[3] - 1;
  for (unsigned month = 1; month < p[2]; ++month) days += monthDays(year, month);
  localUs_ = (days * daySeconds + p[4] * 3600 + p[5] * 60 + p[6]) * secondUs
    + uint64_t(p[8]) * secondUs / 256;
  syncedUs_ = now;
  valid_ = true;
  return true;
}

ClockSnapshot ClockTime::snapshot(uint64_t now) const {
  ClockSnapshot result;
  if (!valid_ || now < syncedUs_) return result;
  const uint64_t elapsed = now - syncedUs_;
  // Reject overflow and dates beyond the Bluetooth year range.
  const uint64_t limit = yearDays(10000) * daySeconds * secondUs;
  if (elapsed >= limit - localUs_) return result;
  const uint64_t seconds = (localUs_ + elapsed) / secondUs;
  const uint64_t days = seconds / daySeconds;
  unsigned low = 1582, high = 10000;
  while (low + 1 < high) {
    const unsigned middle = (low + high) / 2;
    if (yearDays(middle) <= days) low = middle;
    else high = middle;
  }
  auto& t = result.local;
  t.year = low;
  unsigned remaining = days - yearDays(low);
  t.month = 1;
  while (remaining >= monthDays(low, t.month)) remaining -= monthDays(low, t.month++);
  t.day = remaining + 1;
  t.weekday = days % 7 + 1; // Gregorian 0001-01-01 was a Monday
  t.hour = seconds % daySeconds / 3600;
  t.minute = seconds % 3600 / 60;
  t.second = seconds % 60;
  result.valid = true;
  result.syncAgeSeconds = elapsed / secondUs;
  return result;
}

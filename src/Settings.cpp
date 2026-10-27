// SPDX-License-Identifier: GPL-3.0-or-later
#include "Settings.h"
#include <Preferences.h>

bool Settings::valid() const {
  return magic == 0x534c4d31 && std::isfinite(calibration) && fabsf(calibration) <= 30
    && std::isfinite(yellow) && std::isfinite(red)
    && yellow >= 30 && red <= 120 && yellow < red
    && brightness >= 16 && unsigned(mode) <= unsigned(DisplayMode::Leq);
}
bool Settings::load() {
  Preferences nvs;
  if (!nvs.begin("rehearsal-spl", true)) return false;
  Settings candidate;
  const size_t n = nvs.getBytes("settings-v1", &candidate, sizeof(candidate));
  nvs.end();
  if (n != sizeof(candidate) || !candidate.valid()) return false;
  *this = candidate;
  return true;
}
bool Settings::save() const {
  if (!valid()) return false;
  Preferences nvs;
  if (!nvs.begin("rehearsal-spl", false)) return false;
  const bool ok = nvs.putBytes("settings-v1", this, sizeof(*this)) == sizeof(*this);
  nvs.end();
  return ok;
}

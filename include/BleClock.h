// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ClockTime.h"

namespace bleClock {
// BLE/GATT operations run in their own low-priority task, never on the audio or
// UI loop or synchronously inside a NimBLE callback.
bool begin();
ClockSnapshot snapshot();
void requestSync();
void forgetPhone();
}

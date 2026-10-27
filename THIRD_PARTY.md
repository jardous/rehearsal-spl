# Licensing and provenance

This firmware and its tests are licensed **GPL-3.0-or-later**, without warranty.
The full GPL version 3 text is in `LICENSE`.

## ESP32 sound level meter reference

- https://github.com/ikostoski/esp32-i2s-slm
- Reviewed revision: `b643efa1beae51b84d4ad9b2bfa35d439c81e9b6`
- Copyright (c) 2019 Ivan Kostoski; GPL-3.0-or-later.
- `include/AWeighting.h` adapts the three 48 kHz A-weighting SOS coefficient
  sets and gain from `esp32-i2s-slm.ino`. The reference credits the original
  frequency-response fit to Dr. Matt L., https://dsp.stackexchange.com/a/36122.
- Changes dated 2026-10-27: converted feedback signs to conventional denominator
  coefficients; implemented a separate, portable float DF2-transposed cascade.
  No Xtensa assembly or original SOS filter implementation is used.
- Its I2S → IIR → energy → logarithm architecture and full-scale sine/RMS
  correction informed this implementation. This project is not a claim that
  the reference author's accuracy estimates apply to this assembly.

When redistributing binaries, provide the corresponding source, build/install
instructions, this notice, and the GPL license as required by GPL section 6.

## Adafruit and Espressif dependencies

Adafruit's Protomatter `simple` example supplied the MatrixPortal S3 pin map.
Protomatter and Adafruit GFX/BusIO are separate BSD-licensed dependencies;
their original source and copyright/license notices remain in the downloaded
PlatformIO packages. Arduino-ESP32 and ESP-IDF retain their own LGPL/Apache and
component licenses. These dependencies are not relicensed by this project.
No dependency source has been modified.

# Verification record and on-device acceptance

## Verified in the development environment

Recorded 2026-10-27, Linux x86-64, PlatformIO Core 6.1.18, Python 3.14.8,
host GCC 16.2.1. No MatrixPortal S3, panel or ICS-43434 was available.
The connected USB serial device was unrelated; it was **not flashed**.

### Source/API review

- Official Adafruit S3 pinouts, board variant, ICS-43434 pinouts/product
  specifications, Protomatter `simple` example and ESP32-S3 backend reviewed.
- Board `adafruit_matrixportal_esp32s3` verified in pinned Espressif32 6.9.0.
- Arduino core SDK headers confirm ESP-IDF 4.4.7; legacy I2S configuration,
  pins, event types, reads, clock getter and uninstall APIs verified in source.
- S3 I2S standard mode, two 32-bit slots / frame; left is slot 0 in the
  pinned driver's initialization mapping. Upper 24 bits contain the signed mic
  sample. No SPH0645-specific timing register hacks are applied.
- S3's lack of APLL support was verified; `use_apll=false` is explicit.
- Protomatter 1.7.0 uses LCD_CAM/GDMA TX rather than I2S on S3; microphone
  uses I2S0/GDMA RX. GFX double-buffer drawing and software brightness scaling
  use actual library APIs.

### Actual builds

```sh
pio run -e matrix_test
pio run -e meter -e mock
pio run -e matrix_test -e meter -e mock
```

All three environments compiled, linked and generated ESP32-S3 firmware images.
The small matrix build succeeded before the complete integration build.
The full build verifies that Microphone/I2S and Protomatter link together;
it does not demonstrate simultaneous real-time operation on hardware.

Final linked sizes (runtime heap/DMA allocations are additional):

| Environment | Static RAM | Flash image usage |
|---|---:|---:|
| `matrix_test` | 31,088 bytes | 309,661 bytes |
| `meter` | 49,060 bytes | 373,465 bytes |
| `mock` | 49,036 bytes | 364,853 bytes |

The pinned Protomatter release produces a C warning about comparison of distinct
pointer types in `esp32-common.h:95`; no application compiler errors/warnings
were reported. The library was not silently patched. Espressif's old esptool
also emits a Python 3.14 SyntaxWarning on its first import; image generation
succeeded. Static RAM size excludes runtime DMA/framebuffer/task allocations.
PlatformIO also reports `An extra UF2 bootloader image is already added!` for
this upstream board/platform combination; all builds succeed. Upload and
bootloader behavior still require the actual MatrixPortal.

### Host tests

`python3 tools/test_host.py` compiled the real `AudioDSP.cpp` and `SPLMeter.cpp`
with `-Wall -Wextra -Werror` and passed **2846 assertions** with rolling enabled
and **2841 assertions** with it disabled, including generated tone-block checks,
then passed 14 frequency-response probes. Tests use quantized
24-bit signals and the firmware's decoder, float filter coefficients and states.
No NumPy/SciPy is needed for this test run.

The 2,846-check host suite was also compiled and run with AddressSanitizer and
UndefinedBehaviorSanitizer (`-fsanitize=address,undefined`); it passed without
reported sanitizer errors. This does not test ESP32 hardware drivers.

Measured whole-pipeline weighting (DC blocker + A filter), normalized raw RMS:

| Frequency | Nominal A | C++ measured | Error |
|---:|---:|---:|---:|
| 31.5 Hz | −39.525 dB | −39.704 dB | −0.179 dB |
| 63 Hz | −26.220 dB | −26.274 dB | −0.054 dB |
| **100 Hz** | **−19.143 dB** | **−19.139 dB** | **+0.003 dB** |
| 125 Hz | −16.188 dB | −16.179 dB | +0.009 dB |
| 250 Hz | −8.674 dB | −8.679 dB | −0.005 dB |
| 500 Hz | −3.248 dB | −3.255 dB | −0.007 dB |
| **1 kHz** | **0.000 dB** | **+0.004 dB** | **+0.004 dB** |
| 2 kHz | +1.201 dB | +1.208 dB | +0.007 dB |
| 4 kHz | +0.963 dB | +0.954 dB | −0.010 dB |
| 8 kHz | −1.147 dB | −1.141 dB | +0.006 dB |
| **10 kHz** | **−2.492 dB** | **−2.459 dB** | **+0.032 dB** |
| 12.5 kHz | −4.254 dB | −4.217 dB | +0.037 dB |
| 16 kHz | −6.706 dB | −6.752 dB | −0.046 dB |
| 20 kHz | −9.347 dB | −9.373 dB | −0.026 dB |

The independent nominal curve uses analog pole frequencies 20.598997,
107.65265, 737.86223 and 12194.217 Hz, normalized to 0 dB at 1 kHz.
Test bounds are engineering regression thresholds, **not IEC class limits**.
The 20 kHz result is only a digital-filter test; it does not extend the
microphone's useful acoustic bandwidth to 20 kHz.

Other results include nominal 94 dB / 1 kHz → raw −29.0103 peak-referenced
dBFS and about 94.004 dBA; equal-duration 80/100 dBA energies → 97.033 dBA,
not 90 dBA. Exact Fast/Slow energy steps match their specified time constants.

## Not verified — required on the actual assembly

These are acceptance tests, not claims of completed hardware testing.

1. **Power/boot/panel:** Check wiring/polarity first. Run `matrix_test`; confirm
   a stable green 94 and dBA label. Check all rows, alignment, low brightness and
   power integrity. Confirm standalone cold boot with only the runtime supply.
2. **Mock/UI:** Flash `mock`; after ~2 seconds verify ~80/104 dBA alternation,
   green/red indications, F/S/EQ behavior, both short/long buttons, level bar,
   bottom statistics, persistence after `# NVS saved`, and no flicker. Marker
   is DM in this build. Test brightness across its full range within power limits.
3. **I2S clocks/format:** Flash `meter`; use a logic analyzer to verify LRCLK
   near 48 kHz and BCLK near 3.072 MHz (64 clocks per frame), Philips one-bit
   delay, microphone data while LRCLK low and signed upper-24-bit alignment.
   Leave unused right slot out of all energy calculations. Verify raw RMS with
   known sound, rather than assuming compilation proves alignment.
4. **Sensitivity/calibration:** At a reference-confirmed 94 dB / 1 kHz tone,
   expect raw normalized RMS near 0.03544 / −29.01 peak-referenced dBFS before
   per-unit sensitivity correction. Calibrate with the README procedure. Verify
   repeatability at multiple levels and after power cycling. Do not use a tone
   generator's volume setting as an acoustic reference.
5. **Acoustic spectrum:** Compare 100 Hz, 1 kHz, 10 kHz and broadband sound
   against a reference meter in the same position. Establish the complete
   assembly's errors; the host filter table is not that measurement.
6. **Timing/soak:** Log at least 30 minutes with real mic input, panel enabled,
   maximum expected graphics/brightness and serial connected/disconnected.
   Exercise buttons, thresholds and NVS saves. Verify zero increasing overruns,
   deadline misses/errors; `dsp_us` comfortably below 10000; stable heap and
   adequate stack headroom. Measure real sample clock independently. Investigate
   every discontinuity; do not just increase the timeout to hide it.
7. **Statistics:** At stable sound check FAST/SLOW converge and LAeq updates at
   1 Hz. After 15 uninterrupted valid minutes `rolling_s` must reach 900; varying
   levels should be energy-averaged. MAX must be a completed LAeq,1s, not peak.
8. **Failure handling:** With safe power/wiring practices, test a missing mic,
   stuck-low DOUT, and interruption of data. Confirm MIC ERROR/STALE and no
   plausible frozen numerical readout. A logic-level I2S pattern source is
   preferable for injecting rail samples; do not produce hazardous acoustic
   levels just to test CLIP. Induce a controlled >120 ms acquisition stall in a
   temporary test build and verify overrun/deadline diagnostics, invalidation,
   fresh warm-up and reset rolling coverage after recovery.
9. **USB stress/persistence:** Flood invalid/oversized commands; pause host reads;
   unplug USB data without removing runtime power if your setup permits it.
   Audio must continue; lost telemetry is acceptable and counted. Check malformed
   calibration input cannot store NaN/Inf and a changed offset survives reboot.

Only after those checks can hardware operation be described as validated for
your assembly. None of these checks would by itself establish IEC compliance.

## Visualisation and UF2 update checks

The host suite now also exercises frame capture/reset boundaries, silence/DC
rejection, FFT band mapping and energy at 1/4/10 kHz, finite/clipped input
rejection, waveform bounds, button chords including staggered releases and clock
wrap, clipping recovery status, and UF2 block format/round-trip/corruption checks.
The host framebuffer double also checks all five rendering paths for drawing
bounds, unchanged-frame suppression, stale/error priority and waterfall reset;
it does not emulate font pixels, panel DMA or refresh timing.
These are software regression tests, not completed on-device acceptance.

Recorded for the visualisation build on 2026-10-28: **9,459 host checks** with
rolling enabled, **9,454** disabled, all 14 A-weighting frequency probes, the
framebuffer suite and four Python UF2 test cases passed. The rolling-enabled
host and framebuffer suites also passed AddressSanitizer/UndefinedBehaviorSanitizer.
Fresh builds of `meter`, `mock` and `matrix_test` generated checksum/hash-validated
application-only UF2 files. After removing visual name headers and expanding
plots to 64×32, meter static RAM is 92,260 bytes and linked flash usage is
407,409 bytes including the below-range `--` display (runtime allocations are
additional). These new views
have not yet been flashed or verified on the board.

Before accepting the new image on hardware:

1. Retain the known-working original UF2 as recovery. Apply the new app-only
   UF2 through `MATRXS3BOOT`; verify automatic reboot and standalone cold boot.
2. Verify holding both buttons cycles all five views exactly once per hold.
   Release in either order; verify no extra brightness/mode/MAX action. Check
   original single-button actions still work, including after an aborted chord.
3. Use `VIEW ...` and `STATUS` to test every view and analysis timing. The mock
   sine should appear near 1 kHz in spectrum/waterfall. Try ordinary speech/music
   with the real mic; do not generate hazardous levels to test visuals.
4. Confirm microphone faults/clipping/staleness override all views, old
   waterfall/peak history is cleared, and CLIP remains visible through recovery.
5. In every view, soak-test for at least 30 minutes. Confirm no increasing audio
   errors/overruns/stalls, stable heap, adequate stack and acceptable UI analysis
   time. SPL statistics must continue while visualisations are selected.
6. Check panel power/supply temperature with denser graphics, starting at low
   brightness. Do not treat brightness settings as current limiting.
7. Reboot: view returns to SPL; calibration, thresholds, measurement mode and
   brightness retain their existing NVS settings. Test rollback with the original
   UF2 without erasing NVS or replacing the bootloader/partition table.

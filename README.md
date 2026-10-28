# Standalone rehearsal-room SPL indicator

C++ / Arduino / PlatformIO firmware for **Adafruit MatrixPortal S3 (5778)**,
a conventional **64×32 HUB75 panel**, and **ICS-43434 microphone (6049)**.
After flashing, it runs from 5 V USB-C without a computer, network or cloud.

**Approximate dBA only. Not an IEC 61672-compliant or certified sound level
meter. Not suitable for legal, occupational-exposure or hearing-safety decisions.**
Color thresholds are configurable rehearsal indicators, not exposure limits.

## Start here

1. Read [wiring and power](docs/WIRING.md), including the USB power limitations.
2. Build and flash `matrix_test` to verify your panel.
3. Flash `mock` to exercise graphics/buttons without a microphone.
4. Connect the mic and flash `meter`; confirm diagnostics and calibrate.
5. Follow the [hardware acceptance checklist](docs/VALIDATION.md) before relying
   on the readings. Compilation is not hardware validation.

## Linux build and flash

Requirements: Python 3 with venv, Git, network access for initial package
downloads; `g++` for host tests. Python 3.11–3.14 is suitable.

```sh
cd /home/jiri/Projects/rehearsal-spl
python3 -m venv .venv
. .venv/bin/activate
pip install platformio==6.1.18
pio run -e matrix_test
pio run -e mock
pio run -e meter
pio device list
pio run -e meter -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200 --echo
```

**Replace `/dev/ttyACM0` with your MatrixPortal's actual port.** Never flash an
unrelated serial device. Grant your user serial-device access using your
distribution's udev rules / `dialout` or `uucp` group, then log in again. Avoid
running PlatformIO as root. If automatic upload fails, hold BOOT, tap RESET,
release BOOT, rediscover the port and upload. Close other serial monitors first.
The board configuration retains Adafruit's TinyUF2 partition layout.

To flash another environment, replace `-e meter` with `-e mock` or
`-e matrix_test`. Output images are `.pio/build/<environment>/firmware.bin`;
use PlatformIO upload to also write the appropriate bootloader/partition files.

### App-only UF2 updates (recommended with the working TinyUF2 bootloader)

Every build also creates `.pio/build/<environment>/firmware.uf2`. Double-click
RESET to enter `MATRXS3BOOT`, then copy the **meter** UF2 onto that mounted drive:

```sh
pio run -e meter
cp .pio/build/meter/firmware.uf2 /run/media/jiri/MATRXS3BOOT/
```

The drive should disconnect and the board reboot when the transfer completes.
This updates only the application, preserving the working bootloader, partition
table and settings. The UF2 uses ESP32-S3 family `0xc47e5767` and **base 0**:
TinyUF2 interprets addresses relative to the OTA application partition, NOT as
absolute flash addresses. Do not rename a `.bin` to `.uf2` or copy a merged
flash image. The build hook validates the application checksum and SHA-256
before packaging. To package an existing application manually:

```sh
python3 tools/make_uf2.py .pio/build/meter/firmware.bin build/meter.uf2
```

### Reproducible dependency selection

| Component | Selected version |
|---|---|
| PlatformIO Core | 6.1.18 |
| PlatformIO Espressif32 platform | 6.9.0 |
| Arduino-ESP32 | 2.0.17, package `3.20017.0` |
| ESP-IDF embedded in that core | 4.4.7 |
| Protomatter | 1.7.0, Git commit `d55e3bf83306b19651c1905bfb984f3768c2569e` |
| Adafruit GFX | 1.11.11 |
| Adafruit BusIO | 1.16.1 |
| Xtensa compiler | 8.4.0+2021r2-patch5, selected by pinned platform |

`platformio.ini` pins every linked external library. Protomatter's Arduino
metadata additionally downloads optional example dependencies; `lib_ignore`
excludes those from the firmware. They are not part of the linked image.
The Protomatter Git pin is intentional: the PlatformIO registry did not provide
the requested 1.7.0 package during verification.

The APIs were checked against source and actually compiled together:
`Adafruit_Protomatter::begin/show/getFrameCount`, GFX canvas operations and
ESP-IDF 4.4 `driver/i2s.h`. This is the **legacy I2S driver**, not Arduino 3.x
`ESP_I2S` or ESP-IDF 5.x channel APIs. Do not update just one dependency.
On S3, Protomatter uses LCD_CAM rather than I2S. The S3 has no audio PLL:
I2S uses the 160 MHz source with a fractional divider, not `use_apll`.

## Operation

The display updates at **8 Hz**, independently of hardware panel refresh:

- Main reading: rounded whole dBA, 14×20-pixel seven-segment digits; three
  digits occupy 46 pixels. No fabricated decimal precision.
- Right: `dBA` bottom-aligned with the big numbers; a compact 3×5-pixel `F`
  (FAST), `S` (SLOW), or `EQ` (LAeq,1s)
  sits in the bottom-right corner, with one blank pixel to the right and one
  between the marker and the level bar.
- Bottom: `M104 A88` style text: maximum **completed LAeq,1s**, and rolling
  energy average. Average covers available completed seconds until 900 seconds
  have accumulated, then the preceding 15 minutes. It is not a full 15-minute
  result until `rolling_s=900`. With rolling disabled, A is latest LAeq,1s.
- Last two rows: level bar covering 40–110 dBA, clamped at either end.
- Green below 85, yellow 85 through 95, red above 95 by default. The color/bar
  follow the currently selected mode, not necessarily LAeq.
- Below 35 dBA only the main reading becomes `--`; units, MAX/AVG, mode marker
  and the clamped level bar remain visible. Above 116 dBA, `OVER RANGE` replaces
  the measurement display. These
  are conservative display bounds, not verified instrument operating limits.
- `WARMUP`, `MIC ERROR`, `OVERRUN`, `CLIP`, `DSP ERROR`, or `STALE` replaces
  the reading when invalid. No old measurement is shown as current.

| Control | Action |
|---|---|
| Short UP | brightness +16, maximum 255 |
| Short DOWN | brightness −16, minimum 16 |
| Hold UP ≥800 ms | FAST → SLOW → LAeq → FAST |
| Hold DOWN ≥800 ms | reset maximum; next candidate uses a full post-reset second |
| Hold UP + DOWN together ≥825 ms | cycle SPL → spectrum → waterfall → waveform → reactive → SPL |

Buttons debounce for 25 ms. A long press does not also cause a short press.
Brightness/mode/thresholds/calibration persist to NVS after 2 seconds without
settings changes. Wait for `# NVS saved` before unplugging. Maximum/history
are RAM-only and reset at power-up. RESET clears maximum, not rolling history.
Calibration changes clear all accumulated measurements to avoid mixed scales.
Brightness scales RGB before Protomatter quantization; it does not use a
nonexistent `setBrightness()` API. Double buffering prevents visible clears,
and identical frames are not sent again.

### Visualisations

All five views are implemented; SPL measurement and statistics continue in every
view. Press both buttons together and hold until the view changes, then release
both. A chord does not also change brightness/measurement mode or reset MAX.
Single-button controls retain their existing meaning in every view.

- **Spectrum:** 16 logarithmic bands, 60 Hz–16 kHz, low frequencies on the left,
  with falling peak markers. Values are relative, automatically ranged digital
  band energy, **not per-band dBA**.
- **Waterfall:** the same bands as coloured rows; newest at the top, about
  4 seconds of history at the 8 Hz display rate. History resets on entry/fault.
- **Waveform:** 64-column min/max envelope of a 2048-sample (~42.7 ms) frame;
  visual auto-gain is used, not a calibrated amplitude scale.
- **Reactive:** sparse moving rainbow diamond rings respond to unweighted
  signal energy. Colours are decorative, not exposure indicators.

Visualisations use the full 64×32 panel without name headers. SPL units and
invalid-audio/error indications remain visible in their respective displays.

The current view is RAM-only and resets to SPL at power-up. All persisted SPL
settings keep their original storage format. Use serial `VIEW SPL`,
`VIEW SPECTRUM`, `VIEW WATERFALL`, `VIEW WAVEFORM` or `VIEW REACTIVE` to select
a view directly. `STATUS` adds a `# view=...` line including UI analysis timing
(`visual_us`, `max_visual_us`) and visual-buffer allocation status.

Visual input is unweighted and DC-removed. A 2048-point Hann-windowed FFT gives
23.44 Hz bin spacing; low-frequency bands have limited resolution and spectral
leakage. Bands sum energy rather than report power spectral density. The 48 dB
visual range follows the strongest band with a bounded reference and slow decay.
Visual auto-ranging never changes microphone gain or the SPL pipeline.

The audio task fills a fixed frame and performs a non-blocking overwrite of a
one-frame queue. FFT/rendering run on the UI core at up to 8 Hz; late frames are
discarded, never back-pressured into acquisition. Invalid/stale audio overrides
all visualisations with its error indication. Visual buffer allocation failure
leaves SPL available. Real hardware timing/boot/power checks are still required;
host tests and compilation do not prove uninterrupted acquisition on the board.
See [validation](docs/VALIDATION.md).

**Power:** filled bars/waterfalls may light many more pixels than the SPL
digits. Start dim and use an adequately rated panel supply, not an assumed
computer-USB current budget. Software brightness is not a current limiter.

## USB serial and calibration

USB serial is optional; boot never waits for it. Commands are uppercase,
newline-terminated, with one space between tokens. Commands over 95 characters
are rejected. Values must be finite and within range.

```text
HELP
STATUS
MODE FAST
MODE SLOW
MODE LAEQ
VIEW SPECTRUM
VIEW WATERFALL
VIEW WAVEFORM
VIEW REACTIVE
VIEW SPL
BRIGHT 64
THRESH 85 95
CAL 94.0
OFFSET -1.5
RESET
CSV ON
CSV OFF
```

- `CAL <reference_dBA>` adjusts the existing correction by reference minus
  the latest valid completed LAeq,1s. Reference range: 40–110 dBA, correction
  limited to ±30 dB. Rejected in mock mode or with invalid audio.
- `OFFSET <dB>` directly sets the correction, including `OFFSET 0` to restore
  nominal sensitivity. It does not change sample gain.
- `THRESH <yellow> <red>` accepts 30 ≤ yellow < red ≤ 120.
- Settings changes acknowledge immediately and are saved later. NVS failure
  is reported and retried; persistence must not be assumed after an error.

### Calibration procedure

1. Use a trustworthy external reference meter in A weighting. Put its microphone
   close to the ICS-43434 without obstructing either mic or touching them.
2. Use a steady 1 kHz tone, preferably around 80–94 dB, or stable broadband
   sound. Avoid clipping, strong room gradients, airflow and vibrating mounts.
   Do not generate hazardous test levels just to calibrate this indicator.
3. Allow 10 seconds to settle. Compare equal 1-second LAeq intervals; do not
   compare this LAeq with a reference meter's instantaneous peak reading.
4. If the reference reads 88.4 dBA, send `CAL 88.4`. The correction is relative
   to the last valid second, so keep the sound stable while sending it.
5. Wait for fresh readings and `# NVS saved`; verify agreement at another
   level and after a power cycle. `STATUS` reports the applied correction.

A normal acoustic calibrator does not directly seal to this breakout. A custom
coupler needs its own validation. One-point adjustment corrects sensitivity,
not microphone frequency response, directionality or room-position errors.

### Diagnostics / CSV

Human-readable telemetry is emitted once per second. `CSV ON` emits a header
and switches subsequent telemetry to CSV. `#`-prefixed acknowledgments/notices
may appear in either mode: CSV parsers must skip these. Unavailable numeric
values are `nan`; check `valid`, `status` and `mock` before using results.

Fields include raw normalized RMS, raw dBFS referenced to **full-scale peak**,
10 ms A-weighted estimate, FAST, SLOW, LAeq,1s, rolling LAeq and its coverage,
maximum LAeq,1s, calibration, raw peak, clipping count, RX overflow events,
read/error and deadline-miss counters, total acquired frames, free/DMA heap,
current/worst DSP time, audio-task stack headroom, requested sample rate,
panel refresh and dropped serial lines. `frames` includes startup/discarded
blocks; it is not the integration-window length.

`i2s_get_clk()` in this pinned legacy core returns its stored configured rate,
**not an independently measured clock**. Verify LRCLK/BCLK with a logic analyzer.
Serial output is bounded and may drop whole telemetry lines if the host cannot
keep up; it never back-pressures the audio task. Send one command at a time and
wait for its acknowledgment. Serial is not a lossless logger.

## Measurement pipeline

```text
I2S0 stereo 32-bit slots, left selected → sign-extend upper 24 bits
→ normalize by 2^23 (fixed unity gain) → DC blocker (~3.8 Hz)
→ three SOS A-weighting IIR sections at 48 kHz → square
→ sample-by-sample FAST/SLOW exponential energy integration
→ exact 48000-sample LAeq bins → optional 900-second energy ring / maximum
→ 10 log10(mean-square) + nominal offset + persisted calibration
```

The ICS-43434 −26 dBFS sensitivity at 94 dB SPL is relative to a full-scale
**sine**, whose normalized RMS is 1/√2. Thus:

```text
dBA ≈ 123.0103 + 10 log10(A-weighted normalized mean-square) + correction_dB
94 dB / 1 kHz nominal sine: peak = 10^(−26/20), RMS ≈ 0.0354393
raw peak-referenced dBFS ≈ −29.0103, not −26
```

FAST and SLOW use `alpha = 1 − exp(−1/(48000*tau))`, tau=0.125 s / 1 s,
applied to each squared sample. LAeq and rolling averages never average dB.
LAeq,1s is a sequence of completed, non-overlapping one-second windows, not
a sliding instantaneous estimate. Rolling average advances on those boundaries.

At boot/reconnection, one second is acquired and filtered but discarded to
settle mic/DC/IIR startup, followed by a full integration second before valid
readings. Exponential integrators start at zero; SLOW initially underreads and
needs about five integration seconds to settle to within ~0.03 dB for a steady
signal. Do not calibrate during that period.

Audio runs in a priority-5 FreeRTOS task on core 0; display/USB/buttons run on
core 1. I2S has twelve 480-frame DMA buffers (46,080 bytes nominal), a 64-entry
event queue, and 10 ms reads. DSP allocates nothing per block. Only a latest
snapshot crosses to the UI; slow rendering cannot fill an audio-summary queue.
No intentional gaps or delay calls occur in healthy hardware acquisition.

Timeout/partial reads, stuck identical samples for 100 ms, non-finite DSP,
near-rail digital clipping, driver overflow events and >100 ms read/task stalls
invalidate readings. Fault recovery discards queued data and resets time
integrations/history; only an already-valid maximum survives. The driver is
reinstalled after acquisition faults, not during normal measurement. A stale
snapshot (>250 ms) is invalidated by the UI. Clipped seconds do not enter MAX
or the 15-minute average; missing data is not replaced by silence. After an
overload, `CLIP` stays visible until a full clean integration second is available,
rather than briefly flashing `CLIP` and then showing `WARMUP`.

Set `-DSPL_ENABLE_ROLLING=0` in build flags to omit the 900-float history (~3.6 KB).
No microphone-specific response equalizer, AGC, noise suppression or Wi-Fi is
enabled. See [measurement limitations](docs/LIMITATIONS.md).

## Tests and validation

```sh
python3 tools/test_host.py
pio run -e matrix_test -e meter -e mock
```

The Python standard-library test driver compiles the same C++ DSP/meter used
on the device with `g++`, then generates quantized 24-bit sine signals. It tests
sample scaling/sign extension, DC rejection, nominal SPL and calibration,
Fast/Slow step response, correct energy averages, exact window boundaries,
maximum/reset, continuity loss, 15-minute wrap, clipping/non-finite detection,
and button semantics. It compares measured filter response with an independent
analytic A-weighting curve, writing `build/a_weighting_response.csv`.
No Python, NumPy or SciPy is used on the device; tests require neither NumPy
nor SciPy on the host.

`mock` generates a real-time quantized 1 kHz sine alternating between nominal
80/104 dBA every five seconds. It runs the same DSP, meter, UI and settings.
`DM` replaces the mode marker, and telemetry has `mock=1`. The sound is
synthetic, not a microphone measurement. Real I2S timing and mic-disconnection
detection cannot be tested in this environment. Flash `meter` afterward.

See [recorded verification and hardware checklist](docs/VALIDATION.md).

## Source layout and license

`Microphone`, `AudioDSP`, `SPLMeter`, `Visualization`, `MatrixDisplay`, `Settings` and `main.cpp`
are separate modules under `include/` and `src/`. The only alternate entry
point is the deliberately small `matrix_smoke.cpp` smoke-test environment.

GPL-3.0-or-later; see [LICENSE](LICENSE) and [third-party attribution](THIRD_PARTY.md).
This includes adapted A-weighting coefficients from Ivan Kostoski's
[esp32-i2s-slm](https://github.com/ikostoski/esp32-i2s-slm), with original credit
to Dr. Matt L. This is not an unlicensed rewrite of its assembly filter.

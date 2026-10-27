# Measurement uncertainty and limitations

This is a useful **approximate rehearsal loudness indicator**, not a certified
meter. Firmware filtering alone cannot establish IEC 61672 compliance. No
instrument class, traceable absolute accuracy or exposure-dose accuracy is
claimed. The default 85/95 dBA colors are not occupational exposure limits.

## Microphone and acoustic assembly

- Adafruit specifies nominal sensitivity −26 dBFS ±1 dB, 65 dBA SNR in high
  performance mode and 120 dB SPL acoustic overload point. At 48 kHz the mic is
  in high performance mode. Individual sensitivity tolerance already limits
  uncalibrated absolute accuracy; calibration-meter uncertainty adds to it.
- Nominal A-weighted self-noise inferred from 94−65 is about 29 dBA. Readings
  near that floor are dominated by microphone noise. The display's 35 dBA
  lower bound is a conservative UI choice, not a tested lower measurement limit.
- Acoustic overload is not synonymous with digital rail clipping. Distortion
  or compression can precede the firmware's `|sample| ≥ 0.999` clipping flag.
  A loud, strongly low-frequency signal may overload the mic while its
  A-weighted reading remains well below 116 dBA. A missing CLIP indication does
  not prove absence of distortion. Do not trust near-overload measurements.
- The breakout product describes roughly 50 Hz–15 kHz useful response. The
  microphone, port, PCB, enclosure and mounting have frequency/directional
  errors not represented by the digital filter test. A one-point offset cannot
  correct these. This version deliberately omits the reference project's
  typical-unit response equalizer; that is not individual microphone calibration.
- Room modes, proximity to drums/speakers/walls, microphone orientation, crowd
  shielding and placement can change a reading by several dB. Put the mic near
  a representative listening position, not behind the LED panel. A single mic
  cannot characterize all positions in a rehearsal room.
- Airflow, handling, structure-borne vibration and electrically coupled panel
  noise can create false sound energy. Enclose/protect appropriately without
  obstructing the bottom port; validate after changing the enclosure.

## DSP and timing

- A-weighting is a six-pole digital approximation of the nominal IEC frequency
  characteristic at nominal 48 kHz, preceded by an extra ~3.8 Hz DC blocker.
  Host results validate this digital transfer, **not the acoustic transfer**.
  Additional low-frequency attenuation is visible at 31.5 Hz. No class-tolerance
  assessment, tone-burst test suite or full IEC laboratory evaluation is supplied.
- Filter coefficients/state and exponential energy integrators use float;
  sums of energies use double. Quantization, roundoff and timer/clock error
  remain. Host tests cannot guarantee identical numerical behavior on Xtensa.
- ESP32-S3 uses a 160 MHz fractional I2S clock divider; it does not have the
  original ESP32's audio PLL. Actual sample rate and clock duty/timing should be
  measured. Integrations use sample counts at nominal 48000 Hz, not wall-clock
  timestamps. Oscillator/divider errors affect actual window durations and
  weighting frequency locations. `i2s_get_clk()` is not a frequency counter.
- FAST (125 ms) and SLOW (1 s) are sample-by-sample exponential mean-square
  responses, not average dB or block RMS smoothing. They intentionally lag sudden
  changes. SLOW starts from zero after a continuity reset; allow settling.
- LAeq,1s uses non-overlapping complete seconds; it updates once a second.
  MAX means max LAeq,1s, not LAFmax, peak SPL, dBC peak or musical transient peak.
  A hit can be much higher than the displayed whole-second average.
- Rolling LAeq covers the last 900 completed valid seconds only after full
  warm-up. Any detected discontinuity/clipping clears the entire rolling window.
  Partial/new windows are clearly quantified by `rolling_s`; they must not be
  reported as a complete 15-minute measurement. There is no dose/exposure model.

## Failure detection and runtime

- There is no microphone identity, CRC, self-test transducer or independent
  acoustic reference. Timeout, constant-data, overflow and non-finite checks
  catch common failures, but disconnected/noisy wires, wrong microphones,
  intermittent corruption or acoustically blocked ports may produce plausible
  nonconstant samples and evade them. Always test the sensor with known sound.
- The fixed DMA ring provides nominal 120 ms buffering (driver RX queue has
  eleven slots). This is not infinite storage. Priority-5 acquisition, separate
  cores/peripherals, bounded USB output and allocation-free block processing
  reduce risk; real-hardware worst-case timing must still be measured.
- Flash/NVS writes can suspend interrupt/task service. Settings writes are
  deferred/coalesced, not done in the audio loop. Long stalls are detected and
  invalidate continuity instead of silently integrating missing audio. Display
  refresh can pause during flash operations. Power loss during an NVS write may
  leave the previous settings; invalid/missing blobs fall back to defaults.
- Overrun counters count **received driver events**, not exact lost samples.
  A separate deadline counter catches long service gaps even if event reporting
  is saturated. Neither can reconstruct lost audio or prove every possible
  hardware fault was detected.
- Protomatter's LCD_CAM and I2S have independent functions but share internal
  memory/GDMA resources and power. Stable panel refresh and sustained audio
  need a soak test on the actual panel/board/supply combination.
- Whole-number display resolution does not imply ±0.5 dB measurement accuracy.
  Keep a certified meter and appropriate hearing protection available regardless
  of what this indicator reports.

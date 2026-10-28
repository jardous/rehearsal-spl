// SPDX-License-Identifier: GPL-3.0-or-later
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "AudioDSP.h"
#include "Button.h"
#include "MatrixDisplay.h"
#include "Microphone.h"
#include "Visualization.h"

namespace {
Settings settings;
MatrixDisplay display;
Microphone microphone;
AudioDSP dsp;
SPLMeter meter;
QueueHandle_t snapshots = nullptr, controls = nullptr, visualInputs = nullptr;
TaskHandle_t audioTaskHandle = nullptr;
struct Control { bool resetMax; float calibration; };
AudioSnapshot latest;
bool taskStarted = false, csv = false, dirty = false;
float initialCalibration = 0;
uint32_t changedMs = 0, serialDropped = 0, refreshHz = 0;
char tx[2048];
size_t txLength = 0, txPosition = 0;
ButtonPair buttonPair;
ViewMode view = ViewMode::Meter; // RAM-only: always boot into the familiar SPL view
VisualCapture visualCapture;
VisualInput pendingVisual;
VisualFrame visualFrame;
SpectrumAnalyzer spectrum;
uint32_t visualUs = 0, maxVisualUs = 0;

// USB work is confined to the UI task. A bounded output queue; drop whole lines
// instead of back-pressuring the audio task or growing memory.
void output(const char* format, ...) {
  if (!Serial) return;
  char line[1024];
  va_list args;
  va_start(args, format);
  const int count = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (count <= 0) return;
  const size_t pending = txLength - txPosition;
  if (size_t(count) >= sizeof(line) || pending + count > sizeof(tx)) {
    ++serialDropped;
    return;
  }
  memmove(tx, tx + txPosition, pending);
  memcpy(tx + pending, line, count);
  txPosition = 0;
  txLength = pending + count;
}
void pumpOutput() {
  if (!Serial) { txLength = txPosition = 0; return; }
  const int space = Serial.availableForWrite();
  if (space > 0 && txPosition < txLength) {
    const size_t n = std::min(size_t(space), txLength - txPosition);
    txPosition += Serial.write(reinterpret_cast<const uint8_t*>(tx + txPosition), n);
  }
}
void changed() { dirty = true; changedMs = millis(); }

void publish(AudioSnapshot& s) {
  s.timestampMs = millis();
  s.overruns = microphone.overruns();
  s.errors = microphone.errors();
  s.stackFree = uxTaskGetStackHighWaterMark(nullptr); // ESP-IDF reports bytes
  xQueueOverwrite(snapshots, &s);
}

void audioTask(void*) {
  static float input[config::blockFrames], weighted[config::blockFrames];
  AudioSnapshot s;
  uint32_t warmupFrames = 0;
  bool running = false;
  meter.setCalibration(initialCalibration);
  s.calibration = initialCalibration;
  for (;;) {
    Control ctl;
    while (xQueueReceive(controls, &ctl, 0) == pdTRUE) {
      if (ctl.resetMax) meter.resetMaximum();
      else {
        meter.setCalibration(ctl.calibration);
        s.calibration = ctl.calibration;
        s.status = AudioStatus::Warming;
      }
    }
    if (!running) {
      running = microphone.begin();
      dsp.reset();
      meter.resetContinuity();
      warmupFrames = 0;
      s.sampleRate = microphone.sampleRate();
      s.levels = {};
      s.status = running ? AudioStatus::Warming : AudioStatus::MicError;
      publish(s);
      if (!running) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
    }
    const int64_t beforeRead = esp_timer_get_time();
    const uint32_t oldOverruns = microphone.overruns();
    const bool readOk = microphone.read(input);
    const bool deadline = esp_timer_get_time() - beforeRead > 100000;
    // Also catch task stalls between successive reads, e.g. a long flash erase.
    static int64_t lastRead = 0;
    const int64_t nowUs = esp_timer_get_time();
    const bool stalled = lastRead && nowUs - lastRead > 100000;
    lastRead = nowUs;
    if (!readOk || deadline || stalled) {
      visualCapture.reset();
      if (deadline || stalled) ++s.deadlineMisses;
      s.status = microphone.overruns() != oldOverruns || deadline || stalled
        ? AudioStatus::Overrun : AudioStatus::MicError;
      meter.resetContinuity();
      s.levels = {};
      s.rawRms = s.rawDbfs = s.weightedDB = s.peak = NAN;
      publish(s);
      microphone.end(); // discard queued samples; never integrate across missing audio
      running = false;
      lastRead = 0;
      vTaskDelay(pdMS_TO_TICKS(500)); // fault retry only, never in healthy acquisition
      continue;
    }
    const int64_t start = esp_timer_get_time();
    const DSPBlock block = dsp.process(input, weighted, config::blockFrames);
    s.frames += config::blockFrames;
    s.clippedSamples += block.clippedSamples;
    s.rawRms = sqrt(block.rawEnergy / config::blockFrames);
    s.rawDbfs = s.rawRms > 0 ? 20 * log10f(s.rawRms) : NAN;
    s.peak = block.peak;
    s.weightedDB = SPLMeter::energyDB(block.weightedEnergy / config::blockFrames, s.calibration);
    if (!block.finite || !std::isfinite(s.weightedDB)) {
      dsp.reset();
      meter.resetContinuity();
      warmupFrames = 0;
      s.status = AudioStatus::DSPError;
      s.levels = {};
    } else if (block.clippedSamples) {
      meter.resetContinuity();
      s.status = AudioStatus::Clipping;
      s.levels = {};
    } else if (warmupFrames < config::sampleRate) {
      warmupFrames += config::blockFrames; // discard 1 s for mic/DC/IIR startup
      s.status = integrationStatus(false, s.status);
      s.levels = {};
    } else {
      meter.process(weighted, config::blockFrames);
      s.levels = meter.levels();
      s.status = integrationStatus(s.levels.valid, s.status);
    }
    if (visualInputs && s.status == AudioStatus::Ok) {
      for (float sample : input) {
        if (visualCapture.push(sample)) {
          visualCapture.frame.timestampMs = millis();
          xQueueOverwrite(visualInputs, &visualCapture.frame); // never wait for UI/FFT
        }
      }
    } else visualCapture.reset();
    s.processingUs = esp_timer_get_time() - start;
    s.maxProcessingUs = std::max(s.maxProcessingUs, s.processingUs);
    publish(s);
  }
}

bool sendControl(bool reset, float calibration = 0) {
  Control ctl{reset, calibration};
  return controls && xQueueSend(controls, &ctl, 0) == pdTRUE;
}
bool setCalibration(float value) {
  if (!std::isfinite(value) || fabsf(value) > 30 || !sendControl(false, value)) return false;
  settings.calibration = value;
  latest.levels.valid = false;
  latest.status = AudioStatus::Warming;
  changed();
  return true;
}

void diagnostics() {
  const auto& a = latest;
  const auto& l = a.levels;
  const bool ok = a.status == AudioStatus::Ok && l.valid;
  output(csv
    ? "%lu,%s,%u,%u,%.8f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%u,%.2f,%.2f,%.6f,%llu,%lu,%lu,%lu,%llu,%u,%u,%lu,%lu,%lu,%.3f,%lu,%lu\n"
    : "ms=%lu status=%s valid=%u mock=%u raw_rms=%.8f raw_dBFS=%.3f A10ms=%.2f FAST=%.2f SLOW=%.2f LAeq1s=%.2f AVG=%.2f avg_s=%u MAX=%.2f cal=%.2f peak=%.6f clips=%llu overruns=%lu errors=%lu stalls=%lu frames=%llu heap=%u dma_heap=%u dsp_us=%lu max_dsp_us=%lu stack_B=%lu fs=%.3f refresh_Hz=%lu serial_drop=%lu\n",
    (unsigned long)millis(), statusName(a.status), unsigned(ok), unsigned(SPL_MOCK_INPUT),
    a.rawRms, a.rawDbfs, a.weightedDB, ok ? l.fast : NAN, ok ? l.slow : NAN,
    ok ? l.leq1s : NAN, ok ? l.rolling : NAN, ok ? l.rollingSeconds : 0,
    ok ? l.maximum : NAN, a.calibration, a.peak,
    (unsigned long long)a.clippedSamples, (unsigned long)a.overruns,
    (unsigned long)a.errors, (unsigned long)a.deadlineMisses, (unsigned long long)a.frames,
    unsigned(ESP.getFreeHeap()), unsigned(heap_caps_get_free_size(MALLOC_CAP_DMA)),
    (unsigned long)a.processingUs, (unsigned long)a.maxProcessingUs, (unsigned long)a.stackFree,
    a.sampleRate, (unsigned long)refreshHz, (unsigned long)serialDropped);
}

void command(char* line) {
  char name[16] = {}, args[80] = {}, extra;
  const int parts = sscanf(line, "%15s %79[^\r\n]", name, args);
  if (parts < 1) return;
  float v = 0, w = 0;
  bool ok = false;
  if (!strcmp(name, "HELP") && parts == 1) {
    output("# HELP | STATUS | CSV ON/OFF | CAL reference_dBA | OFFSET dB | BRIGHT 16..255 | THRESH yellow red | MODE FAST/SLOW/LAEQ | VIEW SPL/SPECTRUM/WATERFALL/WAVEFORM/REACTIVE | RESET\n# Approximate readings; not IEC 61672 compliant. GPL-3.0-or-later; no warranty. See LICENSE and THIRD_PARTY.md.\n");
    return;
  }
  if (!strcmp(name, "STATUS") && parts == 1) {
    diagnostics();
    output("# view=%s visual_buffer=%u visual_us=%lu max_visual_us=%lu\n", viewName(view),
      unsigned(visualInputs != nullptr), (unsigned long)visualUs, (unsigned long)maxVisualUs);
    return;
  }
  if (!strcmp(name, "CSV")) {
    if (!strcmp(args, "ON")) {
      csv = true;
      output("ms,status,valid,mock,raw_rms,raw_dbfs_peak_ref,a_10ms,laf,las,laeq_1s,laeq_rolling,rolling_s,max_laeq_1s,cal_db,raw_peak,clipped_samples,rx_overruns,errors,deadline_misses,frames,heap_bytes,dma_heap_bytes,dsp_us,max_dsp_us,stack_free_bytes,sample_rate_hz,refresh_hz,serial_dropped\n");
      return;
    }
    if (!strcmp(args, "OFF")) { csv = false; ok = true; }
  } else if (!strcmp(name, "CAL") && sscanf(args, "%f %c", &v, &extra) == 1) {
    ok = !SPL_MOCK_INPUT && latest.status == AudioStatus::Ok && latest.levels.valid
      && std::isfinite(v) && v >= 40 && v <= 110
      && latest.levels.leq1s >= 35 && latest.levels.leq1s <= 110
      && setCalibration(settings.calibration + v - latest.levels.leq1s);
  } else if (!strcmp(name, "OFFSET") && sscanf(args, "%f %c", &v, &extra) == 1) {
    ok = setCalibration(v);
  } else if (!strcmp(name, "BRIGHT") && sscanf(args, "%f %c", &v, &extra) == 1) {
    if (std::isfinite(v) && v >= 16 && v <= 255 && floorf(v) == v) {
      settings.brightness = v; changed(); ok = true;
    }
  } else if (!strcmp(name, "THRESH") && sscanf(args, "%f %f %c", &v, &w, &extra) == 2) {
    if (std::isfinite(v) && std::isfinite(w) && v >= 30 && w <= 120 && v < w) {
      settings.yellow = v; settings.red = w; changed(); ok = true;
    }
  } else if (!strcmp(name, "MODE")) {
    if (!strcmp(args, "FAST")) { settings.mode = DisplayMode::Fast; ok = true; }
    if (!strcmp(args, "SLOW")) { settings.mode = DisplayMode::Slow; ok = true; }
    if (!strcmp(args, "LAEQ")) { settings.mode = DisplayMode::Leq; ok = true; }
    if (ok) changed();
  } else if (!strcmp(name, "VIEW")) {
    for (unsigned i = 0; i < unsigned(ViewMode::Count); ++i) {
      if (!strcmp(args, viewName(ViewMode(i)))) {
        view = ViewMode(i);
        output("# view=%s (RAM-only)\n", viewName(view));
        return;
      }
    }
  } else if (!strcmp(name, "RESET") && parts == 1) ok = sendControl(true);
  output(ok ? "# OK (settings saved after 2 s idle)\n" : "# ERROR: invalid command, range, unavailable audio or full control queue; HELP\n");
}

void serialInput() {
  static char line[96];
  static size_t length = 0;
  static bool overflow = false;
  // Bound work even if a host floods USB.
  for (unsigned n = 0; n < 64 && Serial.available(); ++n) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (overflow) output("# ERROR: command too long\n");
      else if (length) { line[length] = 0; command(line); }
      length = 0; overflow = false;
    } else if (c >= 32 && c <= 126) {
      if (length + 1 < sizeof(line)) line[length++] = c;
      else overflow = true;
    }
  }
}

void buttons(uint32_t now) {
  const auto events = buttonPair.update(digitalRead(config::buttonUp) == LOW,
                                        digitalRead(config::buttonDown) == LOW, now);
  const auto u = events.up, d = events.down;
  if (events.nextView) {
    view = ViewMode((unsigned(view) + 1) % unsigned(ViewMode::Count));
    output("# view=%s (RAM-only)\n", viewName(view));
  }
  if (u == ButtonEvent::Short) {
    settings.brightness = std::min(255, int(settings.brightness) + 16); changed();
  }
  if (d == ButtonEvent::Short) {
    settings.brightness = std::max(16, int(settings.brightness) - 16); changed();
  }
  if (u == ButtonEvent::Long) {
    settings.mode = DisplayMode((unsigned(settings.mode) + 1) % 3); changed();
  }
  if (d == ButtonEvent::Long && !sendControl(true)) output("# ERROR: control queue full\n");
}
} // namespace

void setup() {
  Serial.begin(115200); // never wait for a USB host: standalone boot
  Serial.setTxTimeoutMs(0); // API verified in both USBCDC and HWCDC in core 2.0.17
  const bool loaded = settings.load();
  initialCalibration = settings.calibration;
  pinMode(config::buttonUp, INPUT_PULLUP);
  pinMode(config::buttonDown, INPUT_PULLUP);
  const bool matrixOk = display.begin(); // timer ISR allocated on Arduino core 1
  snapshots = xQueueCreate(1, sizeof(AudioSnapshot));
  controls = xQueueCreate(8, sizeof(Control));
  visualInputs = xQueueCreate(1, sizeof(VisualInput)); // optional: SPL works if allocation fails
  if (snapshots && controls) taskStarted = xTaskCreatePinnedToCore(
    audioTask, "audio", 8192, nullptr, 5, &audioTaskHandle, 0) == pdPASS;
  if (!taskStarted) latest.status = AudioStatus::MicError;
  output("# Rehearsal SPL: approximate dBA, NOT IEC 61672 compliant. GPL-3.0-or-later, NO WARRANTY; see LICENSE. HELP for commands.\n# matrix=%u audio_task=%u NVS_loaded=%u mock=%u\n", unsigned(matrixOk), unsigned(taskStarted), unsigned(loaded), unsigned(SPL_MOCK_INPUT));
}

void loop() {
  if (snapshots) xQueueReceive(snapshots, &latest, 0);
  // Read the clock after the queue: the other core may have just published.
  const uint32_t now = millis();
  if (!taskStarted || uint32_t(now - latest.timestampMs) > config::staleMs) {
    latest.status = taskStarted ? AudioStatus::Stale : AudioStatus::MicError;
    latest.levels.valid = false;
    latest.rawRms = latest.rawDbfs = latest.weightedDB = latest.peak = NAN;
  }
  if (latest.calibration != settings.calibration && latest.status == AudioStatus::Ok) {
    latest.status = AudioStatus::Warming;
    latest.levels.valid = false;
  }
  buttons(now);
  pumpOutput();
  serialInput();
  static uint32_t displayAt = 0, telemetryAt = 0;
  if (uint32_t(now - displayAt) >= 125) {
    displayAt = now;
    if (view != ViewMode::Meter && latest.status == AudioStatus::Ok && latest.levels.valid
        && visualInputs && xQueueReceive(visualInputs, &pendingVisual, 0) == pdTRUE) {
      const int64_t started = esp_timer_get_time();
      spectrum.process(pendingVisual, visualFrame);
      visualUs = esp_timer_get_time() - started;
      maxVisualUs = std::max(maxVisualUs, visualUs);
    }
    display.render(latest, settings, view, visualFrame, millis());
  }
  if (uint32_t(now - telemetryAt) >= 1000) {
    const uint32_t interval = now - telemetryAt;
    telemetryAt = now;
    refreshHz = uint64_t(display.refreshFrames()) * 1000 / interval;
    diagnostics();
  }
  if (dirty && uint32_t(now - changedMs) >= 2000) {
    if (settings.save()) { dirty = false; output("# NVS saved\n"); }
    else { changedMs = now; output("# ERROR: NVS save failed, retrying\n"); }
  }
  delay(1); // UI task only; microphone task blocks on DMA, not this delay
}

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build/test the *firmware* DSP on Linux. Python standard library only."""
import csv
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
BUILD.mkdir(exist_ok=True)
EXE = BUILD / "host_tests"
compile_command = [
    "g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
    "-I", str(ROOT / "include"), str(ROOT / "tests/host_tests.cpp"),
    str(ROOT / "src/AudioDSP.cpp"), str(ROOT / "src/SPLMeter.cpp"),
]
subprocess.run(compile_command + ["-o", str(EXE)], check=True)
subprocess.run([str(EXE)], check=True)
no_rolling = BUILD / "host_tests_no_rolling"
subprocess.run(compile_command + ["-DSPL_ENABLE_ROLLING=0", "-o", str(no_rolling)], check=True)
subprocess.run([str(no_rolling)], check=True)


def analog_a(hz):
    """IEC A-weighting nominal analog characteristic, normalized at 1 kHz."""
    def response(f):
        f2 = f * f
        return (12194.217**2 * f2**2 /
                ((f2 + 20.598997**2) * (f2 + 12194.217**2) *
                 math.sqrt((f2 + 107.65265**2) * (f2 + 737.86223**2))))
    return 20 * math.log10(response(hz) / response(1000))


rows = []
for hz in (31.5, 63, 100, 125, 250, 500, 1000, 2000, 4000, 8000, 10000, 12500, 16000, 20000):
    data = subprocess.check_output([str(EXE), str(hz), "94"], text=True)
    raw, weighted, fast, slow, leq = map(float, data.split(","))
    measured = 20 * math.log10(weighted / raw)
    expected = analog_a(hz)
    error = measured - expected
    # Engineering regression bounds, NOT IEC class tolerances or certification.
    tolerance = 0.35 if hz <= 10000 else 1.5 if hz <= 16000 else 4.0
    assert abs(error) < tolerance, (hz, measured, expected, error)
    rows.append((hz, expected, measured, error, leq))
    print(f"{hz:7g} Hz: target {expected:7.3f}, C++ {measured:7.3f}, error {error:+.3f} dB")

with (BUILD / "a_weighting_response.csv").open("w") as f:
    writer = csv.writer(f)
    writer.writerow(("frequency_hz", "nominal_a_db", "firmware_a_db", "error_db", "laeq_94db_sine"))
    writer.writerows(rows)
print("PASS: A-weighting response, including 100 Hz, 1 kHz and 10 kHz")

// SPDX-License-Identifier: GPL-3.0-or-later
// A-weighting coefficients from esp32-i2s-slm, (c)2019 Ivan Kostoski.
// Original fit credited there to Dr. Matt L.: https://dsp.stackexchange.com/a/36122
// Modified 2026-10-27: conventional denominator signs and independent DF2T.
// See THIRD_PARTY.md and LICENSE. Fixed Fs = 48000 Hz. NOT a compliance claim.
#pragma once
struct SOSCoefficients { float b1, b2, a1, a2; };
constexpr SOSCoefficients aWeightSOS[] = {
  {-2.00026996133106f, 1.00027056142719f, 1.060868438509278f, 0.163987445885926f},
  {4.35912384203144f, 3.09120265783884f, -1.208419926363593f, 0.273166998428332f},
  {-0.70930303489759f, -0.29071868393580f, -1.982242159753048f, 0.982298594928989f}
};
constexpr float aWeightGain = 0.169994948147430f;

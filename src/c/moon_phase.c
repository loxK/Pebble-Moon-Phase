/*
 * Moon Phase -- astronomy engine.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "moon_phase.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Mean length of the synodic month, in days.
#define SYNODIC_MONTH 29.530588861

// Julian Day of the Unix epoch (1970-01-01T00:00:00 UTC).
#define JD_UNIX_EPOCH 2440587.5

#define MP_SECONDS_PER_DAY 86400.0

#define DEG_TO_RAD (M_PI / 180.0)

/** Convert a UTC epoch to a Julian Day. */
static double prv_jd_from_epoch(time_t epoch) {
  return (double)epoch / MP_SECONDS_PER_DAY + JD_UNIX_EPOCH;
}

/** Convert a Julian Day back to a UTC epoch. */
static time_t prv_epoch_from_jd(double jd) {
  return (time_t)round((jd - JD_UNIX_EPOCH) * MP_SECONDS_PER_DAY);
}

// Taylor series for sin/cos of a small argument x in [0, pi/4].
static double prv_sin_small(double x) {
  double x2 = x * x;
  return x * (1.0 + x2 * (-1.0 / 6 + x2 * (1.0 / 120
           + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880)))));
}
static double prv_cos_small(double x) {
  double x2 = x * x;
  return 1.0 + x2 * (-0.5 + x2 * (1.0 / 24
       + x2 * (-1.0 / 720 + x2 * (1.0 / 40320))));
}

double mp_cos_deg(double deg) {
  double d = fmod(deg, 360.0);
  if (d < 0) d += 360.0;       // [0, 360)
  if (d > 180.0) d -= 360.0;   // (-180, 180]
  if (d < 0) d = -d;           // [0, 180]  (cos is even)
  double sign = 1.0;
  if (d > 90.0) { d = 180.0 - d; sign = -1.0; }  // [0, 90], cos(180-x) = -cos x
  // Now d in [0, 90]; split at 45 so the Taylor argument stays in [0, pi/4].
  if (d <= 45.0) {
    return sign * prv_cos_small(d * DEG_TO_RAD);
  }
  return sign * prv_sin_small((90.0 - d) * DEG_TO_RAD);
}

double mp_sin_deg(double deg) {
  return mp_cos_deg(90.0 - deg);
}

// sin/cos taking degrees, used by the Meeus series below.
static double prv_sind(double deg) { return mp_sin_deg(deg); }
static double prv_cosd(double deg) { return mp_cos_deg(deg); }

/**
 * Julian Ephemeris Day of the phase identified by the continuous lunation
 * value `k`. Integer k -> new moon; +0.25 first quarter; +0.5 full moon;
 * +0.75 last quarter (Meeus, Astronomical Algorithms, ch. 49).
 *
 * Dynamical-time / UTC difference (delta-T, ~70 s) is ignored on purpose: it is
 * negligible for a "in X days" display.
 */
static double prv_phase_jde(double k) {
  double T = k / 1236.85;
  double T2 = T * T;
  double T3 = T2 * T;
  double T4 = T3 * T;

  // Mean phase time.
  double jde = 2451550.09766 + 29.530588861 * k
             + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;

  // Sun's mean anomaly at time JDE.
  double M = 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3;
  // Moon's mean anomaly.
  double Mp = 201.5643 + 385.81693528 * k + 0.0107582 * T2
            + 0.00001238 * T3 - 0.000000058 * T4;
  // Moon's argument of latitude.
  double F = 160.7108 + 390.67050284 * k - 0.0016118 * T2
           - 0.00000227 * T3 + 0.000000011 * T4;
  // Longitude of the ascending node of the lunar orbit.
  double Omega = 124.7746 - 1.56375588 * k + 0.0020672 * T2 + 0.00000215 * T3;
  // Eccentricity of the Earth's orbit around the Sun.
  double E = 1.0 - 0.002516 * T - 0.0000074 * T2;
  double E2 = E * E;

  // Which quarter of the lunation? 0=new, 1=first, 2=full, 3=last.
  int quarter = (int)round((k - floor(k)) * 4.0) % 4;

  double corr = 0.0;
  if (quarter == 0 || quarter == 2) {
    // New moon and full moon share the same set of corrections.
    corr =
        -0.40720 * prv_sind(Mp)
      + 0.17241 * E * prv_sind(M)
      + 0.01608 * prv_sind(2 * Mp)
      + 0.01039 * prv_sind(2 * F)
      + 0.00739 * E * prv_sind(Mp - M)
      - 0.00514 * E * prv_sind(Mp + M)
      + 0.00208 * E2 * prv_sind(2 * M)
      - 0.00111 * prv_sind(Mp - 2 * F)
      - 0.00057 * prv_sind(Mp + 2 * F)
      + 0.00056 * E * prv_sind(2 * Mp + M)
      - 0.00042 * prv_sind(3 * Mp)
      + 0.00042 * E * prv_sind(M + 2 * F)
      + 0.00038 * E * prv_sind(M - 2 * F)
      - 0.00024 * E * prv_sind(2 * Mp - M)
      - 0.00017 * prv_sind(Omega)
      - 0.00007 * prv_sind(Mp + 2 * M)
      + 0.00004 * prv_sind(2 * Mp - 2 * F)
      + 0.00004 * prv_sind(3 * M)
      + 0.00003 * prv_sind(Mp + M - 2 * F)
      + 0.00003 * prv_sind(2 * Mp + 2 * F)
      - 0.00003 * prv_sind(Mp + M + 2 * F)
      + 0.00003 * prv_sind(Mp - M + 2 * F)
      - 0.00002 * prv_sind(Mp - M - 2 * F)
      - 0.00002 * prv_sind(3 * Mp + M)
      + 0.00002 * prv_sind(4 * Mp);
  } else {
    // First and last quarter.
    corr =
        -0.62801 * prv_sind(Mp)
      + 0.17172 * E * prv_sind(M)
      - 0.01183 * E * prv_sind(Mp + M)
      + 0.00862 * prv_sind(2 * Mp)
      + 0.00804 * prv_sind(2 * F)
      + 0.00454 * E * prv_sind(Mp - M)
      + 0.00204 * E2 * prv_sind(2 * M)
      - 0.00180 * prv_sind(Mp - 2 * F)
      - 0.00070 * prv_sind(Mp + 2 * F)
      - 0.00040 * prv_sind(3 * Mp)
      - 0.00034 * E * prv_sind(2 * Mp - M)
      + 0.00032 * E * prv_sind(M + 2 * F)
      + 0.00032 * E * prv_sind(M - 2 * F)
      - 0.00028 * E2 * prv_sind(Mp + 2 * M)
      + 0.00027 * E * prv_sind(2 * Mp + M)
      - 0.00017 * prv_sind(Omega)
      - 0.00005 * prv_sind(Mp - M - 2 * F)
      + 0.00004 * prv_sind(2 * Mp + 2 * F)
      - 0.00004 * prv_sind(Mp + M + 2 * F)
      + 0.00004 * prv_sind(Mp - 2 * M)
      + 0.00003 * prv_sind(Mp + M - 2 * F)
      + 0.00003 * prv_sind(3 * M)
      + 0.00002 * prv_sind(2 * Mp - 2 * F)
      + 0.00002 * prv_sind(Mp - M + 2 * F)
      - 0.00002 * prv_sind(3 * Mp + M);

    // Extra correction W, added for first quarter, subtracted for last quarter.
    double W = 0.00306
             - 0.00038 * E * prv_cosd(M)
             + 0.00026 * prv_cosd(Mp)
             - 0.00002 * prv_cosd(Mp - M)
             + 0.00002 * prv_cosd(Mp + M)
             + 0.00002 * prv_cosd(2 * F);
    corr += (quarter == 1) ? W : -W;
  }

  return jde + corr;
}

/** Approximate continuous lunation index `k` for a given epoch. */
static double prv_k_estimate(time_t now) {
  struct tm *utc = gmtime(&now);
  double year_dec = (utc->tm_year + 1900) + (utc->tm_yday / 365.25);
  return (year_dec - 2000.0) * 12.3685;
}

MoonState moon_phase_state(time_t now) {
  double jd = prv_jd_from_epoch(now);
  double k0 = floor(prv_k_estimate(now));

  // Find the new moon bracketing `now`: the latest new moon <= jd and the next.
  double last_new = 0.0;
  double next_new = 0.0;
  for (double k = k0 - 2.0; k <= k0 + 2.0; k += 1.0) {
    double t = prv_phase_jde(k);
    if (t <= jd && (last_new == 0.0 || t > last_new)) {
      last_new = t;
    }
    if (t > jd && (next_new == 0.0 || t < next_new)) {
      next_new = t;
    }
  }

  MoonState state;
  if (last_new > 0.0 && next_new > last_new) {
    state.fraction = (jd - last_new) / (next_new - last_new);
  } else {
    // Fallback to the mean synodic month if bracketing failed.
    state.fraction = fmod((jd - last_new) / SYNODIC_MONTH, 1.0);
  }
  if (state.fraction < 0.0) state.fraction += 1.0;
  if (state.fraction >= 1.0) state.fraction -= 1.0;

  state.illumination = (1.0 - mp_cos_deg(360.0 * state.fraction)) / 2.0;
  state.waxing = state.fraction < 0.5;
  return state;
}

void moon_phase_upcoming(time_t now, MoonPhaseEvent *events, int count) {
  double jd = prv_jd_from_epoch(now);
  double k_base = floor(prv_k_estimate(now)) - 1.0;

  int found = 0;
  // Walk forward through lunations; each integer k yields four quarter events.
  for (double k = k_base; found < count && k < k_base + 40.0; k += 1.0) {
    for (int q = 0; q < 4 && found < count; q++) {
      double kk = k + q * 0.25;
      double t = prv_phase_jde(kk);
      if (t > jd) {
        events[found].type = (MoonPhaseType)q;
        events[found].time = prv_epoch_from_jd(t);
        found++;
      }
    }
  }
}

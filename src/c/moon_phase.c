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

// Dynamical time runs ahead of UTC by delta-T, about 69 s through the 2020s and
// drifting by about a second a decade. The series below are in dynamical time.
#define DELTA_T_DAYS (69.0 / MP_SECONDS_PER_DAY)

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

// Planetary arguments of Meeus ch. 49 (A1..A14): angle at k = 0 and per
// lunation, in degrees, then the amplitude of the correction, in days. Floats
// keep the table small; their rounding moves a phase by a second at most.
typedef struct {
  float a0, a1, amplitude;
} PlanetaryTerm;

static const PlanetaryTerm PLANETARY_TERMS[] = {
  {299.77f, 0.107408f, 0.000325f}, {251.88f, 0.016321f, 0.000165f},
  {251.83f, 26.651886f, 0.000164f}, {349.42f, 36.412478f, 0.000126f},
  {84.66f, 18.206239f, 0.000110f}, {141.74f, 53.303771f, 0.000062f},
  {207.14f, 2.453732f, 0.000060f}, {154.84f, 7.306860f, 0.000056f},
  {34.52f, 27.261239f, 0.000047f}, {207.19f, 0.121824f, 0.000042f},
  {291.34f, 1.844379f, 0.000040f}, {161.72f, 24.198154f, 0.000037f},
  {239.56f, 25.513099f, 0.000035f}, {331.55f, 3.592518f, 0.000023f},
};

/**
 * Julian Day, in UTC, of the phase identified by the continuous lunation value
 * `k`. Integer k -> new moon; +0.25 first quarter; +0.5 full moon; +0.75 last
 * quarter (Meeus, Astronomical Algorithms, ch. 49).
 */
static double prv_phase_jd(double k) {
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
    // New moon and full moon share their corrections from the eighth term on;
    // the first seven differ slightly.
    const bool full = quarter == 2;
    corr =
        (full ? -0.40614 : -0.40720) * prv_sind(Mp)
      + (full ? 0.17302 : 0.17241) * E * prv_sind(M)
      + (full ? 0.01614 : 0.01608) * prv_sind(2 * Mp)
      + (full ? 0.01043 : 0.01039) * prv_sind(2 * F)
      + (full ? 0.00734 : 0.00739) * E * prv_sind(Mp - M)
      - (full ? 0.00515 : 0.00514) * E * prv_sind(Mp + M)
      + (full ? 0.00209 : 0.00208) * E2 * prv_sind(2 * M)
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

  // The planets add their own small corrections, the same for every phase.
  for (unsigned i = 0; i < sizeof(PLANETARY_TERMS) / sizeof(PLANETARY_TERMS[0]); i++) {
    const PlanetaryTerm *term = &PLANETARY_TERMS[i];
    double angle = term->a0 + (double)term->a1 * k - (i == 0 ? 0.009173 * T2 : 0.0);
    corr += term->amplitude * prv_sind(angle);
  }

  return jde + corr - DELTA_T_DAYS;
}

/** Approximate continuous lunation index `k` for a given epoch. */
static double prv_k_estimate(time_t now) {
  struct tm *utc = gmtime(&now);
  double year_dec = (utc->tm_year + 1900) + (utc->tm_yday / 365.25);
  return (year_dec - 2000.0) * 12.3685;
}

MoonState moon_phase_state(time_t now) {
  double jd = prv_jd_from_epoch(now);

  // Bracket `now` between two principal phases, starting a quarter before the
  // mean one: the true phases stay within a day of the mean ones, well inside
  // a quarter, so a couple of steps either way always find it.
  double q = floor((jd - 2451550.09766) / SYNODIC_MONTH * 4.0) / 4.0 - 0.25;
  double t_a = prv_phase_jd(q);
  double t_b = prv_phase_jd(q + 0.25);
  for (int i = 0; i < 3 && t_b <= jd; i++) {
    q += 0.25;
    t_a = t_b;
    t_b = prv_phase_jd(q + 0.25);
  }
  for (int i = 0; i < 3 && jd < t_a; i++) {
    q -= 0.25;
    t_b = t_a;
    t_a = prv_phase_jd(q);
  }

  // Linear between the two, so the drawing matches each phase at its instant.
  MoonState state;
  state.fraction = (q - floor(q)) + 0.25 * (jd - t_a) / (t_b - t_a);
  if (state.fraction < 0.0) state.fraction += 1.0;
  if (state.fraction >= 1.0) state.fraction -= 1.0;
  return state;
}

// Distance terms of Meeus table 47.A: multiples of the mean elongation D, the
// Sun's mean anomaly M, the Moon's mean anomaly M' and its argument of
// latitude F, with the coefficient in metres. Rows whose distance term is zero
// are left out.
typedef struct {
  int8_t d, m, mp, f;
  int32_t r;
} DistanceTerm;

static const DistanceTerm DISTANCE_TERMS[] = {
  {0, 0, 1, 0, -20905355}, {2, 0, -1, 0, -3699111}, {2, 0, 0, 0, -2955968},
  {0, 0, 2, 0, -569925},   {0, 1, 0, 0, 48888},     {0, 0, 0, 2, -3149},
  {2, 0, -2, 0, 246158},   {2, -1, -1, 0, -152138}, {2, 0, 1, 0, -170733},
  {2, -1, 0, 0, -204586},  {0, 1, -1, 0, -129620},  {1, 0, 0, 0, 108743},
  {0, 1, 1, 0, 104755},    {2, 0, 0, -2, 10321},    {0, 0, 1, -2, 79661},
  {4, 0, -1, 0, -34782},   {0, 0, 3, 0, -23210},    {4, 0, -2, 0, -21636},
  {2, 1, -1, 0, 24208},    {2, 1, 0, 0, 30824},     {1, 0, -1, 0, -8379},
  {1, 1, 0, 0, -16675},    {2, -1, 1, 0, -12831},   {2, 0, 2, 0, -10445},
  {4, 0, 0, 0, -11650},    {2, 0, -3, 0, 14403},    {0, 1, -2, 0, -7003},
  {2, -1, -2, 0, 10056},   {1, 0, 1, 0, 6322},      {2, -2, 0, 0, -9884},
  {0, 1, 2, 0, 5751},      {2, -2, -1, 0, -4950},   {2, 0, 1, -2, 4130},
  {4, -1, -1, 0, -3958},   {3, 0, -1, 0, 3258},     {2, 1, 1, 0, 2616},
  {4, -1, -2, 0, -1897},   {0, 2, -1, 0, -2117},    {2, 2, -1, 0, 2354},
  {4, 0, 1, 0, -1423},     {0, 0, 4, 0, -1117},     {4, -1, 0, 0, -1571},
  {1, 0, -2, 0, -1739},    {0, 0, 2, -2, -4421},    {0, 2, 1, 0, 1165},
  {2, 0, -1, -2, 8752},
};

/** Earth-Moon distance in km at the given UTC time, unrounded. */
static double prv_distance(time_t t) {
  // Julian centuries since J2000, in dynamical time.
  double T = (prv_jd_from_epoch(t) + DELTA_T_DAYS - 2451545.0) / 36525.0;
  double T2 = T * T;
  double T3 = T2 * T;
  double T4 = T3 * T;

  double D = fmod(297.8501921 + 445267.1114034 * T - 0.0018819 * T2
                  + T3 / 545868.0 - T4 / 113065000.0, 360.0);
  double M = fmod(357.5291092 + 35999.0502909 * T - 0.0001536 * T2
                  + T3 / 24490000.0, 360.0);
  double Mp = fmod(134.9633964 + 477198.8675055 * T + 0.0087414 * T2
                   + T3 / 69699.0 - T4 / 14712000.0, 360.0);
  double F = fmod(93.2720950 + 483202.0175233 * T - 0.0036539 * T2
                  - T3 / 3526000.0 + T4 / 863310000.0, 360.0);
  // Terms involving M shrink with the eccentricity of the Earth's orbit.
  double E = 1.0 - 0.002516 * T - 0.0000074 * T2;

  double sum = 0.0;
  for (unsigned i = 0; i < sizeof(DISTANCE_TERMS) / sizeof(DISTANCE_TERMS[0]); i++) {
    const DistanceTerm *term = &DISTANCE_TERMS[i];
    double c = term->r;
    if (term->m == 1 || term->m == -1) c *= E;
    else if (term->m == 2 || term->m == -2) c *= E * E;
    sum += c * mp_cos_deg(term->d * D + term->m * M + term->mp * Mp + term->f * F);
  }
  return 385000.56 + sum / 1000.0;
}

int32_t moon_distance_km(time_t t) {
  return (int32_t)(prv_distance(t) + 0.5);
}

// Direction of travel, from the distance a minute later. Both are compared
// unrounded: next to an apsis the Moon moves by metres in a minute, far below
// the km rounding.
#define APPROACH_STEP_S 60

bool moon_is_approaching(time_t t) {
  return prv_distance(t + APPROACH_STEP_S) < prv_distance(t);
}

void moon_phase_upcoming(time_t now, MoonPhaseEvent *events, int count) {
  double jd = prv_jd_from_epoch(now);
  double k_base = floor(prv_k_estimate(now)) - 1.0;

  int found = 0;
  // Walk forward through lunations; each integer k yields four quarter events.
  for (double k = k_base; found < count && k < k_base + 40.0; k += 1.0) {
    for (int q = 0; q < 4 && found < count; q++) {
      double kk = k + q * 0.25;
      double t = prv_phase_jd(kk);
      if (t > jd) {
        events[found].type = (MoonPhaseType)q;
        events[found].time = prv_epoch_from_jd(t);
        double distance = prv_distance(events[found].time);
        events[found].distance_km = (int32_t)(distance + 0.5);
        events[found].approaching =
            prv_distance(events[found].time + APPROACH_STEP_S) < distance;
        found++;
      }
    }
  }
}

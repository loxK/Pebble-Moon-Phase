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
 * Arguments of Meeus ch. 49 at lunation `k`, shared by the phases and the
 * eclipses (ch. 54), angles in degrees.
 */
typedef struct {
  double T2;
  double jde;    //!< Mean phase time.
  double M;      //!< Sun's mean anomaly.
  double Mp;     //!< Moon's mean anomaly.
  double F;      //!< Moon's argument of latitude.
  double Omega;  //!< Longitude of the ascending node of the lunar orbit.
  double E;      //!< Eccentricity of the Earth's orbit around the Sun.
} LunationArgs;

static LunationArgs prv_lunation_args(double k) {
  double T = k / 1236.85;
  double T2 = T * T;
  double T3 = T2 * T;
  double T4 = T3 * T;
  LunationArgs a;
  a.T2 = T2;
  a.jde = 2451550.09766 + 29.530588861 * k
        + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;
  a.M = 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3;
  a.Mp = 201.5643 + 385.81693528 * k + 0.0107582 * T2
       + 0.00001238 * T3 - 0.000000058 * T4;
  a.F = 160.7108 + 390.67050284 * k - 0.0016118 * T2
      - 0.00000227 * T3 + 0.000000011 * T4;
  a.Omega = 124.7746 - 1.56375588 * k + 0.0020672 * T2 + 0.00000215 * T3;
  a.E = 1.0 - 0.002516 * T - 0.0000074 * T2;
  return a;
}

/**
 * Julian Day, in UTC, of the phase identified by the continuous lunation value
 * `k`. Integer k -> new moon; +0.25 first quarter; +0.5 full moon; +0.75 last
 * quarter (Meeus, Astronomical Algorithms, ch. 49).
 */
static double prv_phase_jd(double k) {
  LunationArgs a = prv_lunation_args(k);
  double T2 = a.T2;
  double jde = a.jde;
  double M = a.M, Mp = a.Mp, F = a.F, Omega = a.Omega;
  double E = a.E;
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

/**
 * Earth-Moon distance in km at UTC epoch `t`, in seconds that may carry a
 * fraction, unrounded.
 */
static double prv_distance_at(double t) {
  // Julian centuries since J2000, in dynamical time.
  double T = (t / MP_SECONDS_PER_DAY + JD_UNIX_EPOCH + DELTA_T_DAYS - 2451545.0) / 36525.0;
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

/** Earth-Moon distance in km at the given UTC time, unrounded. */
static double prv_distance(time_t t) {
  return prv_distance_at((double)t);
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

#if MOON_HAS_DETAILS
/**
 * The eclipse at the new moon (integer `k`) or full moon (`k` + 0.5) of a
 * lunation (Meeus ch. 54). A solar eclipse is the kind seen from somewhere on
 * Earth.
 *
 * @return false, with `out` holding MOON_ECLIPSE_NONE, when there is none.
 */
static bool prv_eclipse_at(double k, MoonEclipse *out) {
  out->type = MOON_ECLIPSE_NONE;
  LunationArgs a = prv_lunation_args(k);
  // Far from a node, the Moon passes above or below the Sun or the shadow.
  if (fabs(prv_sind(a.F)) > 0.36) return false;

  double E = a.E, M = a.M, Mp = a.Mp;
  double F1 = a.F - 0.02665 * prv_sind(a.Omega);
  bool full = ((int)round((k - floor(k)) * 2.0)) % 2 == 1;

  double A1 = 299.77 + 0.107408 * k - 0.009173 * a.T2;
  double jde = a.jde
             + (full ? -0.4065 : -0.4075) * prv_sind(Mp)
             + (full ? 0.1727 : 0.1721) * E * prv_sind(M)
             + 0.0161 * prv_sind(2 * Mp)
             - 0.0097 * prv_sind(2 * F1)
             + 0.0073 * E * prv_sind(Mp - M)
             - 0.0050 * E * prv_sind(Mp + M)
             - 0.0023 * prv_sind(Mp - 2 * F1)
             + 0.0021 * E * prv_sind(2 * M)
             + 0.0012 * prv_sind(Mp + 2 * F1)
             + 0.0006 * E * prv_sind(2 * Mp + M)
             - 0.0004 * prv_sind(3 * Mp)
             - 0.0003 * E * prv_sind(M + 2 * F1)
             + 0.0003 * prv_sind(A1)
             - 0.0002 * E * prv_sind(M - 2 * F1)
             - 0.0002 * E * prv_sind(2 * Mp - M)
             - 0.0002 * prv_sind(a.Omega);

  double P = 0.2070 * E * prv_sind(M)
           + 0.0024 * E * prv_sind(2 * M)
           - 0.0392 * prv_sind(Mp)
           + 0.0116 * prv_sind(2 * Mp)
           - 0.0073 * E * prv_sind(Mp + M)
           + 0.0067 * E * prv_sind(Mp - M)
           + 0.0118 * prv_sind(2 * F1);
  double Q = 5.2207
           - 0.0048 * E * prv_cosd(M)
           + 0.0020 * E * prv_cosd(2 * M)
           - 0.3299 * prv_cosd(Mp)
           - 0.0060 * E * prv_cosd(Mp + M)
           + 0.0041 * E * prv_cosd(Mp - M);
  double W = fabs(prv_cosd(F1));
  // Least distance from the axis of the shadow to the Earth's centre, in Earth
  // radii, and the radius of the umbral cone in the fundamental plane.
  double gamma = fabs((P * prv_cosd(F1) + Q * prv_sind(F1)) * (1 - 0.0048 * W));
  double u = 0.0059
           + 0.0046 * E * prv_cosd(M)
           - 0.0182 * prv_cosd(Mp)
           + 0.0004 * prv_cosd(2 * Mp)
           - 0.0005 * prv_cosd(M + Mp);

  MoonEclipseType type;
  double magnitude = 0.0;
  if (!full) {
    if (gamma > 1.5433 + u) return false;
    if (gamma < 0.9972) {
      // Hybrid when 0 <= u < 0.00464 * sqrt(1 - gamma^2), compared squared to
      // keep the libm square root off the stack.
      bool hybrid = u >= 0 && u <= 0.0047
                 && u * u < 0.00464 * 0.00464 * (1 - gamma * gamma);
      type = u < 0 ? MOON_ECLIPSE_TOTAL : hybrid ? MOON_ECLIPSE_HYBRID : MOON_ECLIPSE_ANNULAR;
    } else if (gamma < 0.9972 + fabs(u)) {
      // The axis of the shadow misses the Earth: the umbra or antumbra can
      // still graze it near the poles.
      type = u < 0 ? MOON_ECLIPSE_TOTAL : MOON_ECLIPSE_ANNULAR;
    } else {
      type = MOON_ECLIPSE_PARTIAL;
      magnitude = (1.5433 + u - gamma) / (0.5461 + 2 * u);
    }
  } else {
    double penumbral = (1.5573 + u - gamma) / 0.5450;
    double umbral = (1.0128 - u - gamma) / 0.5450;
    if (penumbral <= 0) return false;
    if (umbral >= 1) {
      type = MOON_ECLIPSE_TOTAL;
      magnitude = umbral;
    } else if (umbral > 0) {
      type = MOON_ECLIPSE_PARTIAL;
      magnitude = umbral;
    } else {
      type = MOON_ECLIPSE_PENUMBRAL;
      magnitude = penumbral;
    }
  }
  out->type = type;
  out->solar = !full;
  out->magnitude = (float)magnitude;
  out->time = prv_epoch_from_jd(jde - DELTA_T_DAYS);
  return true;
}
#endif

void moon_phase_upcoming(time_t now, MoonPhaseEvent *events, int count) {
  double k_base = floor(prv_k_estimate(now)) - 1.0;

  int found = 0;
  // Walk forward through lunations; each integer k yields four quarter events.
  for (double k = k_base; found < count && k < k_base + 40.0; k += 1.0) {
    for (int q = 0; q < 4 && found < count; q++) {
      double kk = k + q * 0.25;
      // Compared once rounded to the second, as the caller sees it: asked from
      // an event's own time, the search must not return that event.
      time_t time = prv_epoch_from_jd(prv_phase_jd(kk));
      if (time > now) {
        events[found].type = (MoonPhaseType)q;
        events[found].time = time;
        double distance = prv_distance(events[found].time);
        events[found].distance_km = (int32_t)(distance + 0.5);
        events[found].approaching =
            prv_distance(events[found].time + APPROACH_STEP_S) < distance;
#if MOON_HAS_DETAILS
        events[found].eclipse.type = MOON_ECLIPSE_NONE;
        if (q == MOON_PHASE_NEW || q == MOON_PHASE_FULL) {
          prv_eclipse_at(kk, &events[found].eclipse);
        }
#endif
        found++;
      }
    }
  }
}

#if MOON_HAS_DETAILS
int moon_distance_class(int32_t km) {
  static const int32_t limits[] = {MOON_SUPERMOON_KM, 375000, 394000, 404000};
  for (int i = 0; i < 4; i++) {
    if (km < limits[i]) return i;
  }
  return 4;
}

bool moon_next_eclipse(time_t from, bool (*accept)(const MoonEclipse *eclipse),
                       MoonEclipse *out) {
  double k_base = floor(prv_k_estimate(from)) - 1.0;
  for (double k = k_base; k < k_base + 62.0; k += 0.5) {
    MoonEclipse eclipse;
    if (prv_eclipse_at(k, &eclipse) && eclipse.time > from && accept(&eclipse)) {
      *out = eclipse;
      return true;
    }
  }
  return false;
}

/**
 * Julian Day, in UTC, of the Moon's passage through the ascending node of its
 * orbit (integer `k`) or the descending one (`k` + 0.5), Meeus ch. 51.
 */
static double prv_node_jd(double k) {
  double T = k / 1342.23;
  double T2 = T * T;
  double T3 = T2 * T;
  double T4 = T3 * T;
  double D = 183.6380 + 331.73735682 * k + 0.0014852 * T2 + 0.00000209 * T3 - 0.000000010 * T4;
  double M = 17.4006 + 26.82037250 * k + 0.0001186 * T2 + 0.00000006 * T3;
  double Mp = 38.3776 + 355.52747313 * k + 0.0123499 * T2 + 0.000014627 * T3
            - 0.000000069 * T4;
  double Omega = 123.9767 - 1.44098956 * k + 0.0020608 * T2 + 0.00000214 * T3
               - 0.000000016 * T4;
  double V = 299.75 + 132.85 * T - 0.009173 * T2;
  double P = Omega + 272.75 - 2.3 * T;
  double E = 1.0 - 0.002516 * T - 0.0000074 * T2;

  double jde = 2451565.1619 + 27.212220817 * k
             + 0.0002762 * T2 + 0.000000021 * T3 - 0.000000000088 * T4
             - 0.4721 * prv_sind(Mp)
             - 0.1649 * prv_sind(2 * D)
             - 0.0868 * prv_sind(2 * D - Mp)
             + 0.0084 * prv_sind(2 * D + Mp)
             - 0.0083 * E * prv_sind(2 * D - M)
             - 0.0039 * E * prv_sind(2 * D - M - Mp)
             + 0.0034 * prv_sind(2 * Mp)
             - 0.0031 * prv_sind(2 * D - 2 * Mp)
             + 0.0030 * E * prv_sind(2 * D + M)
             + 0.0028 * E * prv_sind(M - Mp)
             + 0.0026 * E * prv_sind(M)
             + 0.0025 * prv_sind(4 * D)
             + 0.0024 * prv_sind(D)
             + 0.0022 * E * prv_sind(M + Mp)
             + 0.0017 * prv_sind(Omega)
             + 0.0014 * prv_sind(4 * D - Mp)
             + 0.0005 * E * prv_sind(2 * D + M - Mp)
             + 0.0004 * E * prv_sind(2 * D - M + Mp)
             - 0.0003 * E * prv_sind(2 * D - 2 * M)
             + 0.0003 * E * prv_sind(4 * D - M)
             + 0.0003 * prv_sind(V)
             + 0.0003 * prv_sind(P);
  return jde - DELTA_T_DAYS;
}

int moon_nodes(time_t from, time_t to, MoonNode *out, int max) {
  struct tm *utc = gmtime(&from);
  double year_dec = (utc->tm_year + 1900) + (utc->tm_yday / 365.25);
  double jd_from = prv_jd_from_epoch(from);
  double jd_to = prv_jd_from_epoch(to);
  int found = 0;
  // A draconic month is 27.2 days; start one passage early to catch the first.
  for (double k = floor((year_dec - 2000.05) * 13.4223) - 1.0; found < max; k += 0.5) {
    double t = prv_node_jd(k);
    if (t > jd_to) break;
    if (t > jd_from) {
      out[found].time = prv_epoch_from_jd(t);
      out[found].ascending = k == floor(k);
      found++;
    }
  }
  return found;
}

#define APSIS_STEP_S (6 * 3600)
#define INV_GOLDEN 0.6180339887

int moon_apsides(time_t from, time_t to, MoonApsis *out, int max) {
  int found = 0;
  // The grid starts a step early and ends a step late: a turn is only seen
  // from the sample after the one closest to it.
  double t0 = floor((double)from / APSIS_STEP_S) * APSIS_STEP_S - APSIS_STEP_S;
  double d0 = prv_distance_at(t0);
  double t1 = t0 + APSIS_STEP_S;
  double d1 = prv_distance_at(t1);
  for (double t2 = t1 + APSIS_STEP_S; t2 <= (double)to + 2 * APSIS_STEP_S && found < max;
       t2 += APSIS_STEP_S) {
    double d2 = prv_distance_at(t2);
    bool perigee = d1 < d0 && d1 <= d2;
    if (perigee || (d1 > d0 && d1 >= d2)) {
      // Search for the minimum of sign * distance; each round keeps one probe.
      double sign = perigee ? 1.0 : -1.0;
      double a = t0, b = t2;
      double c1 = b - (b - a) * INV_GOLDEN, f1 = sign * prv_distance_at(c1);
      double c2 = a + (b - a) * INV_GOLDEN, f2 = sign * prv_distance_at(c2);
      while (b - a > 30) {
        if (f1 < f2) {
          b = c2; c2 = c1; f2 = f1;
          c1 = b - (b - a) * INV_GOLDEN; f1 = sign * prv_distance_at(c1);
        } else {
          a = c1; c1 = c2; f1 = f2;
          c2 = a + (b - a) * INV_GOLDEN; f2 = sign * prv_distance_at(c2);
        }
      }
      time_t time = (time_t)(round((a + b) / 120.0) * 60.0);
      if (time > from && time <= to) {
        out[found].time = time;
        out[found].perigee = perigee;
        out[found].distance_km = moon_distance_km(time);
        found++;
      }
    }
    t0 = t1; d0 = d1;
    t1 = t2; d1 = d2;
  }
  return found;
}
#endif

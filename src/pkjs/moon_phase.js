/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon phase engine (JS port of src/c/moon_phase.c).
 *
 * Computes the dates of the upcoming principal Moon phases using Meeus'
 * algorithm (Astronomical Algorithms, ch. 49) with the principal periodic
 * correction terms, and the Earth-Moon distance (ch. 47). Kept in sync with the
 * C implementation that drives the watch UI; this copy is only used to push
 * timeline pins from the phone, and alone looks for perigees and apogees.
 */

var JD_UNIX_EPOCH = 2440587.5;
var SECONDS_PER_DAY = 86400.0;
var DEG = Math.PI / 180.0;

function sind(d) { return Math.sin(d * DEG); }
function cosd(d) { return Math.cos(d * DEG); }

function jdFromEpoch(epoch) { return epoch / SECONDS_PER_DAY + JD_UNIX_EPOCH; }
function epochFromJd(jd) { return Math.round((jd - JD_UNIX_EPOCH) * SECONDS_PER_DAY); }

// Dynamical time runs ahead of UTC by delta-T, about 69 s through the 2020s and
// drifting by about a second a decade. The series below are in dynamical time.
var DELTA_T_DAYS = 69.0 / SECONDS_PER_DAY;

// Planetary arguments of Meeus ch. 49 (A1..A14): [angle at k = 0, per lunation
// (degrees), amplitude of the correction (days)].
var PLANETARY_TERMS = [
  [299.77, 0.107408, 0.000325], [251.88, 0.016321, 0.000165],
  [251.83, 26.651886, 0.000164], [349.42, 36.412478, 0.000126],
  [84.66, 18.206239, 0.000110], [141.74, 53.303771, 0.000062],
  [207.14, 2.453732, 0.000060], [154.84, 7.306860, 0.000056],
  [34.52, 27.261239, 0.000047], [207.19, 0.121824, 0.000042],
  [291.34, 1.844379, 0.000040], [161.72, 24.198154, 0.000037],
  [239.56, 25.513099, 0.000035], [331.55, 3.592518, 0.000023],
];

/**
 * Julian Day, in UTC, of the phase identified by the continuous lunation value
 * `k` (integer => new moon, +0.25 first quarter, +0.5 full, +0.75 last).
 */
function phaseJD(k) {
  var T = k / 1236.85;
  var T2 = T * T, T3 = T2 * T, T4 = T3 * T;

  var jde = 2451550.09766 + 29.530588861 * k
          + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4;

  var M = 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3;
  var Mp = 201.5643 + 385.81693528 * k + 0.0107582 * T2
         + 0.00001238 * T3 - 0.000000058 * T4;
  var F = 160.7108 + 390.67050284 * k - 0.0016118 * T2
        - 0.00000227 * T3 + 0.000000011 * T4;
  var Omega = 124.7746 - 1.56375588 * k + 0.0020672 * T2 + 0.00000215 * T3;
  var E = 1.0 - 0.002516 * T - 0.0000074 * T2;
  var E2 = E * E;

  var quarter = Math.round((k - Math.floor(k)) * 4.0) % 4;

  var corr;
  if (quarter === 0 || quarter === 2) {
    // New moon and full moon share their corrections from the eighth term on;
    // the first seven differ slightly.
    var full = quarter === 2;
    corr =
        (full ? -0.40614 : -0.40720) * sind(Mp)
      + (full ? 0.17302 : 0.17241) * E * sind(M)
      + (full ? 0.01614 : 0.01608) * sind(2 * Mp)
      + (full ? 0.01043 : 0.01039) * sind(2 * F)
      + (full ? 0.00734 : 0.00739) * E * sind(Mp - M)
      - (full ? 0.00515 : 0.00514) * E * sind(Mp + M)
      + (full ? 0.00209 : 0.00208) * E2 * sind(2 * M)
      - 0.00111 * sind(Mp - 2 * F)
      - 0.00057 * sind(Mp + 2 * F)
      + 0.00056 * E * sind(2 * Mp + M)
      - 0.00042 * sind(3 * Mp)
      + 0.00042 * E * sind(M + 2 * F)
      + 0.00038 * E * sind(M - 2 * F)
      - 0.00024 * E * sind(2 * Mp - M)
      - 0.00017 * sind(Omega)
      - 0.00007 * sind(Mp + 2 * M)
      + 0.00004 * sind(2 * Mp - 2 * F)
      + 0.00004 * sind(3 * M)
      + 0.00003 * sind(Mp + M - 2 * F)
      + 0.00003 * sind(2 * Mp + 2 * F)
      - 0.00003 * sind(Mp + M + 2 * F)
      + 0.00003 * sind(Mp - M + 2 * F)
      - 0.00002 * sind(Mp - M - 2 * F)
      - 0.00002 * sind(3 * Mp + M)
      + 0.00002 * sind(4 * Mp);
  } else {
    corr =
        -0.62801 * sind(Mp)
      + 0.17172 * E * sind(M)
      - 0.01183 * E * sind(Mp + M)
      + 0.00862 * sind(2 * Mp)
      + 0.00804 * sind(2 * F)
      + 0.00454 * E * sind(Mp - M)
      + 0.00204 * E2 * sind(2 * M)
      - 0.00180 * sind(Mp - 2 * F)
      - 0.00070 * sind(Mp + 2 * F)
      - 0.00040 * sind(3 * Mp)
      - 0.00034 * E * sind(2 * Mp - M)
      + 0.00032 * E * sind(M + 2 * F)
      + 0.00032 * E * sind(M - 2 * F)
      - 0.00028 * E2 * sind(Mp + 2 * M)
      + 0.00027 * E * sind(2 * Mp + M)
      - 0.00017 * sind(Omega)
      - 0.00005 * sind(Mp - M - 2 * F)
      + 0.00004 * sind(2 * Mp + 2 * F)
      - 0.00004 * sind(Mp + M + 2 * F)
      + 0.00004 * sind(Mp - 2 * M)
      + 0.00003 * sind(Mp + M - 2 * F)
      + 0.00003 * sind(3 * M)
      + 0.00002 * sind(2 * Mp - 2 * F)
      + 0.00002 * sind(Mp - M + 2 * F)
      - 0.00002 * sind(3 * Mp + M);

    var W = 0.00306
          - 0.00038 * E * cosd(M)
          + 0.00026 * cosd(Mp)
          - 0.00002 * cosd(Mp - M)
          + 0.00002 * cosd(Mp + M)
          + 0.00002 * cosd(2 * F);
    corr += (quarter === 1) ? W : -W;
  }

  // The planets add their own small corrections, the same for every phase.
  for (var i = 0; i < PLANETARY_TERMS.length; i++) {
    var a = PLANETARY_TERMS[i];
    corr += a[2] * sind(a[0] + a[1] * k - (i === 0 ? 0.009173 * T2 : 0.0));
  }

  return jde + corr - DELTA_T_DAYS;
}

function kEstimate(epoch) {
  var d = new Date(epoch * 1000);
  var yearDec = d.getUTCFullYear() + (d.getUTCMonth() / 12.0);
  return (yearDec - 2000.0) * 12.3685;
}

// A full moon closer than this is a supermoon (timeanddate.com convention;
// there is no official rule).
var SUPERMOON_KM = 360000;

// Distance terms of Meeus table 47.A: [D, M, M', F, coefficient in metres].
// Rows whose distance term is zero are left out.
var DISTANCE_TERMS = [
  [0, 0, 1, 0, -20905355], [2, 0, -1, 0, -3699111], [2, 0, 0, 0, -2955968],
  [0, 0, 2, 0, -569925], [0, 1, 0, 0, 48888], [0, 0, 0, 2, -3149],
  [2, 0, -2, 0, 246158], [2, -1, -1, 0, -152138], [2, 0, 1, 0, -170733],
  [2, -1, 0, 0, -204586], [0, 1, -1, 0, -129620], [1, 0, 0, 0, 108743],
  [0, 1, 1, 0, 104755], [2, 0, 0, -2, 10321], [0, 0, 1, -2, 79661],
  [4, 0, -1, 0, -34782], [0, 0, 3, 0, -23210], [4, 0, -2, 0, -21636],
  [2, 1, -1, 0, 24208], [2, 1, 0, 0, 30824], [1, 0, -1, 0, -8379],
  [1, 1, 0, 0, -16675], [2, -1, 1, 0, -12831], [2, 0, 2, 0, -10445],
  [4, 0, 0, 0, -11650], [2, 0, -3, 0, 14403], [0, 1, -2, 0, -7003],
  [2, -1, -2, 0, 10056], [1, 0, 1, 0, 6322], [2, -2, 0, 0, -9884],
  [0, 1, 2, 0, 5751], [2, -2, -1, 0, -4950], [2, 0, 1, -2, 4130],
  [4, -1, -1, 0, -3958], [3, 0, -1, 0, 3258], [2, 1, 1, 0, 2616],
  [4, -1, -2, 0, -1897], [0, 2, -1, 0, -2117], [2, 2, -1, 0, 2354],
  [4, 0, 1, 0, -1423], [0, 0, 4, 0, -1117], [4, -1, 0, 0, -1571],
  [1, 0, -2, 0, -1739], [0, 0, 2, -2, -4421], [0, 2, 1, 0, 1165],
  [2, 0, -1, -2, 8752],
];

/**
 * Earth-Moon distance at `epoch` (seconds), in km, centre to centre (Meeus
 * ch. 47). Unrounded, so the apsis search below sees a smooth curve.
 */
function distanceKm(epoch) {
  var T = (jdFromEpoch(epoch) + DELTA_T_DAYS - 2451545.0) / 36525.0;  // dynamical time
  var T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  var D = 297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868.0 - T4 / 113065000.0;
  var M = 357.5291092 + 35999.0502909 * T - 0.0001536 * T2 + T3 / 24490000.0;
  var Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699.0 - T4 / 14712000.0;
  var F = 93.2720950 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000.0 + T4 / 863310000.0;
  var E = 1.0 - 0.002516 * T - 0.0000074 * T2;

  var sum = 0.0;
  for (var i = 0; i < DISTANCE_TERMS.length; i++) {
    var t = DISTANCE_TERMS[i];
    var m = Math.abs(t[1]);
    var c = t[4] * (m === 1 ? E : m === 2 ? E * E : 1.0);
    sum += c * cosd(t[0] * D + t[1] * M + t[2] * Mp + t[3] * F);
  }
  return 385000.56 + sum / 1000.0;
}

/**
 * Perigees and apogees between `fromEpoch` and `toEpoch`, in chronological
 * order: { perigee: bool, time: epochSeconds, distance: km }.
 *
 * The distance is sampled every six hours, on a grid fixed to the epoch so that
 * any call finds the same turns; each one is then narrowed down by golden-
 * section search and rounded to the minute, so its time is stable too. The
 * grid starts a step early: a turn is only seen from the sample after it.
 */
function apsides(fromEpoch, toEpoch) {
  var STEP = 6 * 3600;
  var R = 0.6180339887;  // inverse golden ratio
  var out = [];
  var t0 = Math.floor(fromEpoch / STEP) * STEP - STEP, d0 = distanceKm(t0);
  var t1 = t0 + STEP, d1 = distanceKm(t1);
  for (var t2 = t1 + STEP; t2 <= toEpoch + STEP; t2 += STEP) {
    var d2 = distanceKm(t2);
    var perigee = d1 < d0 && d1 <= d2;
    if (perigee || (d1 > d0 && d1 >= d2)) {
      // Search for the minimum of sign * distance; each round keeps one probe.
      var sign = perigee ? 1 : -1;
      var a = t0, b = t2;
      var c1 = b - (b - a) * R, f1 = sign * distanceKm(c1);
      var c2 = a + (b - a) * R, f2 = sign * distanceKm(c2);
      while (b - a > 30) {
        if (f1 < f2) {
          b = c2; c2 = c1; f2 = f1;
          c1 = b - (b - a) * R; f1 = sign * distanceKm(c1);
        } else {
          a = c1; c1 = c2; f1 = f2;
          c2 = a + (b - a) * R; f2 = sign * distanceKm(c2);
        }
      }
      var time = Math.round((a + b) / 120) * 60;
      if (time > fromEpoch && time <= toEpoch) {
        out.push({ perigee: perigee, time: time, distance: distanceKm(time) });
      }
    }
    t0 = t1; d0 = d1;
    t1 = t2; d1 = d2;
  }
  return out;
}

/**
 * Return the next `count` principal phase events strictly after `nowEpoch`,
 * in chronological order: { type: 0..3, time: epochSeconds, distance: km,
 * supermoon: bool }.
 */
function upcoming(nowEpoch, count) {
  var jd = jdFromEpoch(nowEpoch);
  var kBase = Math.floor(kEstimate(nowEpoch)) - 1.0;
  var events = [];
  for (var k = kBase; events.length < count && k < kBase + 40.0; k += 1.0) {
    for (var q = 0; q < 4 && events.length < count; q++) {
      var kk = k + q * 0.25;
      var t = phaseJD(kk);
      if (t > jd) {
        var time = epochFromJd(t);
        // Rounded to the km like the watch, so both name the same supermoons.
        var distance = Math.round(distanceKm(time));
        events.push({
          type: q,
          time: time,
          distance: distance,
          supermoon: q === 2 && distance < SUPERMOON_KM,
        });
      }
    }
  }
  return events;
}

module.exports = { upcoming: upcoming, apsides: apsides, distanceKm: distanceKm };

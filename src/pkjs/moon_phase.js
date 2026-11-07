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
 * Arguments of Meeus ch. 49 at lunation `k`, shared by the phases and the
 * eclipses (ch. 54): the Sun's mean anomaly M, the Moon's Mp, its argument of
 * latitude F and the longitude of its ascending node Omega, in degrees.
 */
function lunationArgs(k) {
  var T = k / 1236.85;
  var T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  return {
    T: T,
    T2: T2,
    jde: 2451550.09766 + 29.530588861 * k
       + 0.00015437 * T2 - 0.000000150 * T3 + 0.00000000073 * T4,
    M: 2.5534 + 29.10535670 * k - 0.0000014 * T2 - 0.00000011 * T3,
    Mp: 201.5643 + 385.81693528 * k + 0.0107582 * T2
      + 0.00001238 * T3 - 0.000000058 * T4,
    F: 160.7108 + 390.67050284 * k - 0.0016118 * T2
     - 0.00000227 * T3 + 0.000000011 * T4,
    Omega: 124.7746 - 1.56375588 * k + 0.0020672 * T2 + 0.00000215 * T3,
    E: 1.0 - 0.002516 * T - 0.0000074 * T2,
  };
}

/**
 * Julian Day, in UTC, of the phase identified by the continuous lunation value
 * `k` (integer => new moon, +0.25 first quarter, +0.5 full, +0.75 last).
 */
function phaseJD(k) {
  var args = lunationArgs(k);
  var T2 = args.T2;
  var jde = args.jde;
  var M = args.M, Mp = args.Mp, F = args.F, Omega = args.Omega;
  var E = args.E;
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

/**
 * The eclipse at the new moon (integer `k`) or full moon (`k` + 0.5) of a
 * lunation, or null when there is none (Meeus ch. 54).
 *
 * Returns { solar: bool, type, magnitude, time }. A solar eclipse is 'total',
 * 'annular', 'hybrid' or 'partial', seen from somewhere on Earth; its
 * magnitude is only given for a partial one. A lunar eclipse is 'total',
 * 'partial' or 'penumbral', with its umbral magnitude, or its penumbral one
 * for a penumbral eclipse. `time` is the instant of greatest eclipse, in
 * epoch seconds, which can be some way off the instant of the phase.
 */
function eclipseAt(k) {
  var a = lunationArgs(k);
  // Far from a node, the Moon passes above or below the Sun or the shadow.
  if (Math.abs(sind(a.F)) > 0.36) return null;

  var E = a.E, M = a.M, Mp = a.Mp;
  var F1 = a.F - 0.02665 * sind(a.Omega);
  var full = Math.round((k - Math.floor(k)) * 2) % 2 === 1;

  var A1 = 299.77 + 0.107408 * k - 0.009173 * a.T2;
  var jde = a.jde
          + (full ? -0.4065 : -0.4075) * sind(Mp)
          + (full ? 0.1727 : 0.1721) * E * sind(M)
          + 0.0161 * sind(2 * Mp)
          - 0.0097 * sind(2 * F1)
          + 0.0073 * E * sind(Mp - M)
          - 0.0050 * E * sind(Mp + M)
          - 0.0023 * sind(Mp - 2 * F1)
          + 0.0021 * E * sind(2 * M)
          + 0.0012 * sind(Mp + 2 * F1)
          + 0.0006 * E * sind(2 * Mp + M)
          - 0.0004 * sind(3 * Mp)
          - 0.0003 * E * sind(M + 2 * F1)
          + 0.0003 * sind(A1)
          - 0.0002 * E * sind(M - 2 * F1)
          - 0.0002 * E * sind(2 * Mp - M)
          - 0.0002 * sind(a.Omega);
  var time = epochFromJd(jde - DELTA_T_DAYS);
  function eclipse(solar, type, magnitude) {
    return { solar: solar, type: type, magnitude: magnitude, time: time };
  }
  var P = 0.2070 * E * sind(M)
        + 0.0024 * E * sind(2 * M)
        - 0.0392 * sind(Mp)
        + 0.0116 * sind(2 * Mp)
        - 0.0073 * E * sind(Mp + M)
        + 0.0067 * E * sind(Mp - M)
        + 0.0118 * sind(2 * F1);
  var Q = 5.2207
        - 0.0048 * E * cosd(M)
        + 0.0020 * E * cosd(2 * M)
        - 0.3299 * cosd(Mp)
        - 0.0060 * E * cosd(Mp + M)
        + 0.0041 * E * cosd(Mp - M);
  var W = Math.abs(cosd(F1));
  // Least distance from the axis of the shadow to the Earth's centre, in Earth
  // radii, and the radius of the umbral cone in the fundamental plane.
  var gamma = Math.abs((P * cosd(F1) + Q * sind(F1)) * (1 - 0.0048 * W));
  var u = 0.0059
        + 0.0046 * E * cosd(M)
        - 0.0182 * cosd(Mp)
        + 0.0004 * cosd(2 * Mp)
        - 0.0005 * cosd(M + Mp);

  if (!full) {
    if (gamma > 1.5433 + u) return null;
    if (gamma < 0.9972) {
      var type = u < 0 ? 'total'
               : u > 0.0047 || u >= 0.00464 * Math.sqrt(1 - gamma * gamma) ? 'annular'
               : 'hybrid';
      return eclipse(true, type, null);
    }
    // The axis of the shadow misses the Earth: the umbra or antumbra can still
    // graze it near the poles, otherwise only the penumbra does.
    if (gamma < 0.9972 + Math.abs(u)) {
      return eclipse(true, u < 0 ? 'total' : 'annular', null);
    }
    return eclipse(true, 'partial', (1.5433 + u - gamma) / (0.5461 + 2 * u));
  }

  var penumbral = (1.5573 + u - gamma) / 0.5450;
  var umbral = (1.0128 - u - gamma) / 0.5450;
  if (penumbral <= 0) return null;
  if (umbral >= 1) return eclipse(false, 'total', umbral);
  if (umbral > 0) return eclipse(false, 'partial', umbral);
  return eclipse(false, 'penumbral', penumbral);
}

/**
 * Julian Day, in UTC, of the Moon's passage through the ascending node of its
 * orbit (integer `k`) or the descending one (`k` + 0.5), Meeus ch. 51.
 */
function nodeJD(k) {
  var T = k / 1342.23;
  var T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  var D = 183.6380 + 331.73735682 * k + 0.0014852 * T2 + 0.00000209 * T3 - 0.000000010 * T4;
  var M = 17.4006 + 26.82037250 * k + 0.0001186 * T2 + 0.00000006 * T3;
  var Mp = 38.3776 + 355.52747313 * k + 0.0123499 * T2 + 0.000014627 * T3 - 0.000000069 * T4;
  var Omega = 123.9767 - 1.44098956 * k + 0.0020608 * T2 + 0.00000214 * T3 - 0.000000016 * T4;
  var V = 299.75 + 132.85 * T - 0.009173 * T2;
  var P = Omega + 272.75 - 2.3 * T;
  var E = 1.0 - 0.002516 * T - 0.0000074 * T2;

  var jde = 2451565.1619 + 27.212220817 * k
          + 0.0002762 * T2 + 0.000000021 * T3 - 0.000000000088 * T4
          - 0.4721 * sind(Mp)
          - 0.1649 * sind(2 * D)
          - 0.0868 * sind(2 * D - Mp)
          + 0.0084 * sind(2 * D + Mp)
          - 0.0083 * E * sind(2 * D - M)
          - 0.0039 * E * sind(2 * D - M - Mp)
          + 0.0034 * sind(2 * Mp)
          - 0.0031 * sind(2 * D - 2 * Mp)
          + 0.0030 * E * sind(2 * D + M)
          + 0.0028 * E * sind(M - Mp)
          + 0.0026 * E * sind(M)
          + 0.0025 * sind(4 * D)
          + 0.0024 * sind(D)
          + 0.0022 * E * sind(M + Mp)
          + 0.0017 * sind(Omega)
          + 0.0014 * sind(4 * D - Mp)
          + 0.0005 * E * sind(2 * D + M - Mp)
          + 0.0004 * E * sind(2 * D - M + Mp)
          - 0.0003 * E * sind(2 * D - 2 * M)
          + 0.0003 * E * sind(4 * D - M)
          + 0.0003 * sind(V)
          + 0.0003 * sind(P);
  return jde - DELTA_T_DAYS;
}

/**
 * The Moon's passages through the nodes of its orbit, where it crosses the
 * plane of the Earth's orbit, between `fromEpoch` and `toEpoch`, in
 * chronological order: { ascending: bool, time: epochSeconds }. Ascending is
 * northward. Eclipses only happen at a new or full moon close to one.
 */
function nodes(fromEpoch, toEpoch) {
  var d = new Date(fromEpoch * 1000);
  var yearDec = d.getUTCFullYear() + d.getUTCMonth() / 12.0;
  var from = jdFromEpoch(fromEpoch), to = jdFromEpoch(toEpoch);
  var out = [];
  // A draconic month is 27.2 days; start one passage early to catch the first.
  for (var k = Math.floor((yearDec - 2000.05) * 13.4223) - 1.0; ; k += 0.5) {
    var t = nodeJD(k);
    if (t > to) break;
    if (t > from) {
      out.push({ ascending: k === Math.floor(k), time: epochFromJd(t) });
    }
  }
  return out;
}

/**
 * The first eclipse whose greatest phase comes after `fromEpoch` and for which
 * `accept(eclipse)` is true, as eclipseAt() gives it, or null if none comes
 * within five years.
 */
function nextEclipse(fromEpoch, accept) {
  var kBase = Math.floor(kEstimate(fromEpoch)) - 1.0;
  for (var k = kBase; k < kBase + 62.0; k += 0.5) {
    var eclipse = eclipseAt(k);
    if (eclipse && eclipse.time > fromEpoch && accept(eclipse)) return eclipse;
  }
  return null;
}

// Mean length of the synodic month, in days.
var SYNODIC_MONTH = 29.530588861;

/**
 * Position of the Moon in the lunation at `nowEpoch`: 0 new, 0.25 first
 * quarter, 0.5 full, 0.75 last quarter.
 *
 * `nowEpoch` is bracketed between two principal phases, starting a quarter
 * before the mean one: the true phases stay within a day of the mean ones,
 * well inside a quarter, so a couple of steps either way always find it. The
 * position runs linearly between the two, so it matches each phase at its
 * instant.
 */
function lunationFraction(nowEpoch) {
  var jd = jdFromEpoch(nowEpoch);
  var q = Math.floor((jd - 2451550.09766) / SYNODIC_MONTH * 4.0) / 4.0 - 0.25;
  var ta = phaseJD(q), tb = phaseJD(q + 0.25);
  for (var i = 0; i < 3 && tb <= jd; i++) {
    q += 0.25; ta = tb; tb = phaseJD(q + 0.25);
  }
  for (var j = 0; j < 3 && jd < ta; j++) {
    q -= 0.25; tb = ta; ta = phaseJD(q);
  }
  var fraction = (q - Math.floor(q)) + 0.25 * (jd - ta) / (tb - ta);
  if (fraction < 0.0) fraction += 1.0;
  if (fraction >= 1.0) fraction -= 1.0;
  return fraction;
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

// Longitude terms of Meeus table 47.A: [D, M, M', F, coefficient in 1e-6
// degree]. Rows whose longitude term is zero are left out.
var LONGITUDE_TERMS = [
  [0, 0, 1, 0, 6288774], [2, 0, -1, 0, 1274027], [2, 0, 0, 0, 658314],
  [0, 0, 2, 0, 213618], [0, 1, 0, 0, -185116], [0, 0, 0, 2, -114332],
  [2, 0, -2, 0, 58793], [2, -1, -1, 0, 57066], [2, 0, 1, 0, 53322],
  [2, -1, 0, 0, 45758], [0, 1, -1, 0, -40923], [1, 0, 0, 0, -34720],
  [0, 1, 1, 0, -30383], [2, 0, 0, -2, 15327], [0, 0, 1, 2, -12528],
  [0, 0, 1, -2, 10980], [4, 0, -1, 0, 10675], [0, 0, 3, 0, 10034],
  [4, 0, -2, 0, 8548], [2, 1, -1, 0, -7888], [2, 1, 0, 0, -6766],
  [1, 0, -1, 0, -5163], [1, 1, 0, 0, 4987], [2, -1, 1, 0, 4036],
  [2, 0, 2, 0, 3994], [4, 0, 0, 0, 3861], [2, 0, -3, 0, 3665],
  [0, 1, -2, 0, -2689], [2, 0, -1, 2, -2602], [2, -1, -2, 0, 2390],
  [1, 0, 1, 0, -2348], [2, -2, 0, 0, 2236], [0, 1, 2, 0, -2120],
  [0, 2, 0, 0, -2069], [2, -2, -1, 0, 2048], [2, 0, 1, -2, -1773],
  [2, 0, 0, 2, -1595], [4, -1, -1, 0, 1215], [0, 0, 2, 2, -1110],
  [3, 0, -1, 0, -892], [2, 1, 1, 0, -810], [4, -1, -2, 0, 759],
  [0, 2, -1, 0, -713], [2, 2, -1, 0, -700], [2, 1, -2, 0, 691],
  [2, -1, 0, -2, 596], [4, 0, 1, 0, 549], [0, 0, 4, 0, 537],
  [4, -1, 0, 0, 520], [1, 0, -2, 0, -487], [2, 1, 0, -2, -399],
  [0, 0, 2, -2, -381], [1, 1, 1, 0, 351], [3, 0, -2, 0, -340],
  [4, 0, -3, 0, 330], [2, -1, 2, 0, 327], [0, 2, 1, 0, -323],
  [1, 1, -1, 0, 299], [2, 0, 3, 0, 294],
];

// Latitude terms of Meeus table 47.B: [D, M, M', F, coefficient in 1e-6 degree].
var LATITUDE_TERMS = [
  [0, 0, 0, 1, 5128122], [0, 0, 1, 1, 280602], [0, 0, 1, -1, 277693],
  [2, 0, 0, -1, 173237], [2, 0, -1, 1, 55413], [2, 0, -1, -1, 46271],
  [2, 0, 0, 1, 32573], [0, 0, 2, 1, 17198], [2, 0, 1, -1, 9266],
  [0, 0, 2, -1, 8822], [2, -1, 0, -1, 8216], [2, 0, -2, -1, 4324],
  [2, 0, 1, 1, 4200], [2, 1, 0, -1, -3359], [2, -1, -1, 1, 2463],
  [2, -1, 0, 1, 2211], [2, -1, -1, -1, 2065], [0, 1, -1, -1, -1870],
  [4, 0, -1, -1, 1828], [0, 1, 0, 1, -1794], [0, 0, 0, 3, -1749],
  [0, 1, -1, 1, -1565], [1, 0, 0, 1, -1491], [0, 1, 1, 1, -1475],
  [0, 1, 1, -1, -1410], [0, 1, 0, -1, -1344], [1, 0, 0, -1, -1335],
  [0, 0, 3, 1, 1107], [4, 0, 0, -1, 1021], [4, 0, -1, 1, 833],
  [0, 0, 1, -3, 777], [4, 0, -2, 1, 671], [2, 0, 0, -3, 607],
  [2, 0, 2, -1, 596], [2, -1, 1, -1, 491], [2, 0, -2, 1, -451],
  [0, 0, 3, -1, 439], [2, 0, 2, 1, 422], [2, 0, -3, -1, 421],
  [2, 1, -1, 1, -366], [2, 1, 0, 1, -351], [4, 0, 0, 1, 331],
  [2, -1, 1, 1, 315], [2, -2, 0, -1, 302], [0, 0, 1, 3, -283],
  [2, 1, 1, -1, -229], [1, 1, 0, -1, 223], [1, 1, 0, 1, 223],
  [0, 1, -2, -1, -220], [2, 1, -1, -1, -220], [1, 0, 1, 1, -185],
  [2, -1, -2, -1, 181], [0, 1, 2, 1, -177], [4, 0, -2, -1, 176],
  [4, -1, -1, -1, 166], [1, 0, 1, -1, -164], [4, 0, 1, -1, 132],
  [1, 0, -1, -1, -119], [4, -1, 0, -1, 115], [2, -2, 0, 1, 107],
];

/** Sum of a Meeus table 47 series, its terms in M shrunk by the eccentricity E. */
function seriesSum(terms, D, M, Mp, F, E) {
  var sum = 0.0;
  for (var i = 0; i < terms.length; i++) {
    var t = terms[i];
    var m = Math.abs(t[1]);
    sum += t[4] * (m === 1 ? E : m === 2 ? E * E : 1.0)
         * sind(t[0] * D + t[1] * M + t[2] * Mp + t[3] * F);
  }
  return sum;
}

/**
 * Apparent geocentric position of the Moon at `epoch` (seconds), Meeus ch. 47,
 * in degrees: { longitude, latitude } on the ecliptic, the longitude with the
 * nutation, and { rightAscension, declination } on the sky. Good to about 10"
 * in longitude and 4" in latitude.
 */
function moonPosition(epoch) {
  var T = (jdFromEpoch(epoch) + DELTA_T_DAYS - 2451545.0) / 36525.0;  // dynamical time
  var T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  var Lp = 218.3164477 + 481267.88123421 * T - 0.0015786 * T2 + T3 / 538841.0 - T4 / 65194000.0;
  var D = 297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868.0 - T4 / 113065000.0;
  var M = 357.5291092 + 35999.0502909 * T - 0.0001536 * T2 + T3 / 24490000.0;
  var Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699.0 - T4 / 14712000.0;
  var F = 93.2720950 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000.0 + T4 / 863310000.0;
  var E = 1.0 - 0.002516 * T - 0.0000074 * T2;
  // Venus, Jupiter and the flattening of the Earth.
  var A1 = 119.75 + 131.849 * T;
  var A2 = 53.09 + 479264.290 * T;
  var A3 = 313.45 + 481266.484 * T;

  var sumL = seriesSum(LONGITUDE_TERMS, D, M, Mp, F, E)
           + 3958 * sind(A1) + 1962 * sind(Lp - F) + 318 * sind(A2);
  var sumB = seriesSum(LATITUDE_TERMS, D, M, Mp, F, E)
           - 2235 * sind(Lp) + 382 * sind(A3) + 175 * sind(A1 - F) + 175 * sind(A1 + F)
           + 127 * sind(Lp - Mp) - 115 * sind(Lp + Mp);

  // Nutation and obliquity, to the 0.5" of Meeus ch. 22's short series.
  var omega = 125.04452 - 1934.136261 * T;
  var Ls = 280.4665 + 36000.7698 * T;
  var Lm = 218.3165 + 481267.8813 * T;
  var dPsi = (-17.20 * sind(omega) - 1.32 * sind(2 * Ls) - 0.23 * sind(2 * Lm)
              + 0.21 * sind(2 * omega)) / 3600.0;
  var dEps = (9.20 * cosd(omega) + 0.57 * cosd(2 * Ls) + 0.10 * cosd(2 * Lm)
              - 0.09 * cosd(2 * omega)) / 3600.0;
  var eps = 23.4392911 - (46.8150 * T + 0.00059 * T2 - 0.001813 * T3) / 3600.0 + dEps;

  var lambda = Lp + sumL / 1e6 + dPsi;
  var beta = sumB / 1e6;
  var dec = Math.asin(sind(beta) * cosd(eps) + cosd(beta) * sind(eps) * sind(lambda)) / DEG;
  var ra = Math.atan2(sind(lambda) * cosd(eps) - Math.tan(beta * DEG) * sind(eps),
                      cosd(lambda)) / DEG;
  return {
    longitude: ((lambda % 360) + 360) % 360,
    latitude: beta,
    rightAscension: (ra + 360) % 360,
    declination: dec,
  };
}

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
 * Whether the Moon is coming closer to the Earth at `epoch`, from the distance
 * a minute later. Both are compared unrounded: next to an apsis the Moon moves
 * by metres in a minute, far below the km rounding.
 */
function isApproaching(epoch) {
  return distanceKm(epoch + 60) < distanceKm(epoch);
}

/**
 * Turns of `value(epoch)` between `fromEpoch` and `toEpoch`, in chronological
 * order: { minimum: bool, time: epochSeconds }.
 *
 * The value is sampled every six hours, on a grid fixed to the epoch so that
 * any call finds the same turns; each one is then narrowed down by golden-
 * section search and rounded to the minute, so its time is stable too. The
 * grid starts a step early and ends a step late: a turn is only seen from the
 * sample after the one closest to it.
 */
function turns(value, fromEpoch, toEpoch) {
  var STEP = 6 * 3600;
  var R = 0.6180339887;  // inverse golden ratio
  var out = [];
  var t0 = Math.floor(fromEpoch / STEP) * STEP - STEP, d0 = value(t0);
  var t1 = t0 + STEP, d1 = value(t1);
  for (var t2 = t1 + STEP; t2 <= toEpoch + 2 * STEP; t2 += STEP) {
    var d2 = value(t2);
    var minimum = d1 < d0 && d1 <= d2;
    if (minimum || (d1 > d0 && d1 >= d2)) {
      // Search for the minimum of sign * value; each round keeps one probe.
      var sign = minimum ? 1 : -1;
      var a = t0, b = t2;
      var c1 = b - (b - a) * R, f1 = sign * value(c1);
      var c2 = a + (b - a) * R, f2 = sign * value(c2);
      while (b - a > 30) {
        if (f1 < f2) {
          b = c2; c2 = c1; f2 = f1;
          c1 = b - (b - a) * R; f1 = sign * value(c1);
        } else {
          a = c1; c1 = c2; f1 = f2;
          c2 = a + (b - a) * R; f2 = sign * value(c2);
        }
      }
      var time = Math.round((a + b) / 120) * 60;
      if (time > fromEpoch && time <= toEpoch) out.push({ minimum: minimum, time: time });
    }
    t0 = t1; d0 = d1;
    t1 = t2; d1 = d2;
  }
  return out;
}

/**
 * Perigees and apogees between `fromEpoch` and `toEpoch`, in chronological
 * order: { perigee: bool, time: epochSeconds, distance: km }.
 */
function apsides(fromEpoch, toEpoch) {
  return turns(distanceKm, fromEpoch, toEpoch).map(function (t) {
    // Rounded to the km like the watch, so both class it the same.
    return { perigee: t.minimum, time: t.time, distance: Math.round(distanceKm(t.time)) };
  });
}

/** The Moon's declination at `epoch`, in degrees, north positive. */
function declination(epoch) {
  return moonPosition(epoch).declination;
}

/**
 * The turns of the Moon's declination between `fromEpoch` and `toEpoch`, in
 * chronological order: { northern: bool, time: epochSeconds, declination }.
 * At a northern turn the Moon is as far north as it goes this month, at its
 * highest seen from the northern hemisphere and its lowest from the southern
 * one, and the other way round at a southern turn.
 */
function declinationTurns(fromEpoch, toEpoch) {
  return turns(declination, fromEpoch, toEpoch).map(function (t) {
    return { northern: !t.minimum, time: t.time, declination: declination(t.time) };
  });
}

/**
 * Whether the Moon's declination grows northward at `epoch`, from its value a
 * minute later. Seen from the northern hemisphere the Moon then passes higher
 * in the sky each day; seen from the southern one, lower.
 */
function isDeclinationNorthing(epoch) {
  return declination(epoch + 60) > declination(epoch);
}

/**
 * Return the next `count` principal phase events strictly after `nowEpoch`,
 * in chronological order: { type: 0..3, time: epochSeconds, distance: km,
 * supermoon: bool, eclipse: eclipseAt() of a new or full moon, else null }.
 */
function upcoming(nowEpoch, count) {
  var kBase = Math.floor(kEstimate(nowEpoch)) - 1.0;
  var events = [];
  for (var k = kBase; events.length < count && k < kBase + 40.0; k += 1.0) {
    for (var q = 0; q < 4 && events.length < count; q++) {
      var kk = k + q * 0.25;
      // Compared once rounded to the second, as the caller sees it: asked from
      // an event's own time, the search must not return that event.
      var time = epochFromJd(phaseJD(kk));
      if (time > nowEpoch) {
        // Rounded to the km like the watch, so both name the same supermoons.
        var distance = Math.round(distanceKm(time));
        events.push({
          type: q,
          time: time,
          distance: distance,
          supermoon: q === 2 && distance < SUPERMOON_KM,
          eclipse: q === 0 || q === 2 ? eclipseAt(kk) : null,
        });
      }
    }
  }
  return events;
}

module.exports = {
  moonPosition: moonPosition,
  declinationTurns: declinationTurns,
  isDeclinationNorthing: isDeclinationNorthing,
  lunationFraction: lunationFraction,
  isApproaching: isApproaching,
  upcoming: upcoming,
  apsides: apsides,
  nodes: nodes,
  distanceKm: distanceKm,
  eclipseAt: eclipseAt,
  nextEclipse: nextEclipse,
  phaseJD: phaseJD,
  nodeJD: nodeJD,
};

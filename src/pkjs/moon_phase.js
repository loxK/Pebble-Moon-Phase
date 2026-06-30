/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon phase engine (JS port of src/c/moon_phase.c).
 *
 * Computes the dates of the upcoming principal Moon phases using Meeus'
 * algorithm (Astronomical Algorithms, ch. 49) with the principal periodic
 * correction terms. Kept in sync with the C implementation that drives the
 * watch UI; this copy is only used to push timeline pins from the phone.
 */

var JD_UNIX_EPOCH = 2440587.5;
var SECONDS_PER_DAY = 86400.0;
var DEG = Math.PI / 180.0;

function sind(d) { return Math.sin(d * DEG); }
function cosd(d) { return Math.cos(d * DEG); }

function jdFromEpoch(epoch) { return epoch / SECONDS_PER_DAY + JD_UNIX_EPOCH; }
function epochFromJd(jd) { return Math.round((jd - JD_UNIX_EPOCH) * SECONDS_PER_DAY); }

/**
 * Julian Ephemeris Day of the phase identified by the continuous lunation
 * value `k` (integer => new moon, +0.25 first quarter, +0.5 full, +0.75 last).
 */
function phaseJDE(k) {
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
    corr =
        -0.40720 * sind(Mp)
      + 0.17241 * E * sind(M)
      + 0.01608 * sind(2 * Mp)
      + 0.01039 * sind(2 * F)
      + 0.00739 * E * sind(Mp - M)
      - 0.00514 * E * sind(Mp + M)
      + 0.00208 * E2 * sind(2 * M)
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

  return jde + corr;
}

function kEstimate(epoch) {
  var d = new Date(epoch * 1000);
  var yearDec = d.getUTCFullYear() + (d.getUTCMonth() / 12.0);
  return (yearDec - 2000.0) * 12.3685;
}

/**
 * Return the next `count` principal phase events strictly after `nowEpoch`,
 * in chronological order. Each element is { type: 0..3, time: epochSeconds }.
 */
function upcoming(nowEpoch, count) {
  var jd = jdFromEpoch(nowEpoch);
  var kBase = Math.floor(kEstimate(nowEpoch)) - 1.0;
  var events = [];
  for (var k = kBase; events.length < count && k < kBase + 40.0; k += 1.0) {
    for (var q = 0; q < 4 && events.length < count; q++) {
      var kk = k + q * 0.25;
      var t = phaseJDE(kk);
      if (t > jd) {
        events.push({ type: q, time: epochFromJd(t) });
      }
    }
  }
  return events;
}

module.exports = { upcoming: upcoming };

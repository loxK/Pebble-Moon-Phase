/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Checks of the phone-side engine against published references. Plain Node,
 * no dependency: `node tests/engine.test.js` exits non-zero on a failure.
 */

var assert = require('assert');
var path = require('path');
var Moon = require(path.join(__dirname, '..', 'src', 'pkjs', 'moon_phase.js'));
var Distance = require(path.join(__dirname, '..', 'src', 'pkjs', 'distance.js'));

var DELTA_T_DAYS = 69 / 86400;
var failures = 0;

function test(name, body) {
  try {
    body();
    console.log('ok   ' + name);
  } catch (e) {
    failures++;
    console.log('FAIL ' + name + '\n     ' + e.message);
  }
}

function utcDate(jd) {
  return new Date((jd - 2440587.5) * 86400000).toISOString().slice(0, 10);
}

test('new moon of Meeus example 49.a (JDE 2443192.65118)', function () {
  var jde = Moon.phaseJD(-283) + DELTA_T_DAYS;
  assert.ok(Math.abs(jde - 2443192.65118) * 1440 < 1, 'JDE ' + jde.toFixed(5));
});

test('ascending node of Meeus example 51.a (JDE 2446938.76803)', function () {
  var jde = Moon.nodeJD(-170) + DELTA_T_DAYS;
  assert.ok(Math.abs(jde - 2446938.76803) * 1440 < 1, 'JDE ' + jde.toFixed(5));
});

// The perturbations of the lunar orbit move each passage by up to about half
// a day, so single intervals spread from about 12.4 to 14.7 days; over twenty
// years they average half a draconic month, 13.606 days.
test('nodes alternate, half a draconic month apart on average', function () {
  var from = Date.UTC(2020, 0, 1) / 1000;
  var list = Moon.nodes(from, from + 20 * 365.25 * 86400);
  var total = 0;
  for (var i = 1; i < list.length; i++) {
    assert.notStrictEqual(list[i].ascending, list[i - 1].ascending);
    var days = (list[i].time - list[i - 1].time) / 86400;
    assert.ok(days > 12 && days < 15, days.toFixed(2) + ' days between passages');
    total += days;
  }
  var mean = total / (list.length - 1);
  assert.ok(Math.abs(mean - 13.606) < 0.01, 'mean interval ' + mean.toFixed(4));
});

// Eclipses of 2025-2028 and January 2029, from the NASA eclipse catalogues
// (Espenak), dated in UTC. The penumbral lunar eclipse of 2027-07-18 is left
// out: its penumbral magnitude is about 0.001, below the resolution of the
// method, which finds the Moon just outside the penumbra.
var ECLIPSES = {
  '2025-03-14': 'lunar total', '2025-03-29': 'solar partial',
  '2025-09-07': 'lunar total', '2025-09-21': 'solar partial',
  '2026-02-17': 'solar annular', '2026-03-03': 'lunar total',
  '2026-08-12': 'solar total', '2026-08-28': 'lunar partial',
  '2027-02-06': 'solar annular', '2027-02-20': 'lunar penumbral',
  '2027-08-02': 'solar total', '2027-08-17': 'lunar penumbral',
  '2028-01-12': 'lunar partial', '2028-01-26': 'solar annular',
  '2028-07-06': 'lunar partial', '2028-07-22': 'solar total',
  '2028-12-31': 'lunar total', '2029-01-14': 'solar partial',
};

function computedEclipses() {
  var out = {};
  for (var k = 300; k <= 360; k += 0.5) {
    var e = Moon.eclipseAt(k);
    if (!e) continue;
    var jd = Moon.phaseJD(k);
    var date = utcDate(jd);
    if (date < '2025-01-01' || date > '2029-01-31') continue;
    out[date] = { kind: (e.solar ? 'solar ' : 'lunar ') + e.type, jd: jd };
  }
  return out;
}

test('eclipses 2025 to January 2029 match the published list', function () {
  var computed = computedEclipses();
  Object.keys(ECLIPSES).forEach(function (date) {
    assert.ok(computed[date], 'missed ' + date + ' ' + ECLIPSES[date]);
    assert.strictEqual(computed[date].kind, ECLIPSES[date], date);
  });
  Object.keys(computed).forEach(function (date) {
    assert.ok(ECLIPSES[date], 'unexpected ' + date + ' ' + computed[date].kind);
  });
});

test('every eclipse falls within a day and a half of a node', function () {
  var computed = computedEclipses();
  var from = Date.UTC(2025, 0, 1) / 1000;
  var list = Moon.nodes(from - 30 * 86400, from + 4.2 * 365 * 86400);
  Object.keys(computed).forEach(function (date) {
    var t = (computed[date].jd - 2440587.5) * 86400;
    var nearest = list.reduce(function (best, n) {
      return Math.abs(n.time - t) < Math.abs(best.time - t) ? n : best;
    });
    var days = Math.abs(nearest.time - t) / 86400;
    assert.ok(days < 1.5, date + ' is ' + days.toFixed(1) + ' days from a node');
  });
});

test('upcoming() carries the eclipse of new and full moons only', function () {
  var events = Moon.upcoming(Date.UTC(2026, 7, 1) / 1000, 8);
  events.forEach(function (ev) {
    if (ev.type === 1 || ev.type === 3) assert.strictEqual(ev.eclipse, null);
  });
  var total = events.filter(function (ev) { return ev.eclipse && ev.eclipse.solar; })[0];
  assert.ok(total, 'no solar eclipse in August 2026');
  assert.strictEqual(total.eclipse.type, 'total');
});

test('distance classes change at their limits', function () {
  var cases = [[356400, 0], [359999, 0], [360000, 1], [374999, 1], [375000, 2],
               [393999, 2], [394000, 3], [403999, 3], [404000, 4], [406700, 4]];
  cases.forEach(function (c) {
    assert.strictEqual(Distance.distanceClass(c[0]), c[1], c[0] + ' km');
  });
  assert.strictEqual(Distance.distanceLabel(404000, 'fr'), 'Très lointaine');
  assert.strictEqual(Distance.distanceLabel(404000, 'xx'), 'Very far');
});

if (failures) {
  console.log(failures + ' failed');
  process.exit(1);
}

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

test('upcoming() from a phase\'s own time returns the phase after it', function () {
  var events = Moon.upcoming(Date.UTC(2026, 0, 1) / 1000, 150);
  for (var i = 0; i + 1 < events.length; i++) {
    var next = Moon.upcoming(events[i].time, 1)[0];
    assert.strictEqual(next.time, events[i + 1].time, new Date(events[i].time * 1000).toISOString());
  }
});

test('nextEclipse() skips what the filter rejects', function () {
  function lunarUmbral(e) { return !e.solar && e.type !== 'penumbral'; }
  // After the partial eclipse of 2026-08-28, 2027 only has penumbral ones.
  var next = Moon.nextEclipse(Date.UTC(2026, 8, 1) / 1000, lunarUmbral);
  assert.strictEqual(new Date(next.time * 1000).toISOString().slice(0, 10), '2028-01-12');
  assert.strictEqual(next.type, 'partial');
  var any = Moon.nextEclipse(Date.UTC(2026, 8, 1) / 1000, function () { return true; });
  assert.strictEqual(new Date(any.time * 1000).toISOString().slice(0, 10), '2027-02-06');
  assert.ok(any.solar);
});

// Greatest eclipse in UTC, from the NASA eclipse pages, to the minute.
var GREATEST = {
  '2025-03-14T06:59': 'lunar', '2025-09-07T18:12': 'lunar',
  '2026-03-03T11:34': 'lunar', '2026-08-12T17:46': 'solar',
  '2026-08-28T04:13': 'lunar', '2027-08-02T10:07': 'solar',
  '2028-01-12T04:13': 'lunar', '2028-12-31T16:51': 'lunar',
};

test('greatest eclipse within two minutes of the published time', function () {
  Object.keys(GREATEST).forEach(function (iso) {
    var expected = Date.parse(iso + ':00Z') / 1000;
    var e = Moon.nextEclipse(expected - 3 * 86400, function () { return true; });
    assert.strictEqual(e.solar, GREATEST[iso] === 'solar', iso);
    var minutes = (e.time - expected) / 60;
    assert.ok(Math.abs(minutes) < 2, iso + ' computed ' + new Date(e.time * 1000).toISOString());
  });
});

test('Moon position of Meeus example 47.a (1992-04-12 0h TD)', function () {
  var p = Moon.moonPosition((2448724.5 - 2440587.5) * 86400 - 69);
  assert.ok(Math.abs(p.longitude - 133.167265) < 0.0001, 'longitude ' + p.longitude);
  assert.ok(Math.abs(p.latitude - -3.229126) < 0.0001, 'latitude ' + p.latitude);
  assert.ok(Math.abs(p.rightAscension - 134.688470) < 0.0001, 'right ascension ' + p.rightAscension);
  assert.ok(Math.abs(p.declination - 13.768368) < 0.0001, 'declination ' + p.declination);
});

test('declination turns alternate and reach the lunar standstills', function () {
  function extreme(from, to, pick) {
    return Moon.declinationTurns(Date.UTC(from, 0, 1) / 1000, Date.UTC(to, 0, 1) / 1000)
      .reduce(function (a, t) { return pick(Math.abs(t.declination), Math.abs(a.declination)) ? t : a; });
  }
  // Major standstill: 23.44 + 5.15 degrees, the farthest south on 2025-03-22.
  var major = extreme(2024, 2026, function (a, b) { return a > b; });
  assert.ok(Math.abs(major.declination - -28.73) < 0.02, 'major ' + major.declination);
  assert.strictEqual(new Date(major.time * 1000).toISOString().slice(0, 10), '2025-03-22');
  // Minor standstill: 23.44 - 5.30 degrees, the inclination at its widest.
  var minor = extreme(2033, 2036, function (a, b) { return a < b; });
  assert.ok(Math.abs(Math.abs(minor.declination) - 18.14) < 0.02, 'minor ' + minor.declination);

  var turns = Moon.declinationTurns(Date.UTC(2026, 0, 1) / 1000, Date.UTC(2027, 0, 1) / 1000);
  turns.forEach(function (t, i) {
    if (i > 0) {
      assert.notStrictEqual(t.northern, turns[i - 1].northern, 'alternation ' + t.time);
      var days = (t.time - turns[i - 1].time) / 86400;
      assert.ok(days > 12 && days < 15.5, 'half a tropical month: ' + days);
      var mid = (t.time + turns[i - 1].time) / 2;
      assert.strictEqual(Moon.isDeclinationNorthing(mid), t.northern, 'direction ' + mid);
    }
  });
});

// The watch engine, src/c/moon_phase.c, built on the host against
// tests/host/pebble.h, must find the same events as this one. Its own sine and
// cosine are good to 3e-8, which can move a time by a second, an apsis or a
// declination turn rounded to the minute by one minute, a distance rounded to
// the km by one km.
function dumpCEngine(from, to) {
  var childProcess = require('child_process');
  var fs = require('fs');
  var os = require('os');
  var root = path.join(__dirname, '..');
  var dir = fs.mkdtempSync(path.join(os.tmpdir(), 'moon-engine-'));
  var bin = path.join(dir, 'engine_dump');
  try {
    childProcess.execFileSync('cc', [
      '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1',
      '-I', path.join(__dirname, 'host'), '-I', path.join(root, 'src', 'c'),
      '-o', bin, path.join(__dirname, 'host', 'engine_dump.c'),
      path.join(root, 'src', 'c', 'moon_phase.c'), '-lm',
    ], { stdio: ['ignore', 'ignore', 'inherit'] });
    return childProcess.execFileSync(bin, ['' + from, '' + to],
                                     { encoding: 'utf8', maxBuffer: 64 << 20 });
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

function hasCompiler() {
  try {
    require('child_process').execFileSync('cc', ['--version'], { stdio: 'ignore' });
    return true;
  } catch (e) {
    return false;
  }
}

function jsEngineLines(from, to) {
  function lunarUmbral(e) { return !e.solar && e.type !== 'penumbral'; }
  var lines = [];
  // upcoming() looks 40 lunations ahead at most, so the range goes in steps.
  for (var start = from, done = false; !done;) {
    var batch = Moon.upcoming(start, 100);
    for (var i = 0; i < batch.length; i++) {
      var ev = batch[i], e = ev.eclipse;
      if (ev.time > to) { done = true; break; }
      lines.push(['phase', ev.time, ev.type, ev.distance, e ? e.type : 'none', e && e.solar ? 1 : 0,
                  e ? e.time : 0, e ? e.magnitude || 0 : 0]);
    }
    start = batch[batch.length - 1].time;
  }
  Moon.nodes(from, to).forEach(function (n) { lines.push(['node', n.time, n.ascending ? 1 : 0]); });
  Moon.apsides(from, to).forEach(function (a) {
    lines.push(['apsis', a.time, a.perigee ? 1 : 0, a.distance]);
  });
  Moon.declinationTurns(from, to).forEach(function (t) {
    lines.push(['turn', t.time, t.northern ? 1 : 0, t.declination]);
  });
  for (var d = from; d <= to; d += 86400) {
    lines.push(['state', d, Moon.lunationFraction(d), Moon.isApproaching(d) ? 1 : 0,
                Math.round(Moon.distanceKm(d))]);
    var p = Moon.moonPosition(d);
    lines.push(['position', d, p.longitude, p.latitude, p.rightAscension, p.declination,
                Moon.isDeclinationNorthing(d) ? 1 : 0]);
  }
  for (var t = from; t <= to; t += 30 * 86400) {
    var next = Moon.nextEclipse(t, lunarUmbral);
    if (next) lines.push(['next', t, next.time, next.type, next.magnitude]);
  }
  return lines;
}

test('the watch engine finds the same events, 2000 to 2050', function () {
  if (!hasCompiler()) {
    console.log('     (no C compiler, skipped)');
    return;
  }
  var from = Date.UTC(2000, 0, 1) / 1000, to = Date.UTC(2050, 0, 1) / 1000;
  var c = dumpCEngine(from, to).trim().split('\n').map(function (l) { return l.split(' '); });
  var js = jsEngineLines(from, to);
  // Asked from a phase's own time, the search returns the phase after it.
  var phases = c.filter(function (l) { return l[0] === 'phase'; });
  c.filter(function (l) { return l[0] === 'follow'; }).forEach(function (l, i) {
    if (i + 1 < phases.length) {
      assert.strictEqual(l[2], phases[i + 1][1], 'C follow ' + new Date(l[1] * 1000).toISOString());
    }
  });
  ['phase', 'node', 'apsis', 'turn', 'next', 'state', 'position'].forEach(function (kind) {
    var a = c.filter(function (l) { return l[0] === kind; });
    var b = js.filter(function (l) { return l[0] === kind; });
    assert.strictEqual(a.length, b.length, kind + ': ' + a.length + ' in C, ' + b.length + ' in JS');
    a.forEach(function (l, i) {
      var r = b[i];
      var where = kind + ' ' + new Date(r[1] * 1000).toISOString();
      function near(field, tolerance) {
        var d = Math.abs(Number(l[field]) - Number(r[field]));
        assert.ok(d <= tolerance, where + ': field ' + field + ' C ' + l[field] + ', JS ' + r[field]);
      }
      function nearAngle(field, tolerance) {
        var d = Math.abs(Number(l[field]) - Number(r[field])) % 360;
        assert.ok(Math.min(d, 360 - d) <= tolerance,
                  where + ': field ' + field + ' C ' + l[field] + ', JS ' + r[field]);
      }
      function same(field) {
        assert.strictEqual(String(l[field]), String(r[field]), where + ': field ' + field);
      }
      if (kind === 'phase') {
        near(1, 2); same(2); near(3, 1); same(4); same(5); near(6, 2); near(7, 0.001);
      } else if (kind === 'node') {
        near(1, 2); same(2);
      } else if (kind === 'apsis') {
        near(1, 60); same(2); near(3, 1);
      } else if (kind === 'turn') {
        near(1, 60); same(2); near(3, 0.001);
      } else if (kind === 'position') {
        same(1); nearAngle(2, 0.00001); near(3, 0.00001); nearAngle(4, 0.00001); near(5, 0.00001);
        same(6);
      } else if (kind === 'state') {
        same(1); near(2, 0.00001); same(3); near(4, 1);
      } else {
        same(1); near(2, 2); same(3); near(4, 0.001);
      }
    });
  });
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

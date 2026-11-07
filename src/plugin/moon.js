/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- plugin, run by the phone app for any watchface or app that
 * subscribes to its sources with Pebble.subscribeToSource:
 *
 *  - moon/now: the Moon at this moment, waxing or waning, how far it is and
 *    which way it goes, whether it passes higher or lower each day, and the
 *    next phase.
 *  - moon/event: what comes in the next 30 days, soonest first: phases,
 *    perigees and apogees, node passages, highest and lowest passes, and the
 *    next lunar eclipse, however far.
 *
 * Texts are in the language of the phone. The hemisphere and the clock style
 * are those the app last stored, since a plugin shares its app's localStorage.
 * The wscript bundles this file with the modules it requires into plugin.js.
 */

var Moon = require('../pkjs/moon_phase');
var Distance = require('../pkjs/distance');
var Store = require('../pkjs/store');
var Text = require('../pkjs/moon_text');

var DAY = 86400;
var EVENT_WINDOW = 30 * DAY;
// The longest lunations last 29.8 days.
var MAX_AGE_DAYS = 30;

// Kinds of the events, for subscribers to tell them apart whatever the language.
var PHASE_KINDS = ['new_moon', 'first_quarter', 'full_moon', 'last_quarter'];

function shortText(text) {
  return { shortText: { text: text } };
}

function numeric(value, unit, min, max) {
  var shape = { value: value };
  if (unit) shape.unit = unit;
  if (min !== undefined) shape.min = min;
  if (max !== undefined) shape.max = max;
  return shape;
}

/** A moment as a timestamp, its time of day and its full date. */
function when(time, withYear) {
  return {
    timestamp: { value: time },
    shortText: { text: Text.clockTime(time) },
    longText: { text: Text.longDate(time, withYear) + ' ' + Text.clockTime(time) },
  };
}

function distance(km, lang) {
  return {
    numericValue: numeric(km, 'km', Moon.DISTANCE_MIN_KM, Moon.DISTANCE_MAX_KM),
    shortText: { text: Distance.distanceLabel(km, lang) },
  };
}

/** Time of the last new moon at or before `now`. */
function lastNewMoon(now) {
  var last = null;
  Moon.upcoming(now - 31 * DAY, 6).forEach(function (ev) {
    if (ev.type === 0 && ev.time <= now) last = ev.time;
  });
  return last;
}

/** The Moon at `now`. */
function nowInstance(now, lang) {
  var fraction = Moon.lunationFraction(now);
  var waxing = fraction < 0.5;
  var km = Math.round(Moon.distanceKm(now));
  var approaching = Moon.isApproaching(now);
  var ascending = Text.skyAt(now).ascending;
  var next = Moon.upcoming(now, 1)[0];
  return {
    instanceId: 'now',
    properties: {
      waxing: { boolean: { value: waxing }, shortText: { text: Text.LIGHT_WORDS[lang][waxing ? 0 : 1] } },
      lunation: { numericValue: numeric(fraction, null, 0, 1) },
      age: { numericValue: numeric((now - lastNewMoon(now)) / DAY, 'd', 0, MAX_AGE_DAYS) },
      distance: distance(km, lang),
      approaching: {
        boolean: { value: approaching },
        shortText: { text: Text.TOWARD_WORDS[lang][approaching ? 0 : 1] },
      },
      ascending: {
        boolean: { value: ascending },
        shortText: { text: Text.SKY_WORDS[lang][ascending ? 0 : 1] },
      },
      next_phase: {
        shortText: { text: Text.phaseTitle(next, lang) },
        timestamp: { value: Text.lunarEclipse(next) ? next.eclipse.time : next.time },
      },
    },
  };
}

function event(kind, time, name, extra) {
  var properties = { kind: { longText: { text: kind } }, name: shortText(name), time: when(time) };
  for (var key in extra) properties[key] = extra[key];
  return { instanceId: kind + '-' + time, time: time, properties: properties };
}

function eclipseEvent(eclipse, lang) {
  var instance = event('lunar_eclipse', eclipse.time,
                       Text.ECLIPSE_TITLES[lang][eclipse.type === 'total' ? 0 : 1], {
    shadow: {
      numericValue: numeric(eclipse.type === 'total' ? 100
                            : Math.min(99, Math.round(eclipse.magnitude * 100)), '%', 0, 100),
      shortText: { text: Text.formatShadow(eclipse, lang) },
    },
  });
  // It can be more than a year ahead.
  instance.properties.time = when(eclipse.time, true);
  return instance;
}

/** The events from `now` to EVENT_WINDOW ahead, and the next lunar eclipse. */
function eventInstances(now, lang) {
  var end = now + EVENT_WINDOW;
  var out = [];
  Moon.upcoming(now, 6).forEach(function (ev) {
    if (ev.time > end) return;
    out.push(event(PHASE_KINDS[ev.type], ev.time,
                   ev.supermoon ? Text.SUPERMOON_TITLES[lang] : Text.PHASE_TITLES[lang][ev.type],
                   { distance: distance(ev.distance, lang) }));
  });
  Moon.apsides(now, end).forEach(function (a) {
    out.push(event(a.perigee ? 'perigee' : 'apogee', a.time, Text.apsisTitle(a, lang),
                   { distance: distance(a.distance, lang) }));
  });
  Moon.nodes(now, end).forEach(function (n) {
    out.push(event(n.ascending ? 'ascending_node' : 'descending_node', n.time,
                   Text.nodeTitle(n, lang), {}));
  });
  var southern = Store.lsGet('hemi') === '1';
  Moon.declinationTurns(now, end).forEach(function (t) {
    var highest = t.northern !== southern;
    out.push(event(highest ? 'highest' : 'lowest', t.time,
                   Text.SKY_WORDS[lang][highest ? 2 : 3], {}));
  });
  var eclipse = Moon.nextEclipse(now, Text.isShownEclipse);
  if (eclipse) out.push(eclipseEvent(eclipse, lang));
  out.sort(function (a, b) { return a.time - b.time; });
  return out.map(function (e) { return { instanceId: e.instanceId, properties: e.properties }; });
}

Pebble.registerSourceHandler(function (request, respond) {
  var now = Math.floor(Date.now() / 1000);
  var lang = Text.pickLang();
  if (request.item === 'now') {
    respond.data({ instances: [nowInstance(now, lang)] });
  } else if (request.item === 'event') {
    respond.data({ instances: eventInstances(now, lang) });
  } else {
    respond.error('INVALID_REQUEST', 'moon/' + request.item + ' is not a source of this plugin');
  }
});

/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- PebbleKit JS (phone side).
 *
 *  - Sends the hemisphere (from the phone GPS) to the watch, but only fetches a
 *    new GPS fix when the cached value is missing or stale, to save battery.
 *  - Pushes timeline pins for the upcoming phases, lunar eclipses, perigees,
 *    apogees and node passages on every launch, and deletes those an event
 *    left behind when its computed time moved.
 */

var Moon = require('./moon_phase');
var Distance = require('./distance');
var Store = require('./store');
var Text = require('./moon_text');
var timeline = require('./timeline');

// Re-check the hemisphere at most this often (it essentially never changes).
var HEMI_TTL = 7 * 24 * 60 * 60 * 1000;

// Debug logging. Keep false for production builds (emits nothing); set true
// only to trace behaviour over a `pebble logs` developer connection.
var DEBUG = false;
function log(msg) { if (DEBUG) { console.log(msg); } }

// Heading of the share of the Moon's diameter in the Earth's shadow, in the
// pin detail view of an eclipse.
var SHADOW_HEADINGS = {
  en: 'In the shadow', fr: 'Dans l’ombre', de: 'Im Schatten', es: 'En la sombra',
  it: 'In ombra', pt: 'Na sombra', 'zh-CN': '食分', 'zh-TW': '食分',
};

// Headings of the next perigee and apogee in the pin detail view.
var NEXT_APSIS_HEADINGS = {
  en: ['Next perigee', 'Next apogee'],
  fr: ['Prochain périgée', 'Prochain apogée'],
  de: ['Nächstes Perigäum', 'Nächstes Apogäum'],
  es: ['Próximo perigeo', 'Próximo apogeo'],
  it: ['Prossimo perigeo', 'Prossimo apogeo'],
  pt: ['Próximo perigeu', 'Próximo apogeu'],
  'zh-CN': ['下次近地点', '下次远地点'],
  'zh-TW': ['下次近地點', '下次遠地點'],
};

// Section headings of the pin detail view:
// [distance, next phase, nearest phase, next eclipse, next node].
var DETAIL_HEADINGS = {
  en: ['Distance', 'Next phase', 'Nearest phase', 'Next eclipse', 'Next node'],
  fr: ['Distance', 'Phase suivante', 'Phase la plus proche', 'Prochaine éclipse', 'Prochain nœud'],
  de: ['Entfernung', 'Nächste Phase', 'Nächstgelegene Phase', 'Nächste Finsternis', 'Nächster Knoten'],
  es: ['Distancia', 'Próxima fase', 'Fase más cercana', 'Próximo eclipse', 'Próximo nodo'],
  it: ['Distanza', 'Prossima fase', 'Fase più vicina', 'Prossima eclissi', 'Prossimo nodo'],
  pt: ['Distância', 'Próxima fase', 'Fase mais próxima', 'Próximo eclipse', 'Próximo nodo'],
  'zh-CN': ['距离', '下一月相', '最近的月相', '下次月食', '下一交点'],
  'zh-TW': ['距離', '下一月相', '最近的月相', '下次月食', '下一交點'],
};

// publishedMedia names declared in package.json (referenced as app://images/NAME).
var PHASE_ICON_NAME = ['MOON_NEW', 'MOON_FIRST', 'MOON_FULL', 'MOON_LAST'];
var APSIS_ICON_NAME = ['MOON_PERIGEE', 'MOON_APOGEE'];
var NODE_ICON_NAME = ['MOON_NODE_ASC', 'MOON_NODE_DESC'];

function sendHemisphere(southern) {
  Pebble.sendAppMessage({ Hemisphere: southern ? 1 : 0 },
    function () { log('hemisphere sent (' + (southern ? 'S' : 'N') + ')'); },
    function () { log('hemisphere send failed'); });
}

/**
 * Tell the watch which hemisphere it's in. Uses the cached value (no GPS) when
 * it is still fresh; only powers up geolocation on first run or after HEMI_TTL.
 */
function updateHemisphere() {
  var cached = Store.lsGet('hemi');
  var ts = parseInt(Store.lsGet('hemiTime') || '0', 10);

  if (cached !== null && (Date.now() - ts) < HEMI_TTL) {
    sendHemisphere(cached === '1');  // known & fresh -> no GPS
    return;
  }
  if (!navigator.geolocation) {
    if (cached !== null) sendHemisphere(cached === '1');
    return;
  }
  navigator.geolocation.getCurrentPosition(
    function (pos) {
      var southern = pos.coords.latitude < 0;
      Store.lsSet('hemi', southern ? '1' : '0');
      Store.lsSet('hemiTime', '' + Date.now());
      sendHemisphere(southern);
      // The pins went out with the hemisphere known before this fix; the
      // quarters swap their icons if it changed.
      pushMoonPins();
    },
    function (err) {
      log('geolocation error: ' + err.message);
      if (cached !== null) sendHemisphere(cached === '1');  // fall back
    },
    { timeout: 15000, maximumAge: HEMI_TTL });
}

/** Time of the event a pin id stands for: the number after its last dash. */
function pinTime(id) {
  return parseInt(id.slice(id.lastIndexOf('-') + 1), 10);
}

/**
 * Ids of the pins listed in the signature that earlier versions stored instead
 * of pinsPushed ("frN|2@1792988119,3@1793564989,...", then "|P@...,A@...").
 * The signature goes once its ids are queued for deletion.
 */
function legacyPinIds() {
  var sig = Store.lsGet('pinSig');
  if (!sig) return [];
  var ids = [];
  var re = /([0-3PA])S?@(\d+)/g;
  var m;
  while ((m = re.exec(sig)) !== null) {
    var kind = m[1] === 'P' ? 'perigee-' : m[1] === 'A' ? 'apogee-' : '';
    ids.push('moon-' + kind + m[2]);
  }
  return ids;
}

/**
 * Delete the pins pushed earlier under ids that are gone from `pins` while
 * their time is still ahead: an event whose computed time moved got a new id,
 * and its old pin would sit next to the new one. Past ones expire on their
 * own. The ids wait in storage until the timeline confirms each deletion.
 */
function deleteSupersededPins(pins, earlierIds, now) {
  var current = {};
  pins.forEach(function (pin) { current[pin.id] = true; });

  var queue = [];
  try { queue = JSON.parse(Store.lsGet('pinsToDelete') || '[]'); } catch (e) { /* start over */ }
  earlierIds.forEach(function (id) {
    if (!current[id] && pinTime(id) > now && queue.indexOf(id) < 0) queue.push(id);
  });
  Store.lsSet('pinsToDelete', JSON.stringify(queue));
  Store.lsRemove('pinSig');

  queue.slice().forEach(function (id) {
    timeline.deleteUserPin({ id: id }, function (status) {
      log('delete ' + id + ' -> ' + status);
      if ((status >= 200 && status < 300) || status === 404 || status === 410) {
        queue.splice(queue.indexOf(id), 1);
        Store.lsSet('pinsToDelete', JSON.stringify(queue));
      }
    });
  });
}

/**
 * A timeline pin in the app's colours. `text` holds the layout's title,
 * subtitle, body, headings and paragraphs. The background is GColorOxfordBlue
 * exactly: the phone app maps #RRGGBB onto the watch palette by flooring each
 * channel, so a colour off the palette can come out darker, down to black.
 */
function moonPin(id, time, text, iconName) {
  var iconUri = 'app://images/' + iconName;
  return {
    id: id,
    time: new Date(time * 1000).toISOString(),
    layout: {
      type: 'genericPin',
      title: text.title,
      subtitle: text.subtitle,
      body: text.body,
      headings: text.headings,
      paragraphs: text.paragraphs,
      tinyIcon: iconUri,
      largeIcon: iconUri,
      backgroundColor: '#000055',
      primaryColor: '#FFFFFF',
    },
  };
}

/**
 * Text of a pin. Its detail starts with `lead`, the phase it relates to, then
 * follows the cards of the watch app: the distance `km` in words and in km,
 * the next apogee and perigee, whether the Moon passes higher or lower each
 * day and until when, the next node passage and the next lunar eclipse. Each
 * is looked for from a minute after `time`, so a pin whose time is rounded
 * does not find itself.
 */
function pinText(title, subtitle, time, km, lead, lang) {
  var sections = lead.concat([distanceSection(km, lang)]);
  Moon.apsides(time + 60, time + 32 * 86400).slice(0, 2).forEach(function (a) {
    sections.push([NEXT_APSIS_HEADINGS[lang][a.perigee ? 0 : 1],
      Text.longDate(a.time) + '\n' + Text.clockTime(a.time) + '\n' + Text.formatKm(a.distance)]);
  });
  var sky = Text.skyAt(time);
  if (sky.turn) {
    sections.push([Text.SKY_WORDS[lang][sky.ascending ? 0 : 1],
      Text.SKY_WORDS[lang][sky.turn.highest ? 2 : 3] + '\n' + Text.longDate(sky.turn.time) + '\n'
        + Text.clockTime(sky.turn.time)]);
  }
  var node = Moon.nodes(time + 60, time + 16 * 86400)[0];
  if (node) {
    sections.push([DETAIL_HEADINGS[lang][4],
      Text.nodeTitle(node, lang) + '\n' + Text.longDate(node.time) + '\n'
        + Text.clockTime(node.time)]);
  }
  var next = Moon.nextEclipse(time, Text.isShownEclipse);
  if (next) {
    sections.push([DETAIL_HEADINGS[lang][3],
      Text.ECLIPSE_TITLES[lang][next.type === 'total' ? 0 : 1]
        + ' (' + Text.formatShadow(next, lang) + ')\n'
        + Text.longDate(next.time, true) + '\n' + Text.clockTime(next.time)]);
  }
  return {
    title: title,
    subtitle: subtitle,
    body: Text.longDate(time),
    headings: sections.map(function (s) { return s[0]; }),
    paragraphs: sections.map(function (s) { return s[1]; }),
  };
}

/** Detail section giving the distance in words, then in km. */
function distanceSection(km, lang) {
  return [DETAIL_HEADINGS[lang][0], Distance.distanceLabel(km, lang) + '\n' + Text.formatKm(km)];
}

/**
 * Text of a phase pin. The list shows whether the Moon waxes or wanes and
 * whether it passes higher or lower each day, both from a minute after the
 * phase, so that a new moon waxes and a full moon wanes; the detail starts
 * with the next phase and how much of the Moon its eclipse covers, if any.
 */
function phaseText(ev, next, lang) {
  var eclipse = Text.lunarEclipse(ev);
  // An eclipsed full moon is dated at its greatest eclipse, which can come
  // after the phase: what follows is looked for from there, not to find this
  // eclipse again.
  var time = eclipse ? eclipse.time : ev.time;
  var lead = [[DETAIL_HEADINGS[lang][1],
    Text.phaseTitle(next, lang) + '\n' + Text.longDate(next.time)]];
  if (eclipse) lead.push([SHADOW_HEADINGS[lang], Text.formatShadow(eclipse, lang)]);
  var subtitle = Text.LIGHT_WORDS[lang][Moon.lunationFraction(ev.time + 60) < 0.5 ? 0 : 1] + ' & '
    + Text.lowerFirst(Text.SKY_WORDS[lang][Text.skyAt(time + 60).ascending ? 0 : 1]);
  return pinText(Text.phaseTitle(ev, lang), subtitle, time, ev.distance, lead, lang);
}

/**
 * Text of a perigee or apogee pin. Its class in words says little (a perigee
 * is always close, an apogee always far), so the list shows the km; the
 * detail starts with the phase nearest in time.
 */
function apsisText(a, nearest, lang) {
  return pinText(Text.apsisTitle(a, lang), Text.formatKm(a.distance), a.time, a.distance, [
    [DETAIL_HEADINGS[lang][2], Text.phaseTitle(nearest, lang) + '\n' + Text.longDate(nearest.time)],
  ], lang);
}

/**
 * Text of a node pin: which way the Moon crosses the plane of the Earth's
 * orbit, and in the detail the phase nearest in time, which names the eclipse
 * when there is one.
 */
function nodeText(n, nearest, lang) {
  return pinText(Text.nodeTitle(n, lang), Text.NODE_DIRECTIONS[lang][n.ascending ? 0 : 1], n.time,
                 Math.round(Moon.distanceKm(n.time)), [
    [DETAIL_HEADINGS[lang][2], Text.phaseTitle(nearest, lang) + '\n' + Text.longDate(nearest.time)],
  ], lang);
}

/** The event of `events` closest in time to `time`. */
function nearestInTime(events, time) {
  return events.reduce(function (best, ev) {
    return Math.abs(ev.time - time) < Math.abs(best.time - time) ? ev : best;
  });
}

/**
 * Push timeline pins for the upcoming phases, and for the perigees, apogees and
 * node passages falling among them. Every pin is pushed each time: the phone
 * app answers the timeline requests itself, with a 2xx even for a pin it later
 * loses (an uninstall drops the pins but keeps localStorage), and it cannot
 * list the pins it holds, so nothing here can tell which ones are still there.
 * A pin pushed again under its id replaces the previous one.
 */
function pushMoonPins() {
  var now = Math.floor(Date.now() / 1000);
  // One phase beyond the pinned ones, to name the phase after the last pin and
  // find what comes before it.
  var computed = Moon.upcoming(now, 9);
  var events = computed.slice(0, 8);
  var lastPinned = events[events.length - 1].time;
  var end = computed[computed.length - 1].time;
  // Perigees, apogees and node passages up to that phase, in order.
  var others = Moon.apsides(now, end).concat(Moon.nodes(now, end))
    .sort(function (a, b) { return a.time - b.time; });
  // The phases around the pinned events, past ones included, to find the
  // nearest one of each.
  var around = Moon.upcoming(now - 8 * 86400, 12);
  var lang = Text.pickLang();
  var southern = Store.lsGet('hemi') === '1';

  var pins = events.map(function (ev, i) {
    // Southern hemisphere mirrors the quarters: swap first/last quarter icons
    // (new and full are symmetric).
    var iconType = ev.type;
    if (southern && ev.type === 1) iconType = 3;
    else if (southern && ev.type === 3) iconType = 1;
    var next = computed[i + 1];
    // An eclipsed full moon is pinned at its greatest eclipse, under the id of
    // its phase so the pin replaces the plain one.
    var eclipse = Text.lunarEclipse(ev);
    return moonPin('moon-' + ev.time, eclipse ? eclipse.time : ev.time,
                   phaseText(ev, next, lang), PHASE_ICON_NAME[iconType]);
  }).concat(others.filter(function (o) { return o.time <= lastPinned; }).map(function (o) {
    var nearest = nearestInTime(around, o.time);
    if ('perigee' in o) {
      return moonPin('moon-' + (o.perigee ? 'perigee-' : 'apogee-') + o.time, o.time,
                     apsisText(o, nearest, lang), APSIS_ICON_NAME[o.perigee ? 0 : 1]);
    }
    return moonPin('moon-node-' + (o.ascending ? 'asc-' : 'desc-') + o.time, o.time,
                   nodeText(o, nearest, lang), NODE_ICON_NAME[o.ascending ? 0 : 1]);
  }));

  // Ids of the pins pushed so far, the keys of an object, for
  // deleteSupersededPins. Pins that left the window drop out of it.
  var pushed = {};
  try { pushed = JSON.parse(Store.lsGet('pinsPushed') || '{}'); } catch (e) { /* start over */ }
  var record = {};
  pins.forEach(function (pin) {
    if (pushed[pin.id]) record[pin.id] = 1;
  });
  Store.lsSet('pinsPushed', JSON.stringify(record));

  deleteSupersededPins(pins, Object.keys(pushed).concat(legacyPinIds()), now);

  pins.forEach(function (pin) {
    timeline.insertUserPin(pin, function (status) {
      log('pin ' + pin.id + ' -> ' + status);
      if (status >= 200 && status < 300) {
        record[pin.id] = 1;
        Store.lsSet('pinsPushed', JSON.stringify(record));
      }
    });
  });
}

Pebble.addEventListener('ready', function () {
  log('moonphase pkjs ready');
  updateHemisphere();
  pushMoonPins();
});

// The watch answers the hemisphere with its clock style; the pins go out again
// when it changed, since their times are written by it.
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload.Clock24h === undefined) return;
  var value = e.payload.Clock24h ? '1' : '0';
  if (Store.lsGet('clock24h') === value) return;
  Store.lsSet('clock24h', value);
  pushMoonPins();
});

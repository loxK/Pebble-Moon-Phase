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
var timeline = require('./timeline');

// Re-check the hemisphere at most this often (it essentially never changes).
var HEMI_TTL = 7 * 24 * 60 * 60 * 1000;

// Debug logging. Keep false for production builds (emits nothing); set true
// only to trace behaviour over a `pebble logs` developer connection.
var DEBUG = false;
function log(msg) { if (DEBUG) { console.log(msg); } }

// Localized phase titles (sent already translated so the timeline service does
// not substitute its own wording), in the language of the watch (pickLang).
var PHASE_TITLES = {
  en: ['New Moon', 'First Quarter', 'Full Moon', 'Last Quarter'],
  fr: ['Nouvelle lune', 'Premier quartier', 'Pleine lune', 'Dernier quartier'],
  de: ['Neumond', 'Erstes Viertel', 'Vollmond', 'Letztes Viertel'],
  es: ['Luna nueva', 'Cuarto creciente', 'Luna llena', 'Cuarto menguante'],
  it: ['Luna nuova', 'Primo quarto', 'Luna piena', 'Ultimo quarto'],
  pt: ['Lua nova', 'Quarto crescente', 'Lua cheia', 'Quarto minguante'],
  'zh-CN': ['新月', '上弦月', '满月', '下弦月'],
  'zh-TW': ['新月', '上弦月', '滿月', '下弦月'],
};

// A full moon close to the Earth takes this title instead of "Full Moon".
var SUPERMOON_TITLES = {
  en: 'Supermoon', fr: 'Super lune', de: 'Supermond', es: 'Superluna',
  it: 'Superluna', pt: 'Superlua', 'zh-CN': '超级月亮', 'zh-TW': '超級月亮',
};

// Closest and farthest points of the Moon's orbit: [perigee, apogee].
var APSIS_TITLES = {
  en: ['Perigee', 'Apogee'],
  fr: ['Périgée', 'Apogée'],
  de: ['Perigäum', 'Apogäum'],
  es: ['Perigeo', 'Apogeo'],
  it: ['Perigeo', 'Apogeo'],
  pt: ['Perigeu', 'Apogeu'],
  'zh-CN': ['近地点', '远地点'],
  'zh-TW': ['近地點', '遠地點'],
};

// A full moon in the Earth's shadow takes this title: [total, partial]. The
// app only shows lunar eclipses, so "lunar" is left out.
var ECLIPSE_TITLES = {
  en: ['Total eclipse', 'Partial eclipse'],
  fr: ['Éclipse totale', 'Éclipse partielle'],
  de: ['Totale Finsternis', 'Partielle Finsternis'],
  es: ['Eclipse total', 'Eclipse parcial'],
  it: ['Eclissi totale', 'Eclissi parziale'],
  pt: ['Eclipse total', 'Eclipse parcial'],
  'zh-CN': ['月全食', '月偏食'],
  'zh-TW': ['月全食', '月偏食'],
};

// Heading of the share of the Moon's diameter in the Earth's shadow, in the
// pin detail view of an eclipse.
var SHADOW_HEADINGS = {
  en: 'In the shadow', fr: 'Dans l’ombre', de: 'Im Schatten', es: 'En la sombra',
  it: 'In ombra', pt: 'Na sombra', 'zh-CN': '食分', 'zh-TW': '食分',
};

// Where the Moon crosses the plane of the Earth's orbit: [ascending, descending].
var NODE_TITLES = {
  en: ['Ascending node', 'Descending node'],
  fr: ['Nœud ascendant', 'Nœud descendant'],
  de: ['Aufsteigender Knoten', 'Absteigender Knoten'],
  es: ['Nodo ascendente', 'Nodo descendente'],
  it: ['Nodo ascendente', 'Nodo discendente'],
  pt: ['Nodo ascendente', 'Nodo descendente'],
  'zh-CN': ['升交点', '降交点'],
  'zh-TW': ['升交點', '降交點'],
};

// Which way the Moon crosses it, shown under a node pin's title.
var NODE_DIRECTIONS = {
  en: ['Heading north', 'Heading south'],
  fr: ['Vers le nord', 'Vers le sud'],
  de: ['Nach Norden', 'Nach Süden'],
  es: ['Hacia el norte', 'Hacia el sur'],
  it: ['Verso nord', 'Verso sud'],
  pt: ['Rumo ao norte', 'Rumo ao sul'],
  'zh-CN': ['向北', '向南'],
  'zh-TW': ['向北', '向南'],
};

// Section headings of the pin detail view:
// [distance, next phase, nearest phase, next eclipse].
var DETAIL_HEADINGS = {
  en: ['Distance', 'Next phase', 'Nearest phase', 'Next eclipse'],
  fr: ['Distance', 'Phase suivante', 'Phase la plus proche', 'Prochaine éclipse'],
  de: ['Entfernung', 'Nächste Phase', 'Nächstgelegene Phase', 'Nächste Finsternis'],
  es: ['Distancia', 'Próxima fase', 'Fase más cercana', 'Próximo eclipse'],
  it: ['Distanza', 'Prossima fase', 'Fase più vicina', 'Prossima eclissi'],
  pt: ['Distância', 'Próxima fase', 'Fase mais próxima', 'Próximo eclipse'],
  'zh-CN': ['距离', '下一月相', '最近的月相', '下次月食'],
  'zh-TW': ['距離', '下一月相', '最近的月相', '下次月食'],
};

// Thousands separator for the distances, and what goes between a number and
// its percent sign.
var THOUSANDS = { en: ',', fr: '\u00a0', de: '.', es: '.', it: '.', pt: '.', 'zh-CN': ',', 'zh-TW': ',' };
var PERCENT_SPACE = { en: '', fr: '\u00a0', de: '\u00a0', es: '\u00a0', it: '', pt: '', 'zh-CN': '', 'zh-TW': '' };

// publishedMedia names declared in package.json (referenced as app://images/NAME).
var PHASE_ICON_NAME = ['MOON_NEW', 'MOON_FIRST', 'MOON_FULL', 'MOON_LAST'];
var APSIS_ICON_NAME = ['MOON_PERIGEE', 'MOON_APOGEE'];
var NODE_ICON_NAME = ['MOON_NODE_ASC', 'MOON_NODE_DESC'];

// Safe localStorage access: if it is unavailable, the app simply degrades to
// always fetching/pushing (the pre-optimization behavior) instead of crashing.
function lsGet(k) {
  try { return localStorage.getItem(k); } catch (e) { return null; }
}
function lsSet(k, v) {
  try { localStorage.setItem(k, v); } catch (e) { /* no-op */ }
}
function lsRemove(k) {
  try { localStorage.removeItem(k); } catch (e) { /* no-op */ }
}

/**
 * Language of a watch locale ("fr_FR", "zh_TW"), by the rules of i18n_lang()
 * on the watch: the Chinese language packs also come as en_CN and en_TW.
 */
function langFromWatch(locale) {
  if (locale.indexOf('zh') === 0) {
    return /TW|HK/.test(locale) ? 'zh-TW' : 'zh-CN';
  }
  if (/TW|HK/.test(locale)) return 'zh-TW';
  if (/CN/.test(locale)) return 'zh-CN';
  var p = locale.slice(0, 2).toLowerCase();
  return PHASE_TITLES[p] ? p : 'en';
}

/**
 * Language of a phone locale ("fr-FR", "zh-Hant-HK", "en_HK"), from its
 * language subtag: a phone's region says where it is, not what its owner
 * reads.
 */
function langFromPhone(tag) {
  var parts = tag.toLowerCase().split(/[-_]/);
  if (parts[0] === 'zh') {
    var traditional = parts.some(function (p) {
      return p === 'hant' || p === 'tw' || p === 'hk' || p === 'mo';
    });
    return traditional ? 'zh-TW' : 'zh-CN';
  }
  return PHASE_TITLES[parts[0]] ? parts[0] : 'en';
}

/**
 * Language of the pins: the watch's, so they read like the app, falling back
 * to the phone's when the watch does not say.
 */
function pickLang() {
  var info = null;
  try {
    info = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
  } catch (e) { /* not connected */ }
  if (info && info.language) return langFromWatch(info.language);
  return langFromPhone(navigator.language || 'en');
}

// Localized weekday / month names (long form), keyed like PHASE_TITLES and
// mirroring the C-side i18n so pin dates read exactly like the watch screen.
// Dates are formatted by hand rather than with toLocaleDateString: not every
// engine PebbleKit JS runs on ships Intl (the emulator's pypkjs has none and
// ignores the locale), and these tables give the same output on all of them.
var WEEKDAYS = {
  en: ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'],
  fr: ['dimanche', 'lundi', 'mardi', 'mercredi', 'jeudi', 'vendredi', 'samedi'],
  de: ['Sonntag', 'Montag', 'Dienstag', 'Mittwoch', 'Donnerstag', 'Freitag', 'Samstag'],
  es: ['domingo', 'lunes', 'martes', 'miércoles', 'jueves', 'viernes', 'sábado'],
  it: ['domenica', 'lunedì', 'martedì', 'mercoledì', 'giovedì', 'venerdì', 'sabato'],
  pt: ['domingo', 'segunda-feira', 'terça-feira', 'quarta-feira', 'quinta-feira', 'sexta-feira', 'sábado'],
  'zh-CN': ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'],
  'zh-TW': ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'],
};
var MONTHS = {
  en: ['January', 'February', 'March', 'April', 'May', 'June',
       'July', 'August', 'September', 'October', 'November', 'December'],
  fr: ['janvier', 'février', 'mars', 'avril', 'mai', 'juin',
       'juillet', 'août', 'septembre', 'octobre', 'novembre', 'décembre'],
  de: ['Januar', 'Februar', 'März', 'April', 'Mai', 'Juni',
       'Juli', 'August', 'September', 'Oktober', 'November', 'Dezember'],
  es: ['enero', 'febrero', 'marzo', 'abril', 'mayo', 'junio',
       'julio', 'agosto', 'septiembre', 'octubre', 'noviembre', 'diciembre'],
  it: ['gennaio', 'febbraio', 'marzo', 'aprile', 'maggio', 'giugno',
       'luglio', 'agosto', 'settembre', 'ottobre', 'novembre', 'dicembre'],
  pt: ['janeiro', 'fevereiro', 'março', 'abril', 'maio', 'junho',
       'julho', 'agosto', 'setembro', 'outubro', 'novembro', 'dezembro'],
};

// Full localized date with weekday, e.g. "mardi 28 juin", or "mardi 28 juin
// 2028" with `withYear`, in the phone's local timezone. Shown in the pin
// detail view. Layout follows the same per-locale convention as the C
// DateStyle.
function longDate(epoch, withYear) {
  var d = new Date(epoch * 1000);
  var lang = pickLang();
  var wd = WEEKDAYS[lang][d.getDay()];
  var day = d.getDate();
  var mon = d.getMonth();  // 0-11
  var y = d.getFullYear();
  switch (lang) {
    case 'en':    return wd + ', ' + MONTHS.en[mon] + ' ' + day + (withYear ? ', ' + y : '');
    case 'de':    return wd + ', ' + day + '. ' + MONTHS.de[mon] + (withYear ? ' ' + y : '');
    case 'es':    return wd + ', ' + day + ' de ' + MONTHS.es[mon] + (withYear ? ' de ' + y : '');
    case 'pt':    return wd + ', ' + day + ' de ' + MONTHS.pt[mon] + (withYear ? ' de ' + y : '');
    case 'zh-CN':
    case 'zh-TW': return (withYear ? y + '年' : '') + (mon + 1) + '月' + day + '日 ' + wd;
    case 'fr':
    case 'it':
    default:      return wd + ' ' + day + ' ' + MONTHS[lang][mon] + (withYear ? ' ' + y : '');
  }
}

/**
 * Whether the watch shows the time on a 24-hour clock, as it last said (see
 * the appmessage listener). Until it has, English reads 12-hour and the other
 * languages 24-hour.
 */
function clock24h() {
  var stored = lsGet('clock24h');
  return stored !== null ? stored === '1' : pickLang() !== 'en';
}

// Time of day in the phone's local timezone, "21:07" or "9:07 PM" by the
// watch's clock style.
function clockTime(epoch) {
  var d = new Date(epoch * 1000);
  var min = (d.getMinutes() < 10 ? '0' : '') + d.getMinutes();
  if (clock24h()) return d.getHours() + ':' + min;
  var h = d.getHours() % 12 || 12;
  return h + ':' + min + (d.getHours() < 12 ? '\u00a0AM' : '\u00a0PM');
}

// Distance rounded to the km with the language's grouping, e.g. "356 979 km";
// the unit stays on the line of the number.
function formatKm(km) {
  var digits = '' + Math.round(km);
  return digits.replace(/\B(?=(\d{3})+$)/g, THOUSANDS[pickLang()]) + '\u00a0km';
}

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
  var cached = lsGet('hemi');
  var ts = parseInt(lsGet('hemiTime') || '0', 10);

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
      lsSet('hemi', southern ? '1' : '0');
      lsSet('hemiTime', '' + Date.now());
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
  var sig = lsGet('pinSig');
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
  try { queue = JSON.parse(lsGet('pinsToDelete') || '[]'); } catch (e) { /* start over */ }
  earlierIds.forEach(function (id) {
    if (!current[id] && pinTime(id) > now && queue.indexOf(id) < 0) queue.push(id);
  });
  lsSet('pinsToDelete', JSON.stringify(queue));
  lsRemove('pinSig');

  queue.slice().forEach(function (id) {
    timeline.deleteUserPin({ id: id }, function (status) {
      log('delete ' + id + ' -> ' + status);
      if ((status >= 200 && status < 300) || status === 404 || status === 410) {
        queue.splice(queue.indexOf(id), 1);
        lsSet('pinsToDelete', JSON.stringify(queue));
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
 * Whether the app shows an eclipse: a total or partial lunar one. Solar
 * eclipses depend on where they are seen from, and penumbral ones are hardly
 * visible.
 */
function isShownEclipse(e) {
  return !e.solar && (e.type === 'total' || e.type === 'partial');
}

/** The eclipse of a phase event, if the app shows it. */
function lunarEclipse(ev) {
  return ev.eclipse && isShownEclipse(ev.eclipse) ? ev.eclipse : null;
}

/** Title of a phase event: the eclipse, the supermoon or its phase name. */
function phaseTitle(ev, lang) {
  var eclipse = lunarEclipse(ev);
  if (eclipse) return ECLIPSE_TITLES[lang][eclipse.type === 'total' ? 0 : 1];
  return ev.supermoon ? SUPERMOON_TITLES[lang] : PHASE_TITLES[lang][ev.type];
}

/** Title of a perigee or apogee. */
function apsisTitle(a, lang) {
  return APSIS_TITLES[lang][a.perigee ? 0 : 1];
}

/** Title of a passage through a node. */
function nodeTitle(n, lang) {
  return NODE_TITLES[lang][n.ascending ? 0 : 1];
}

/**
 * Share of the Moon's diameter in the Earth's shadow, as a percentage: 100 for
 * a total eclipse, whose umbral magnitude goes past 1 when the shadow is wider
 * than the Moon, and at most 99 for a partial one.
 */
function formatShadow(eclipse, lang) {
  var percent = eclipse.type === 'total' ? 100 : Math.min(99, Math.round(eclipse.magnitude * 100));
  return percent + PERCENT_SPACE[lang] + '%';
}

/**
 * Layout text of a pin; `sections` is a list of [heading, paragraph]. Every
 * pin ends with the next eclipse after its time and the time of its greatest
 * phase, dated with the year since it can be more than a year ahead.
 */
function pinText(title, subtitle, time, sections, lang) {
  var next = Moon.nextEclipse(time, isShownEclipse);
  if (next) {
    sections = sections.concat([[DETAIL_HEADINGS[lang][3],
      ECLIPSE_TITLES[lang][next.type === 'total' ? 0 : 1] + ' (' + formatShadow(next, lang) + ')\n'
        + longDate(next.time, true) + '\n' + clockTime(next.time)]]);
  }
  return {
    title: title,
    subtitle: subtitle,
    body: longDate(time),
    headings: sections.map(function (s) { return s[0]; }),
    paragraphs: sections.map(function (s) { return s[1]; }),
  };
}

/** Detail section giving the distance in words, then in km. */
function distanceSection(km, lang) {
  return [DETAIL_HEADINGS[lang][0], Distance.distanceLabel(km, lang) + '\n' + formatKm(km)];
}

/**
 * Text of a phase pin. The list shows how far the Moon is in words; the detail
 * adds how much of the Moon its eclipse covers, if any, the km, then what
 * comes before the next phase, in order: perigees and apogees ({ perigee,
 * time, distance }) and node passages ({ ascending, time }), and that next
 * phase.
 */
function phaseText(ev, next, coming, lang) {
  var sections = [];
  var eclipse = lunarEclipse(ev);
  if (eclipse) sections.push([SHADOW_HEADINGS[lang], formatShadow(eclipse, lang)]);
  sections.push(distanceSection(ev.distance, lang));
  coming.forEach(function (c) {
    sections.push('perigee' in c
      ? [apsisTitle(c, lang), longDate(c.time) + '\n' + formatKm(c.distance)]
      : [nodeTitle(c, lang), longDate(c.time)]);
  });
  sections.push([DETAIL_HEADINGS[lang][1], phaseTitle(next, lang) + '\n' + longDate(next.time)]);
  return pinText(phaseTitle(ev, lang), Distance.distanceLabel(ev.distance, lang), ev.time,
                 sections, lang);
}

/**
 * Text of a perigee or apogee pin. Its class in words says little (a perigee
 * is always close, an apogee always far), so the list shows the km; the
 * detail adds the phase nearest in time.
 */
function apsisText(a, nearest, lang) {
  return pinText(apsisTitle(a, lang), formatKm(a.distance), a.time, [
    distanceSection(a.distance, lang),
    [DETAIL_HEADINGS[lang][2], phaseTitle(nearest, lang) + '\n' + longDate(nearest.time)],
  ], lang);
}

/**
 * Text of a node pin: which way the Moon crosses the plane of the Earth's
 * orbit, and in the detail its distance and the phase nearest in time, which
 * names the eclipse when there is one.
 */
function nodeText(n, nearest, lang) {
  return pinText(nodeTitle(n, lang), NODE_DIRECTIONS[lang][n.ascending ? 0 : 1], n.time, [
    distanceSection(Moon.distanceKm(n.time), lang),
    [DETAIL_HEADINGS[lang][2], phaseTitle(nearest, lang) + '\n' + longDate(nearest.time)],
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
  var lang = pickLang();
  var southern = lsGet('hemi') === '1';

  var pins = events.map(function (ev, i) {
    // Southern hemisphere mirrors the quarters: swap first/last quarter icons
    // (new and full are symmetric).
    var iconType = ev.type;
    if (southern && ev.type === 1) iconType = 3;
    else if (southern && ev.type === 3) iconType = 1;
    var next = computed[i + 1];
    var coming = others.filter(function (o) { return o.time > ev.time && o.time < next.time; });
    // An eclipsed full moon is pinned at its greatest eclipse, under the id of
    // its phase so the pin replaces the plain one.
    var eclipse = lunarEclipse(ev);
    return moonPin('moon-' + ev.time, eclipse ? eclipse.time : ev.time,
                   phaseText(ev, next, coming, lang), PHASE_ICON_NAME[iconType]);
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
  try { pushed = JSON.parse(lsGet('pinsPushed') || '{}'); } catch (e) { /* start over */ }
  var record = {};
  pins.forEach(function (pin) {
    if (pushed[pin.id]) record[pin.id] = 1;
  });
  lsSet('pinsPushed', JSON.stringify(record));

  deleteSupersededPins(pins, Object.keys(pushed).concat(legacyPinIds()), now);

  pins.forEach(function (pin) {
    timeline.insertUserPin(pin, function (status) {
      log('pin ' + pin.id + ' -> ' + status);
      if (status >= 200 && status < 300) {
        record[pin.id] = 1;
        lsSet('pinsPushed', JSON.stringify(record));
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
  if (lsGet('clock24h') === value) return;
  lsSet('clock24h', value);
  pushMoonPins();
});

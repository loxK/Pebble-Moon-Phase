/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- PebbleKit JS (phone side).
 *
 *  - Sends the hemisphere (from the phone GPS) to the watch, but only fetches a
 *    new GPS fix when the cached value is missing or stale, to save battery.
 *  - Pushes timeline pins for the upcoming phases, perigees and apogees, each
 *    only when it changed since it was last pushed.
 */

var Moon = require('./moon_phase');
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

// Thousands separator for the distances.
var THOUSANDS = { en: ',', fr: '\u00a0', de: '.', es: '.', it: '.', pt: '.', 'zh-CN': ',', 'zh-TW': ',' };

// publishedMedia names declared in package.json (referenced as app://images/NAME).
var PHASE_ICON_NAME = ['MOON_NEW', 'MOON_FIRST', 'MOON_FULL', 'MOON_LAST'];
var APSIS_ICON_NAME = ['MOON_PERIGEE', 'MOON_APOGEE'];

// Safe localStorage access: if it is unavailable, the app simply degrades to
// always fetching/pushing (the pre-optimization behavior) instead of crashing.
function lsGet(k) {
  try { return localStorage.getItem(k); } catch (e) { return null; }
}
function lsSet(k, v) {
  try { localStorage.setItem(k, v); } catch (e) { /* no-op */ }
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
// We format by hand instead of using Date.prototype.toLocaleDateString because
// the emulator's JS engine (pypkjs / STPyV8) fatally OOMs inside V8's ICU
// DateTimePatternGenerator on that call, which kills the JS runtime. Manual
// formatting is engine-independent and produces the same output on hardware.
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

// Full localized date with weekday, e.g. "mardi 28 juin", in the phone's local
// timezone. Shown in the pin detail view. Layout follows the same per-locale
// convention as the C DateStyle.
function longDate(epoch) {
  var d = new Date(epoch * 1000);
  var lang = pickLang();
  var wd = WEEKDAYS[lang][d.getDay()];
  var day = d.getDate();
  var mon = d.getMonth();  // 0-11
  switch (lang) {
    case 'en':    return wd + ', ' + MONTHS.en[mon] + ' ' + day;
    case 'de':    return wd + ', ' + day + '. ' + MONTHS.de[mon];
    case 'es':    return wd + ', ' + day + ' de ' + MONTHS.es[mon];
    case 'pt':    return wd + ', ' + day + ' de ' + MONTHS.pt[mon];
    case 'zh-CN': return (mon + 1) + '月' + day + '日 ' + wd;
    case 'zh-TW': return (mon + 1) + '月' + day + '日 ' + wd;
    case 'fr':
    case 'it':
    default:      return wd + ' ' + day + ' ' + MONTHS[lang][mon];
  }
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

/** A timeline pin in the app's colours. */
function moonPin(id, time, title, body, iconName) {
  var iconUri = 'app://images/' + iconName;
  return {
    id: id,
    time: new Date(time * 1000).toISOString(),
    layout: {
      type: 'genericPin',
      title: title,
      body: body,
      tinyIcon: iconUri,
      largeIcon: iconUri,
      backgroundColor: '#001E41',
      primaryColor: '#FFFFFF',
    },
  };
}

// Short fingerprint of a string (djb2), to tell whether a pin changed.
function fingerprint(str) {
  var h = 5381;
  for (var i = 0; i < str.length; i++) {
    h = ((h * 33) ^ str.charCodeAt(i)) >>> 0;
  }
  return h.toString(36);
}

/**
 * Push timeline pins for the upcoming phases, and for the perigees and apogees
 * falling among them. Each pin is pushed only when it differs from what was
 * last pushed under its id: its fingerprint covers everything it shows, so a
 * new language, hemisphere, time zone or wording pushes again exactly the pins
 * it changes, and a failed one is retried on its own next time.
 */
function pushMoonPins() {
  var now = Math.floor(Date.now() / 1000);
  var events = Moon.upcoming(now, 8);
  var apsides = Moon.apsides(now, events[events.length - 1].time);
  var lang = pickLang();
  var southern = lsGet('hemi') === '1';

  var pins = events.map(function (ev) {
    // Southern hemisphere mirrors the quarters: swap first/last quarter icons
    // (new and full are symmetric).
    var iconType = ev.type;
    if (southern && ev.type === 1) iconType = 3;
    else if (southern && ev.type === 3) iconType = 1;
    // A supermoon owes its name to the distance, so the pin gives it.
    var title = ev.supermoon ? SUPERMOON_TITLES[lang] : PHASE_TITLES[lang][ev.type];
    var body = longDate(ev.time) + (ev.supermoon ? '\n' + formatKm(ev.distance) : '');
    return moonPin('moon-' + ev.time, ev.time, title, body, PHASE_ICON_NAME[iconType]);
  }).concat(apsides.map(function (a) {
    var kind = a.perigee ? 0 : 1;
    return moonPin('moon-' + (a.perigee ? 'perigee-' : 'apogee-') + a.time, a.time,
                   APSIS_TITLES[lang][kind], longDate(a.time) + '\n' + formatKm(a.distance),
                   APSIS_ICON_NAME[kind]);
  }));

  // Fingerprints of the pins as last pushed, by id. Pins that left the window
  // drop out of it.
  var pushed = {};
  try { pushed = JSON.parse(lsGet('pinsPushed') || '{}'); } catch (e) { /* start over */ }
  var record = {};
  pins.forEach(function (pin) {
    if (pushed[pin.id]) record[pin.id] = pushed[pin.id];
  });
  lsSet('pinsPushed', JSON.stringify(record));

  pins.forEach(function (pin) {
    var print = fingerprint(JSON.stringify(pin));
    if (record[pin.id] === print) return;
    timeline.insertUserPin(pin, function (status) {
      log('pin ' + pin.id + ' -> ' + status);
      if (status >= 200 && status < 300) {
        record[pin.id] = print;
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

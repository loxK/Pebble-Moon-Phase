/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- PebbleKit JS (phone side).
 *
 *  - Sends the hemisphere (from the phone GPS) to the watch, but only fetches a
 *    new GPS fix when the cached value is missing or stale, to save battery.
 *  - Pushes timeline pins for the upcoming phases, but only when the set of
 *    dates actually changed since the last push.
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
// not substitute its own wording). Language follows the phone locale.
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

// publishedMedia names declared in package.json (referenced as app://images/NAME).
var PHASE_ICON_NAME = ['MOON_NEW', 'MOON_FIRST', 'MOON_FULL', 'MOON_LAST'];

// Safe localStorage access: if it is unavailable, the app simply degrades to
// always fetching/pushing (the pre-optimization behavior) instead of crashing.
function lsGet(k) {
  try { return localStorage.getItem(k); } catch (e) { return null; }
}
function lsSet(k, v) {
  try { localStorage.setItem(k, v); } catch (e) { /* no-op */ }
}

function pickLang() {
  var l = (navigator.language || 'en').toLowerCase();
  if (l.indexOf('zh') === 0 || l.indexOf('tw') >= 0 || l.indexOf('hk') >= 0 ||
      l.indexOf('cn') >= 0) {
    return (l.indexOf('tw') >= 0 || l.indexOf('hk') >= 0) ? 'zh-TW' : 'zh-CN';
  }
  var p = l.slice(0, 2);
  return PHASE_TITLES[p] ? p : 'en';
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
    case 'zh-CN': return (mon + 1) + '月' + day + '日' + wd;
    case 'zh-TW': return (mon + 1) + '月' + day + '日 ' + wd;
    case 'fr':
    case 'it':
    default:      return wd + ' ' + day + ' ' + MONTHS[lang][mon];
  }
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
    },
    function (err) {
      log('geolocation error: ' + err.message);
      if (cached !== null) sendHemisphere(cached === '1');  // fall back
    },
    { timeout: 15000, maximumAge: HEMI_TTL });
}

/**
 * Push timeline pins for the upcoming phases, but skip the network entirely
 * when the dates are unchanged since the last successful push.
 */
function pushMoonPins() {
  var now = Math.floor(Date.now() / 1000);
  var events = Moon.upcoming(now, 8);
  var lang = pickLang();
  var southern = lsGet('hemi') === '1';
  // Include language and hemisphere so changing either re-pushes matching pins.
  var sig = lang + (southern ? 'S' : 'N') + '|' +
            events.map(function (e) { return e.type + '@' + e.time; }).join(',');

  if (lsGet('pinSig') === sig) {
    log('pins unchanged; skipping push');
    return;
  }

  var titles = PHASE_TITLES[lang];
  var pending = events.length;
  var ok = 0;
  events.forEach(function (ev) {
    // Southern hemisphere mirrors the quarters: swap first/last quarter icons
    // (new and full are symmetric).
    var iconType = ev.type;
    if (southern && ev.type === 1) iconType = 3;
    else if (southern && ev.type === 3) iconType = 1;
    var iconUri = 'app://images/' + PHASE_ICON_NAME[iconType];
    var pin = {
      id: 'moon-' + ev.time,
      time: new Date(ev.time * 1000).toISOString(),
      layout: {
        type: 'genericPin',
        title: titles[ev.type],
        body: longDate(ev.time),
        tinyIcon: iconUri,
        largeIcon: iconUri,
        backgroundColor: '#001E41',
        primaryColor: '#FFFFFF',
      },
    };
    timeline.insertUserPin(pin, function (status) {
      log('pin ' + pin.id + ' -> ' + status);
      if (status >= 200 && status < 300) ok++;
      if (--pending === 0 && ok === events.length) {
        lsSet('pinSig', sig);  // only cache when all succeeded
      }
    });
  });
}

Pebble.addEventListener('ready', function () {
  log('moonphase pkjs ready');
  updateHemisphere();
  pushMoonPins();
});

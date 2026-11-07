/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- the Moon's events in words, in the language of the watch:
 * titles, dates, times and distances, read alike by the pins and the plugin.
 */

var Moon = require('./moon_phase');
var Store = require('./store');

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

// Whether the Moon passes higher or lower each day, then the turn that ends
// it: [ascending, descending, highest, lowest].
var SKY_WORDS = {
  en: ['Ascending', 'Descending', 'Highest', 'Lowest'],
  fr: ['Montante', 'Descendante', 'Au plus haut', 'Au plus bas'],
  de: ['Aufsteigend', 'Absteigend', 'Höchststand', 'Tiefststand'],
  es: ['Ascendente', 'Descendente', 'Más alta', 'Más baja'],
  it: ['Ascendente', 'Discendente', 'Più alta', 'Più bassa'],
  pt: ['Ascendente', 'Descendente', 'Mais alta', 'Mais baixa'],
  'zh-CN': ['渐升', '渐降', '最高', '最低'],
  'zh-TW': ['漸升', '漸降', '最高', '最低'],
};

// Whether the lit part of the Moon grows or shrinks: [waxing, waning].
var LIGHT_WORDS = {
  en: ['Waxing', 'Waning'],
  fr: ['Croissante', 'Décroissante'],
  de: ['Zunehmend', 'Abnehmend'],
  es: ['Creciente', 'Menguante'],
  it: ['Crescente', 'Calante'],
  pt: ['Crescente', 'Minguante'],
  'zh-CN': ['渐盈', '渐亏'],
  'zh-TW': ['漸盈', '漸虧'],
};

// Thousands separator for the distances, and what goes between a number and
// its percent sign.
var THOUSANDS = { en: ',', fr: '\u00a0', de: '.', es: '.', it: '.', pt: '.', 'zh-CN': ',', 'zh-TW': ',' };
var PERCENT_SPACE = { en: '', fr: '\u00a0', de: '\u00a0', es: '\u00a0', it: '', pt: '', 'zh-CN': '', 'zh-TW': '' };

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
 * Language of the text: the watch's, so it reads like the app, falling back to
 * the phone's when the watch does not say, as in a plugin request.
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
  var stored = Store.lsGet('clock24h');
  return stored !== null ? stored === '1' : pickLang() !== 'en';
}

// Time of day in the phone's local timezone, "09:07" or "9:07 PM" by the
// watch's clock style.
function clockTime(epoch) {
  var d = new Date(epoch * 1000);
  var min = (d.getMinutes() < 10 ? '0' : '') + d.getMinutes();
  if (clock24h()) return (d.getHours() < 10 ? '0' : '') + d.getHours() + ':' + min;
  var h = d.getHours() % 12 || 12;
  return h + ':' + min + (d.getHours() < 12 ? '\u00a0AM' : '\u00a0PM');
}

// Distance rounded to the km with the language's grouping, e.g. "356 979 km";
// the unit stays on the line of the number.
function formatKm(km) {
  var digits = '' + Math.round(km);
  return digits.replace(/\B(?=(\d{3})+$)/g, THOUSANDS[pickLang()]) + '\u00a0km';
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
 * Whether the Moon passes higher or lower each day at `time`, seen from the
 * hemisphere of the phone, and the next turn that ends it: { ascending, turn },
 * `turn` undefined if none comes within half a month and a bit.
 */
function skyAt(time) {
  var southern = Store.lsGet('hemi') === '1';
  var turn = Moon.declinationTurns(time + 60, time + 16 * 86400)[0];
  return {
    ascending: Moon.isDeclinationNorthing(time) !== southern,
    turn: turn && { highest: turn.northern !== southern, time: turn.time },
  };
}

/** `text` with its first letter in lower case, to follow another word. */
function lowerFirst(text) {
  return text.charAt(0).toLowerCase() + text.slice(1);
}

module.exports = {
  PHASE_TITLES: PHASE_TITLES,
  SUPERMOON_TITLES: SUPERMOON_TITLES,
  APSIS_TITLES: APSIS_TITLES,
  ECLIPSE_TITLES: ECLIPSE_TITLES,
  NODE_TITLES: NODE_TITLES,
  NODE_DIRECTIONS: NODE_DIRECTIONS,
  SKY_WORDS: SKY_WORDS,
  LIGHT_WORDS: LIGHT_WORDS,
  THOUSANDS: THOUSANDS,
  PERCENT_SPACE: PERCENT_SPACE,
  langFromWatch: langFromWatch,
  langFromPhone: langFromPhone,
  pickLang: pickLang,
  WEEKDAYS: WEEKDAYS,
  MONTHS: MONTHS,
  longDate: longDate,
  clock24h: clock24h,
  clockTime: clockTime,
  formatKm: formatKm,
  isShownEclipse: isShownEclipse,
  lunarEclipse: lunarEclipse,
  phaseTitle: phaseTitle,
  apsisTitle: apsisTitle,
  nodeTitle: nodeTitle,
  formatShadow: formatShadow,
  skyAt: skyAt,
  lowerFirst: lowerFirst,
};

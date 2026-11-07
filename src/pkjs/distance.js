/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Earth-Moon distance in words, from "very close" to "very far".
 *
 * The Moon stays between about 356,400 and 406,700 km, so the classes are
 * relative to its orbit. The outer two are the rare ones: "very close" starts
 * where a full moon counts as a supermoon (SUPERMOON_KM in moon_phase.js), and
 * "very far" only covers the region of the apogees.
 */

var VERY_CLOSE = 0;
var CLOSE = 1;
var MIDWAY = 2;
var FAR = 3;
var VERY_FAR = 4;

// Upper bound (km, exclusive) of each class but the last.
var LIMITS_KM = [360000, 375000, 394000, 404000];

var LABELS = {
  en: ['Very close', 'Close', 'Midway', 'Far', 'Very far'],
  fr: ['Très proche', 'Proche', 'À mi-distance', 'Lointaine', 'Très lointaine'],
  de: ['Sehr nah', 'Nah', 'Mittlere Entfernung', 'Fern', 'Sehr fern'],
  es: ['Muy cerca', 'Cerca', 'A media distancia', 'Lejos', 'Muy lejos'],
  it: ['Molto vicina', 'Vicina', 'A metà distanza', 'Lontana', 'Molto lontana'],
  pt: ['Muito perto', 'Perto', 'A meia distância', 'Longe', 'Muito longe'],
  'zh-CN': ['非常近', '较近', '中等距离', '较远', '非常远'],
  'zh-TW': ['非常近', '較近', '中等距離', '較遠', '非常遠'],
};

/** Class of a distance in km: VERY_CLOSE (0) to VERY_FAR (4). */
function distanceClass(km) {
  for (var i = 0; i < LIMITS_KM.length; i++) {
    if (km < LIMITS_KM[i]) return i;
  }
  return VERY_FAR;
}

/**
 * The class of a distance in km as a word, in `lang` (English if unknown).
 * The words describe the Moon, so they agree with it where the language
 * inflects them (feminine in French and Italian).
 */
function distanceLabel(km, lang) {
  return (LABELS[lang] || LABELS.en)[distanceClass(km)];
}

module.exports = {
  VERY_CLOSE: VERY_CLOSE,
  CLOSE: CLOSE,
  MIDWAY: MIDWAY,
  FAR: FAR,
  VERY_FAR: VERY_FAR,
  LIMITS_KM: LIMITS_KM,
  distanceClass: distanceClass,
  distanceLabel: distanceLabel,
};

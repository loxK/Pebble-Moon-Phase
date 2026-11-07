/*
 * Moon Phase -- localization.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>
#include "moon_phase.h"

/**
 * Lightweight localization for the Moon Phase app.
 *
 * All user-facing strings are kept in a compact per-language table and the
 * active language is derived from the watch system locale. Date formatting
 * follows each language's convention (day/month order, Chinese "X月Y日", ...).
 */

typedef enum {
  LANG_EN = 0,
  LANG_FR,
  LANG_DE,
  LANG_ES,
  LANG_IT,
  LANG_PT,
  LANG_ZH_CN,
  LANG_ZH_TW,
  LANG_COUNT,
} Lang;

/** Detect the active language from the watch system locale. */
Lang i18n_lang(void);

// Phase names are indexed by MoonPhaseType, followed by the supermoon, which
// takes the place of "Full Moon" when the Moon is close to the Earth.
#define I18N_SUPERMOON 4
#define I18N_NAME_COUNT 5

/** Localized name of a phase (0..3, see MoonPhaseType, or I18N_SUPERMOON). */
const char *i18n_phase_name(Lang lang, int name);

/** A shorter form of i18n_phase_name, where the language has one. */
const char *i18n_phase_name_short(Lang lang, int name);

/**
 * Shortened phase name for the launcher glance, whose subtitle is a single
 * clipped line. `waiting` picks the tighter form used while the line also
 * carries the countdown to the phase.
 */
const char *i18n_glance_phase(Lang lang, int name, bool waiting);

/** Name index for a phase event: its MoonPhaseType, or I18N_SUPERMOON. */
int i18n_event_name(const MoonPhaseEvent *event);

/**
 * Countdown branches of the glance template string. Both keep the firmware's
 * own conversion specifier: "%d" is the number of days left, expanded by the
 * launcher every time it re-evaluates the line.
 */
const char *i18n_glance_in_days(Lang lang);
const char *i18n_glance_tomorrow(Lang lang);

/**
 * Localized relative-day string into `buf`.
 * days == 0 -> "Today", 1 -> "Tomorrow", otherwise "In N days".
 */
void i18n_relative(Lang lang, int days, char *buf, size_t len);

/**
 * Localized calendar date into `buf` (month0 is 0-based). When `abbrev` is true
 * the month name is shortened so it fits a narrower line: to three characters
 * ("November" -> "Nov"), or to the language's usual short form ("juillet" ->
 * "juil.").
 */
void i18n_date(Lang lang, int day, int month0, bool abbrev, char *buf, size_t len);

/**
 * Local time of day `t` into `buf`, by the watch's 12/24-hour setting. In
 * 12-hour style the AM/PM marker comes in the watch's language, or in English
 * when the app fell back to it.
 */
void i18n_time(Lang lang, const struct tm *t, char *buf, size_t len);

#if MOON_HAS_DETAILS
/** Name of a lunar eclipse, total or partial ("lunar" is left out). */
const char *i18n_eclipse_name(Lang lang, bool total);

/** Name of a perigee or apogee. */
const char *i18n_apsis_name(Lang lang, bool perigee);

/** Name of a passage through the ascending or descending node. */
const char *i18n_node_name(Lang lang, bool ascending);

/**
 * How far the Moon is, in words, for a distance class of
 * moon_distance_class(), agreeing with the Moon where the language inflects.
 */
const char *i18n_distance_label(Lang lang, int distance_class);

/** Words of the detail cards: their headings and how the Moon moves. */
typedef enum {
  I18N_DISTANCE = 0,
  I18N_NEXT_NODE,
  I18N_NEXT_ECLIPSE,
  I18N_NOW,
  I18N_NEXT_PERIGEE,
  I18N_NEXT_APOGEE,
  I18N_TOWARD_PERIGEE,  //!< the Moon approaching
  I18N_TOWARD_APOGEE,   //!< the Moon receding
  I18N_ASCENDING,       //!< the Moon passing higher each day
  I18N_DESCENDING,      //!< the Moon passing lower each day
  I18N_HIGHEST,
  I18N_LOWEST,
  I18N_WORD_COUNT,
} I18nWord;

const char *i18n_word(Lang lang, I18nWord word);

/**
 * Short local date into `buf`, "14 nov.", or "12 janv. 2028" with `with_year`,
 * the month abbreviated as on the cards.
 */
void i18n_short_date(Lang lang, const struct tm *t, bool with_year, char *buf, size_t len);

/** A distance in km with the language's grouping, "356 979 km". */
void i18n_km(Lang lang, int32_t km, char *buf, size_t len);

/** A percentage with the language's spacing, "92 %" or "92%". */
void i18n_percent(Lang lang, int percent, char *buf, size_t len);
#endif

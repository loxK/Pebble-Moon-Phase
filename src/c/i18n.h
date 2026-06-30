/*
 * Moon Phase -- localization.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>

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

/** Localized name of a principal phase (type 0..3, see MoonPhaseType). */
const char *i18n_phase_name(Lang lang, int phase_type);

/**
 * Localized relative-day string into `buf`.
 * days == 0 -> "Today", 1 -> "Tomorrow", otherwise "In N days".
 */
void i18n_relative(Lang lang, int days, char *buf, size_t len);

/**
 * Localized calendar date into `buf` (month0 is 0-based). When `abbrev` is true
 * the month name is shortened to three characters so it fits the narrower round
 * displays (e.g. "novembre" -> "nov").
 */
void i18n_date(Lang lang, int day, int month0, bool abbrev, char *buf, size_t len);

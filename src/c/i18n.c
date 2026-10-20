/*
 * Moon Phase -- localization.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "i18n.h"
#include <string.h>

// Date layout conventions.
typedef enum {
  DS_MONTH_D,      // "June 30"
  DS_D_MONTH,      // "30 juin"
  DS_D_DOT_MONTH,  // "30. Juni"
  DS_D_DE_MONTH,   // "30 de junio"
  DS_ZH,           // "6月30日"
} DateStyle;

typedef struct {
  const char *phases[I18N_NAME_COUNT];   // new, first quarter, full, last quarter, supermoon
  const char *today;
  const char *tomorrow;
  const char *in_days;     // printf format with a single %d
  const char *months[12];  // unused for DS_ZH
  // Short month names where cutting to three letters would be wrong or
  // ambiguous; NULL to cut.
  const char *const *months_short;
  DateStyle date_style;
} LangStrings;

// "juin" and "juillet" would both cut to "jui".
static const char *const FR_MONTHS_SHORT[12] = {
  "janv.", "févr.", "mars", "avr.", "mai", "juin",
  "juil.", "août", "sept.", "oct.", "nov.", "déc.",
};

static const LangStrings LANGS[LANG_COUNT] = {
  [LANG_EN] = {
    .phases = {"New Moon", "First Quarter", "Full Moon", "Last Quarter", "Supermoon"},
    .today = "Today", .tomorrow = "Tomorrow", .in_days = "In %d days",
    .months = {"January", "February", "March", "April", "May", "June",
               "July", "August", "September", "October", "November", "December"},
    .date_style = DS_MONTH_D,
  },
  [LANG_FR] = {
    .phases = {"Nouvelle lune", "Premier quartier", "Pleine lune", "Dernier quartier", "Super lune"},
    .today = "Aujourd'hui", .tomorrow = "Demain", .in_days = "Dans %d jours",
    .months = {"janvier", "février", "mars", "avril", "mai", "juin",
               "juillet", "août", "septembre", "octobre", "novembre", "décembre"},
    .months_short = FR_MONTHS_SHORT,
    .date_style = DS_D_MONTH,
  },
  [LANG_DE] = {
    .phases = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel", "Supermond"},
    .today = "Heute", .tomorrow = "Morgen", .in_days = "In %d Tagen",
    .months = {"Januar", "Februar", "März", "April", "Mai", "Juni",
               "Juli", "August", "September", "Oktober", "November", "Dezember"},
    .date_style = DS_D_DOT_MONTH,
  },
  [LANG_ES] = {
    .phases = {"Luna nueva", "Cuarto creciente", "Luna llena", "Cuarto menguante", "Superluna"},
    .today = "Hoy", .tomorrow = "Mañana", .in_days = "En %d días",
    .months = {"enero", "febrero", "marzo", "abril", "mayo", "junio",
               "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"},
    .date_style = DS_D_DE_MONTH,
  },
  [LANG_IT] = {
    .phases = {"Luna nuova", "Primo quarto", "Luna piena", "Ultimo quarto", "Superluna"},
    .today = "Oggi", .tomorrow = "Domani", .in_days = "Tra %d giorni",
    .months = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
               "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"},
    .date_style = DS_D_MONTH,
  },
  [LANG_PT] = {
    .phases = {"Lua nova", "Quarto crescente", "Lua cheia", "Quarto minguante", "Superlua"},
    .today = "Hoje", .tomorrow = "Amanhã", .in_days = "Em %d dias",
    .months = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"},
    .date_style = DS_D_DE_MONTH,
  },
  [LANG_ZH_CN] = {
    .phases = {"新月", "上弦月", "满月", "下弦月", "超级月亮"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天后",
    .months = {0},
    .date_style = DS_ZH,
  },
  [LANG_ZH_TW] = {
    .phases = {"新月", "上弦月", "滿月", "下弦月", "超級月亮"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天後",
    .months = {0},
    .date_style = DS_ZH,
  },
};

#if !defined(PBL_PLATFORM_APLITE)
// The launcher glance draws its subtitle on a single line, clipped with an
// ellipsis around 19 characters on the widest watch and 16 on the others, so
// it takes its own shortened names. Aplite has no glance.
typedef struct {
  const char *phases[I18N_NAME_COUNT];  // on the day of the phase, the line holds nothing else
  const char *wait[I18N_NAME_COUNT];    // ahead of it, the line also carries the countdown
  const char *in_days;    // template branch; %d is expanded by the firmware
  const char *tomorrow;
} GlanceStrings;

static const GlanceStrings GLANCES[LANG_COUNT] = {
  [LANG_EN] = {
    .phases = {"New moon", "1st quarter", "Full moon", "Last quarter", "Supermoon"},
    .wait = {"New moon", "1st quarter", "Full moon", "Last quarter", "Supermoon"},
    .in_days = " in %dd", .tomorrow = " tomorrow",
  },
  [LANG_FR] = {
    .phases = {"Nouvelle lune", "1er quartier", "Pleine lune", "Dern. quartier", "Super lune"},
    .wait = {"Nelle lune", "1er quart.", "Pleine lune", "Dern. quart.", "Super lune"},
    .in_days = " dans %d j", .tomorrow = " demain",
  },
  [LANG_DE] = {
    .phases = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel", "Supermond"},
    .wait = {"Neumond", "1. Viertel", "Vollmond", "Letztes V.", "Supermond"},
    .in_days = " in %d T", .tomorrow = " morgen",
  },
  [LANG_ES] = {
    .phases = {"Luna nueva", "C. creciente", "Luna llena", "C. menguante", "Superluna"},
    .wait = {"L. nueva", "C. crec.", "L. llena", "C. meng.", "Superluna"},
    .in_days = " en %d d", .tomorrow = " mañana",
  },
  [LANG_IT] = {
    .phases = {"Luna nuova", "Primo quarto", "Luna piena", "Ultimo quarto", "Superluna"},
    .wait = {"L. nuova", "1º quarto", "L. piena", "Ult. quarto", "Superluna"},
    .in_days = " tra %d g", .tomorrow = " domani",
  },
  [LANG_PT] = {
    .phases = {"Lua nova", "Q. crescente", "Lua cheia", "Q. minguante", "Superlua"},
    .wait = {"L. nova", "Q. cresc.", "L. cheia", "Q. ming.", "Superlua"},
    .in_days = " em %d d", .tomorrow = " amanhã",
  },
  [LANG_ZH_CN] = {
    .phases = {"新月", "上弦月", "满月", "下弦月", "超级月亮"},
    .wait = {"新月", "上弦月", "满月", "下弦月", "超级月亮"},
    .in_days = " %d天后", .tomorrow = " 明天",
  },
  [LANG_ZH_TW] = {
    .phases = {"新月", "上弦月", "滿月", "下弦月", "超級月亮"},
    .wait = {"新月", "上弦月", "滿月", "下弦月", "超級月亮"},
    .in_days = " %d天後", .tomorrow = " 明天",
  },
};
#endif

Lang i18n_lang(void) {
  const char *loc = i18n_get_system_locale();
  if (!loc) {
    return LANG_EN;
  }
  // Chinese: by language tag, or by the region codes the user listed.
  if (strncmp(loc, "zh", 2) == 0) {
    return (strstr(loc, "TW") || strstr(loc, "HK")) ? LANG_ZH_TW : LANG_ZH_CN;
  }
  if (strstr(loc, "TW") || strstr(loc, "HK")) {
    return LANG_ZH_TW;
  }
  if (strstr(loc, "CN")) {
    return LANG_ZH_CN;
  }
  if (strncmp(loc, "fr", 2) == 0) return LANG_FR;
  if (strncmp(loc, "de", 2) == 0) return LANG_DE;
  if (strncmp(loc, "es", 2) == 0) return LANG_ES;
  if (strncmp(loc, "it", 2) == 0) return LANG_IT;
  if (strncmp(loc, "pt", 2) == 0) return LANG_PT;
  return LANG_EN;
}

const char *i18n_phase_name(Lang lang, int name) {
  return LANGS[lang].phases[name % I18N_NAME_COUNT];
}

#if !defined(PBL_PLATFORM_APLITE)
const char *i18n_glance_phase(Lang lang, int name, bool waiting) {
  const GlanceStrings *s = &GLANCES[lang];
  return waiting ? s->wait[name % I18N_NAME_COUNT] : s->phases[name % I18N_NAME_COUNT];
}
#endif

int i18n_event_name(const MoonPhaseEvent *event) {
  return moon_phase_is_supermoon(event) ? I18N_SUPERMOON : (int)event->type;
}

#if !defined(PBL_PLATFORM_APLITE)
const char *i18n_glance_in_days(Lang lang) {
  return GLANCES[lang].in_days;
}

const char *i18n_glance_tomorrow(Lang lang) {
  return GLANCES[lang].tomorrow;
}
#endif

void i18n_relative(Lang lang, int days, char *buf, size_t len) {
  const LangStrings *s = &LANGS[lang];
  if (days <= 0) {
    snprintf(buf, len, "%s", s->today);
  } else if (days == 1) {
    snprintf(buf, len, "%s", s->tomorrow);
  } else {
    snprintf(buf, len, s->in_days, days);
  }
}

void i18n_date(Lang lang, int day, int month0, bool abbrev, char *buf, size_t len) {
  const LangStrings *s = &LANGS[lang];
  const char *month = s->months[month0];

  // Clip the month to its first three UTF-8 code points so long names
  // ("November", "septiembre") don't run past the edge, unless the language
  // has its own short names.
  char abuf[12];
  if (abbrev && s->months_short) {
    month = s->months_short[month0];
  } else if (abbrev && month) {
    size_t bi = 0;
    int cps = 0;
    while (month[bi] && cps < 3 && bi + 4 < sizeof(abuf)) {
      unsigned char c = (unsigned char)month[bi];
      int adv = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
      for (int k = 0; k < adv && month[bi]; k++, bi++) {
        abuf[bi] = month[bi];
      }
      cps++;
    }
    abuf[bi] = '\0';
    month = abuf;
  }

  switch (s->date_style) {
    case DS_MONTH_D:
      snprintf(buf, len, "%s %d", month, day);
      break;
    case DS_D_DOT_MONTH:
      snprintf(buf, len, "%d. %s", day, month);
      break;
    case DS_D_DE_MONTH:
      snprintf(buf, len, "%d de %s", day, month);
      break;
    case DS_ZH:
      snprintf(buf, len, "%d月%d日", month0 + 1, day);
      break;
    case DS_D_MONTH:
    default:
      snprintf(buf, len, "%d %s", day, month);
      break;
  }
}

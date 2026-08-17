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
  const char *phases[4];   // new, first quarter, full, last quarter
  const char *today;
  const char *tomorrow;
  const char *in_days;     // printf format with a single %d
  const char *header;
  const char *months[12];  // unused for DS_ZH
  DateStyle date_style;
  // The launcher glance draws its subtitle on a single line, clipped with an
  // ellipsis around 19 characters on the widest watch and 16 on the others, so
  // it takes its own shortened names.
  const char *glance_phases[4];  // on the day of the phase, the line holds nothing else
  const char *glance_wait[4];    // ahead of it, the line also carries the countdown
  const char *glance_in_days;    // template branch; %d is expanded by the firmware
  const char *glance_tomorrow;
} LangStrings;

static const LangStrings LANGS[LANG_COUNT] = {
  [LANG_EN] = {
    .phases = {"New Moon", "First Quarter", "Full Moon", "Last Quarter"},
    .today = "Today", .tomorrow = "Tomorrow", .in_days = "In %d days",
    .header = "Upcoming phases",
    .months = {"January", "February", "March", "April", "May", "June",
               "July", "August", "September", "October", "November", "December"},
    .date_style = DS_MONTH_D,
    .glance_phases = {"New moon", "1st quarter", "Full moon", "Last quarter"},
    .glance_wait = {"New moon", "1st quarter", "Full moon", "Last quarter"},
    .glance_in_days = " in %dd", .glance_tomorrow = " tomorrow",
  },
  [LANG_FR] = {
    .phases = {"Nouvelle lune", "Premier quartier", "Pleine lune", "Dernier quartier"},
    .today = "Aujourd'hui", .tomorrow = "Demain", .in_days = "Dans %d jours",
    .header = "Prochaines phases",
    .months = {"janvier", "février", "mars", "avril", "mai", "juin",
               "juillet", "août", "septembre", "octobre", "novembre", "décembre"},
    .date_style = DS_D_MONTH,
    .glance_phases = {"Nouvelle lune", "1er quartier", "Pleine lune", "Dern. quartier"},
    .glance_wait = {"Nelle lune", "1er quart.", "Pleine lune", "Dern. quart."},
    .glance_in_days = " dans %d j", .glance_tomorrow = " demain",
  },
  [LANG_DE] = {
    .phases = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel"},
    .today = "Heute", .tomorrow = "Morgen", .in_days = "In %d Tagen",
    .header = "Nächste Phasen",
    .months = {"Januar", "Februar", "März", "April", "Mai", "Juni",
               "Juli", "August", "September", "Oktober", "November", "Dezember"},
    .date_style = DS_D_DOT_MONTH,
    .glance_phases = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel"},
    .glance_wait = {"Neumond", "1. Viertel", "Vollmond", "Letztes V."},
    .glance_in_days = " in %d T", .glance_tomorrow = " morgen",
  },
  [LANG_ES] = {
    .phases = {"Luna nueva", "Cuarto creciente", "Luna llena", "Cuarto menguante"},
    .today = "Hoy", .tomorrow = "Mañana", .in_days = "En %d días",
    .header = "Próximas fases",
    .months = {"enero", "febrero", "marzo", "abril", "mayo", "junio",
               "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"},
    .date_style = DS_D_DE_MONTH,
    .glance_phases = {"Luna nueva", "C. creciente", "Luna llena", "C. menguante"},
    .glance_wait = {"L. nueva", "C. crec.", "L. llena", "C. meng."},
    .glance_in_days = " en %d d", .glance_tomorrow = " mañana",
  },
  [LANG_IT] = {
    .phases = {"Luna nuova", "Primo quarto", "Luna piena", "Ultimo quarto"},
    .today = "Oggi", .tomorrow = "Domani", .in_days = "Tra %d giorni",
    .header = "Prossime fasi",
    .months = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
               "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"},
    .date_style = DS_D_MONTH,
    .glance_phases = {"Luna nuova", "Primo quarto", "Luna piena", "Ultimo quarto"},
    .glance_wait = {"L. nuova", "1º quarto", "L. piena", "Ult. quarto"},
    .glance_in_days = " tra %d g", .glance_tomorrow = " domani",
  },
  [LANG_PT] = {
    .phases = {"Lua nova", "Quarto crescente", "Lua cheia", "Quarto minguante"},
    .today = "Hoje", .tomorrow = "Amanhã", .in_days = "Em %d dias",
    .header = "Próximas fases",
    .months = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"},
    .date_style = DS_D_DE_MONTH,
    .glance_phases = {"Lua nova", "Q. crescente", "Lua cheia", "Q. minguante"},
    .glance_wait = {"L. nova", "Q. cresc.", "L. cheia", "Q. ming."},
    .glance_in_days = " em %d d", .glance_tomorrow = " amanhã",
  },
  [LANG_ZH_CN] = {
    .phases = {"新月", "上弦月", "满月", "下弦月"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天后",
    .header = "接下来的月相",
    .months = {0},
    .date_style = DS_ZH,
    .glance_phases = {"新月", "上弦月", "满月", "下弦月"},
    .glance_wait = {"新月", "上弦月", "满月", "下弦月"},
    .glance_in_days = " %d天后", .glance_tomorrow = " 明天",
  },
  [LANG_ZH_TW] = {
    .phases = {"新月", "上弦月", "滿月", "下弦月"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天後",
    .header = "接下來的月相",
    .months = {0},
    .date_style = DS_ZH,
    .glance_phases = {"新月", "上弦月", "滿月", "下弦月"},
    .glance_wait = {"新月", "上弦月", "滿月", "下弦月"},
    .glance_in_days = " %d天後", .glance_tomorrow = " 明天",
  },
};

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

const char *i18n_phase_name(Lang lang, int phase_type) {
  return LANGS[lang].phases[phase_type & 3];
}

const char *i18n_glance_phase(Lang lang, int phase_type, bool waiting) {
  const LangStrings *s = &LANGS[lang];
  return waiting ? s->glance_wait[phase_type & 3] : s->glance_phases[phase_type & 3];
}

const char *i18n_glance_in_days(Lang lang) {
  return LANGS[lang].glance_in_days;
}

const char *i18n_glance_tomorrow(Lang lang) {
  return LANGS[lang].glance_tomorrow;
}

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

  // On round displays, clip the month to its first three UTF-8 code points so
  // long names ("novembre", "September") don't run past the rounded edge.
  char abuf[12];
  if (abbrev && month) {
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

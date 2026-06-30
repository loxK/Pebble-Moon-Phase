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
} LangStrings;

static const LangStrings LANGS[LANG_COUNT] = {
  [LANG_EN] = {
    .phases = {"New Moon", "First Quarter", "Full Moon", "Last Quarter"},
    .today = "Today", .tomorrow = "Tomorrow", .in_days = "In %d days",
    .header = "Upcoming phases",
    .months = {"January", "February", "March", "April", "May", "June",
               "July", "August", "September", "October", "November", "December"},
    .date_style = DS_MONTH_D,
  },
  [LANG_FR] = {
    .phases = {"Nouvelle lune", "Premier quartier", "Pleine lune", "Dernier quartier"},
    .today = "Aujourd'hui", .tomorrow = "Demain", .in_days = "Dans %d jours",
    .header = "Prochaines phases",
    .months = {"janvier", "février", "mars", "avril", "mai", "juin",
               "juillet", "août", "septembre", "octobre", "novembre", "décembre"},
    .date_style = DS_D_MONTH,
  },
  [LANG_DE] = {
    .phases = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel"},
    .today = "Heute", .tomorrow = "Morgen", .in_days = "In %d Tagen",
    .header = "Nächste Phasen",
    .months = {"Januar", "Februar", "März", "April", "Mai", "Juni",
               "Juli", "August", "September", "Oktober", "November", "Dezember"},
    .date_style = DS_D_DOT_MONTH,
  },
  [LANG_ES] = {
    .phases = {"Luna nueva", "Cuarto creciente", "Luna llena", "Cuarto menguante"},
    .today = "Hoy", .tomorrow = "Mañana", .in_days = "En %d días",
    .header = "Próximas fases",
    .months = {"enero", "febrero", "marzo", "abril", "mayo", "junio",
               "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"},
    .date_style = DS_D_DE_MONTH,
  },
  [LANG_IT] = {
    .phases = {"Luna nuova", "Primo quarto", "Luna piena", "Ultimo quarto"},
    .today = "Oggi", .tomorrow = "Domani", .in_days = "Tra %d giorni",
    .header = "Prossime fasi",
    .months = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno",
               "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"},
    .date_style = DS_D_MONTH,
  },
  [LANG_PT] = {
    .phases = {"Lua nova", "Quarto crescente", "Lua cheia", "Quarto minguante"},
    .today = "Hoje", .tomorrow = "Amanhã", .in_days = "Em %d dias",
    .header = "Próximas fases",
    .months = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
               "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"},
    .date_style = DS_D_DE_MONTH,
  },
  [LANG_ZH_CN] = {
    .phases = {"新月", "上弦月", "满月", "下弦月"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天后",
    .header = "接下来的月相",
    .months = {0},
    .date_style = DS_ZH,
  },
  [LANG_ZH_TW] = {
    .phases = {"新月", "上弦月", "滿月", "下弦月"},
    .today = "今天", .tomorrow = "明天", .in_days = "%d 天後",
    .header = "接下來的月相",
    .months = {0},
    .date_style = DS_ZH,
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

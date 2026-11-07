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
  // Shorter phase names for a line too narrow for the full ones; NULL if none.
  const char *const *phases_short;
  const char *today;
  const char *tomorrow;
  const char *in_days;     // printf format with a single %d
  const char *months[12];  // unused for DS_ZH
  // Short month names where cutting to three letters would be wrong or
  // ambiguous; NULL to cut.
  const char *const *months_short;
  DateStyle date_style;
} LangStrings;

// "Cuarto menguante" runs past the edge of 144 px screens even at 24 px.
static const char *const ES_PHASES_SHORT[I18N_NAME_COUNT] = {
  "Luna nueva", "C. creciente", "Luna llena", "C. menguante", "Superluna",
};

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
    .phases_short = ES_PHASES_SHORT,
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

#if MOON_HAS_DETAILS
// No-break space, UTF-8.
#define NBSP "\xC2\xA0"

// Words of the detail view, the same as the timeline pins (src/pkjs/index.js).
typedef struct {
  const char *eclipse[2];        // total, partial
  const char *apsis[2];          // perigee, apogee
  const char *node[2];           // ascending, descending
  const char *distance[5];       // very close .. very far
  const char *words[I18N_WORD_COUNT];
  const char *thousands;         // between groups of three digits
  const char *percent_space;     // between a number and its percent sign
} DetailStrings;

static const DetailStrings DETAILS[LANG_COUNT] = {
  [LANG_EN] = {
    .eclipse = {"Total eclipse", "Partial eclipse"},
    .apsis = {"Perigee", "Apogee"},
    .node = {"Ascending node", "Descending node"},
    .distance = {"Very close", "Close", "Midway", "Far", "Very far"},
    .words = {"Distance", "Next node", "Next eclipse", "Now",
              "Next perigee", "Next apogee", "Toward perigee", "Toward apogee"},
    .thousands = ",", .percent_space = "",
  },
  [LANG_FR] = {
    .eclipse = {"Éclipse totale", "Éclipse partielle"},
    .apsis = {"Périgée", "Apogée"},
    .node = {"Nœud ascendant", "Nœud descendant"},
    .distance = {"Très proche", "Proche", "À mi-distance", "Lointaine", "Très lointaine"},
    .words = {"Distance", "Prochain nœud", "Prochaine éclipse", "Maintenant",
              "Prochain périgée", "Prochain apogée", "Vers le périgée", "Vers l’apogée"},
    .thousands = NBSP, .percent_space = NBSP,
  },
  [LANG_DE] = {
    .eclipse = {"Totale Finsternis", "Partielle Finsternis"},
    .apsis = {"Perigäum", "Apogäum"},
    .node = {"Aufsteigender Knoten", "Absteigender Knoten"},
    .distance = {"Sehr nah", "Nah", "Mittlere Entfernung", "Fern", "Sehr fern"},
    .words = {"Entfernung", "Nächster Knoten", "Nächste Finsternis", "Jetzt",
              "Nächstes Perigäum", "Nächstes Apogäum", "Richtung Perigäum", "Richtung Apogäum"},
    .thousands = ".", .percent_space = NBSP,
  },
  [LANG_ES] = {
    .eclipse = {"Eclipse total", "Eclipse parcial"},
    .apsis = {"Perigeo", "Apogeo"},
    .node = {"Nodo ascendente", "Nodo descendente"},
    .distance = {"Muy cerca", "Cerca", "A media distancia", "Lejos", "Muy lejos"},
    .words = {"Distancia", "Próximo nodo", "Próximo eclipse", "Ahora",
              "Próximo perigeo", "Próximo apogeo", "Hacia el perigeo", "Hacia el apogeo"},
    .thousands = ".", .percent_space = NBSP,
  },
  [LANG_IT] = {
    .eclipse = {"Eclissi totale", "Eclissi parziale"},
    .apsis = {"Perigeo", "Apogeo"},
    .node = {"Nodo ascendente", "Nodo discendente"},
    .distance = {"Molto vicina", "Vicina", "A metà distanza", "Lontana", "Molto lontana"},
    .words = {"Distanza", "Prossimo nodo", "Prossima eclissi", "Adesso",
              "Prossimo perigeo", "Prossimo apogeo", "Verso il perigeo", "Verso l’apogeo"},
    .thousands = ".", .percent_space = "",
  },
  [LANG_PT] = {
    .eclipse = {"Eclipse total", "Eclipse parcial"},
    .apsis = {"Perigeu", "Apogeu"},
    .node = {"Nodo ascendente", "Nodo descendente"},
    .distance = {"Muito perto", "Perto", "A meia distância", "Longe", "Muito longe"},
    .words = {"Distância", "Próximo nodo", "Próximo eclipse", "Agora",
              "Próximo perigeu", "Próximo apogeu", "Rumo ao perigeu", "Rumo ao apogeu"},
    .thousands = ".", .percent_space = "",
  },
  [LANG_ZH_CN] = {
    .eclipse = {"月全食", "月偏食"},
    .apsis = {"近地点", "远地点"},
    .node = {"升交点", "降交点"},
    .distance = {"非常近", "较近", "中等距离", "较远", "非常远"},
    .words = {"距离", "下一交点", "下次月食", "现在",
              "下次近地点", "下次远地点", "趋向近地点", "趋向远地点"},
    .thousands = ",", .percent_space = "",
  },
  [LANG_ZH_TW] = {
    .eclipse = {"月全食", "月偏食"},
    .apsis = {"近地點", "遠地點"},
    .node = {"升交點", "降交點"},
    .distance = {"非常近", "較近", "中等距離", "較遠", "非常遠"},
    .words = {"距離", "下一交點", "下次月食", "現在",
              "下次近地點", "下次遠地點", "趨向近地點", "趨向遠地點"},
    .thousands = ",", .percent_space = "",
  },
};
#endif

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
    .phases = {"New Moon", "1st Quarter", "Full Moon", "Last Quarter", "Supermoon"},
    .wait = {"New Moon", "1st Quarter", "Full Moon", "Last Quarter", "Supermoon"},
    // " tomorrow" runs past the edge after most names on 144 px watches.
    .in_days = " in %dd", .tomorrow = " in 1d",
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

const char *i18n_phase_name_short(Lang lang, int name) {
  const char *const *names = LANGS[lang].phases_short;
  return names ? names[name % I18N_NAME_COUNT] : LANGS[lang].phases[name % I18N_NAME_COUNT];
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

void i18n_time(Lang lang, const struct tm *t, char *buf, size_t len) {
  if (clock_is_24h_style()) {
    snprintf(buf, len, "%02d:%02d", t->tm_hour, t->tm_min);
    return;
  }
  // strftime's %l pads the hour with a space, which would shift a centred
  // line; only the AM/PM marker comes from it, in the watch's language. A
  // watch language the app does not speak falls back to English, and so does
  // the marker, rather than mixing two languages on one line.
  char am_pm[8] = "";  // strftime leaves it as is when the marker does not fit
  if (lang == LANG_EN) {
    strncpy(am_pm, t->tm_hour < 12 ? "AM" : "PM", sizeof(am_pm));
  } else {
    strftime(am_pm, sizeof(am_pm), "%p", t);
  }
  int h = t->tm_hour % 12;
  snprintf(buf, len, "%d:%02d %s", h ? h : 12, t->tm_min, am_pm);
}

#if MOON_HAS_DETAILS
const char *i18n_eclipse_name(Lang lang, bool total) {
  return DETAILS[lang].eclipse[total ? 0 : 1];
}

const char *i18n_apsis_name(Lang lang, bool perigee) {
  return DETAILS[lang].apsis[perigee ? 0 : 1];
}

const char *i18n_node_name(Lang lang, bool ascending) {
  return DETAILS[lang].node[ascending ? 0 : 1];
}

const char *i18n_distance_label(Lang lang, int distance_class) {
  return DETAILS[lang].distance[distance_class < 0 ? 0 : distance_class > 4 ? 4 : distance_class];
}

const char *i18n_word(Lang lang, I18nWord word) {
  return DETAILS[lang].words[word];
}

void i18n_short_date(Lang lang, const struct tm *t, bool with_year, char *buf, size_t len) {
  char date[24];
  i18n_date(lang, t->tm_mday, t->tm_mon, true, date, sizeof(date));
  if (!with_year) {
    snprintf(buf, len, "%s", date);
    return;
  }
  int year = t->tm_year + 1900;
  switch (LANGS[lang].date_style) {
    case DS_MONTH_D:
      snprintf(buf, len, "%s, %d", date, year);
      break;
    case DS_ZH:
      snprintf(buf, len, "%d年%s", year, date);
      break;
    default:
      snprintf(buf, len, "%s %d", date, year);
      break;
  }
}

void i18n_km(Lang lang, int32_t km, char *buf, size_t len) {
  // Distances stay below a million km: two groups at most.
  const char *sep = DETAILS[lang].thousands;
  if (km >= 1000) {
    snprintf(buf, len, "%ld%s%03ld" NBSP "km", (long)(km / 1000), sep, (long)(km % 1000));
  } else {
    snprintf(buf, len, "%ld" NBSP "km", (long)km);
  }
}

void i18n_percent(Lang lang, int percent, char *buf, size_t len) {
  snprintf(buf, len, "%d%s%%", percent, DETAILS[lang].percent_space);
}
#endif

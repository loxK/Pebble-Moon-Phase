/*
 * Moon Phase -- launcher glance.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "glance.h"
#include "moon_phase.h"
#include <string.h>

// The eight slices the launcher keeps are cut at icon and phase changes, which
// come at least every few days, so they never reach beyond a lunation.
#define PHASE_LOOKAHEAD 5
#define DAY_HORIZON 32

// Subtitles are capped at 150 bytes by the firmware; the longest template here
// is a phase name followed by the countdown filter.
#define SUBTITLE_MAX 112

// Published resource ids in lunation order, one every eighth of a cycle.
static const uint32_t ICONS[8] = {
  PUBLISHED_ID_MOON_NEW,   PUBLISHED_ID_MOON_WAX_CRESC,
  PUBLISHED_ID_MOON_FIRST, PUBLISHED_ID_MOON_WAX_GIBB,
  PUBLISHED_ID_MOON_FULL,  PUBLISHED_ID_MOON_WAN_GIBB,
  PUBLISHED_ID_MOON_LAST,  PUBLISHED_ID_MOON_WAN_CRESC,
};

// Lunation position of each principal phase, in the order of MoonPhaseType.
static const double PHASE_FRACTION[4] = {0.0, 0.25, 0.5, 0.75};

typedef struct {
  Lang lang;
  bool southern;
} GlanceContext;

/**
 * Published icon depicting a given position in the lunation.
 *
 * Seen from the Southern hemisphere a phase is the mirror image of the same
 * phase seen from the North, and that mirror image is the artwork of the
 * opposite point in the lunation -- so the flip is a lookup, and the two
 * hemispheres share one set of images.
 */
static uint32_t prv_icon_for(double fraction, bool southern) {
  const double f = southern ? 1.0 - fraction : fraction;
  return ICONS[(int)(f * 8.0 + 0.5) & 7];
}

/** Local midnight opening the day that contains `t`. */
static time_t prv_day_start(time_t t) {
  struct tm lt = *localtime(&t);
  lt.tm_hour = 0;
  lt.tm_min = 0;
  lt.tm_sec = 0;
  return mktime(&lt);
}

/** Local midnight closing the day that contains `t`. */
static time_t prv_day_end(time_t t) {
  struct tm lt = *localtime(&t);
  lt.tm_mday++;  // mktime normalizes the overflow into the next month or year
  lt.tm_hour = 0;
  lt.tm_min = 0;
  lt.tm_sec = 0;
  return mktime(&lt);
}

static bool prv_add(AppGlanceReloadSession *session, const char *subtitle,
                    uint32_t icon, time_t expires) {
  const AppGlanceSlice slice = {
    .layout = {
      .icon = icon,
      .subtitle_template_string = subtitle,
    },
    .expiration_time = expires,
  };
  return app_glance_add_slice(session, slice) == APP_GLANCE_RESULT_SUCCESS;
}

/**
 * Describe one calendar day: the icon standing for the sky that evening, and
 * the line to show. Returns false once the phases run out.
 */
static bool prv_describe_day(const GlanceContext *ctx, const MoonPhaseEvent *events,
                             time_t day_begin, time_t day_finish,
                             uint32_t *icon_out, char *subtitle, size_t subtitle_size) {
  for (int i = 0; i < PHASE_LOOKAHEAD; i++) {
    if (events[i].time >= day_finish) {
      // Ahead of the phase. The countdown aims at the end of the phase's day,
      // not at the phase itself: the firmware only ever subtracts seconds, and
      // against that target its truncated day count is exactly the number of
      // calendar days left.
      snprintf(subtitle, subtitle_size, "%s{time_until(%ld)|format(>2d:'%s','%s')}",
               i18n_glance_phase(ctx->lang, events[i].type, true),
               (long)prv_day_end(events[i].time),
               i18n_glance_in_days(ctx->lang),
               i18n_glance_tomorrow(ctx->lang));
      // Noon stands for the day as a whole.
      *icon_out = prv_icon_for(moon_phase_state(day_begin / 2 + day_finish / 2).fraction,
                               ctx->southern);
      return true;
    }
    if (events[i].time >= day_begin) {
      // The phase falls on this very day: name it, and show it exactly.
      snprintf(subtitle, subtitle_size, "%s",
               i18n_glance_phase(ctx->lang, events[i].type, false));
      *icon_out = prv_icon_for(PHASE_FRACTION[events[i].type & 3], ctx->southern);
      return true;
    }
  }
  return false;
}

static void prv_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  const GlanceContext *ctx = context;
  const time_t now = time(NULL);

  // Start the search at this morning's midnight so a phase that already
  // occurred today still gets its day slice.
  MoonPhaseEvent events[PHASE_LOOKAHEAD];
  moon_phase_upcoming(prv_day_start(now), events, PHASE_LOOKAHEAD);

  char subtitle[SUBTITLE_MAX];
  char held[SUBTITLE_MAX];
  uint32_t held_icon = 0;
  time_t held_expires = 0;
  bool holding = false;
  size_t used = 0;

  // Walk day by day and only cut a slice where something actually changes --
  // the icon at a quarter of the disc, the line at a phase. Consecutive days
  // that would say the same thing ride on one slice, whose countdown the
  // launcher keeps re-evaluating on its own.
  time_t day_begin = prv_day_start(now);
  for (int i = 0; i < DAY_HORIZON && used < limit; i++) {
    const time_t day_finish = prv_day_end(day_begin);
    uint32_t icon;
    if (!prv_describe_day(ctx, events, day_begin, day_finish, &icon, subtitle, sizeof(subtitle))) {
      break;
    }

    if (holding && icon == held_icon && strcmp(subtitle, held) == 0) {
      held_expires = day_finish;
    } else {
      if (holding && !prv_add(session, held, held_icon, held_expires)) {
        return;
      }
      used += holding ? 1 : 0;
      strncpy(held, subtitle, sizeof(held));
      held[sizeof(held) - 1] = '\0';
      held_icon = icon;
      held_expires = day_finish;
      holding = true;
    }

    day_begin = day_finish;
  }

  if (holding && used < limit) {
    prv_add(session, held, held_icon, held_expires);
  }
}

void glance_publish(Lang lang, bool southern) {
  GlanceContext ctx = { .lang = lang, .southern = southern };
  app_glance_reload(prv_reload, &ctx);
}

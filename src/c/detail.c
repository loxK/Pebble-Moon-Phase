/*
 * Moon Phase -- the Moon now, in a deck of cards.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "detail.h"
#include "art.h"

#if MOON_HAS_DETAILS

// The night sky of the phase cards, without the stars, under a band that sets
// these cards apart.
#define DETAIL_BG COL_BG
#define BAND_BG PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorWhite)
#define BAND_FG PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack)

// How far the Moon is and which way it goes, the next apogee and perigee in
// the order they come, whether it passes higher or lower each day, the next
// node, the next eclipse.
#define MAX_CARDS 6
#define MAX_LINES 4
#define LINE_LEN 48

// The next node passage and the next declination turn come within half a
// month and a bit, the next apogee and perigee within a month and a bit.
#define SEARCH_S (16 * 86400)
#define APSIS_SEARCH_S (32 * 86400)

typedef enum {
  CARD_ART_DISTANCE,
  CARD_ART_APSIS,
  CARD_ART_NODE,
  CARD_ART_ECLIPSE,
  CARD_ART_SKY,
} CardArt;

typedef struct {
  CardArt art;
  int32_t km;        //!< CARD_ART_DISTANCE
  bool flag;         //!< perigee for CARD_ART_APSIS, ascending for CARD_ART_NODE
  int percent;       //!< CARD_ART_ECLIPSE: share of the diameter in the shadow
  int level;         //!< CARD_ART_SKY: how high the Moon passes, 0 to 100
  int trend;         //!< CARD_ART_SKY: 1 higher each day, -1 lower, 0 neither
  const char *heading;  //!< NULL for none
  int count;
  char lines[MAX_LINES][LINE_LEN];  //!< the first one set larger
  // When the event comes, drawn as "time | date" after line `when_line`.
  bool timed;
  int when_line;
  char clock[16];
  char date[32];
} DetailCard;

typedef struct {
  int count;
  DetailCard cards[MAX_CARDS];
} DetailContent;

static Window *s_window;
static Layer *s_layer;
static DetailContent *s_c;
static Lang s_lang;
static bool s_southern;
static int s_card;
static ArtSlide s_slide;

time_t detail_event_time(const MoonPhaseEvent *event) {
  return moon_eclipse_is_named(&event->eclipse) ? event->eclipse.time : event->time;
}

const char *detail_event_title(const MoonPhaseEvent *event, Lang lang) {
  if (moon_eclipse_is_named(&event->eclipse)) {
    return i18n_eclipse_name(lang, event->eclipse.type == MOON_ECLIPSE_TOTAL);
  }
  return i18n_phase_name(lang, i18n_event_name(event));
}

/** Share of the Moon's diameter in the shadow: 100 when total, at most 99 when partial. */
static int prv_shadow_percent(const MoonEclipse *e) {
  if (e->type == MOON_ECLIPSE_TOTAL) return 100;
  int percent = (int)(e->magnitude * 100.0f + 0.5f);
  return percent > 99 ? 99 : percent;
}

/** Local midnight at or before `t`. */
static time_t prv_midnight(time_t t) {
  struct tm tm = *localtime(&t);
  tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0; tm.tm_isdst = -1;
  return mktime(&tm);
}

static DetailCard *prv_add(DetailContent *c, CardArt art, const char *heading) {
  if (c->count >= MAX_CARDS) return NULL;
  DetailCard *card = &c->cards[c->count++];
  card->art = art;
  card->heading = heading;
  card->count = 0;
  return card;
}

static char *prv_line(DetailCard *card) {
  return card->count < MAX_LINES ? card->lines[card->count++] : card->lines[MAX_LINES - 1];
}

/** The time and short date of `t`, drawn after the last line added so far. */
static void prv_set_when(DetailCard *card, time_t t, bool with_year) {
  struct tm tm = *localtime(&t);
  card->timed = true;
  card->when_line = card->count - 1;
  i18n_short_date(s_lang, &tm, with_year, card->date, sizeof(card->date));
  i18n_time(s_lang, &tm, card->clock, sizeof(card->clock));
}

/** "Today", "Tomorrow" or "In N days" until `t`, then its time and short date. */
static void prv_add_when(DetailCard *card, time_t now, time_t t, bool with_year) {
  int days = (int)((prv_midnight(t) - prv_midnight(now) + 43200) / 86400);
  i18n_relative(s_lang, days, prv_line(card), LINE_LEN);
  prv_set_when(card, t, with_year);
}

// The declination keeps within this many degrees of the equator: 23.44 plus
// the widest inclination of the Moon's orbit, 5.30.
#define DECLINATION_MAX 28.8

/**
 * How high the Moon passes at declination `degrees`, from 0 to 100, seen from
 * the hemisphere of the app: highest when it is farthest on that side.
 */
static int prv_sky_level(double degrees) {
  double toward = s_southern ? -degrees : degrees;
  int level = (int)((toward + DECLINATION_MAX) * 50.0 / DECLINATION_MAX + 0.5);
  return level < 0 ? 0 : level > 100 ? 100 : level;
}

static void prv_build(DetailContent *c, time_t now) {
  Lang lang = s_lang;
  DetailCard *card;

  // How far the Moon is, and which way it goes.
  int32_t km = moon_distance_km(now);
  bool approaching = moon_is_approaching(now);
  card = prv_add(c, CARD_ART_DISTANCE, i18n_word(lang, I18N_DISTANCE));
  card->km = km;
  snprintf(prv_line(card), LINE_LEN, "%s",
           i18n_word(lang, approaching ? I18N_TOWARD_PERIGEE : I18N_TOWARD_APOGEE));
  snprintf(prv_line(card), LINE_LEN, "%s", i18n_distance_label(lang, moon_distance_class(km)));
  i18n_km(lang, km, prv_line(card), LINE_LEN);

  // The next apogee and perigee, which alternate: first the one it is heading
  // for, a perigee while it approaches, an apogee while it recedes.
  MoonApsis apsides[2];
  int na = moon_apsides(now, now + APSIS_SEARCH_S, apsides, 2);
  for (int i = 0; i < na; i++) {
    const MoonApsis *a = &apsides[i];
    card = prv_add(c, CARD_ART_APSIS,
                   i18n_word(lang, a->perigee ? I18N_NEXT_PERIGEE : I18N_NEXT_APOGEE));
    card->flag = a->perigee;
    prv_add_when(card, now, a->time, false);
    i18n_km(lang, a->distance_km, prv_line(card), LINE_LEN);
  }

  // Ascending while the declination grows toward the hemisphere of the app,
  // so that the Moon passes higher in its sky each day, until the next turn.
  bool ascending = moon_is_declination_northing(now) != s_southern;
  card = prv_add(c, CARD_ART_SKY, NULL);
  card->level = prv_sky_level(moon_position(now).declination);
  card->trend = ascending ? 1 : -1;
  snprintf(prv_line(card), LINE_LEN, "%s",
           i18n_word(lang, ascending ? I18N_ASCENDING : I18N_DESCENDING));
  MoonDeclinationTurn turn;
  if (moon_declination_turns(now, now + SEARCH_S, &turn, 1) == 1) {
    snprintf(prv_line(card), LINE_LEN, "%s",
             i18n_word(lang, turn.northern != s_southern ? I18N_HIGHEST : I18N_LOWEST));
    prv_set_when(card, turn.time, false);
  }

  MoonNode node;
  if (moon_nodes(now, now + SEARCH_S, &node, 1) == 1) {
    card = prv_add(c, CARD_ART_NODE, i18n_node_name(lang, node.ascending));
    card->flag = node.ascending;
    prv_add_when(card, now, node.time, false);
  }

  // The next named eclipse, dated with the year since it can be more than a
  // year ahead.
  MoonEclipse eclipse;
  if (moon_next_eclipse(now, moon_eclipse_is_named, &eclipse)) {
    card = prv_add(c, CARD_ART_ECLIPSE, i18n_word(lang, I18N_NEXT_ECLIPSE));
    card->percent = prv_shadow_percent(&eclipse);
    prv_add_when(card, now, eclipse.time, true);
    char percent[12];
    i18n_percent(lang, card->percent, percent, sizeof(percent));
    snprintf(prv_line(card), LINE_LEN, "%s (%s)",
             i18n_eclipse_name(lang, eclipse.type == MOON_ECLIPSE_TOTAL), percent);
  }
}

static int prv_band_height(GRect b) {
  return PBL_IF_ROUND_ELSE(b.size.h / 6, b.size.h >= 200 ? 28 : 22);
}

/** The band on top, naming what the cards stand for: the Moon now. */
static void prv_draw_band(GContext *ctx, GRect b) {
  int h = prv_band_height(b);
  graphics_context_set_fill_color(ctx, BAND_BG);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, h), 0, GCornerNone);
  graphics_context_set_text_color(ctx, BAND_FG);
  GFont font = fonts_get_system_font(b.size.h >= 200 ? FONT_KEY_GOTHIC_18_BOLD
                                                     : FONT_KEY_GOTHIC_14_BOLD);
  int text_h = b.size.h >= 200 ? 22 : 18;
  graphics_draw_text(ctx, i18n_word(s_lang, I18N_NOW), font,
                     GRect(4, h - text_h - 2, b.size.w - 8, text_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/** Draw the art of `card` filling at most `r` around `center`. */
static void prv_draw_art(GContext *ctx, const DetailCard *card, GPoint center, int r) {
  switch (card->art) {
    case CARD_ART_ECLIPSE:
      art_draw_eclipse(ctx, center, r, card->percent);
      break;
    case CARD_ART_APSIS:
      art_draw_apsis(ctx, center, r, card->flag);
      break;
    case CARD_ART_NODE:
      art_draw_node(ctx, center, r, card->flag);
      break;
    case CARD_ART_SKY:
      art_draw_sky(ctx, center, r, card->level, card->trend);
      break;
    case CARD_ART_DISTANCE: {
      // A full Moon over a bar that fills as the Moon comes closer.
      double v = art_closeness(card->km);
      art_draw_moon(ctx, center, r, 0.5, false, COL_FG, DETAIL_BG);
      int w = r * 2, x = center.x - r, y = center.y + r + 4;
      graphics_context_set_stroke_color(ctx, COL_FG);
      graphics_draw_round_rect(ctx, GRect(x, y, w, 5), 2);
      graphics_context_set_fill_color(ctx, COL_FG);
      graphics_fill_rect(ctx, GRect(x, y, (int)(w * v), 5), 2, GCornersAll);
      break;
    }
  }
}

/**
 * Draw "time | date" centred across `cw` from `margin`, at `y`: the date takes
 * what width the time leaves, wrapping when it must, and the bar runs the
 * height of whichever is taller, the other centred against it. Without `ctx`,
 * only measure.
 *
 * @return The bottom of the block.
 */
static int prv_draw_when(GContext *ctx, const DetailCard *card, GFont font, int margin, int cw,
                         int y) {
  const int gap = 6, bar = 2;
  GSize clock = graphics_text_layout_get_content_size(
      card->clock, font, GRect(0, 0, cw, 100), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  int room = cw - clock.w - 2 * gap - bar;
  GSize date = graphics_text_layout_get_content_size(
      card->date, font, GRect(0, 0, room, 100), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  int h = clock.h > date.h ? clock.h : date.h;
  int x = margin + (cw - (clock.w + 2 * gap + bar + date.w)) / 2;

  if (!ctx) return y + h;
  graphics_draw_text(ctx, card->clock, font, GRect(x, y + (h - clock.h) / 2, clock.w + 2, clock.h + 6),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  x += clock.w + gap;
  // The text's measured height starts above the cap line: the bar starts a
  // little lower so it lines up with the letters.
  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_fill_rect(ctx, GRect(x, y + 5, bar, h - 2), 0, GCornerNone);
  x += bar + gap;
  graphics_draw_text(ctx, card->date, font, GRect(x, y + (h - date.h) / 2, date.w + 2, date.h + 6),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  return y + h;
}

/**
 * Lay card `idx` out from `y`: its art, the same size on every card, then its
 * heading and lines under it, drawn when `ctx` is given. `compact` sets the
 * first line smaller, the margins narrower and the art a quarter smaller, for
 * a card that would not fit otherwise.
 *
 * @return The bottom of the card.
 */
static int prv_layout_card(GContext *ctx, GRect b, int idx, int y, bool compact) {
  const DetailCard *card = &s_c->cards[idx];
  bool large = b.size.h >= 200;
  int margin = PBL_IF_ROUND_ELSE(b.size.w / 8, compact ? 4 : 12);
  int cw = b.size.w - 2 * margin;
  GFont f_heading = fonts_get_system_font(large ? FONT_KEY_GOTHIC_18_BOLD : FONT_KEY_GOTHIC_14_BOLD);
  GFont f_first = fonts_get_system_font(
      compact ? (large ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_18_BOLD)
              : (large ? FONT_KEY_GOTHIC_28_BOLD : FONT_KEY_GOTHIC_24_BOLD));
  GFont f_rest = fonts_get_system_font(large ? FONT_KEY_GOTHIC_24 : FONT_KEY_GOTHIC_18);

  int r = b.size.h * PBL_IF_ROUND_ELSE(11, 13) / 100;
  if (compact) r = r * 3 / 4;
  if (ctx) prv_draw_art(ctx, card, GPoint(b.size.w / 2, y + r), r);
  y += 2 * r + (card->art == CARD_ART_DISTANCE ? 12 : 4);

  if (ctx) graphics_context_set_text_color(ctx, COL_FG);
  for (int i = card->heading ? -1 : 0; i < card->count; i++) {
    const char *text = i < 0 ? card->heading : card->lines[i];
    GFont font = i < 0 ? f_heading : i == 0 ? f_first : f_rest;
    int h = graphics_text_layout_get_content_size(
        text, font, GRect(0, 0, cw, 200), GTextOverflowModeWordWrap, GTextAlignmentCenter).h;
    if (ctx) {
      graphics_draw_text(ctx, text, font, GRect(margin, y, cw, h + 6),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
    y += h + 2;
    if (i == card->when_line && card->timed) y = prv_draw_when(ctx, card, f_rest, margin, cw, y) + 2;
  }
  return y;
}

/**
 * Draw card `idx` shifted down by `dy`, centred in the room under the band,
 * its first line smaller when the card would not fit.
 */
static void prv_draw_card(GContext *ctx, GRect b, int idx, int dy) {
  int top = prv_band_height(b);
  int bottom = b.size.h - PBL_IF_ROUND_ELSE(b.size.h / 12, 0);
  bool compact = false;
  int height = prv_layout_card(NULL, b, idx, 0, false);
  if (height > bottom - top) {
    compact = true;
    height = prv_layout_card(NULL, b, idx, 0, true);
  }
  int y = top + (bottom - top - height) / 2;
  if (y < top + 2) y = top + 2;
  prv_layout_card(ctx, b, idx, y + dy, compact);
}

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, DETAIL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  if (s_slide.anim) {
    int h = b.size.h;
    int shift = (int)(s_slide.progress * h);
    prv_draw_card(ctx, b, s_slide.from, -s_slide.dir * shift);
    prv_draw_card(ctx, b, s_card, s_slide.dir * (h - shift));
  } else {
    prv_draw_card(ctx, b, s_card, 0);
  }

  prv_draw_band(ctx, b);
  art_draw_pager(ctx, b, s_c->count, s_card);
}

static void prv_go(int dir) {
  int to = s_card + dir;
  if (to < 0 || to >= s_c->count) return;
  art_slide_start(&s_slide, s_card, dir);
  s_card = to;
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  prv_go(-1);
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  prv_go(1);
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_slide.layer = s_layer;
}

static void prv_window_unload(Window *window) {
  art_slide_stop(&s_slide);
  s_slide.layer = NULL;
  layer_destroy(s_layer);
  free(s_c);
  s_c = NULL;
  window_destroy(window);
  s_window = NULL;
}

void detail_push(time_t now, Lang lang, bool southern) {
  if (s_window) return;
  s_c = calloc(1, sizeof(DetailContent));
  if (!s_c) return;
  s_lang = lang;
  s_southern = southern;
  prv_build(s_c, now);
  s_card = 0;

  s_window = window_create();
  window_set_background_color(s_window, DETAIL_BG);
  window_set_click_config_provider(s_window, prv_click_config);
  window_set_window_handlers(s_window, (WindowHandlers){
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

#endif

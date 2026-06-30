/*
 * Moon Phase -- a Pebble watchapp showing the Moon and the upcoming phases.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <pebble.h>
#include <math.h>
#include "moon_phase.h"
#include "i18n.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------

#define PERSIST_KEY_HEMISPHERE 1

// Number of upcoming phase events kept for the main screen and the list.
#define EVENT_COUNT 8

// Colors. The phase screen uses a navy "night sky" background on color
// displays (black on B/W, where no blue is available); the Moon and text are
// drawn in white over it.
#define COL_BG PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define COL_FG GColorWhite

// ----------------------------------------------------------------------------
// State
// ----------------------------------------------------------------------------

static Window *s_main_window;
static Window *s_list_window;
static Layer *s_main_layer;
static MenuLayer *s_menu_layer;

static MoonState s_state;
static MoonPhaseEvent s_events[EVENT_COUNT];

// 0 = Northern hemisphere (default), 1 = Southern hemisphere.
static int s_southern;

// Active UI language, derived from the system locale at launch.
static Lang s_lang;

// Screen height of the list window, used to pick launcher-style metrics.
static int16_t s_screen_h;

// Launch animation: the lit area sweeps from new moon to the current phase.
static double s_anim_progress;  // 0..1 (eased), only meaningful while animating
static bool s_animating;
static bool s_did_animate;      // play once per launch, not on every appear

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// Current time. Set DBG_NOW to a fixed UTC epoch ONLY to capture store
// screenshots of a chosen phase; it must be 0 in the shipped app.
#define DBG_NOW 0  /* fixed UTC epoch for store screenshots only; 0 = live */
static time_t prv_now(void) { return DBG_NOW ? (time_t)DBG_NOW : time(NULL); }

// A value unique and increasing per local calendar day, for "is it today?".
static int prv_local_day(time_t t) {
  struct tm lt = *localtime(&t);
  return (lt.tm_year + 1900) * 366 + lt.tm_yday;
}

static void prv_refresh_data(void) {
  time_t now = prv_now();
  s_state = moon_phase_state(now);

  // A principal phase should stay featured for the whole local day it falls on,
  // even after its exact instant has passed (people think in days, not minutes).
  // Gather events from a couple of days back, then start at the first one whose
  // local date is today or later.
  MoonPhaseEvent buf[EVENT_COUNT + 2];
  moon_phase_upcoming(now - 2 * 86400, buf, EVENT_COUNT + 2);

  int today = prv_local_day(now);
  int start = 0;
  for (int i = 0; i < EVENT_COUNT + 2; i++) {
    if (prv_local_day(buf[i].time) >= today) { start = i; break; }
  }
  if (start > 2) start = 2;  // defensive: keep start + EVENT_COUNT within buf
  for (int i = 0; i < EVENT_COUNT; i++) {
    s_events[i] = buf[start + i];
  }
}

/**
 * Number of calendar days (local time) between `now` and `event`.
 * 0 = today, 1 = tomorrow, etc.
 */
static int prv_days_until(time_t now, time_t event) {
  struct tm a = *localtime(&now);
  a.tm_hour = 0; a.tm_min = 0; a.tm_sec = 0;
  time_t now_midnight = mktime(&a);

  struct tm b = *localtime(&event);
  b.tm_hour = 0; b.tm_min = 0; b.tm_sec = 0;
  time_t ev_midnight = mktime(&b);

  return (int)round((double)(ev_midnight - now_midnight) / 86400.0);
}

/** Format the relative day count for an event into `buf` (localized). */
static void prv_format_relative(time_t event, char *buf, size_t len) {
  int days = prv_days_until(prv_now(), event);
  i18n_relative(s_lang, days, buf, len);
}

/** Format the calendar date of an event into `buf` (localized). */
static void prv_format_date(time_t event, char *buf, size_t len) {
  struct tm t = *localtime(&event);
  // Round screens get the 3-letter month so long names don't run off the edge.
  i18n_date(s_lang, t.tm_mday, t.tm_mon, PBL_IF_ROUND_ELSE(true, false), buf, len);
}

// Integer square root (avoids the double libm sqrt, which is heavy on stack).
static int prv_isqrt(int n) {
  if (n <= 0) return 0;
  int x = n, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + n / x) / 2; }
  return x;
}

// Round a double to the nearest int without pulling in libm round().
#define MP_IRND(v) ((int)((v) < 0 ? (v) - 0.5 : (v) + 0.5))

/**
 * Draw a Moon at `center` with the given `radius`, depicting lunation position
 * `fraction` (0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter).
 *
 * The illuminated part is filled with `fg`, the dark part is `bg`, and the
 * disc rim is outlined with `fg`. `southern` mirrors the lit side left/right.
 */
static void prv_draw_moon(GContext *ctx, GPoint center, int radius,
                          double fraction, bool southern, GColor fg, GColor bg) {
  // Dark side: fill the whole disc with the background color.
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_circle(ctx, center, radius);

  // Illuminated side: fill scanlines between the terminator and the limb.
  graphics_context_set_stroke_color(ctx, fg);
  double cosp = mp_cos_deg(360.0 * fraction);
  bool waxing = fraction < 0.5;

  for (int dy = -radius; dy <= radius; dy++) {
    int xe = prv_isqrt(radius * radius - dy * dy);  // limb x at this row
    double xt = xe * cosp;                          // terminator x
    int lo, hi;
    if (waxing) {
      lo = MP_IRND(xt);
      hi = xe;
    } else {
      lo = -xe;
      hi = MP_IRND(-xt);
    }
    if (southern) {  // mirror left/right
      int nlo = -hi, nhi = -lo;
      lo = nlo; hi = nhi;
    }
    if (hi >= lo) {
      graphics_draw_line(ctx, GPoint(center.x + lo, center.y + dy),
                         GPoint(center.x + hi, center.y + dy));
    }
  }

  // Rim, so the dark side is always outlined.
  graphics_context_set_stroke_color(ctx, fg);
  graphics_draw_circle(ctx, center, radius);
}

/** Draw a single line of text horizontally centered in a content width. */
static int prv_draw_centered(GContext *ctx, const char *text, GFont font,
                             int content_w, int y, int h) {
  graphics_draw_text(ctx, text, font, GRect(0, y, content_w, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                     NULL);
  return y + h;
}

/**
 * Draw the system "SELECT does something" affordance: a filled circle glued to
 * the right edge, vertically centered next to the SELECT button, mostly off the
 * screen so only a rounded bump shows -- exactly like the Health / Timeline UI.
 * Rendered in `fg` so it stays legible over the background.
 */
static void prv_draw_select_hint(GContext *ctx, GRect b, GColor fg) {
  int radius = PBL_IF_ROUND_ELSE(12, 13);
  int off = PBL_IF_ROUND_ELSE((b.size.h >= 200 ? 4 : 1),
                              (b.size.h >= 200 ? 4 : 8));
  GPoint center = GPoint(b.size.w + off, b.size.h / 2);
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_circle(ctx, center, radius);
}

// ----------------------------------------------------------------------------
// Main window
// ----------------------------------------------------------------------------

/**
 * Per-line stagger for the launch animation. Returns the vertical rise offset
 * for a text line and whether it is visible yet, given the global progress `p`
 * and the line's [start, end] window.
 */
static int prv_line_anim(double p, double start, double end, bool *visible) {
  if (p < start) { *visible = false; return 0; }
  *visible = true;
  if (p >= end) return 0;
  double lp = (p - start) / (end - start);
  lp = 1.0 - (1.0 - lp) * (1.0 - lp);  // ease out
  return (int)((1.0 - lp) * 18.0);
}

// A sparse starfield behind the scene. Positions are percentages of the screen,
// kept in the periphery (never over the Moon or the text). Each star "twinkles
// in" once the launch animation passes its threshold; after the animation they
// all stay, quietly. On round screens the corner stars fall outside the bezel.
typedef struct { uint8_t nx, ny, sz, at; } Star;  // position %, size (0 dot/1 sparkle), reveal %
static const Star s_stars[] = {
  {10, 11, 1, 25}, {16, 19, 0, 58},   // a close pair, top-left
  {85, 12, 1, 38},                    // lone top-right
  {83, 37, 0, 70},                    // right of the Moon, mid-height
  { 7, 53, 0, 47},                    // low-left
  {88, 56, 1, 64},                    // low-right
};

static void prv_draw_star(GContext *ctx, int x, int y, int sz) {
  graphics_fill_circle(ctx, GPoint(x, y), 1);
  if (sz) {  // brighter stars get a small sparkle cross
    graphics_draw_pixel(ctx, GPoint(x - 2, y));
    graphics_draw_pixel(ctx, GPoint(x + 2, y));
    graphics_draw_pixel(ctx, GPoint(x, y - 2));
    graphics_draw_pixel(ctx, GPoint(x, y + 2));
  }
}

static void prv_draw_stars(GContext *ctx, GRect b) {
  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_context_set_stroke_color(ctx, COL_FG);
  int prog = s_animating ? (int)(s_anim_progress * 100.0) : 100;
  for (unsigned i = 0; i < sizeof(s_stars) / sizeof(s_stars[0]); i++) {
    if (prog < s_stars[i].at) continue;
    prv_draw_star(ctx, s_stars[i].nx * b.size.w / 100, s_stars[i].ny * b.size.h / 100, s_stars[i].sz);
  }
}

static void prv_main_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  prv_draw_stars(ctx, b);
  graphics_context_set_text_color(ctx, COL_FG);

  // The SELECT bump pokes in from the right edge; reserve a little room there.
  int cw = b.size.w - PBL_IF_ROUND_ELSE(0, 10);
  int cx = cw / 2;

  // Next phase data.
  MoonPhaseEvent *next = &s_events[0];
  const char *name_str = i18n_phase_name(s_lang, next->type);
  char relbuf[24], datebuf[24];
  prv_format_relative(next->time, relbuf, sizeof(relbuf));
  prv_format_date(next->time, datebuf, sizeof(datebuf));

  // The phase name uses the big font and wraps to two lines when long, so the
  // layout (and the Moon above it) adapts instead of truncating.
  GFont f_name = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_dans = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_date = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  GSize name_sz = graphics_text_layout_get_content_size(
      name_str, f_name, GRect(0, 0, cw, 64),
      GTextOverflowModeWordWrap, GTextAlignmentCenter);
  int name_h = name_sz.h;

  // Text block sits at the bottom; the Moon fills the area above it.
  int y_date = b.size.h - 26;
  int y_dans = y_date - 30;
  int y_name = y_dans - name_h;
  int moon_bottom = y_name - 2;

  // Moon geometry.
  int moon_cy = 6 + moon_bottom / 2;
  int max_r_w = cx - 8;
  int max_r_h = (moon_bottom - 10) / 2;
  int r = max_r_w < max_r_h ? max_r_w : max_r_h;
  if (r > 46) r = 46;
  if (r < 18) r = 18;

  // Launch animation: sweep the lit area from new moon to the current phase,
  // and scale the disc in.
  double frac = s_state.fraction;
  int draw_r = r;
  if (s_animating) {
    double p = s_anim_progress;
    double mp = p * 2.0;            // the Moon sweeps over the first half (~700 ms)
    if (mp > 1.0) mp = 1.0;
    double me = mp * mp * (3.0 - 2.0 * mp);  // smoothstep ease-in-out
    frac = s_state.fraction * me;
    draw_r = (int)(r * (0.82 + 0.18 * me));
  }
  prv_draw_moon(ctx, GPoint(cx, moon_cy), draw_r, frac, s_southern, COL_FG, COL_BG);

  // Name, "Dans X jours", date. During the launch animation the three lines
  // stack in one after another, rising from the bottom (date, then "Dans X",
  // then the phase name).
  int off_name = 0, off_dans = 0, off_date = 0;
  bool vis_name = true, vis_dans = true, vis_date = true;
  if (s_animating) {
    double p = s_anim_progress;
    off_date = prv_line_anim(p, 0.50, 0.68, &vis_date);
    off_dans = prv_line_anim(p, 0.66, 0.84, &vis_dans);
    off_name = prv_line_anim(p, 0.82, 1.00, &vis_name);
  }
  if (vis_name) {
    graphics_draw_text(ctx, name_str, f_name,
                       GRect(0, y_name + off_name, cw, name_h + 6),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
  if (vis_dans) prv_draw_centered(ctx, relbuf, f_dans, cw, y_dans + off_dans, 30);
  if (vis_date) prv_draw_centered(ctx, datebuf, f_date, cw, y_date + off_date, 24);

  // SELECT affordance (Health / Timeline style), white over the dark UI.
  prv_draw_select_hint(ctx, b, COL_FG);
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context);
static void prv_select_long_click(ClickRecognizerRef recognizer, void *context);

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click, NULL);
}

static void prv_anim_update(Animation *anim, const AnimationProgress progress) {
  s_anim_progress = (double)progress / ANIMATION_NORMALIZED_MAX;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void prv_anim_teardown(Animation *anim) {
  s_animating = false;
  if (s_main_layer) layer_mark_dirty(s_main_layer);  // settle on the real phase
}

static const AnimationImplementation s_anim_impl = {
  .update = prv_anim_update,
  .teardown = prv_anim_teardown,
};

static void prv_main_appear(Window *window) {
  if (s_did_animate) return;  // only on launch, not when returning from the list
  s_did_animate = true;
  s_animating = true;
  s_anim_progress = 0.0;

  Animation *anim = animation_create();
  animation_set_duration(anim, 1400);
  animation_set_curve(anim, AnimationCurveLinear);  // per-component easing below
  animation_set_implementation(anim, &s_anim_impl);
  animation_schedule(anim);
}

static void prv_main_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_main_layer = layer_create(bounds);
  layer_set_update_proc(s_main_layer, prv_main_update);
  layer_add_child(root, s_main_layer);

  window_set_click_config_provider(window, prv_click_config);
}

static void prv_main_unload(Window *window) {
  layer_destroy(s_main_layer);
  s_main_layer = NULL;  // a pending animation tick checks this guard
}

// ----------------------------------------------------------------------------
// List window (upcoming phases)
// ----------------------------------------------------------------------------

static uint16_t prv_menu_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return EVENT_COUNT;
}

// Launcher-style row metrics, mirroring the system app launcher.
static int16_t prv_menu_cell_height(MenuLayer *menu, MenuIndex *index, void *ctx) {
#if defined(PBL_ROUND)
  MenuIndex sel = menu_layer_get_selected_index(menu);
  bool focused = (index->row == sel.row);
  if (s_screen_h >= 200) return focused ? 55 : 45;
  return focused ? 52 : 38;
#else
  return s_screen_h >= 200 ? 50 : 42;
#endif
}

/** Title font, matching the launcher (bigger on Emery-class displays). */
static GFont prv_title_font(void) {
  return fonts_get_system_font(s_screen_h >= 200 ? FONT_KEY_GOTHIC_24_BOLD
                                                 : FONT_KEY_GOTHIC_18_BOLD);
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index,
                              void *data) {
  GRect b = layer_get_bounds(cell);
  bool hl = menu_cell_layer_is_highlighted(cell);
  // Launcher convention: black icon/text on white, white over the highlight.
  GColor fg = hl ? GColorWhite : GColorBlack;
  GColor bg = hl ? PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack) : GColorWhite;

  MoonPhaseEvent *ev = &s_events[index->row];

  int inset = PBL_IF_ROUND_ELSE(23, (s_screen_h >= 200 ? 10 : 6));
  int margin = (s_screen_h >= 200) ? 9 : 5;

  // 25x25 icon slot, left-aligned and vertically centered.
  int icon_cx = b.origin.x + inset + 12;
  int icon_cy = b.origin.y + b.size.h / 2;
  prv_draw_moon(ctx, GPoint(icon_cx, icon_cy), 11, ev->type * 0.25,
                s_southern, fg, bg);

  // Title (phase name) + subtitle (date · relative).
  char datebuf[24], relbuf[24], sub[56];
  prv_format_date(ev->time, datebuf, sizeof(datebuf));
  prv_format_relative(ev->time, relbuf, sizeof(relbuf));
  snprintf(sub, sizeof(sub), "%s · %s", datebuf, relbuf);

  int text_x = b.origin.x + inset + 25 + margin;
  int text_w = b.size.w - (text_x - b.origin.x) - 4;
  int title_h = (s_screen_h >= 200) ? 28 : 22;
  int sub_h = (s_screen_h >= 200) ? 20 : 16;
  int top = b.origin.y + (b.size.h - title_h - sub_h) / 2;

  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, i18n_phase_name(s_lang, ev->type), prv_title_font(),
                     GRect(text_x, top - 4, text_w, title_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, sub,
                     fonts_get_system_font((s_screen_h >= 200) ? FONT_KEY_GOTHIC_18
                                                               : FONT_KEY_GOTHIC_14),
                     GRect(text_x, top + title_h - 6, text_w, sub_h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_list_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_screen_h = bounds.size.h;

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_menu_num_rows,
    .get_cell_height = prv_menu_cell_height,
    .draw_row = prv_menu_draw_row,
  });
  GColor hl_bg = PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorBlack);
  menu_layer_set_normal_colors(s_menu_layer, GColorWhite, GColorBlack);
  menu_layer_set_highlight_colors(s_menu_layer, hl_bg, GColorWhite);
#if defined(PBL_ROUND)
  menu_layer_set_center_focused(s_menu_layer, true);
#endif
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
}

static void prv_list_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;  // prv_inbox_received checks this; avoid a dangling deref
  window_destroy(s_list_window);
  s_list_window = NULL;
}

static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_list_window) return;
  s_list_window = window_create();
  window_set_background_color(s_list_window, GColorWhite);
  window_set_window_handlers(s_list_window, (WindowHandlers) {
    .load = prv_list_load,
    .unload = prv_list_unload,
  });
  window_stack_push(s_list_window, true);
}

static void prv_select_long_click(ClickRecognizerRef recognizer, void *context) {
  // Manual hemisphere override (fallback when no phone/GPS is available).
  s_southern = !s_southern;
  persist_write_int(PERSIST_KEY_HEMISPHERE, s_southern);
  layer_mark_dirty(s_main_layer);
}

// ----------------------------------------------------------------------------
// AppMessage (hemisphere update from the phone)
// ----------------------------------------------------------------------------

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *hemi = dict_find(iter, MESSAGE_KEY_Hemisphere);
  if (hemi) {
    s_southern = hemi->value->int32 ? 1 : 0;
    persist_write_int(PERSIST_KEY_HEMISPHERE, s_southern);
    if (s_main_layer) layer_mark_dirty(s_main_layer);
    if (s_menu_layer) layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}

// ----------------------------------------------------------------------------
// App lifecycle
// ----------------------------------------------------------------------------

// Recompute on the hour so "Today"/"Tomorrow" and the drawn Moon stay correct
// if the app is left open across local midnight (or a phase instant).
static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  prv_refresh_data();
  if (s_main_layer) layer_mark_dirty(s_main_layer);
  if (s_menu_layer) menu_layer_reload_data(s_menu_layer);
}

static void prv_init(void) {
  s_lang = i18n_lang();
  s_southern = persist_exists(PERSIST_KEY_HEMISPHERE)
             ? persist_read_int(PERSIST_KEY_HEMISPHERE) : 0;

  prv_refresh_data();

  app_message_register_inbox_received(prv_inbox_received);
  app_message_open(64, 64);
  tick_timer_service_subscribe(HOUR_UNIT, prv_tick);

  s_main_window = window_create();
  window_set_background_color(s_main_window, COL_BG);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = prv_main_load,
    .appear = prv_main_appear,
    .unload = prv_main_unload,
  });
  window_stack_push(s_main_window, true);
}

static void prv_deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

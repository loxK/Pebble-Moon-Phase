/*
 * Moon Phase -- a Pebble watchapp showing the Moon and the upcoming phases.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <pebble.h>
#include <math.h>
#include "moon_phase.h"
#include "i18n.h"
#include "glance.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------

#define PERSIST_KEY_HEMISPHERE 1

// Number of upcoming phase events, one card each.
#define EVENT_COUNT 8

// Vertical slide between two cards.
#define SLIDE_DURATION_MS 300

// Colors. The phase screen uses a navy "night sky" background on color
// displays (black on B/W, where no blue is available); the Moon and text are
// drawn in white over it.
#define COL_BG PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define COL_FG GColorWhite

// ----------------------------------------------------------------------------
// State
// ----------------------------------------------------------------------------

static Window *s_main_window;
static Layer *s_main_layer;
static bool s_visible;

static MoonState s_state;
static int32_t s_distance_now;
static bool s_approaching_now;
static MoonPhaseEvent s_events[EVENT_COUNT];

// 0 = Northern hemisphere (default), 1 = Southern hemisphere.
static int s_southern;

// Active UI language, derived from the system locale at launch.
static Lang s_lang;

// Card on screen, an index into s_events. Card 0 draws tonight's Moon, the
// others the shape of their own phase.
static int s_card;

// Phase name font, picked once the screen width and the language are known.
static GFont s_name_font;

// Slide from s_slide_from to s_card. s_slide_dir is +1 when the new card comes
// up from below (DOWN), -1 when it comes down from above (UP).
static Animation *s_slide_anim;
static double s_slide_progress;  // 0..1, eased by the animation curve
static int s_slide_from;
static int s_slide_dir;

// Launch animation: the lit area sweeps from new moon to the current phase.
static Animation *s_launch_anim;
static double s_anim_progress;  // 0..1 (eased), only meaningful while animating
static bool s_animating;
static bool s_did_animate;      // play once per launch, not on every appear

// Shooting star: idle waits, then one streak across the sky. The first pass
// comes right after the launch animation settles, the rest at random.
#define SHOOT_FIRST_MS 1600
#define SHOOT_WAIT_MIN_MS 3000
#define SHOOT_WAIT_MAX_MS 9000
#define SHOOT_DURATION_MS 520
#define SHOOT_TAIL 14           // segments trailing the head, 2% of the path apart

static bool s_shoot_active;
static double s_shoot_progress;      // 0..1 along the path
static int s_shoot_x0, s_shoot_y0;   // entry point, in % of the screen
static int s_shoot_dx, s_shoot_dy;   // travel, in % of the screen
static AppTimer *s_shoot_timer;

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
  s_distance_now = moon_distance_km(now);
  s_approaching_now = moon_is_approaching(now);

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

/**
 * Format the calendar date of an event into `buf` (localized). `abbrev` cuts
 * the month to three letters, for the places where a long name would run past
 * the edge.
 */
static void prv_format_date(time_t event, bool abbrev, char *buf, size_t len) {
  struct tm t = *localtime(&event);
  i18n_date(s_lang, t.tm_mday, t.tm_mon, abbrev, buf, len);
}

/** Format the local time of an event into `buf`, following the watch's 12/24h setting. */
static void prv_format_time(time_t event, char *buf, size_t len) {
  struct tm t = *localtime(&event);
  if (clock_is_24h_style()) {
    snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
  } else {
    // strftime's %l pads the hour with a space, which would shift the centred
    // line; only the AM/PM marker comes from it, in the watch's language.
    char am_pm[8];
    strftime(am_pm, sizeof(am_pm), "%p", &t);
    int h = t.tm_hour % 12;
    snprintf(buf, len, "%d:%02d %s", h ? h : 12, t.tm_min, am_pm);
  }
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

/** Width left for the text on a card; the pager dots take the right edge. */
static int prv_content_width(GRect b) {
  return b.size.w - PBL_IF_ROUND_ELSE(0, 10);
}

/**
 * Progress of the Moon's sweep in the launch animation, eased in and out. It
 * runs over the first half of the animation (~700 ms), and stays at 1 after.
 */
static double prv_launch_sweep(void) {
  if (!s_animating) return 1.0;
  double mp = s_anim_progress * 2.0;
  if (mp > 1.0) mp = 1.0;
  return mp * mp * (3.0 - 2.0 * mp);  // smoothstep
}

/** Earth-Moon distance shown with card `idx`: tonight's, or at the phase. */
static int32_t prv_card_distance(int idx) {
  return idx == 0 ? s_distance_now : s_events[idx].distance_km;
}

/** Whether the Moon is coming closer at the time card `idx` stands for. */
static bool prv_card_approaching(int idx) {
  return idx == 0 ? s_approaching_now : s_events[idx].approaching;
}

/** How close the Moon is, from 0 at the farthest apogee to 1 at the closest perigee. */
static double prv_closeness(int32_t km) {
  double v = (double)(MOON_DISTANCE_MAX_KM - km)
           / (MOON_DISTANCE_MAX_KM - MOON_DISTANCE_MIN_KM);
  return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
}

#if defined(PBL_ROUND)
/** Point at `radius` from `c` towards `angle` (0 points up, clockwise). */
static GPoint prv_polar(GPoint c, int32_t angle, int radius) {
  return GPoint(c.x + sin_lookup(angle) * radius / TRIG_MAX_RATIO,
                c.y - cos_lookup(angle) * radius / TRIG_MAX_RATIO);
}
#endif

/**
 * Draw a small triangle, its left side or tip at `left`, pointing right while
 * the Moon approaches (the gauge fills towards the right) and left otherwise.
 * Larger screens pack their pixels tighter and get a larger one.
 */
static void prv_draw_direction(GContext *ctx, GRect b, GPoint left, bool approaching) {
  bool large = b.size.h >= 200;
  int depth = large ? 8 : 5;
  int half = large ? 5 : 4;
  int tip = approaching ? left.x + depth : left.x;
  int back = approaching ? left.x : left.x + depth;
  GPoint points[] = {GPoint(tip, left.y), GPoint(back, left.y - half), GPoint(back, left.y + half)};
  GPath path = {.num_points = 3, .points = points};
  gpath_draw_filled(ctx, &path);
}

/**
 * Draw the distance gauge across the top of the screen, filled by `v` (see
 * prv_closeness): a full gauge is a Moon at its closest. Round screens follow
 * the bezel with an arc, where a straight bar would have no room.
 *
 * A tick marks the supermoon distance. It sticks out of the gauge, and turns
 * to the background colour where the fill runs past it. A triangle past the
 * near end tells whether the Moon is approaching.
 */
static void prv_draw_gauge(GContext *ctx, GRect b, double v, bool approaching) {
  const double tick = prv_closeness(MOON_SUPERMOON_KM);
  graphics_context_set_fill_color(ctx, COL_FG);
#if defined(PBL_ROUND)
  const int thickness = 4;
  const int32_t start = DEG_TO_TRIGANGLE(-40);
  const int32_t span = DEG_TO_TRIGANGLE(80);
  GRect ring = grect_inset(b, GEdgeInsets(3));
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, thickness, start, start + span);
  if (v > 0.0) {
    graphics_context_set_fill_color(ctx, COL_FG);
    graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, thickness,
                         start, start + (int32_t)(span * v));
  }

  // The tick is a narrow slice of a ring running from the outer edge of the
  // gauge inwards; the bezel leaves no room to stick out the other way.
  const int32_t at = start + (int32_t)(span * tick);
  const int32_t half = DEG_TO_TRIGANGLE(1);
  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, thickness + 4, at - half, at + half);
  if (v >= tick) {
    graphics_context_set_fill_color(ctx, COL_BG);
    graphics_fill_radial(ctx, ring, GOvalScaleModeFitCircle, thickness, at - half, at + half);
  }

  // Past the near end of the arc, 7 px further along it.
  int mid = ring.size.w / 2 - thickness / 2;
  GPoint at_end = prv_polar(grect_center_point(&ring),
                            start + span + TRIG_MAX_ANGLE * 7 * 1000 / (6283 * mid), mid);
  graphics_context_set_fill_color(ctx, COL_FG);
  prv_draw_direction(ctx, b, GPoint(at_end.x - 2, at_end.y), approaching);
#else
  int cw = prv_content_width(b);
  int w = cw * 3 / 5;
  GRect track = GRect((cw - w) / 2, 4, w, 5);

  // Past the near end of the bar.
  prv_draw_direction(ctx, b, GPoint(track.origin.x + w + 3, track.origin.y + track.size.h / 2),
                     approaching);

#if defined(PBL_COLOR)
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_rect(ctx, track, 2, GCornersAll);
#else
  graphics_context_set_stroke_color(ctx, COL_FG);
  graphics_draw_round_rect(ctx, track, 2);
#endif
  int filled = (int)(w * v);
  if (filled > 0) {
    graphics_context_set_fill_color(ctx, COL_FG);
    graphics_fill_rect(ctx, GRect(track.origin.x, track.origin.y, filled, track.size.h),
                       filled >= 4 ? 2 : 0, GCornersAll);
  }

  int tx = track.origin.x + (int)(w * tick);
  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_fill_rect(ctx, GRect(tx, track.origin.y - 2, 2, track.size.h + 4), 0, GCornerNone);
  if (v >= tick) {
    graphics_context_set_fill_color(ctx, COL_BG);
    graphics_fill_rect(ctx, GRect(tx, track.origin.y, 2, track.size.h), 0, GCornerNone);
  }
#endif
}

/**
 * Draw the card position as a column of dots along the right edge, between the
 * UP and DOWN buttons, the current card as the bigger dot.
 */
static void prv_draw_pager(GContext *ctx, GRect b, GColor fg) {
  const int step = 7;
  int x = b.size.w - PBL_IF_ROUND_ELSE(9, 5);
  int y = b.size.h / 2 - (EVENT_COUNT - 1) * step / 2;
  graphics_context_set_fill_color(ctx, fg);
  for (int i = 0; i < EVENT_COUNT; i++) {
    graphics_fill_circle(ctx, GPoint(x, y + i * step), i == s_card ? 2 : 1);
  }
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

// Xorshift, seeded from the clock at launch: the shooting star needs a cheap
// source of randomness and the C library offers none here.
static uint32_t s_rand_state = 1;

static uint32_t prv_rand(void) {
  uint32_t x = s_rand_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_rand_state = x;
  return x;
}

/** Random integer in [lo, hi). */
static int prv_rand_range(int lo, int hi) {
  return lo + (int)(prv_rand() % (uint32_t)(hi - lo));
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

// A streak crossing the sky, entering and leaving past the edges. Both ends of
// the path sit off screen, so the star is only ever seen mid-flight. There is
// no alpha to fade with, so the tail thins into spaced pixels and shortens over
// the last third of the run.
static void prv_draw_shooting_star(GContext *ctx, GRect b) {
  if (!s_shoot_active) return;

  const double p = s_shoot_progress;
  const int tail = (p > 0.66) ? (int)((1.0 - p) * 3.0 * SHOOT_TAIL) : SHOOT_TAIL;

  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_context_set_stroke_color(ctx, COL_FG);

  for (int i = tail; i >= 0; i--) {
    double q = p - i * 0.02;
    if (q < 0.0) continue;
    int x = b.origin.x + (int)((s_shoot_x0 + s_shoot_dx * q) * b.size.w / 100.0);
    int y = b.origin.y + (int)((s_shoot_y0 + s_shoot_dy * q) * b.size.h / 100.0);
    if (i == 0) {
      graphics_fill_circle(ctx, GPoint(x, y), 2);
    } else if (i < 3) {
      graphics_fill_circle(ctx, GPoint(x, y), 1);
    } else if ((i & 1) == 0) {
      graphics_draw_pixel(ctx, GPoint(x, y));
    }
  }
}

/** Width of a single line of `text` set in `font`. */
static int prv_text_width(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 1000, 64),
      GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
}

/**
 * Font for the phase names: the larger one only when all the names of the
 * language fit on one line clear of the edges, so every card keeps the same
 * size and the Moon does not shrink from one card to the next.
 */
static GFont prv_pick_name_font(int cw) {
  GFont large = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  for (int name = 0; name < I18N_NAME_COUNT; name++) {
    if (prv_text_width(i18n_phase_name(s_lang, name), large) > cw - 20) {
      return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    }
  }
  return large;
}

/**
 * Draw card `idx` shifted down by `dy` pixels: the Moon, the phase name and,
 * under it, a big line over a small one. The next phase reads as how far away
 * it is over its date; the later ones as their date over their time. The sky is
 * drawn underneath by the caller.
 */
static void prv_draw_card(GContext *ctx, GRect b, int idx, int dy) {
  int cw = prv_content_width(b);
  int cx = cw / 2;

  // The phase name may still wrap to two lines in the smaller font; the layout
  // (and the Moon above it) adapts instead of truncating. Larger screens have
  // room for larger lines under the name too.
  bool large_screen = b.size.h >= 200;
  GFont f_name = s_name_font;
  GFont f_big = fonts_get_system_font(large_screen ? FONT_KEY_GOTHIC_28_BOLD
                                                   : FONT_KEY_GOTHIC_24_BOLD);
  GFont f_small = fonts_get_system_font(large_screen ? FONT_KEY_GOTHIC_24
                                                     : FONT_KEY_GOTHIC_18);
  int big_h = large_screen ? 34 : 30;
  int small_h = large_screen ? 30 : 24;

  MoonPhaseEvent *ev = &s_events[idx];
  const char *name_str = i18n_phase_name(s_lang, i18n_event_name(ev));
  char big[24], small[24];
  if (idx == 0) {
    prv_format_relative(ev->time, big, sizeof(big));
    prv_format_date(ev->time, PBL_IF_ROUND_ELSE(true, false), small, sizeof(small));
  } else {
    // A rectangular screen keeps the full month unless it overflows the line
    // ("22 de septiembre"); a round one narrows too fast near the bottom.
    bool abbrev = PBL_IF_ROUND_ELSE(true, false);
    prv_format_date(ev->time, abbrev, big, sizeof(big));
    if (!abbrev && prv_text_width(big, f_big) > cw) {
      prv_format_date(ev->time, true, big, sizeof(big));
    }
    prv_format_time(ev->time, small, sizeof(small));
  }

  GSize name_sz = graphics_text_layout_get_content_size(
      name_str, f_name, GRect(0, 0, cw, 72),
      GTextOverflowModeWordWrap, GTextAlignmentCenter);
  int name_h = name_sz.h;

  // Text block sits at the bottom; the Moon fills the area above it.
  int y_small = b.size.h - small_h - 2;
  int y_big = y_small - big_h;
  int y_name = y_big - name_h;
  int moon_bottom = y_name - 2;

  // Moon geometry, below the distance gauge.
  const int moon_top = 12;
  int moon_cy = moon_top + (moon_bottom - moon_top) / 2;
  int max_r_w = cx - 8;
  int max_r_h = (moon_bottom - moon_top - 4) / 2;
  int r = max_r_w < max_r_h ? max_r_w : max_r_h;
  if (r > 46) r = 46;
  if (r < 18) r = 18;

  // Launch animation: sweep the lit area from new moon to the current phase,
  // and scale the disc in.
  double frac = idx == 0 ? s_state.fraction : ev->type * 0.25;
  int draw_r = r;
  bool launching = s_animating && idx == 0;
  if (launching) {
    double me = prv_launch_sweep();
    frac = s_state.fraction * me;
    draw_r = (int)(r * (0.82 + 0.18 * me));
  }
  prv_draw_moon(ctx, GPoint(cx, moon_cy + dy), draw_r, frac, s_southern, COL_FG, COL_BG);

  // During the launch animation the three lines stack in one after another,
  // rising from the bottom (small line, then big line, then the phase name).
  int off_name = dy, off_big = dy, off_small = dy;
  bool vis_name = true, vis_big = true, vis_small = true;
  if (launching) {
    double p = s_anim_progress;
    off_small = prv_line_anim(p, 0.50, 0.68, &vis_small);
    off_big = prv_line_anim(p, 0.66, 0.84, &vis_big);
    off_name = prv_line_anim(p, 0.82, 1.00, &vis_name);
  }
  if (vis_name) {
    graphics_draw_text(ctx, name_str, f_name,
                       GRect(0, y_name + off_name, cw, name_h + 6),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
  if (vis_big) prv_draw_centered(ctx, big, f_big, cw, y_big + off_big, big_h);
  if (vis_small) prv_draw_centered(ctx, small, f_small, cw, y_small + off_small, small_h);
}

static void prv_main_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);
  prv_draw_stars(ctx, b);
  prv_draw_shooting_star(ctx, b);
  graphics_context_set_text_color(ctx, COL_FG);

  // While sliding, the outgoing card leaves on one side as the incoming one
  // enters from the other; the sky stays put behind both, and the gauge runs
  // from one distance to the other.
  double closeness = prv_closeness(prv_card_distance(s_card));
  if (s_slide_anim) {
    int h = b.size.h;
    int shift = (int)(s_slide_progress * h);
    prv_draw_card(ctx, b, s_slide_from, -s_slide_dir * shift);
    prv_draw_card(ctx, b, s_card, s_slide_dir * (h - shift));
    double from = prv_closeness(prv_card_distance(s_slide_from));
    closeness = from + (closeness - from) * s_slide_progress;
  } else {
    prv_draw_card(ctx, b, s_card, 0);
  }

  prv_draw_gauge(ctx, b, closeness * prv_launch_sweep(), prv_card_approaching(s_card));
  prv_draw_pager(ctx, b, COL_FG);
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context);
static void prv_down_click(ClickRecognizerRef recognizer, void *context);
static void prv_select_long_click(ClickRecognizerRef recognizer, void *context);

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click, NULL);
}

static void prv_anim_update(Animation *anim, const AnimationProgress progress) {
  s_anim_progress = (double)progress / ANIMATION_NORMALIZED_MAX;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void prv_anim_teardown(Animation *anim) {
  s_launch_anim = NULL;
  s_animating = false;
  if (s_main_layer) layer_mark_dirty(s_main_layer);  // settle on the real phase
}

static const AnimationImplementation s_anim_impl = {
  .update = prv_anim_update,
  .teardown = prv_anim_teardown,
};

static void prv_shoot_schedule(void);

static void prv_shoot_update(Animation *anim, const AnimationProgress progress) {
  s_shoot_progress = (double)progress / ANIMATION_NORMALIZED_MAX;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void prv_shoot_teardown(Animation *anim) {
  s_shoot_active = false;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
  if (s_visible) prv_shoot_schedule();
}

static const AnimationImplementation s_shoot_impl = {
  .update = prv_shoot_update,
  .teardown = prv_shoot_teardown,
};

static void prv_shoot_fire(void *data) {
  s_shoot_timer = NULL;

  // Crosses downwards, entering from the top or from a side, and always exits
  // past an edge.
  const bool leftwards = prv_rand() & 1;
  s_shoot_x0 = leftwards ? prv_rand_range(75, 115) : prv_rand_range(-15, 25);
  s_shoot_y0 = prv_rand_range(-15, 15);
  s_shoot_dx = leftwards ? -prv_rand_range(60, 95) : prv_rand_range(60, 95);
  s_shoot_dy = prv_rand_range(35, 65);

  s_shoot_active = true;
  s_shoot_progress = 0.0;

  Animation *anim = animation_create();
  animation_set_duration(anim, SHOOT_DURATION_MS);
  animation_set_curve(anim, AnimationCurveLinear);
  animation_set_implementation(anim, &s_shoot_impl);
  animation_schedule(anim);
}

static void prv_shoot_schedule_in(uint32_t delay_ms) {
  if (s_shoot_timer) return;
  s_shoot_timer = app_timer_register(delay_ms, prv_shoot_fire, NULL);
}

static void prv_shoot_schedule(void) {
  prv_shoot_schedule_in(prv_rand_range(SHOOT_WAIT_MIN_MS, SHOOT_WAIT_MAX_MS));
}

static void prv_slide_update(Animation *anim, const AnimationProgress progress) {
  s_slide_progress = (double)progress / ANIMATION_NORMALIZED_MAX;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void prv_slide_teardown(Animation *anim) {
  s_slide_anim = NULL;
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static const AnimationImplementation s_slide_impl = {
  .update = prv_slide_update,
  .teardown = prv_slide_teardown,
};

/** Slide to the card `dir` steps away, if there is one. */
static void prv_go(int dir) {
  int to = s_card + dir;
  if (to < 0 || to >= EVENT_COUNT) return;

  // A press cuts short whatever is still moving, so the slide always starts
  // from a settled card.
  animation_unschedule(s_launch_anim);
  animation_unschedule(s_slide_anim);

  s_slide_from = s_card;
  s_slide_dir = dir;
  s_slide_progress = 0.0;
  s_card = to;

  s_slide_anim = animation_create();
  animation_set_duration(s_slide_anim, SLIDE_DURATION_MS);
  animation_set_curve(s_slide_anim, AnimationCurveEaseInOut);
  animation_set_implementation(s_slide_anim, &s_slide_impl);
  animation_schedule(s_slide_anim);
}

static void prv_up_click(ClickRecognizerRef recognizer, void *context) {
  prv_go(-1);
}

static void prv_down_click(ClickRecognizerRef recognizer, void *context) {
  prv_go(1);
}

static void prv_main_appear(Window *window) {
  s_visible = true;
  if (s_did_animate) {  // shown again after being hidden
    prv_shoot_schedule();
    return;
  }
  prv_shoot_schedule_in(SHOOT_FIRST_MS);
  s_did_animate = true;
  s_animating = true;
  s_anim_progress = 0.0;

  s_launch_anim = animation_create();
  animation_set_duration(s_launch_anim, 1400);
  animation_set_curve(s_launch_anim, AnimationCurveLinear);  // per-component easing below
  animation_set_implementation(s_launch_anim, &s_anim_impl);
  animation_schedule(s_launch_anim);
}

static void prv_main_disappear(Window *window) {
  s_visible = false;
  if (s_shoot_timer) {
    app_timer_cancel(s_shoot_timer);
    s_shoot_timer = NULL;
  }
}

static void prv_main_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_name_font = prv_pick_name_font(prv_content_width(bounds));
  s_main_layer = layer_create(bounds);
  layer_set_update_proc(s_main_layer, prv_main_update);
  layer_add_child(root, s_main_layer);

  window_set_click_config_provider(window, prv_click_config);
}

static void prv_main_unload(Window *window) {
  layer_destroy(s_main_layer);
  s_main_layer = NULL;  // a pending animation tick checks this guard
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
  }
}

// ----------------------------------------------------------------------------
// App lifecycle
// ----------------------------------------------------------------------------

// Recompute on the hour so "Today"/"Tomorrow" and the drawn Moon stay correct
// if the app is left open across local midnight (or a phase instant).
static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  // The refresh drops the events whose day is over and shifts the rest, so the
  // card on screen is found again by its event; back to the first card when
  // that event is gone.
  time_t shown = s_events[s_card].time;
  animation_unschedule(s_slide_anim);
  prv_refresh_data();
  s_card = 0;
  for (int i = 0; i < EVENT_COUNT; i++) {
    if (s_events[i].time == shown) {
      s_card = i;
      break;
    }
  }
  if (s_main_layer) layer_mark_dirty(s_main_layer);
}

#if PBL_API_EXISTS(app_touch_navigation_enable)
// The touch navigation dispatcher runs on the app task and reads the firmware's
// nav state through syscalls only from 4.33.2 on. Below that it reaches into
// kernel memory, which the MPU refuses to an unprivileged third-party app: the
// opt-in turns the first touch into an app fault.
static bool prv_touch_nav_is_safe(void) {
  WatchInfoVersion fw = watch_info_get_firmware_version();
  return fw.major > 4
      || (fw.major == 4 && (fw.minor > 33 || (fw.minor == 33 && fw.patch >= 2)));
}
#endif

static void prv_init(void) {
  s_rand_state = (uint32_t)prv_now() | 1u;
  s_lang = i18n_lang();
  // strftime stays in English until the app takes the watch's locale.
  setlocale(LC_TIME, "");
  s_southern = persist_exists(PERSIST_KEY_HEMISPHERE)
             ? persist_read_int(PERSIST_KEY_HEMISPHERE) : 0;

  prv_refresh_data();

  app_message_register_inbox_received(prv_inbox_received);
  // Only the phone talks: the watch has nothing to send, and needs no outbox.
  app_message_open(64, 0);
  tick_timer_service_subscribe(HOUR_UNIT, prv_tick);

  // Third-party apps get no touch input until they ask for it: a vertical swipe
  // turns into UP or DOWN, and so moves between cards, only once this is set.
  // The guard keeps the build going on SDKs without the call.
#if PBL_API_EXISTS(app_touch_navigation_enable)
  if (prv_touch_nav_is_safe()) app_touch_navigation_enable(true);
#endif

  s_main_window = window_create();
  window_set_background_color(s_main_window, COL_BG);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = prv_main_load,
    .appear = prv_main_appear,
    .disappear = prv_main_disappear,
    .unload = prv_main_unload,
  });
  window_stack_push(s_main_window, true);
}

static void prv_deinit(void) {
  glance_publish(s_lang, s_southern);
  window_destroy(s_main_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

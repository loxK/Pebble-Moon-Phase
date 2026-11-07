/*
 * Moon Phase -- drawing shared by the phase cards and the detail cards.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "art.h"

// Vertical slide between two cards.
#define SLIDE_DURATION_MS 300

// Integer square root (avoids the double libm sqrt, which is heavy on stack).
static int prv_isqrt(int n) {
  if (n <= 0) return 0;
  int x = n, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + n / x) / 2; }
  return x;
}

// Round a double to the nearest int.
#define MP_IRND(v) ((int)((v) < 0 ? (v) - 0.5 : (v) + 0.5))

int art_content_width(GRect b) {
  return b.size.w - PBL_IF_ROUND_ELSE(0, 10);
}

void art_draw_moon(GContext *ctx, GPoint center, int radius, double fraction, bool southern,
                   GColor fg, GColor bg) {
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

double art_closeness(int32_t km) {
  double v = (double)(MOON_DISTANCE_MAX_KM - km) / (MOON_DISTANCE_MAX_KM - MOON_DISTANCE_MIN_KM);
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

void art_draw_gauge(GContext *ctx, GRect b, double v, bool approaching) {
  const double tick = art_closeness(MOON_SUPERMOON_KM);
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

  // Past the near end of the arc, 7 px further along it and 6 px inside it,
  // so the triangle's corners stay clear of the bezel.
  int mid = ring.size.w / 2 - thickness / 2;
  GPoint at_end = prv_polar(grect_center_point(&ring),
                            start + span + TRIG_MAX_ANGLE * 7 * 1000 / (6283 * mid), mid - 6);
  graphics_context_set_fill_color(ctx, COL_FG);
  prv_draw_direction(ctx, b, GPoint(at_end.x - 2, at_end.y), approaching);
#else
  int cw = art_content_width(b);
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

void art_draw_pager(GContext *ctx, GRect b, int count, int current) {
  const int step = 7;
  int x = b.size.w - PBL_IF_ROUND_ELSE(9, 5);
  int y = b.size.h / 2 - (count - 1) * step / 2;
  graphics_context_set_fill_color(ctx, COL_FG);
  for (int i = 0; i < count; i++) {
    graphics_fill_circle(ctx, GPoint(x, y + i * step), i == current ? 2 : 1);
  }
}

// A sparse starfield. Positions are percentages of the screen, kept in the
// periphery (never over the Moon or the text). On round screens the corner
// stars fall outside the bezel.
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

void art_draw_stars(GContext *ctx, GRect b, int reveal) {
  graphics_context_set_fill_color(ctx, COL_FG);
  graphics_context_set_stroke_color(ctx, COL_FG);
  for (unsigned i = 0; i < sizeof(s_stars) / sizeof(s_stars[0]); i++) {
    if (reveal < s_stars[i].at) continue;
    prv_draw_star(ctx, s_stars[i].nx * b.size.w / 100, s_stars[i].ny * b.size.h / 100,
                  s_stars[i].sz);
  }
}

static void prv_slide_update(Animation *anim, const AnimationProgress progress) {
  ArtSlide *slide = animation_get_context(anim);
  slide->progress = (double)progress / ANIMATION_NORMALIZED_MAX;
  if (slide->layer) layer_mark_dirty(slide->layer);
}

static void prv_slide_teardown(Animation *anim) {
  ArtSlide *slide = animation_get_context(anim);
  slide->anim = NULL;
  if (slide->layer) layer_mark_dirty(slide->layer);
}

static const AnimationImplementation s_slide_impl = {
  .update = prv_slide_update,
  .teardown = prv_slide_teardown,
};

void art_slide_start(ArtSlide *slide, int from, int dir) {
  art_slide_stop(slide);
  slide->from = from;
  slide->dir = dir;
  slide->progress = 0.0;
  slide->anim = animation_create();
  if (!slide->anim) {  // out of memory: show the new card without the slide
    if (slide->layer) layer_mark_dirty(slide->layer);
    return;
  }
  animation_set_duration(slide->anim, SLIDE_DURATION_MS);
  animation_set_curve(slide->anim, AnimationCurveEaseInOut);
  animation_set_implementation(slide->anim, &s_slide_impl);
  animation_set_handlers(slide->anim, (AnimationHandlers){0}, slide);
  animation_schedule(slide->anim);
}

void art_slide_stop(ArtSlide *slide) {
  if (slide->anim) animation_unschedule(slide->anim);
}

#if MOON_HAS_DETAILS
// The Earth's umbra at the Moon's distance is about 2.7 Moon diameters across.
#define UMBRA_RATIO 2.6

void art_draw_eclipse(GContext *ctx, GPoint center, int radius, int percent) {
  GColor shadow = PBL_IF_COLOR_ELSE(GColorBulgarianRose, COL_BG);
  int umbra = (int)(radius * UMBRA_RATIO);
  // The shadow's edge crosses the disc from the left: at `percent` of the
  // diameter, its centre sits an umbra radius further left.
  int edge = center.x - radius + 2 * radius * (percent > 100 ? 100 : percent) / 100;
  int ux = percent >= 100 ? center.x : edge - umbra;
  for (int dy = -radius; dy <= radius; dy++) {
    int xe = prv_isqrt(radius * radius - dy * dy);
    int us = prv_isqrt(umbra * umbra - dy * dy);
    int lo = center.x - xe, hi = center.x + xe;
    int slo = ux - us, shi = ux + us;
    if (shi > hi) shi = hi;
    if (slo < lo) slo = lo;
    graphics_context_set_stroke_color(ctx, COL_FG);
    graphics_draw_line(ctx, GPoint(lo, center.y + dy), GPoint(hi, center.y + dy));
    if (shi >= slo) {
      graphics_context_set_stroke_color(ctx, shadow);
      graphics_draw_line(ctx, GPoint(slo, center.y + dy), GPoint(shi, center.y + dy));
    }
  }
  graphics_context_set_stroke_color(ctx, COL_FG);
  graphics_draw_circle(ctx, center, radius);
}

/** A chevron: two strokes meeting at `tip`. */
static void prv_chevron(GContext *ctx, GPoint a, GPoint tip, GPoint b) {
  graphics_draw_line(ctx, a, tip);
  graphics_draw_line(ctx, tip, b);
}

void art_draw_apsis(GContext *ctx, GPoint center, int radius, bool perigee) {
  art_draw_moon(ctx, center, radius, 0.5, false, COL_FG, COL_BG);
  graphics_context_set_stroke_color(ctx, COL_BG);
  graphics_context_set_stroke_width(ctx, radius >= 30 ? 5 : 3);
  int gap = radius * 12 / 50, depth = radius * 13 / 50, half = radius * 20 / 50;
  for (int side = -1; side <= 1; side += 2) {
    int near = center.x + side * gap, far = center.x + side * (gap + depth);
    int tip = perigee ? near : far, back = perigee ? far : near;
    prv_chevron(ctx, GPoint(back, center.y - half), GPoint(tip, center.y),
                GPoint(back, center.y + half));
  }
  graphics_context_set_stroke_width(ctx, 1);
}

void art_draw_node(GContext *ctx, GPoint center, int radius, bool ascending) {
  art_draw_moon(ctx, center, radius, 0.5, false, COL_FG, COL_BG);
  graphics_context_set_stroke_color(ctx, COL_BG);
  graphics_context_set_stroke_width(ctx, radius >= 30 ? 5 : 3);
  int reach = radius * 36 / 50;
  graphics_draw_line(ctx, GPoint(center.x - reach, center.y), GPoint(center.x + reach, center.y));
  int half = radius * 20 / 50, depth = radius * 16 / 50, off = radius * 12 / 50;
  int up = ascending ? -1 : 1;
  int back = center.y + up * off, tip = center.y + up * (off + depth);
  prv_chevron(ctx, GPoint(center.x - half, back), GPoint(center.x, tip),
              GPoint(center.x + half, back));
  graphics_context_set_stroke_width(ctx, 1);
}
#endif

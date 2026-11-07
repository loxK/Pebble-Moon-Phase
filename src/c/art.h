/*
 * Moon Phase -- drawing shared by the phase cards and the detail cards.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>
#include "moon_phase.h"

// A navy "night sky" on colour displays, black on B/W where no blue is
// available; the Moon and the text are white over it.
#define COL_BG PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define COL_FG GColorWhite

/** Width left for the text on a card; the pager dots take the right edge. */
int art_content_width(GRect b);

/**
 * Draw a Moon at `center` with the given `radius`, depicting lunation position
 * `fraction` (0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter).
 *
 * The illuminated part is filled with `fg`, the dark part is `bg`, and the
 * disc rim is outlined with `fg`. `southern` mirrors the lit side left/right.
 */
void art_draw_moon(GContext *ctx, GPoint center, int radius, double fraction, bool southern,
                   GColor fg, GColor bg);

/** How close the Moon is, from 0 at the farthest apogee to 1 at the closest perigee. */
double art_closeness(int32_t km);

/**
 * Draw the distance gauge across the top of the screen, filled by `v` (see
 * art_closeness): a full gauge is a Moon at its closest. Round screens follow
 * the bezel with an arc, where a straight bar would have no room.
 *
 * A tick marks the supermoon distance. It sticks out of the gauge, and turns
 * to the background colour where the fill runs past it. A triangle past the
 * near end tells whether the Moon is approaching.
 */
void art_draw_gauge(GContext *ctx, GRect b, double v, bool approaching);

/**
 * Draw the position among `count` cards as a column of dots along the right
 * edge, between the UP and DOWN buttons, card `current` as the bigger dot.
 */
void art_draw_pager(GContext *ctx, GRect b, int count, int current);

/**
 * Draw the sparse starfield behind the cards, the stars whose reveal threshold
 * `reveal` (0..100) has passed.
 */
void art_draw_stars(GContext *ctx, GRect b, int reveal);

/**
 * A vertical slide from card `from` to the card `dir` steps away (+1 when it
 * comes up from below, -1 from above), redrawing `layer` as it goes.
 */
typedef struct {
  Animation *anim;
  double progress;  //!< 0..1, eased by the animation curve
  int from;
  int dir;
  Layer *layer;
} ArtSlide;

/** Start sliding away from card `from`, cutting short a slide still running. */
void art_slide_start(ArtSlide *slide, int from, int dir);

/** Stop a slide still running. */
void art_slide_stop(ArtSlide *slide);

#if MOON_HAS_DETAILS
/**
 * Draw a full Moon in an eclipse, the Earth's shadow covering `percent` of its
 * diameter from the left; dark red on colour screens, as an eclipsed Moon
 * looks, and the background colour on B/W.
 */
void art_draw_eclipse(GContext *ctx, GPoint center, int radius, int percent);

/**
 * Draw a full Moon with a chevron on each side of its centre: pointing in at
 * the perigee, where the Moon comes closest, and out at the apogee.
 */
void art_draw_apsis(GContext *ctx, GPoint center, int radius, bool perigee);

/**
 * Draw a full Moon crossed by the plane of the Earth's orbit, a chevron on it
 * pointing up at the ascending node and down at the descending one.
 */
void art_draw_node(GContext *ctx, GPoint center, int radius, bool ascending);
#endif

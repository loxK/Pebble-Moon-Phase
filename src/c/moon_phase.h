/*
 * Moon Phase -- astronomy engine.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>

/**
 * Moon phase astronomical engine.
 *
 * Computes the current illuminated fraction of the Moon (for drawing) and the
 * dates of the upcoming principal phases (new moon, first quarter, full moon,
 * last quarter) using Meeus' algorithm (Astronomical Algorithms, ch. 49) with
 * the principal periodic correction terms. Accuracy is on the order of a few
 * minutes -- far beyond what a watch display needs.
 *
 * All times are computed in UTC (the watch RTC epoch is UTC).
 */

/** The four principal Moon phases, in the order they occur within a lunation. */
typedef enum {
  MOON_PHASE_NEW = 0,
  MOON_PHASE_FIRST_QUARTER = 1,
  MOON_PHASE_FULL = 2,
  MOON_PHASE_LAST_QUARTER = 3,
} MoonPhaseType;

/** A single upcoming phase event. */
typedef struct {
  MoonPhaseType type;
  time_t time;  //!< UTC epoch of the event.
} MoonPhaseEvent;

/** Current state of the Moon, for rendering. */
typedef struct {
  double fraction;       //!< Position in the lunation: 0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter.
  double illumination;   //!< Illuminated fraction of the disk, 0.0 (new) .. 1.0 (full).
  bool waxing;           //!< true while the Moon is growing (new -> full).
} MoonState;

/**
 * Lightweight sine/cosine for an angle in degrees.
 *
 * These avoid the C library's trig (whose large-argument range reduction has a
 * big stack footprint that overflows the small app stack on some platforms,
 * e.g. chalk). They range-reduce to [0, pi/4] and use a short Taylor series,
 * which is accurate to ~1e-9 -- far beyond what phase timing needs.
 */
double mp_sin_deg(double deg);
double mp_cos_deg(double deg);

/**
 * Compute the current Moon state at the given UTC time.
 *
 * @param now UTC epoch (as returned by time(NULL)).
 * @return The illuminated fraction, lunation position and waxing flag.
 */
MoonState moon_phase_state(time_t now);

/**
 * Fill an array with the next `count` principal phase events occurring strictly
 * after `now`, in chronological order.
 *
 * @param now UTC epoch.
 * @param events Output array, must hold at least `count` elements.
 * @param count Number of upcoming events to compute.
 */
void moon_phase_upcoming(time_t now, MoonPhaseEvent *events, int count);

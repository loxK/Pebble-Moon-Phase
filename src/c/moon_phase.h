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
 * Computes the Moon's position in the lunation (for drawing), the dates of the
 * upcoming principal phases (new moon, first quarter, full moon, last quarter)
 * with Meeus' algorithm (Astronomical Algorithms, ch. 49, planetary terms
 * included), good to a few seconds, and the Earth-Moon distance (ch. 47).
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
  time_t time;           //!< UTC epoch of the event.
  int32_t distance_km;   //!< Earth-Moon distance at that instant, centre to centre.
  bool approaching;      //!< Whether the Moon is coming closer at that instant.
} MoonPhaseEvent;

/**
 * Range the Earth-Moon distance stays within from 2000 to 2100: the closest
 * perigee (356425 km, 2052-12-06) and the farthest apogee (406706 km,
 * 2002-03-14) sit just inside it.
 */
#define MOON_DISTANCE_MIN_KM 356400
#define MOON_DISTANCE_MAX_KM 406720

/**
 * A full moon closer than this is a supermoon. There is no official rule; this
 * is the timeanddate.com convention.
 */
#define MOON_SUPERMOON_KM 360000

/** Whether `event` is a full moon close enough to count as a supermoon. */
static inline bool moon_phase_is_supermoon(const MoonPhaseEvent *event) {
  return event->type == MOON_PHASE_FULL && event->distance_km < MOON_SUPERMOON_KM;
}

/** Current state of the Moon, for rendering. */
typedef struct {
  double fraction;       //!< Position in the lunation: 0=new, 0.25=first quarter, 0.5=full, 0.75=last quarter.
} MoonState;

/**
 * Lightweight sine/cosine for an angle in degrees.
 *
 * These avoid the C library's trig (whose large-argument range reduction has a
 * big stack footprint that overflows the small app stack on some platforms,
 * e.g. chalk). They range-reduce to [0, pi/4] and use a short Taylor series,
 * which is accurate to ~3e-8 -- far beyond what phase timing needs.
 */
double mp_sin_deg(double deg);
double mp_cos_deg(double deg);

/**
 * Compute the current Moon state at the given UTC time.
 *
 * @param now UTC epoch (as returned by time(NULL)).
 * @return The position in the lunation.
 */
MoonState moon_phase_state(time_t now);

/**
 * Earth-Moon distance at the given UTC time, in km, centre to centre.
 *
 * Meeus ch. 47 (the distance terms of table 47.A), good to a few tens of km.
 */
int32_t moon_distance_km(time_t t);

/** Whether the Moon is coming closer to the Earth at the given UTC time. */
bool moon_is_approaching(time_t t);

/**
 * Fill an array with the next `count` principal phase events occurring strictly
 * after `now`, in chronological order, each with the distance at its instant
 * and the way it is changing.
 *
 * @param now UTC epoch.
 * @param events Output array, must hold at least `count` elements.
 * @param count Number of upcoming events to compute.
 */
void moon_phase_upcoming(time_t now, MoonPhaseEvent *events, int count);

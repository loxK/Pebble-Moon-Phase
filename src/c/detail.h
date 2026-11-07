/*
 * Moon Phase -- detail view of a phase.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>
#include "i18n.h"
#include "moon_phase.h"

#if MOON_HAS_DETAILS
/**
 * Push a deck of cards about the Moon at `now`: how far it is and which way it
 * goes, then the next apogee and perigee in the order they come, the next node
 * passage and the next lunar eclipse, each with how long until it.
 */
void detail_push(time_t now, Lang lang);

/**
 * When an event shows: the greatest phase of its eclipse when the app names
 * it, otherwise the instant of the phase.
 */
time_t detail_event_time(const MoonPhaseEvent *event);

/** Title of an event: its eclipse, the supermoon or its phase name. */
const char *detail_event_title(const MoonPhaseEvent *event, Lang lang);
#endif

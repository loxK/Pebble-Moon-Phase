/*
 * Moon Phase -- launcher glance.
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <pebble.h>
#include "i18n.h"

/**
 * Launcher glance: the icon and the line of text shown under the app name in
 * the launcher.
 *
 * The glance holds a stack of slices, each with an expiration time, and the
 * launcher walks it on its own -- so one write covers weeks without the app
 * ever running again. A slice is cut wherever a day differs from the one
 * before it: the icon steps through the eight published shapes as the disc
 * fills, and the line names the phase alone on the day it falls, otherwise
 * counting down to it.
 */

/**
 * Rewrite the whole glance from the current time. Call it on the way out of the
 * app, once the hemisphere is settled.
 *
 * @param lang Active UI language.
 * @param southern true for the Southern hemisphere, where the lit side flips.
 */
void glance_publish(Lang lang, bool southern);

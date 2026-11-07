/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/*
 * Prints what the watch engine computes between two UTC epochs, one event per
 * line, for tests/engine.test.js to compare with the phone engine:
 *
 *   phase <time> <type> <km> <eclipse type> <solar> <eclipse time> <magnitude>
 *   node <time> <ascending>
 *   apsis <time> <perigee> <km>
 *   next <from> <time> <eclipse type> <magnitude>
 *   follow <time> <time of the phase found from it>
 *   state <time> <position in the lunation> <approaching> <km>
 *
 * Usage: engine_dump <from> <to>
 */

#include <stdio.h>
#include <stdlib.h>

#include "moon_phase.h"

#define MAX_EVENTS 4096

static const char *const ECLIPSE_TYPES[] = {
  "none", "penumbral", "partial", "total", "annular", "hybrid",
};

static bool prv_lunar_umbral(const MoonEclipse *e) {
  return !e->solar && (e->type == MOON_ECLIPSE_TOTAL || e->type == MOON_ECLIPSE_PARTIAL);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <from> <to>\n", argv[0]);
    return 2;
  }
  time_t from = (time_t)atoll(argv[1]);
  time_t to = (time_t)atoll(argv[2]);

  // moon_phase_upcoming() looks 40 lunations ahead at most, so the range goes
  // in batches of 100 events, well within that.
  static MoonPhaseEvent phases[100];
  for (time_t start = from; start <= to; start = phases[99].time) {
    moon_phase_upcoming(start, phases, 100);
    for (int i = 0; i < 100 && phases[i].time <= to; i++) {
      const MoonEclipse *e = &phases[i].eclipse;
      printf("phase %lld %d %ld %s %d %lld %.5f\n", (long long)phases[i].time, phases[i].type,
             (long)phases[i].distance_km, ECLIPSE_TYPES[e->type], e->type ? e->solar : 0,
             e->type ? (long long)e->time : 0LL, e->type ? e->magnitude : 0.0);
      MoonPhaseEvent follow;
      moon_phase_upcoming(phases[i].time, &follow, 1);
      printf("follow %lld %lld\n", (long long)phases[i].time, (long long)follow.time);
    }
  }

  static MoonNode nodes[MAX_EVENTS];
  int n = moon_nodes(from, to, nodes, MAX_EVENTS);
  for (int i = 0; i < n; i++) {
    printf("node %lld %d\n", (long long)nodes[i].time, nodes[i].ascending);
  }

  static MoonApsis apsides[MAX_EVENTS];
  int a = moon_apsides(from, to, apsides, MAX_EVENTS);
  for (int i = 0; i < a; i++) {
    printf("apsis %lld %d %ld\n", (long long)apsides[i].time, apsides[i].perigee,
           (long)apsides[i].distance_km);
  }

  // The Moon once a day.
  for (time_t t = from; t <= to; t += 86400) {
    printf("state %lld %.7f %d %ld\n", (long long)t, moon_phase_state(t).fraction,
           moon_is_approaching(t), (long)moon_distance_km(t));
  }

  // The next lunar eclipse seen from every 30 days of the range.
  for (time_t t = from; t <= to; t += 30 * 86400) {
    MoonEclipse e;
    if (moon_next_eclipse(t, prv_lunar_umbral, &e)) {
      printf("next %lld %lld %s %.5f\n", (long long)t, (long long)e.time, ECLIPSE_TYPES[e.type],
             e.magnitude);
    }
  }
  return 0;
}

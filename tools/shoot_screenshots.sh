#!/usr/bin/env bash
#
# Capture main + phase-list screenshots for every target platform into
# assets/screenshots/<platform>_{main,list}.png.
#
# To pin a specific Moon phase (e.g. for the store), set DBG_NOW to a fixed UTC
# epoch in src/c/moonphases.c and `pebble build` BEFORE running this, then set it
# back to 0 afterwards. emu-set-time is NOT used: it only refreshes the main
# screen (not the cached SELECT list) and is wiped when the app relaunches.
#
# Single emulator at a time: `pebble kill` clears the running QEMU before the
# next platform. Requires the toLocaleDateString-free pkjs (otherwise pypkjs
# OOM-crashes and every command spawns a fresh emulator).
#
# Usage:  tools/shoot_screenshots.sh
#
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="$ROOT/assets/screenshots"
mkdir -p "$OUT"

PLATFORMS="aplite basalt chalk diorite emery flint gabbro"

for P in $PLATFORMS; do
  echo "===== $P ====="
  pebble install --emulator "$P" 2>&1 | tail -1
  timeout 6 tail -f /dev/null                       # app launch + intro animation
  pebble screenshot --emulator "$P" --no-open "$OUT/${P}_main.png" 2>&1 | tail -1
  pebble emu-button click select --emulator "$P" 2>&1 | tail -1
  timeout 2 tail -f /dev/null                        # SELECT list slides in
  pebble screenshot --emulator "$P" --no-open "$OUT/${P}_list.png" 2>&1 | tail -1
  pebble kill 2>&1 | tail -1                         # tidy: one emulator at a time
done
echo "Done -> $OUT"

#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
#
# Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Draw the eight lunation icons, and the perigee and apogee ones, into
# resources/images/timeline/, at the three sizes the timeline pins and the
# launcher glance ask for.
#
# The disc is opaque -- lit side white, dark side black, black rim -- because
# the launcher draws glance icons untinted on white rows. Shapes are rendered
# oversampled and boxed down, which is where the grey edge pixels come from;
# everything outside the disc stays black under a zero alpha, so the downscale
# cannot bleed a light fringe into the rim.
#
# Usage:  tools/make_phase_icons.py

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parent.parent / "resources" / "images" / "timeline"

# Canvas size and rim thickness, in pixels. The rim keeps the lit side off a
# white launcher row; it stays thin, or it swallows the dark limb of a gibbous
# disc and the icon reads as a full Moon.
SIZES = {"tiny": (25, 1), "small": (50, 2), "large": (80, 3)}

# Position in the lunation, an eighth apart, under the name of the phase there.
PHASES = {
    "new": 0.0,
    "wax_cresc": 0.125,
    "first": 0.25,
    "wax_gibb": 0.375,
    "full": 0.5,
    "wan_gibb": 0.625,
    "last": 0.75,
    "wan_cresc": 0.875,
}

SUPERSAMPLE = 8


def terminator(fraction):
    """
    Where the lit side starts, as (threshold, waxing).

    A point of the disc sits at x / sqrt(r^2 - y^2) along its own chord, running
    from -1 on the left limb to +1 on the right one. The terminator is the
    ellipse crossing that scale at cos(2*pi*fraction), and the Sun lights the
    side it faces: the right one while the Moon waxes, the left one while it
    wanes. A positive threshold cuts a crescent out of the half disc, a negative
    one leaves a gibbous disc.

    Northern hemisphere throughout. The South sees the mirror image, which is
    the artwork of the opposite point of the lunation, so it needs no shape of
    its own.
    """
    angle = 2.0 * math.pi * fraction
    waxing = math.sin(angle) >= 0.0
    return (math.cos(angle) if waxing else -math.cos(angle)), waxing


def render(fraction, size, rim):
    """One icon, oversampled: luma and alpha planes `SUPERSAMPLE` times `size`."""
    threshold, waxing = terminator(fraction)
    span = size * SUPERSAMPLE
    centre = span / 2.0
    outer = (size / 2.0 - 1.0) * SUPERSAMPLE  # one pixel of margin for the rim's antialiasing
    inner = outer - rim * SUPERSAMPLE

    luma = Image.new("L", (span, span), 0)
    alpha = Image.new("L", (span, span), 0)
    lit = luma.load()
    opaque = alpha.load()

    for py in range(span):
        dy = py + 0.5 - centre
        reach = outer * outer - dy * dy
        if reach <= 0.0:
            continue
        half = math.sqrt(reach)
        for px in range(max(0, int(centre - half)), min(span, int(centre + half) + 1)):
            dx = px + 0.5 - centre
            radius = dx * dx + dy * dy
            if radius > outer * outer:
                continue
            opaque[px, py] = 255
            if radius > inner * inner:
                continue
            chord = math.sqrt(inner * inner - dy * dy)
            x = dx / chord
            if (x > threshold) if waxing else (x < threshold):
                lit[px, py] = 255

    return luma, alpha


def reduce(luma, alpha, size):
    box = (size, size)
    return Image.merge("LA", (luma.resize(box, Image.BOX),
                              alpha.resize(box, Image.BOX))).convert("RGBA")


def render_apsis(perigee, size, rim):
    """
    A full Moon with a black chevron on each side of its centre: pointing in at
    the perigee, where the Moon comes closest, and out at the apogee.
    """
    luma, alpha = render(PHASES["full"], size, rim)
    draw = ImageDraw.Draw(luma)
    s = SUPERSAMPLE
    centre = size * s / 2.0
    gap = 0.12 * size * s          # from the centre to the nearest point of a chevron
    depth = 0.13 * size * s
    half = 0.2 * size * s
    width = max(2, round(0.09 * size * s))
    for side in (-1, 1):
        near = centre + side * gap
        far = centre + side * (gap + depth)
        tip, back = (near, far) if perigee else (far, near)
        draw.line([(back, centre - half), (tip, centre), (back, centre + half)],
                  fill=0, width=width, joint="curve")
    return luma, alpha


def save(img, path):
    before = path.read_bytes() if path.exists() else None
    img.save(path, optimize=True)
    print(f"{path.name:34} {'unchanged' if path.read_bytes() == before else 'written'}")


def main():
    if not OUT.is_dir():
        sys.exit(f"missing {OUT}")
    for phase, fraction in PHASES.items():
        for name, (size, rim) in SIZES.items():
            save(reduce(*render(fraction, size, rim), size), OUT / f"moon_{phase}_{name}.png")
    for apsis, perigee in (("perigee", True), ("apogee", False)):
        for name, (size, rim) in SIZES.items():
            save(reduce(*render_apsis(perigee, size, rim), size), OUT / f"moon_{apsis}_{name}.png")


if __name__ == "__main__":
    main()

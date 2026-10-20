<div align="center">

# 🌙 Moon Phase

**See the Moon exactly as it looks in the sky tonight — right on your wrist.**

[![Pebble App Store](https://img.shields.io/badge/Pebble_App_Store-Moon_Phase-4b6cb7)](https://apps.repebble.com/86a2d34ade9646dfb84d51af)
[![Latest release](https://img.shields.io/github/v/release/loxK/Pebble-Moon-Phase?sort=semver&color=2ea44f)](https://github.com/loxK/Pebble-Moon-Phase/releases/latest)
[![Download .pbw](https://img.shields.io/badge/download-.pbw-2ea44f)](https://github.com/loxK/Pebble-Moon-Phase/releases/latest/download/moon-phase.pbw)
[![Build](https://github.com/loxK/Pebble-Moon-Phase/actions/workflows/release.yml/badge.svg)](https://github.com/loxK/Pebble-Moon-Phase/actions/workflows/release.yml)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue)](LICENSE)

<img src="assets/screenshots/emery_main.png" width="200" alt="Moon Phase main screen">

</div>

Moon Phase draws the Moon the way you'd actually see it — the real illuminated
shape, oriented for your hemisphere — then tells you what's next and when.
No phone required: everything is computed on the watch.

## Get it

- 📲 **[Install from the Pebble App Store](https://apps.repebble.com/86a2d34ade9646dfb84d51af)** — the easy way.
- ⬇️ Or grab the latest **[`moon-phase.pbw`](https://github.com/loxK/Pebble-Moon-Phase/releases/latest/download/moon-phase.pbw)** and sideload it.

## Features

- 🌒 **The real Moon** — the current phase drawn as a crisp disc, lit exactly
  like the sky above you.
- ⏳ **Next phase, at a glance** — *“In 4 days”* and the date, front and center.
- 📜 **What's coming** — press **DOWN** to slide through the upcoming new, first‑quarter,
  full and last‑quarter phases, one card each.
- 🧭 **Hemisphere aware** — the crescent flips for the Southern hemisphere,
  set automatically from your location.
- 🌍 **Speaks your language** — English, French, German, Spanish, Italian,
  Portuguese and Chinese, with local date formats.
- 📏 **How far away** — a gauge across the top fills as the Moon comes closer,
  and a full moon within 360,000 km is called out as a **supermoon**.
- 🗓️ **In your Timeline** — upcoming phases, perigees and apogees show up as
  pins, with the distance.
- 🎯 **Accurate & offline** — a proper astronomical model, good to the minute.

## Screenshots

<div align="center">
<img src="assets/screenshots/flint_main.png" width="180" alt="Black & white display">
&nbsp;&nbsp;
<img src="assets/screenshots/emery_main.png" width="180" alt="Pebble Time 2">
&nbsp;&nbsp;
<img src="assets/screenshots/gabbro_main.png" width="180" alt="Round display">

<sub>Black &amp; white · Pebble Time 2 · round — one app, every Pebble.</sub>

</div>

## Build from source

```sh
pebble build
pebble install --emulator emery      # try it in the emulator
pebble install --phone <ip>          # install to your watch
```

Press **DOWN** and **UP** to move between the upcoming phases.
Long‑press **SELECT** to flip the hemisphere by hand.

## Download & verify

Every [release](https://github.com/loxK/Pebble-Moon-Phase/releases/latest) ships
the compiled `moon-phase.pbw` together with its **SHA-256** (`moon-phase.pbw.sha256`).
It's the same build that's published to the Pebble App Store, so you can confirm
the file you got is authentic and untampered:

```sh
sha256sum -c moon-phase.pbw.sha256        # -> moon-phase.pbw: OK
```

The checksum is also printed in each release's notes.

## License

Copyright © 2026 Laurent Dinclaux &lt;laurent@knc.nc&gt;

Released under the **GNU Affero General Public License v3.0 or later**
(AGPL‑3.0‑or‑later). See [`LICENSE`](LICENSE).

---

Built with 🥥 and ☕ by Lox — Kanaky-New Caledonia 🇳🇨

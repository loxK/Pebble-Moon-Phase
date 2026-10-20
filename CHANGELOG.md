# Changelog

Notable changes to Moon Phase. Versions follow [semver](https://semver.org).

## Unreleased

### Added

- The upcoming phases are now cards you slide through with **UP** and **DOWN**, or a swipe on a touch screen, in place of the list. A column of dots shows where you are, and every card after the first gives the date and time of its phase.
- A gauge across the top shows how close the Moon is, with a tick at 360,000 km and an arrow telling whether it is approaching. Round watches get an arc along the bezel.
- Supermoons: a full moon closer than 360,000 km is named a supermoon on the cards, in the launcher and in the timeline.
- Perigees and apogees show up in the timeline with their distance, under two new icons.
- Larger phase names, and larger lines under them on the bigger screens.

### Changed

- Timeline pins are written in the watch's language, and only the pins that changed are pushed again.
- In English, the launcher line reads "in 1d" the day before a phase, so it fits on 144 px watches.

### Fixed

- Full moons were computed with the new moon corrections and came out up to two and a half minutes late. With Meeus' planetary terms and the conversion to UTC added, every phase is now within a second of the full algorithm.
- The Moon drawn for tonight matches each phase at its exact instant, instead of drifting by up to most of a day.
- Around a daylight saving change, the launcher could name a phase on the day before.
- In French, June and July no longer both read "jui" on round watches.
- In Spanish, "Cuarto menguante" no longer shrinks the Moon on 144 px watches.
- Updating from 1.1.1 deletes the old phase pins instead of leaving them next to the new ones.
- AM and PM follow the watch's language.

## 1.1.1 (2026-08-23)

### Fixed

- The gibbous phase icons are drawn as a lit disc.

## 1.1.0 (2026-08-17)

### Added

- Touch navigation, on firmware 4.33.2 and later.
- The launcher shows the upcoming phase and its countdown.
- Redrawn phase icons, with the intermediate shapes.
- A shooting star crosses the sky now and then.

### Changed

- The phase list highlights its rows in night blue and shortens the months.

## 1.0.0 (2026-06-30)

First release: the Moon as it looks tonight, oriented for your hemisphere, the next phase with its countdown and date, the list of upcoming phases, timeline pins, and eight languages.

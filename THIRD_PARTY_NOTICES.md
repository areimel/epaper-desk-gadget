# Third-party notices

This project's own source is MIT (see [LICENSE](LICENSE)). Firmware binaries are
a combined work with the libraries below. Because they link GxEPD2 (GPL-3.0),
**published firmware binaries are distributed under GPL-3.0**; the source for
any binary is this repository at the commit recorded in its `index.json`.

Status column: "in use" means it is a dependency of the current build;
"planned" means it is part of the design but not yet in the tree. Planned items
may change; this file is updated when they land.

| Component | Version or pin | Licence | Status |
|---|---|---|---|
| GxEPD2 (Meshtastic fork) | commit `c7eb4c3c167cf396ef4f541cc5d4c6aa42f3c46b` | GPL-3.0 | in use |
| U8g2 | 2.36.18 | BSD-2-Clause (code); fonts see below | in use |
| Adafruit GFX Library | pulled in by GxEPD2 | BSD | in use |
| ArduinoJson | to be pinned in `platformio.ini` | MIT | planned |
| Weather Icons by Erik Flowers | to be recorded when added | SIL OFL 1.1 | planned |
| Atkinson Hyperlegible Next | to be recorded when added | SIL OFL 1.1 | planned |

## GxEPD2 (Meshtastic fork)

E-paper panel driver. The pin is in `platformio.ini`:
`https://github.com/meshtastic/GxEPD2.git#c7eb4c3c167cf396ef4f541cc5d4c6aa42f3c46b`.
The library's own `LICENSE` file is the GNU General Public License, version 3.
Upstream is by Jean-Marc Zingg; the Meshtastic fork adds drivers for panel
revisions used on the Heltec Wireless Paper. Linking it makes the firmware
binary GPL-3.0, which is why binaries carry that licence while this
repository's source stays MIT.

## U8g2

Bitmap font engine and font data, used by the 1-bit canvas.
The U8g2 `LICENSE` file states that the library code is under the two-clause
BSD licence and that fonts are licensed under different conditions, per font
group (see <https://github.com/olikraus/u8g2/wiki/fntgrp>).

- **X11 Adobe/DEC fonts (Helvetica `helv*` and New Century Schoolbook `ncen*`).**
  The U8g2 `LICENSE` file reproduces the notice for these (copyright 1984-1989,
  1994 Adobe Systems Incorporated and 1988, 1994 Digital Equipment Corporation):
  permission to use, copy, modify, distribute and sell is granted without fee,
  provided the copyright notices appear in all copies and in supporting
  documentation, and that the Adobe and Digital names are not used in
  advertising or publicity without written permission. The fonts are provided
  "as is". That notice is kept in the U8g2 package and must travel with
  redistributions.
- **Logisoso (`logisoso*`, used for the large clock and temperature numerals).**
  The licence is not stated in the U8g2 package's `LICENSE` file, which defers
  to the wiki page above. Licence: **to be confirmed** before the first
  published release.

## Adafruit GFX Library

Drawing primitives and the `GFXcanvas1` framebuffer, pulled in by GxEPD2.
The library's `license.txt` is the "Software License Agreement (BSD License)",
copyright 2012 Adafruit Industries: redistributions must retain the notice, and
binary redistributions must reproduce it in their documentation.

## ArduinoJson (planned)

JSON parsing and serialisation for the feed, weather and Hacker News replies.
Licence: MIT, copyright 2014-2026 Benoit Blanchon (from the copy in this
checkout's build cache). Not yet a dependency in `platformio.ini`.

## Weather Icons by Erik Flowers (planned)

Weather glyphs, rasterised once to 24 px and 48 px 1-bit arrays by a dev-time
script, with the output committed. Licence: SIL Open Font License 1.1 for the
font. Not yet in the tree; the exact source version and the OFL text will be
recorded here when the icons are added.

## Atkinson Hyperlegible Next (planned)

Typeface for the device's web app, self-hosted, Latin subset, two weights.
Licence: SIL Open Font License 1.1. Not yet in the tree; the exact source
version and the OFL text will be recorded here when the font files are added.
Fonts are not sold on their own and keep their reserved font names, per the OFL.

## Data services

The device calls these over HTTPS, with no API keys and no accounts. Check each
service's current terms before relying on them or redistributing a unit.

- **Open-Meteo** (<https://open-meteo.com>): weather forecast and place search.
  Its free API is offered for non-commercial use and asks for attribution;
  weather data is licensed CC BY 4.0. Terms to be re-read before release.
- **Hacker News via Algolia** (<https://hn.algolia.com/api>): the public search
  API's `front_page` listing. Headlines belong to their submitters and
  Hacker News; the device shows only title, points and comment count. Terms to
  be re-read before release.
- **Google Calendar**: the device reads a private iCal address that you paste
  into the web app. It is stored on the device only and never returned by the
  API. Keep it secret: anyone with the link can read your calendar.

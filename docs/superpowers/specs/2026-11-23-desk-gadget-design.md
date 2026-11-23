# Desk Gadget: e-paper dashboard on the Heltec Wireless Paper

## Context

Alec has a Heltec ESP32 e-paper board plugged in at `/dev/ttyUSB0` and wants it to be
a desk dashboard that cycles through weather, Hacker News headlines, cute
motivational messages and quotes, and other day-to-day information, with a web app
for editing what it shows. The repo is a fresh clone of an ESP32 starter template
whose default app is a Bluetooth keyboard on an M5 colour screen.

Decided with Alec:

- The web app is **served by the device itself** on the home WiFi.
- The device is **always on USB power** (no sleep, WiFi always up).
- Pages: **weather, Hacker News, messages/quotes, clock and date, today's
  calendar, notes and to-dos**.
- Panel driver: **GxEPD2, GPL-3.0 accepted** for published firmware binaries.
- Board revision: **V1.1** (JD79656 panel, driver `GxEPD2_213_FC1`).
- Calendar provider: **Google Calendar** (secret iCal address).
- Web app is **open on the WiFi, no login**.

Found during recon:

- USB ID `10c4:ea60` (CP2102, serial `0001`) matches a **Heltec Wireless Paper**:
  ESP32-S3FN8, 8 MB flash, no PSRAM, 2.13" 250x122 1-bit panel, one user button
  (GPIO0), LED GPIO18, display power on GPIO45 (active low). Unverified until
  probed; the panel differs by revision (V1.0 SSD1680, V1.1 JD79656,
  V1.1.1/V1.2 SSD1682).
- The template's config pipeline, Improv WiFi provisioning, packaging, CI and
  installer site are reusable. Its display layer, board driver and app are
  M5/colour/keyboard specific.
- `AGENTS.md` forbids network services and runtime configuration "without a
  request". This request is that request; the rules get rewritten in step 1.

## Goals, non-goals, anti-goals

- **Goals:** a glanceable, calm dashboard readable from 60 cm; feed edited from
  any phone or laptop on the WiFi with no server and no account; the web preview
  shows exactly the pixels the panel shows.
- **Non-goals (v1):** LoRa, Bluetooth, battery operation and sleep, login/auth on
  the web app, sources beyond the six pages, quiet hours, multi-device.
- **Anti-goals:** a screen that flashes often, tiny unreadable text, stale data
  shown as if fresh, secrets in the repo or in published firmware.

## Approaches considered

| Choice | Picked | Rejected, and why |
|---|---|---|
| Where the editor lives | **On the device** (Alec's pick) | Pages site over Web Serial: only works from the plugged-in computer. Server on the PC: gadget goes stale when the PC is off. |
| Repo shape | **Dashboard becomes the default app**; strip the keyboard, M5 and macro example following `docs/new-project-checklist.md` | Keeping it as an `examples/` env: leaves two apps and the M5/BLE code to maintain, against the "fewer components" goal. History keeps the template. |
| Panel driver | **meshtastic/GxEPD2 fork**, pinned to a commit, `GxEPD2_213_FC1` for Alec's V1.1; panel type is one board-table value, so another revision is a one-line change | Upstream GxEPD2: only supports V1.0. heltec-eink-modules: no licence file, no U8g2 fonts. M5GFX/LovyanGFX: no driver for these controllers. Boot auto-detection: BUSY probing cannot tell V1.0 from V1.2, not worth it for one known board. |
| Rendering | **Own 1-bit framebuffer** (`GFXcanvas1`, 250x122) + U8g2 fonts; the panel driver only receives a finished bitmap | Drawing straight into GxEPD2: ties views to one driver and gives no bitmap to serve to the web preview. |
| Web UI build | **Hand-written HTML/CSS/ES modules**, gzipped into a generated header by a Python pre-build script | Astro/Vite bundle: makes every firmware build need Node. LittleFS image: adds a flashed part and installer/packager changes. |
| Fetching | **One background FreeRTOS task** fetches; loop thread renders and serves HTTP | Fetching in `loop()`: TLS requests block the button and web UI for seconds. |

**Licence (accepted by Alec):** the repo's own source stays MIT (`LICENSE`);
firmware binaries that link GxEPD2 are GPL-3.0. Add `THIRD_PARTY_NOTICES.md`
(GxEPD2, fonts, icons) and say so in the README and release notes.

**My calls after the design review** (correct any of these):

- Title bars show the page name and data age, **not the time**; only the clock
  page ticks each minute. Avoids a stale clock or per-minute flashing everywhere.
- Hacker News takes **one slot** in the rotation and shows its next 3 stories on
  each visit, so six pages cycle in 30 min.
- `Canvas`, `Draw` and the page views **also compile on the native env** (a thin
  host shim for the GFX/U8g2 drawing calls). This gives golden-image tests and
  lets the web app's mock server return real rendered screens. If the shim costs
  more than a day in Phase 0, fall back to device-only rendering and say so.
- Calendar polls **hourly** (Google's file carries full history).
- Improv returns the device's **IP URL**, and the setup page shows it on the
  panel; `.local` is a convenience, not the only way in.

## Design

### Device behaviour

- Rotates through enabled pages; default dwell **5 min**, minimum 3 min (panel
  wear). Button: click = next page now, hold = refetch everything.
- **Full refresh** on every page change and at least hourly. **Fast refresh** only
  for the clock's minute tick, with a full refresh after 5 fast ones.
- Every page shows data age; a failed source shows a plain stale/error line, never
  silently old data. No WiFi yet = a setup page with the installer URL.
- Runtime state: `feed.json` on LittleFS (the existing unused 1.5 MB `spiffs`
  partition). First boot writes defaults compiled from `config.yaml`.

### Screens (250x122, black on white, one 14 px inverted title bar)

```
+--------------------------------------------------+
|# Weather                            updated 9:30 #|   helvB08, inverted
|                                                  |
|  (icon)   12°            Light rain              |   logisoso32 numerals
|  48 px    feels 9°       H 14°  L 7°   rain 60%  |   helvB10 / helvR08
|                                                  |
|  Wed (i) 13/6    Thu (i) 9/4    Fri (i) 10/5     |   24 px icons
+--------------------------------------------------+
|# Hacker News  1/3                     updated 9:35#|
| Show HN: I built a tiny e-paper desk gadget      |   helvB10, 2 lines max
| that edits itself                                |
|   412 points · 96 comments                       |   helvR08
| Second headline truncated to two lines with...   |
|   230 points · 41 comments                       |
| Third headline                                   |
+--------------------------------------------------+
|# Note to self                                    #|
|                                                  |
|     "You are doing better than you think."       |   ncenB18 -> 14 -> helvB12
|                                                  |   stepped down to fit
|                                   — your desk    |
+--------------------------------------------------+
```

Clock: weekday/date in helvB10 over large logisoso numerals. Calendar: up to 4
rows `09:30  Standup`, all-day events first, "Nothing else today" when empty.
Notes: title plus up to 5 lines, `[ ]`/`[x]` drawn as boxes. HN shows 3 stories
per visit, stepping through the top 9.

**Type and icons:** U8g2 bitmap fonts only (X11 Adobe helv/ncen, Logisoso; all
permissive/OFL). Weather icons: Erik Flowers' Weather Icons (OFL) rasterised once
to 48 px and 24 px 1-bit arrays by a dev-time script, output committed. Status
glyphs from U8g2 `open_iconic` 2x. Strokes at least 2 px; no dithering under text.
Incoming text is folded to Latin-1 on the device (curly quotes, dashes, ellipsis).

### Data sources (all fetched by the device, no API keys)

- **Weather:** Open-Meteo forecast, every 30 min. Location and units set in the
  web app; the browser does the city search against Open-Meteo geocoding.
- **Hacker News:** one Algolia `front_page` request for 9 stories (title,
  points, comment count only; about 5 KB), every 15 min.
- **Time:** NTP via `configTzTime`; timezone picked in the web app from a bundled
  IANA-to-POSIX table. The feed stores both names. No fetch runs before NTP sync
  (TLS needs the date).
- **Calendar:** Google secret iCal URL, hourly, stream-parsed (never buffered
  whole). Lines are unfolded and over-long ones (descriptions) discarded. Supports
  single, all-day and multi-day events, and DAILY/WEEKLY/YEARLY recurrences
  (INTERVAL, BYDAY, UNTIL, COUNT, EXDATE). Moved and cancelled instances
  (`RECURRENCE-ID`, `STATUS:CANCELLED`) are reconciled through a small bounded
  table of today's candidates. `Z` times are converted; events whose `TZID`
  differs from the device's zone, and unsupported recurrence rules, are skipped
  and **counted, with the count shown in the web app** rather than shown at the
  wrong time. This is the riskiest page; it lands last.
- **Transport:** HTTPS with the core's built-in CA bundle (never `setInsecure`),
  one TLS session at a time. Parsers are byte sinks fed by
  `HTTPClient::writeToStream()` (handles chunked replies) so they also run in
  native tests.
- **Messages/quotes, notes:** typed in the web app; a starter pack of ~30 short
  messages ships in defaults.

### HTTP API (served on port 80, mDNS `desk-gadget.local`)

| Route | Purpose |
|---|---|
| `GET /` + static | the web app (gzipped, embedded) |
| `GET /api/state` | device name, version, panel, IP, time, current page, per-source last-updated/error, **validation limits** |
| `GET /api/feed` / `PUT /api/feed` | read / validate-and-save the feed; the calendar URL is write-only (returned as `set: true`) |
| `GET /api/screen?page=<id>` | 250x122 1-bit bitmap of the live screen, or a page rendered off-panel |
| `POST /api/show` `{page}` | jump the panel to a page |
| `POST /api/refresh` | refetch all sources now |

Limits (message count/length, note lines, dwell range) live in one firmware
header and reach the web app through `/api/state`; they are not duplicated in JS.
No login (Alec's choice): anyone on the WiFi can view and edit. Stated in the
README. The core's synchronous `WebServer` serves one client at a time, so the
UI ships as few files as possible, loads mini-screens one after another, and
request bodies are size-capped. `feed.json` carries a schema version and is
written to a temp file then renamed.

### Web app (device UI)

Subject: a desk object you set up once and glance at. The memorable element is
the **live screen itself**: a pixel-exact canvas of the panel (from
`/api/screen`, integer-scaled, `image-rendering: pixelated`) at the top, with the
page queue beneath it as a strip of real mini-screens you drag to reorder and
toggle. Selecting a page opens its settings next to its own preview, which
re-renders after "Save changes".

- **Palette:** paper `#E3E5DC` (panel white), ink `#17191B`, desk `#F6F7F3`
  (page background), highlighter `#F2D43D` (selection and focus only), red pencil
  `#C2402E` (errors/stale), graphite `#6B7075` (secondary text).
- **Type:** one self-hosted family, Atkinson Hyperlegible Next (OFL, Latin
  subset, two weights) for everything; sizes 14/16/20/28. No mono labels, no
  all-caps eyebrows.
- **Layout:** single left-aligned column, max ~44rem, screen first; works on a
  phone. No cards-with-shadows; the mini-screens are the only boxed elements.
- **Copy:** plain verbs ("Save changes" -> "Saved"), errors say what failed and
  what to do ("Calendar link didn't load. Check the address.").
- Reduced motion respected; visible keyboard focus; drag reorder has button
  equivalents.

The Astro installer site in `web/` stays; only its copy (`site.ts`) changes.

### Code layout (every file at or under 300 lines)

| Path | Responsibility |
|---|---|
| `boards/boards.yaml`, `scripts/config/boards.py` | new `wireless-paper` row, `driver: heltec`, `epaper:` pin block -> `BOARD_EPD_*`; unknown drivers become an error |
| `platformio.ini` | envs become `wireless-paper` (`board = esp32-s3-devkitc-1` with overrides, no PSRAM or USB-CDC flags, as `sticks3` does today) + `native`; `build_src_filter` widened to `src/pages/`; deps GxEPD2 fork, U8g2_for_Adafruit_GFX, Adafruit GFX, ArduinoJson (all pinned) |
| `lib/board/src/BoardHeltec.cpp` | button, LED, Vext power; `Board.h` loses the M5 display handle |
| `lib/epaper/` | `Panel` (probe revision, full/fast refresh, refresh counter), `Canvas` (framebuffer + fonts), `Draw` (title bar, wrapped text, fit-to-box, icon); `FEATURE_EPAPER` |
| `lib/text/` (pure) | UTF-8 fold, word wrap/truncate, relative-age formatting |
| `lib/feed/` (pure) | `FeedConfig` model, JSON (de)serialise + validation, `Limits.h`, page `Rotation` scheduler |
| `lib/sources/` (pure parsers) | `WeatherParse`, `HnParse`, `IcsParse` operating on streams/strings |
| `lib/net/` | existing WiFi/Improv/OTA plus `Fetcher` (task, mutex-copied snapshots, backoff), `Clock` (NTP), `Api` (routes), `FeedFile` (LittleFS; `lib/store` keeps its NVS `Settings`) |
| `src/pages/*.cpp` | one small view per page: plain model in, canvas out (the `StatusView` pattern) |
| `src/main.cpp` | wiring only |
| `scripts/config/sections/dashboard.py` | compile-time defaults (hostname, default pages, starter messages) |
| `scripts/embed_ui.py` | gzip `device-ui/` into a generated header before each build |
| `device-ui/` | `index.html`, `styles/`, `js/` modules, `mock/` dev server, `tests/` (node:test) |

Reused as-is: `lib/core` (`Log.h`, `Every.h`), `lib/input/Gesture.h`,
`lib/improv`, `lib/net` WiFi + Improv, `BootHold.h`, the section contract in
`scripts/config/core.py`, `package_firmware.py`, `firmware_images.py`, CI and
release workflows. Improv's provisioning reply gains the device URL
(`lib/net/src/ImprovSerial.cpp:45`). Removed: `lib/ble_hid`, `lib/display`,
`BoardM5.cpp`, `examples/macro-clicker`, `ble`/`buttons`/`keys` config sections
and their tests.

**Known ripple from the strip** (step 1 handles all of it):

- `button_count` moves from `sections/buttons.py` into `scripts/config/boards.py`
  (used by `boards.py` and `package_firmware.py`); `sections/__init__.py` drops
  `ble`/`buttons`.
- Packaging tests get their own multi-chip fixture `boards.yaml` instead of
  copying the real one (`scripts/tests/packaging_support.py`), so the classic
  ESP32 offset tests keep working. `support.py`, `test_platformio_envs.py`,
  `test_config_core.py`, `test_sections.py` are updated for the new board.
- Deleted: `test_buttons.py`, `test_macro_schema.py`, `test/test_ble`,
  `test/test_macro_*`, `test/macro_controller_sim.h`, NimBLE stubs.
- `index.json` keeps its `buttons`/`display`/`battery` fields
  (`web/src/lib/project.ts` requires them); CI gains `node --test device-ui/tests`.

## Execution

After approval: commit this design as
`docs/superpowers/specs/2026-11-23-desk-gadget-design.md`, then expand it into a
task-level plan with the writing-plans skill before any code.

**Phase 0, in sequence (me + one Sonnet agent):**
1. Strip the template to the new shape, rewrite `AGENTS.md` rules, add the board
   row/env; every env builds, `check.sh` passes.
2. Hardware spike: re-identify `/dev/ttyUSB0` (esptool chip + flash ID), flash a
   test card through the V1.1 driver (border, corner marks, text) to check
   rotation and the 122-of-128 px column offset, full then fast refresh. **Alec
   confirms what the panel shows.** If the card is wrong, try the other two
   drivers; if the fork fails on Arduino-ESP32 3.3.x, vendor and patch the driver
   file (still GPL) and report.
3. I freeze the shared contracts before any team starts: `Canvas`/`Draw` headers
   and the text-measure callback; snapshot structs for each source; `FeedConfig`
   JSON shape, page IDs, status enum, `Limits.h`; the byte-sink and injected-clock
   interfaces; API table including the `/api/screen` byte format and the
   "calendar URL omitted = keep" rule (`docs/api.md`); every `platformio.ini`
   edit and the embedded-file list. After this, `platformio.ini` and
   `sections/__init__.py` are mine alone.

**Phase 1, parallel teams on disjoint files** (an Opus lead plans each team's
tasks; Sonnet builds; Haiku does light work):

| Team | Scope | Models |
|---|---|---|
| A Render | `lib/epaper`, fonts, icon conversion, test card pages | Opus lead, 2 Sonnet, Haiku (icons) |
| B Logic | `lib/text`, `lib/feed`, `lib/sources`, native tests first (TDD) | Opus lead, 3 Sonnet (ICS gets its own), Haiku (fixtures) |
| C Network | `Fetcher`, `Clock`, `Store`, `Api`, `embed_ui.py` | Opus lead, 2 Sonnet |
| D Web app | `device-ui/` against the mock server, Playwright screenshots for design critique | Opus lead, 2 Sonnet, Haiku (copy, starter messages) |
| E Docs/site | `README`, `docs/`, `web/src/content/site.ts`, config section + Python tests | Sonnet, Haiku |

**Phase 2, integration (me):** page views, `main.cpp` wiring, flash, observe each
page on the panel with Alec, tune layout from photos/feedback.

**Phase 3:** independent code-review agents per area (given code, not
conclusions), a DRY/file-size pass, final verification. Roughly 20-30 agents in
total, well under the 50 cap.

**Commit checkpoints (15+, local only, never pushed):** design doc · template
strip · board + env · panel spike · contracts · canvas + fonts · text lib · feed
model · rotation · weather parser · HN parser · ICS parser · fetcher + clock ·
store + API · web app shell + preview · page editors · page views · main wiring ·
config defaults · docs + AGENTS · review fixes.

## Verification

- `bash scripts/check.sh` (native Unity tests + Python tests) and
  `.venv/bin/pio run -e wireless-paper` after every phase; `node --test
  device-ui/tests`; `npm run check && npm test && npm run build` in `web/`.
- Parsers tested against recorded real responses (Open-Meteo, Algolia, a
  recurring-event ICS file). Text wrap/fit and rotation tested on host.
- Web app: run against `device-ui/mock/`, screenshot at phone and desktop widths
  with Playwright, then against the real device at `http://desk-gadget.local`.
- On hardware, each observed by Alec or via `/api/screen` + serial log: panel
  revision detected; each of the six pages renders; button next/hold; edit in the
  web app appears on the panel; WiFi loss shows stale state and recovers; 1 h
  soak with no heap decline in `/api/state`.
- Anything not observed is recorded as pending in `TODO.md`, per `AGENTS.md`.

## Needs from Alec during the build

- WiFi: provision over the installer page, or put dev credentials in the
  gitignored `secrets.yaml` (not in chat).
- Look at the panel at the spike and at integration and say what it shows.
- Flashing is limited to the CP2102 device on `/dev/ttyUSB0`, re-identified
  before every upload.

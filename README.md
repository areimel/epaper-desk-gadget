# Desk Gadget

An e-paper desk dashboard for the Heltec Wireless Paper. It cycles through six
calm pages (clock, weather, Hacker News, messages, calendar and notes) on a
2.13 inch black and white screen, and serves a small web app on your WiFi where
you edit what it shows.

Built from an ESP32 starter template (PlatformIO, Arduino, YAML config, a
browser installer); the template's original app has been replaced.

## Status: mostly pending hardware checks

Be careful what you believe here. The firmware is being built in parts; only
this has been observed on a real unit (details in [TODO.md](TODO.md)): the chip
and flash were identified, and the panel test card showed upright and readable
with the `e0213a367` panel setting. Everything else, including WiFi setup, the
six pages, the web app and the button, is designed but **pending** physical
checks. A successful compile proves only that it compiles. No battery life
claims are made: the gadget is meant to stay on USB power.

## What it shows

| Page | Content |
|---|---|
| Clock | Weekday, date and large time; the only page that ticks each minute |
| Weather | Conditions, feels-like, high and low, rain chance, short forecast (Open-Meteo) |
| Hacker News | Three front-page stories per visit (Algolia API) |
| Messages | One of your messages or quotes, in the biggest type that fits |
| Calendar | Today's events from a private Google Calendar iCal link |
| Notes | A short title and a few lines, with checkboxes |

Pages turn every few minutes on purpose: e-paper refreshes slowly, and a full
refresh flashes the screen. Every page shows how old its data is, and a failed
source says so instead of showing old data as fresh. See
[docs/dashboard.md](docs/dashboard.md).

## Hardware

The Heltec Wireless Paper: ESP32-S3, 8 MB flash, no PSRAM, a 250x122 e-paper
panel, one button (GPIO0). The developer's unit is marked V1.1 but has the
newer `E0213A367` panel, so `boards/boards.yaml` sets `panel: e0213a367`. If
yours differs, that one value is what to change; how to tell is in
[docs/boards.md](docs/boards.md#which-panel-does-yours-have).

## Quick start

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/pio run -e wireless-paper
.venv/bin/pio device list           # identify the board before flashing
```

On Windows use `.venv\Scripts\pio` and `.venv\Scripts\pip`.

**Before flashing:** close serial monitors, re-identify the board (the CP2102
bridge is `10c4:ea60`; port names are only examples), read the chip and flash
size with esptool and expect an ESP32-S3 with 8 MB. Flash only a device you
have named. Then:

```sh
.venv/bin/pio run -e wireless-paper -t upload
```

**WiFi:** provision it over Improv Serial from the installer page after
install, or for development put `wifi_ssid` and `wifi_password` in the
gitignored `secrets.yaml` (copy `secrets.example.yaml`). Never put credentials
in `config.yaml`. Walkthrough: [docs/getting-started.md](docs/getting-started.md).

## The web app has no login

When the device is on your WiFi it serves the web app at its IP address and at
`http://desk-gadget.local`. **There is no login: anyone on the same network can
view and edit** your messages, notes, location and pages. Do not put anything
private in them and do not expose the device to the internet. The HTTP API is
in [docs/api.md](docs/api.md).

Your Google Calendar link is a secret address. You paste it into the web app;
it is stored only on the device, never returned by the API, and never in this
repository or in the firmware image.

## Data sources

| Source | Used for | Notes |
|---|---|---|
| Open-Meteo | Weather and place search | No key; terms in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) |
| Hacker News via Algolia | Front page stories | No key |
| Google Calendar iCal link | Today's events | Private link, stays on the device |
| NTP | Time | Fetches wait until the clock has synced (HTTPS needs the date) |

The device fetches everything itself over HTTPS with the core's certificate
bundle, one request at a time.

## Licensing

- The project's own source is **MIT** ([LICENSE](LICENSE)).
- **Firmware binaries are GPL-3.0**, because they link the GxEPD2 display
  driver. This applies to the binaries the installer writes and to any you
  build and share. Source for a release is this repository at the commit in its
  `index.json`.
- Libraries, fonts, icons and data-service terms:
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Commands

| Task | Command |
|---|---|
| Build the firmware | `.venv/bin/pio run -e wireless-paper` |
| Host C++ tests | `.venv/bin/pio test -e native` |
| Host and Python tests | `bash scripts/check.sh` (build first) |
| Python tests only | `.venv/bin/python -m unittest discover -s scripts/tests -v` |
| Package firmware | `.venv/bin/python scripts/package_firmware.py --output web/public/firmware` |
| Installer site | `cd web && npm ci && npm run check && npm test && npm run build` |

Testing details: [docs/testing.md](docs/testing.md). Flashing, recovery, OTA and
releases: [docs/flashing-and-ota.md](docs/flashing-and-ota.md).

## Configuration

`config.yaml` holds build-time settings: project name, WiFi hostname, OTA
switch, button timing and the `dashboard:` defaults (dwell and starter
messages). It is validated on every build and a bad value fails the build with
the key path. It is embedded in the firmware and published, so it must hold no
secrets. OTA and dev WiFi credentials go in the gitignored `secrets.yaml`; OTA
is compiled in only with `ota.enabled`, `wifi.enabled` and an `ota_password`,
so release builds ship with it off. What the screen shows day to day is edited
in the web app and stored on the device. Keys and limits:
[docs/configuration.md](docs/configuration.md); the limits themselves are in
`lib/feed/src/Limits.h`.

## Repository layout

```
config.yaml            project, wifi, ota, gestures, dashboard
secrets.example.yaml   template for the gitignored secrets.yaml
platformio.ini         env wireless-paper (firmware) and native (host tests)
boards/boards.yaml     board capability table, including the panel type
lib/                   core, board, input, store, improv, net, epaper, text, feed, sources
src/                   app wiring: main.cpp (pages in src/pages/ are planned)
device-ui/             the on-device web app (in progress)
scripts/               config generator, packager, checks, Python tests
test/                  native Unity tests and stubs
web/                   Astro installer site with ESP Web Tools
docs/                  guides; docs/superpowers holds the design and plan
THIRD_PARTY_NOTICES.md libraries, fonts, icons, data services
```

Some modules named in the docs are still being written by parallel workstreams;
[docs/modules.md](docs/modules.md) marks which are in the tree.

## Docs

| Guide | Contents |
|---|---|
| [Getting started](docs/getting-started.md) | Build, flash, WiFi, the web app |
| [The dashboard](docs/dashboard.md) | Pages, data sources, refresh behaviour, web app |
| [Hardware](docs/boards.md) | The board, pins, panel setting, what is verified |
| [Configuration](docs/configuration.md) | Every `config.yaml` and `secrets.yaml` key |
| [HTTP API](docs/api.md) | Device routes and shapes |
| [Modules](docs/modules.md) | Code map and boundaries |
| [Flashing and OTA](docs/flashing-and-ota.md) | Upload, web install, OTA, releases, recovery |
| [Testing](docs/testing.md) | Host tests, Python tests, site tests, CI |
| [AGENTS.md](AGENTS.md) | Rules for coding agents |
| [TODO.md](TODO.md) | Observed and pending physical checks |

# Modules

Each directory under `lib/` is a PlatformIO library with its public headers in
`lib/<name>/src/`. `src/main.cpp` wires them together; the headers are the
authoritative API. Status marks what is in the tree at the time of writing;
"planned" names come from the
[design spec](superpowers/specs/2026-11-23-desk-gadget-design.md) code layout
and may change when they land. `AGENTS.md` is the rule set.

## Feature gating

`config.yaml` and the board row drive these flags, emitted as compiler defines
(0 or 1) by `scripts/platformio_config.py`:

| Flag | On when |
|---|---|
| `FEATURE_WIFI` | `wifi.enabled` |
| `FEATURE_OTA` | `ota.enabled`, `wifi.enabled` and a password; see [flashing-and-ota.md](flashing-and-ota.md#ota-development-only) |
| `FEATURE_DASHBOARD` | `dashboard.enabled` and `wifi.enabled` |
| `FEATURE_EPAPER` | the board row has an `epaper:` block |

Every optional module wraps its whole source in `#if FEATURE_<X>`, so a
disabled module compiles to nothing regardless of PlatformIO's library finder
mode. Pure libraries include no Arduino headers, so they run under
`pio test -e native`.

## In the tree

| Module | Header | API |
|---|---|---|
| `lib/core` | `Log.h`, `Every.h` | `LOG_E/W/I/D`; `Every(periodMs)` wrap-safe periodic gate |
| `lib/board` | `Board.h`, `BootHold.h`, `BoardHeltec.cpp` | `begin`, `update`, `buttonCount`, `isPressed`, `setStatusLed`, `setPeripheralPower`; the only code that touches the button pin, LED and Vext |
| `lib/input` | `Gesture.h` | Pure, header-only debounce, single, double and long press |
| `lib/store` | `Settings.h` | NVS `Settings`; skips unchanged writes |
| `lib/improv` | `Improv.h` | Pure Improv Serial codec |
| `lib/net` | `Net.h` | WiFi, Improv handler (`net::updateImprov()`), ArduinoOTA |
| `lib/epaper` | `Panel.h`, `Canvas.h`, `Fonts.h` | The panel driver wrapper, the 250x122 1-bit framebuffer and the U8g2 font map; `FEATURE_EPAPER` |
| `lib/text` | `Font.h`, `Measure.h` | Pure font ids and the text-measure interface |
| `lib/feed` | `FeedConfig.h`, `Limits.h` | The feed shape, page ids and every size limit, in one header |
| `lib/sources` | `Snapshots.h`, `ByteSink.h` | Plain snapshot structs per source and the byte-sink interface |

Notes:

- `Limits.h` is the one place limits live. Firmware validation, storage sizes,
  the web app (through `GET /api/state`) and the `dashboard` config section
  all read it; do not copy its numbers elsewhere.
- Improv shares the log port, so log text and frames can interleave and
  non-Improv bytes are ignored. Provisioning is advanced only by
  `updateImprov()`, allows a connection attempt 15 s, persists credentials only
  after success and restores the previous ones on failure.
- `lib/net` OTA: `updateOta()` runs a whole transfer inline, so nothing else
  runs during it. No rollback is implemented.
- Never wait for USB serial anywhere.

## Planned

| Where | What |
|---|---|
| `lib/epaper` `Draw.{h,cpp}`, `Icons.h` | Title bar, wrapped and fitted text, weather icons as 1-bit arrays |
| `lib/text` | UTF-8 to Latin-1 folding, word wrap, relative age |
| `lib/feed` `FeedJson`, `Rotation.h` | Feed JSON (de)serialise and validation; the page rotation scheduler |
| `lib/sources` | Weather, Hacker News and ICS parsers over streams or bounded buffers |
| `lib/net` | `Fetcher` (one background task, backoff), `Clock` (NTP), `Api` (the routes in [api.md](api.md)), `FeedFile` (LittleFS) |
| `src/pages/*.cpp` | One small view per page: plain model in, canvas out, no input logic |
| `device-ui/` | The web app, embedded in the firmware by `scripts/embed_ui.py` |

## Config pipeline

`scripts/generate_config.py` plus `scripts/config/core.py` and
`scripts/config/sections/*.py` validate YAML and compile it into
`generated_config.h`. Each section exports `KEY`, `REQUIRED`, `DEFAULT`,
`INCLUDES`, `validate(value, ctx)`, `emit(cfg, ctx)` and optionally
`features(cfg, ctx)`. The built-in sections are `project`, `wifi`, `ota`,
`gestures` and `dashboard`. See [configuration.md](configuration.md).

## Packaging and site

`scripts/package_firmware.py` and `scripts/firmware_images.py` read build
outputs, validate images and the NVS-overlap guard, and write ESP Web Tools
manifests and `index.json`. `web/` is the independent Astro installer site; it
reads the packaged `index.json` and `web/src/content/site.ts` and does not
duplicate `config.yaml`.

## Dependency direction

`main.cpp` depends on every module. Pure libraries (`core`, `improv`, `input`,
`text`, `feed`, `sources`) know nothing about hardware or the network. Page
views render a model and know nothing about input. Keep every file at or under
300 lines.

## Adding a module

1. Create `lib/<name>/src/<Name>.h` (and `.cpp`).
2. If it is optional, add a `FEATURE_<X>` flag (a section's `features()` or the
   board row) and wrap the `.cpp` in `#if FEATURE_<X>`.
3. If it is pure logic, add `test/test_<name>/test_main.cpp`; if it touches
   hardware, add it to `lib_ignore` of `[env:native]` in `platformio.ini`.
4. Update this page and `AGENTS.md`.

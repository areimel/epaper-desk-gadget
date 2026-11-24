# Configuration

Edit `config.yaml`, then rebuild. The generator
(`scripts/generate_config.py`, run by `scripts/platformio_config.py` before
every build) validates the file and compiles it into
`.pio/build/<env>/generated/generated_config.h` (namespace `AppConfig`) plus
`FEATURE_*` and `BOARD_*` compiler defines. Firmware never parses key names,
text or labels.

This is build-time configuration. What the dashboard shows day to day (pages,
place, messages, notes, calendar link) is edited in the device's web app and
stored on the device; see [dashboard.md](dashboard.md).

Rules:

- Unknown keys, duplicate keys and wrong types are errors, reported with the
  path, for example `dashboard.messages[3].text`.
- Never put secrets in `config.yaml`. It is embedded in the firmware and in the
  published `firmware/index.json`.
- Keys marked "required" must be present; every other section may be omitted
  and takes its default.
- A top-level `schema: <path.py>` adds project-specific sections; see
  [Custom sections](#custom-sections).

## Keys

Each section is its own module in `scripts/config/sections/`. Limits are
enforced by those modules; the dashboard limits are read from
`lib/feed/src/Limits.h`, so this table points there instead of copying them.

| Key | Required | Default | Limits | Effect |
|---|---|---|---|---|
| `project.name` | yes | none | 1-32 printable ASCII characters (trimmed) | `AppConfig::PROJECT_NAME`; shown in the log and installer |
| `project.tagline` | no | `""` | text, at most 120 characters | Site only |
| `project.description` | no | `""` | text, at most 500 characters | Site only |
| `wifi.enabled` | no | `false` | boolean | Sets `FEATURE_WIFI`; also enables Improv in the web installer |
| `wifi.hostname` | no | `esp32-device` | RFC 1123 label, 1-32 letters, digits or hyphens | `AppConfig::WIFI_HOSTNAME`; the mDNS and OTA name (`desk-gadget` in the shipped file) |
| `ota.enabled` | no | `false` | boolean | Requests `FEATURE_OTA`; see [OTA gating](#ota-gating) |
| `gestures.debounce_ms` | no | `25` | integer 1-100 | Button debounce |
| `gestures.double_click_ms` | no | `250` | integer 50-1000 | Window for a second press |
| `gestures.long_press_ms` | no | `650` | integer 100-10000 | Hold time for a long press (refetch) |
| `dashboard.enabled` | no | `true` | boolean | Requests `FEATURE_DASHBOARD`; needs `wifi.enabled` |
| `dashboard.dwell_s` | no | the default in `Limits.h` | integer within the dwell range in `Limits.h` | `AppConfig::DEFAULT_DWELL_S`, the first-boot page dwell |
| `dashboard.messages` | no | `[]` | at most the message count in `Limits.h` | Starter messages: `AppConfig::STARTER_MESSAGES[]` and `STARTER_MESSAGE_COUNT` |
| `dashboard.messages[i].text` | yes | none | non-empty, no control characters, bytes of UTF-8 at most the text limit in `Limits.h` | Message text |
| `dashboard.messages[i].by` | no | `""` | bytes of UTF-8 at most the attribution limit in `Limits.h` | Shown under the message |
| `schema` | no | none | path to a `.py` file relative to the config | Adds sections |

`gestures` must satisfy `debounce_ms < double_click_ms < long_press_ms`.

Text limits are bytes, not characters. Keep starter messages to plain ASCII:
text is folded to Latin-1 on the device and anything it cannot map prints as
`?`. If a limit is exceeded the build fails and names the key, for example
`dashboard.messages[3].text: is 150 bytes of UTF-8; the limit is 140`. Because
the validator parses `Limits.h`, changing a limit there changes the check with
no second edit.

Disabling a module removes its code from the build. The e-paper code is
selected by the board row, not by `config.yaml`.

## OTA gating

OTA needs `ota.enabled`, `wifi.enabled` and an `ota_password` in
`secrets.yaml`; the conditions and warnings are in
[flashing-and-ota.md](flashing-and-ota.md#ota-development-only). The dashboard
has the same WiFi dependency: with `wifi.enabled: false` the build prints
`warning: dashboard disabled: wifi.enabled is false`.

## secrets.yaml

Copy `secrets.example.yaml` to `secrets.yaml` (gitignored). Only these keys are
accepted, each a string; anything else is an error.

| Key | Use |
|---|---|
| `ota_password` | ArduinoOTA password; emitted as `AppConfig::OTA_PASSWORD` only when OTA is on |
| `wifi_ssid` | Development WiFi, used only if the device has no stored credentials |
| `wifi_password` | Same |

`DEV_WIFI_SSID` and `DEV_WIFI_PASSWORD` are empty strings when unset. These
values are compiled into your local firmware image, so do not publish a binary
built with them. The packager refuses to package when `secrets.yaml` sets any
value or OTA is compiled in, because `firmware.bin` then holds them in
plaintext; `--allow-secrets` overrides that for a deliberate local package
([flashing-and-ota.md](flashing-and-ota.md#package-locally)). A malformed
`secrets.yaml` is reported by line and column only, never with its text. The
Google Calendar link is not a build-time secret: you enter it in the web app
and it stays on the device.

## Per-env overrides

Set these in `platformio_override.ini` (gitignored; see
`platformio_override.ini.example`) or in an env section:

| Option | Default | Meaning |
|---|---|---|
| `custom_board` | the env name | Row of `boards/boards.yaml` to use |
| `custom_config` | `config.yaml` | Config file for this env |

## Custom sections

A `schema:` module is loaded after the built-in sections and follows the same
contract as `scripts/config/sections/*.py`:

| Name | Meaning |
|---|---|
| `KEY`, `REQUIRED`, `DEFAULT` | Top-level key, whether it must be present, value used when absent |
| `INCLUDES` | C++ includes added to the generated header |
| `validate(value, ctx)` | Return the validated value or raise `ConfigError(path, message)` |
| `emit(cfg, ctx)` | Return lines placed inside `namespace AppConfig` |
| `features(cfg, ctx)` | Optional; return `{FEATURE_X: bool}` |

`ctx` has `board_id`, `board`, `secrets`, `warnings` and `features`. A section
cannot see another section's value, so a dependency such as "needs WiFi" is
applied in `generate_config.render` after every section has run. Helpers
(`fields`, `integer`, `boolean`, `cstr`, `ConfigError`) are in
`scripts/config/core.py`.

## Generated defines

The pre-build script also defines, per env: `APP_BOARD_ID`, `BOARD_LABEL`,
`FIRMWARE_VERSION`, `BOARD_DRIVER_HELTEC`, `BOARD_BUTTON_COUNT`,
`BOARD_BUTTON_PINS`, `BOARD_LED_PIN`, `BOARD_LED_RGB`, `BOARD_HAS_DISPLAY`,
`BOARD_HAS_BATTERY`, `BOARD_POWER_PIN`, `BOARD_POWER_ACTIVE_LOW`, the
`BOARD_EPD_*` pins, one `BOARD_EPD_PANEL_<PANEL>` and
`FEATURE_EPAPER`, plus one `FEATURE_WIFI`/`FEATURE_OTA`/`FEATURE_DASHBOARD`
flag as 0 or 1. `FIRMWARE_VERSION` is `git describe --tags --always`, or
`0.0.0-dev` outside a git checkout; there is no version in `config.yaml`.

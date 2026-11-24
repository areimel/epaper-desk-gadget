# Testing

Run the host checks before every commit that touches logic, then build the
firmware env. Passing tests and compiles do not prove button behaviour, e-paper
rendering, WiFi provisioning, the web app on a real network, OTA or battery
runtime; those are physical checks tracked in [../TODO.md](../TODO.md).

## Commands

| Check | Command | Needs |
|---|---|---|
| Native C++ tests | `.venv/bin/pio test -e native` | tools only |
| Python tests | `.venv/bin/python -m unittest discover -s scripts/tests -v` | a firmware build first |
| Both | `bash scripts/check.sh` | a firmware build first |
| Firmware build | `.venv/bin/pio run -e wireless-paper` | tools only |
| List envs | `python scripts/list_envs.py` | nothing |
| Site | `cd web && npm ci && npm run check && npm test && npm run build` | Node 22.12 or newer |
| Device web app (planned) | `node --test device-ui/tests` | Node |

`scripts/check.sh` uses `.venv/bin` tools when a virtualenv exists and falls back
to tools on `PATH` (as in CI). The packaging tests read the build output and
PlatformIO's esptool, so build first:

```sh
.venv/bin/pio run -e wireless-paper
```

## Native tests (`test/`)

`[env:native]` builds with the host compiler and `-Itest/stubs`, and ignores
`board`, `net` and `store`. Only pure logic runs there; files that include
Arduino or hardware headers cannot. Put logic in a header with no Arduino
includes and test that. Current suites:

| Suite | Covers |
|---|---|
| `test_gesture` | `Gesture`: debounce, double window, one-shot long press, release suppression |
| `test_every` | `Every`: first call, period, millis wraparound |
| `test_improv` | Improv codec: round trips, resync after garbage, bad checksums, RPC results |
| `test_boot_hold` | `holdAllButtonsAtBoot` against a scripted board |
| `test_canvas` | The 1-bit canvas |
| `test_contracts` | Page ids, feed defaults and limits shared by firmware and web app |

Planned suites (from the plan, not in the tree yet): text folding and
wrapping, feed JSON, rotation, the weather, Hacker News and ICS parsers, fetch
backoff policy and page golden images. Add a suite as
`test/test_<name>/test_main.cpp` with Unity.

## Python tests (`scripts/tests/`)

| File | Covers |
|---|---|
| `test_config_core.py` | YAML loader (duplicate keys), `fields`, `integer`, `cstr`, secrets loading, section loading |
| `test_sections.py` | Built-in sections, limits, errors with paths, feature flags, OTA gating, board defines |
| `test_dashboard_section.py` | The `dashboard` section: defaults, every limit boundary read from `Limits.h`, error paths, emitted C++, the WiFi dependency |
| `test_platformio_envs.py` | The env list matches `platformio.ini` and the board table |
| `test_packaging.py` | Manifest shape, offsets, NVS-overlap guard, checksums, atomic publish, image validation (against `scripts/tests/fixtures/boards.yaml`) |
| `test_packaging_secrets.py` | Refusal to package builds with `secrets.yaml` values or OTA, no secret in any packaged file |

## Site tests (`web/`)

| Script | Does |
|---|---|
| `npm run check` | `astro check`, type and template check |
| `npm test` | `tsx --test tests/*.test.ts`: index validation, the build guard `check-firmware.ts`, paths and hashes |
| `npm run build` | Verifies a packaged `public/firmware/` if present, then `astro build` |

Without packaged firmware the build prints "No packaged firmware; building
without a release" and the site shows the "no release" state.

## CI

`.github/workflows/ci.yml` runs on pull requests and pushes to `main`:

| Job | Does |
|---|---|
| `envs` | `scripts/list_envs.py` lists every `[env:*]` except `native` |
| `build` | `pio run -e <env>` per env, no `secrets.yaml`, so OTA is off |
| `test` | Builds, runs `scripts/check.sh`, then a packaging dry run into a temp directory |
| `web` | `npm ci`, `check`, `test`, `build` in `web/` |

Tooling comes from the composite action `.github/actions/setup`. The release
workflow reuses it; see [flashing-and-ota.md](flashing-and-ota.md).

## Before you claim something works

- State what ran: the command, the env and the result.
- A successful compile proves only that it compiles.
- Report physical checks as pending unless you or the user observed them.
- Do not quote battery life; none has been measured.

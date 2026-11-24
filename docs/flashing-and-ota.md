# Flashing, OTA and releases

Pick how you get firmware onto the Wireless Paper: USB upload from PlatformIO,
the browser installer built from a release, or OTA for development builds. The
only upload observed so far is the panel test card; the rest is pending, see
[../TODO.md](../TODO.md).

## USB upload

```sh
.venv/bin/pio device list                    # identify the board
.venv/bin/pio run -e wireless-paper -t upload
.venv/bin/pio device monitor
```

- Port names (`/dev/ttyUSB0`, `COM5`) are examples, not stable identifiers.
  Re-identify the board after every reconnect, confirm the chip (ESP32-S3) and
  8 MB flash with esptool before writing anything, and flash only a device you
  have named. The board's bridge is a CP2102 (`10c4:ea60`).
- Close the serial monitor before uploading.
- Pin a port locally in `platformio_override.ini` (gitignored):
  `upload_port = /dev/ttyUSB0`.
- If the port does not appear, use a data cable. The board's USB bridge is on
  UART0, so a normal upload needs no download-mode button sequence; the exact
  recovery steps for a bad flash are not verified here.
- A normal upload is expected to keep NVS (stored WiFi credentials survive),
  because it does not rewrite that partition. Not yet observed.

### Recovery

```sh
.venv/bin/pio run -e wireless-paper -t erase   # erases the whole flash, including NVS
.venv/bin/pio run -e wireless-paper -t upload
```

After an erase, WiFi must be provisioned again and the stored feed
(`feed.json`, in the LittleFS partition) is gone, so the web app starts from
the defaults. Only run erase against a device you have identified and the user
has authorised.

## Browser installer

The site in `web/` embeds `esp-web-tools` and installs from the packaged
firmware. It needs a browser with Web Serial (Chrome, Edge, Opera on desktop).

What it writes, per board (`<board>/manifest.json`):

| Part | Offset |
|---|---|
| `bootloader.bin` | `0x0` on the ESP32-S3 (the offset map is in `scripts/firmware_images.py`) |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | the `otadata` partition offset from that board's `partitions.bin` |
| `firmware.bin` | the first `app` partition offset |

No merged image is used and nothing is written inside the NVS partition, so a
normal install is expected to keep stored WiFi credentials (pending a physical
check). The packager checks the layout per
board and fails if any part overlaps NVS or the app is larger than its
partition. The manifest sets `new_install_prompt_erase: true`, so the installer
offers an erase and you choose. With `wifi.enabled: true` the manifest sets
`new_install_improv_wait_time: 15`, so the installer offers Improv WiFi setup
after the flash.

### Package locally

Packaging reads existing build outputs, so build first. Build without
`secrets.yaml`: the packager refuses an env whose `secrets.yaml` sets any value
or that has OTA compiled in, because those are plaintext in `firmware.bin`.
Move `secrets.yaml` aside and rebuild before packaging for publication.

```sh
for e in $(python scripts/list_envs.py | python -c "import json,sys;print(' '.join(json.load(sys.stdin)))"); do
  .venv/bin/pio run -e "$e"
done
.venv/bin/python scripts/package_firmware.py --version v0.1.0 --output web/public/firmware
cd web && npm ci && npm run build
```

| Option | Meaning |
|---|---|
| `--output DIR` | Required. Packager-owned names are swapped in atomically; other files in `DIR` are untouched |
| `--version` | `vX.Y.Z` or `X.Y.Z` with optional suffix; default `git describe --tags --always`, else `0.0.0-dev` |
| `--commit` | Full lowercase SHA; default `git rev-parse HEAD` |
| `--env ENV` | Package only this env; repeat for several. Default: every env whose board id equals its env name |
| `--project DIR` | Repository root; default is this repository |
| `--allow-secrets` | Package even though `secrets.yaml` sets a value or OTA is compiled in; prints a warning. Local use only, never publish the output |

Two envs on the same board cannot be packaged in one run. `index.json` holds a
copy of the parsed `config.yaml`, so that file must contain no secrets. Output:
`index.json` (schema version 2) plus a directory per board holding the four
`.bin` files and `manifest.json`.

If `web/public/firmware/` is absent, `npm run build` still succeeds and the
site shows a "No release published yet" state. If it is present, the build
verifies every part's size and SHA-256 and refuses to build on a mismatch.

### Site environment variables

| Variable | Used for |
|---|---|
| `BASE_PATH` | Astro `base`, for example `/my-repo/` on GitHub Pages; default `/` |
| `SITE_URL` | Astro `site`; default `http://localhost:4321` |
| `REPO_URL` | "Source" link on the page; falls back to `GITHUB_REPOSITORY`, else no link |

Page copy is in `web/src/content/site.ts`; project name, tagline and
description come from `index.json` when a release is packaged.

## OTA (development only)

OTA is compiled in (`FEATURE_OTA`) only when all of these hold; otherwise the
build prints the warning and continues without it. CI and release builds have no
`secrets.yaml`, so they ship with OTA off.

| Condition | Warning when missing |
|---|---|
| `ota.enabled: true` | none (OTA simply off) |
| `ota_password` in `secrets.yaml` | `OTA disabled: no ota_password in secrets.yaml` |
| `wifi.enabled: true` | `OTA disabled: wifi.enabled is false` |

To use it on your own device:

1. `cp secrets.example.yaml secrets.yaml` and set `ota_password` and, if needed,
   `wifi_ssid` and `wifi_password`.
2. Keep `wifi.enabled: true` and `ota.enabled: true` in `config.yaml`.
3. Build and flash once over USB. The build must not print
   `OTA disabled: ...`.
4. Once the board is on the same LAN, upload over WiFi. In
   `platformio_override.ini`:

   ```ini
   [env:wireless-paper]
   upload_protocol = espota
   upload_port = desk-gadget.local   ; wifi.hostname, or the device IP
   upload_flags = --auth=<your ota_password>
   ```

   then `.venv/bin/pio run -e wireless-paper -t upload`.

Behavior and limits:

- The OTA hostname is `wifi.hostname`; the password is compiled into the image.
  Do not distribute binaries built with a real `secrets.yaml`; the packager
  refuses them unless given `--allow-secrets`.
- A transfer runs inside `net::updateOta()`, so the app loop (including the web
  app and page rotation) does not run during it. Success restarts the device.
- The panel keeps showing its last image during a transfer.
- No rollback is implemented. A bad OTA image needs a USB reflash.
- OTA needs room for a second app partition. The 8 MB layout in
  `default_8MB.csv` has two. Check the `firmware.bin` size against the app
  partition after adding code.
- The `espota` upload commands above are documented from PlatformIO's
  conventions and have not been run.

## Releases

A release is a tag `vX.Y.Z` pushed to GitHub (or the `Release` workflow run
manually with an existing tag). The workflow:

1. Rejects tags that are not stable `vX.Y.Z` or that already have a release.
2. Builds every env from `scripts/list_envs.py` with no `secrets.yaml`, so OTA is
   off.
3. Runs `scripts/check.sh` (native tests and Python tests).
4. Runs `scripts/package_firmware.py --version <tag> --output web/public/firmware`.
5. Builds the site with `BASE_PATH=/<repo-name>/` and
   `SITE_URL=https://<owner>.github.io`, so a fork needs no edits.
6. Creates the GitHub Release with `index.json` and one `<board>.zip` per
   packaged board, then deploys the site to Pages.

Prerequisite: in the repository settings, set Pages to deploy from GitHub
Actions. To cut a release, from a clean `main`, choose an unused version:

```sh
git tag vX.Y.Z
git push origin vX.Y.Z
```

(Documented, not run: the release workflow has not been executed.) CI (`.github/workflows/ci.yml`) runs on pull requests and pushes to
`main`: one build job per env, the host and packaging checks, and the site
checks. Its jobs and local equivalents are in [testing.md](testing.md).

## Licences

The repository's own source is MIT. Firmware binaries link GxEPD2 (GPL-3.0), so
**published binaries, including the ones the installer writes, are GPL-3.0**.
The corresponding source is this repository at the commit recorded in
`index.json`. Third-party details are in
[../THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md); say the same in release
notes.

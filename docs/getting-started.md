# Getting started

Build the firmware for the Heltec Wireless Paper, flash it, join WiFi and open
the web app. What the code is written to do is described here; what has been
seen on a real unit is only what [../TODO.md](../TODO.md) records under
"Observed". Everything else is pending.

## 1. Set up tools

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt   # PlatformIO and PyYAML
```

On Windows the same tools are `.venv\Scripts\pio` and `.venv\Scripts\pip`. The
generator runs inside PlatformIO's own Python, so PyYAML must be installed in
the environment that provides `pio`. The first build downloads the pinned
pioarduino platform (Arduino-ESP32 3.3.x), U8g2 and the GxEPD2 fork.

## 2. Build

```sh
.venv/bin/pio run -e wireless-paper
```

Each build validates `config.yaml`, writes
`.pio/build/wireless-paper/generated/generated_config.h` and prints warnings
such as `warning: OTA disabled: no ota_password in secrets.yaml`. A
configuration error stops the build with `Configuration error:` and the path of
the bad key. Never edit or commit the generated header.

Check the panel setting for your board first: this unit is marked V1.1 but
needs `panel: e0213a367`. See [boards.md](boards.md#which-panel-does-yours-have).

## 3. Flash

Only flash a device you have named and identified. Follow these steps in order.

1. Close any serial monitor.
2. List devices: `.venv/bin/pio device list`. The board's USB bridge is a
   CP2102 (`10c4:ea60`). Port names such as `/dev/ttyUSB0` are examples, not
   stable identifiers; check again after every reconnect.
3. Confirm the chip and flash with esptool (it ships inside PlatformIO's
   tools): expect an ESP32-S3 with 8 MB flash.
4. Upload: `.venv/bin/pio run -e wireless-paper -t upload`.
5. Watch the log: `.venv/bin/pio device monitor` (115200 baud).

To pin a port without editing `platformio.ini`, copy
`platformio_override.ini.example` to `platformio_override.ini` (gitignored) and
set `upload_port` and `monitor_port`. Never run erase, upload or `espota`
against a device you have not identified. Recovery and the browser installer are
in [flashing-and-ota.md](flashing-and-ota.md).

The firmware never waits for a USB serial host. Levels are set with
`-DLOG_LEVEL=n` (0 off to 4 debug, default 3).

## 4. Join WiFi

There are two ways, both without any network service on the device.

**Improv Serial (normal).** With `wifi.enabled: true`, WiFi is provisioned over
USB from the web installer page right after install, or from any Improv client
on the serial port. Provisioning never blocks the app: a connection attempt
gets 15 seconds, credentials are saved only after the connection succeeds, and
the previous ones are restored if it fails. Improv shares the log port; other
bytes are ignored. The reply is planned to include the device's IP URL.

**Development credentials.** Copy `secrets.example.yaml` to `secrets.yaml`
(gitignored) and set `wifi_ssid` and `wifi_password`. They are used only when
the device has no stored credentials, are held in RAM and never overwrite
credentials saved by Improv. Do not publish a binary built with them. Never put
credentials in `config.yaml` or paste them into chat or issues.

While credentials exist but the device is not connected it retries every 10 s.
Whether a given unit joins your network is pending a physical check.

## 5. Open the web app

On the same WiFi, open the device's IP address, or `http://desk-gadget.local`
(the hostname is `wifi.hostname`; `.local` depends on your network). From there
you set your city, timezone, calendar link, messages and notes, and reorder
pages. **The web app has no login: anyone on your WiFi can view and edit.**
Details, routes and data sources: [dashboard.md](dashboard.md) and [api.md](api.md).

The web app and its device-side server are being built by other workstreams
(planned: `device-ui/`, `lib/net` `Api`). At the time of writing the app in
`src/` is only the panel test card; check `TODO.md` and `git log` for what has landed.

## 6. Use the button

One button (GPIO0). The design is: click shows the next page now, hold
refetches every source. Press timing is `gestures` in `config.yaml`. Button
detection is pending a hardware check.

## 7. Edit the build-time config

Change `config.yaml` (name, hostname, dwell, starter messages) and rebuild.
Keys are in [configuration.md](configuration.md).

## Next

- Development OTA, packaging and releases: [flashing-and-ota.md](flashing-and-ota.md).
- Tests: [testing.md](testing.md).
- Code map: [modules.md](modules.md).

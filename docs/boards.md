# Hardware

The only target is the Heltec Wireless Paper. Facts are split on purpose:
`boards/boards.yaml` says what the hardware has, `platformio.ini` says how to
build for it. Nothing is stated twice.

## The board

| Fact | Value |
|---|---|
| Env and board id | `wireless-paper` |
| Chip | ESP32-S3 (read with esptool: QFN56, rev v0.2) |
| Flash and memory | 8 MB embedded flash, no PSRAM |
| Display | 2.13 inch, 250x122, 1-bit black and white e-paper |
| Button | one user button, GPIO0 (PRG) |
| Status LED | GPIO18, single colour |
| Peripheral power | GPIO45, active low ("Vext"); feeds the panel and must be on before it is used |
| Panel pins | MOSI 2, SCK 3, CS 4, DC 5, RST 6, BUSY 7 |
| USB serial | a CP2102 bridge on UART0 (`10c4:ea60`); no USB-CDC flags are used |

No battery support is configured and no battery claims are made; the gadget is
designed to stay on USB power with WiFi always up.

GPIO0 is also a strapping pin: held during reset it enters ROM download mode,
so a boot-time hold must start right after reset (see
`lib/board/src/BootHold.h`).

Build facts live in `platformio.ini`: the `esp32-s3-devkitc-1` PlatformIO board,
the `default_8MB.csv` partitions (its spiffs partition is what the feed file
uses through LittleFS), 8 MB flash, and the GxEPD2 dependency pin.

## Which panel does yours have?

Heltec has shipped this board with different panel controllers over its
revisions, and the label on the board does not settle which you have. The
panel type is one value, `epaper.panel`, in the `wireless-paper` row of
`boards/boards.yaml`. The accepted values are `bn`, `fc1` and `e0213a367`; an
unknown value stops the build with the key path.

The unit this was developed on is marked V1.1 but carries the newer
`E0213A367` panel (an SSD1682 controller), so the row says
`panel: e0213a367`. The evidence is in [../TODO.md](../TODO.md) under
"Observed" and in the comment at the top of `boards/boards.yaml`:

- With the panel's reset line held low, its BUSY pin reads high on this unit.
  That rules out the V1.1 JD79656 panel, which reads BUSY low in reset.
- A "full refresh" returned in 261 ms with `panel: fc1` but took 3362 ms with
  `panel: e0213a367`, the duration a real refresh takes.
- With `e0213a367`, Alec saw the test card upright in landscape, readable, with
  a border visible on all four edges.

If your board really is a V1.1 with the JD79656 panel, try `panel: fc1`. If the
picture is wrong, change that one value, rebuild and look. Only the
`e0213a367` setting on the one unit above has been observed; the other values
have not been tried on a screen.

## Changing the board row

Edit `boards/boards.yaml`, then rebuild: `.venv/bin/pio run -e wireless-paper`.
The generator turns the row into `BOARD_*` defines (pins, `BOARD_EPD_PANEL_*`,
power pin) and rejects an unknown `driver` or `panel`. The only driver is
`heltec`. `custom_board` and `custom_config` in `platformio_override.ini` pick a
different row or config for an env.

## Other boards

Not supported. Another board needs a new row, a driver in `lib/board`, an env,
and for a new chip an entry in `scripts/firmware_images.py`. That is outside
this project's scope; see `AGENTS.md` before starting.

## What is verified

| Item | Status |
|---|---|
| Chip and flash identity | Read with esptool, 2026-11-23 |
| Panel test card with `e0213a367` | Upright, readable, border on all edges (observed by Alec) |
| WiFi join, the six pages, the web app, the button | Pending; see [../TODO.md](../TODO.md) |
| `native` env | Host unit tests only; no hardware |

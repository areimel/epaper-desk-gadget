# Physical checks

Hardware behaviour is pending until someone observes it. Mark an item done only
with a date, the firmware build and who observed it. A successful compile proves
none of this. Before any flashing: confirm the serial device is the CP2102
bridge (`10c4:ea60`), read the chip and flash size with esptool, and close serial
monitors. Port names are examples.

## Observed

- 2026-11-23, Heltec Wireless Paper on `/dev/ttyUSB0` (CP2102, serial `0001`),
  read by Claude with esptool: ESP32-S3 (QFN56) rev v0.2, 8 MB embedded flash,
  no PSRAM reported.
- 2026-11-23, same board, build `77f1685` plus the canvas and panel driver,
  observed by Claude over serial: with the panel's reset held low its BUSY pin
  reads high (idle low), which rules out the V1.1 JD79656 panel. With
  `panel: fc1` a "full refresh" returned in 261 ms; with `panel: e0213a367` it
  took 3362 ms.
- 2026-11-23, same board and build with `panel: e0213a367`, observed by Alec:
  the test card is upright in landscape, text readable, and both border lines
  are visible on all four edges (no column offset needed).
- 2026-11-23, same board: the boot log shows `wifi: reconnecting` with
  credentials left in NVS by earlier firmware. Whether it joins a network is
  not yet observed.
- 2026-11-24, same board, build `e6c1e60`, observed by Claude over serial and
  HTTP: the stale NVS credentials kept it at `wifi: reconnecting`. Improv
  provisioning from `scripts/dev/provision_wifi.py` (credentials from
  `secrets.yaml`) gave `wifi: connected, ip 192.168.1.170` and
  `improv: connected, credentials saved`. NTP synced (`timeSynced: true`) and
  the HN source fetched over HTTPS. Opening the port resets the board
  (`rst:0x1 POWERON`); the script now waits for the boot Improv frame first.

## Pending

- [ ] Fast refresh: press the button on the test card; the new bar appears
      without a full flash, and the sixth press in a row does a full refresh.
- [ ] Ghosting after several fast refreshes is acceptable.
- [ ] Button click and hold are detected reliably (GPIO0).
- [ ] WiFi provisioning over Improv from the installer page.
- [ ] Each dashboard page renders legibly at desk distance.
- [x] The web app is reachable at the device's IP and at `desk-gadget.local`
      (2026-11-24, build `e6c1e60`, Claude: `GET /` 200 by IP, `/api/state`
      by mDNS name).
- [ ] An edit saved in the web app appears on the panel.
- [ ] WiFi loss shows stale data and recovers without a restart.
- [ ] One hour of running with no decline in free heap.
- [ ] OTA update with a password from `secrets.yaml`.

#!/usr/bin/env python3
"""Send WiFi credentials to the device over USB with Improv Serial.

Reads wifi_ssid and wifi_password from the gitignored secrets.yaml (never from the command line,
so they stay out of shell history) and prints only the outcome. Use it when the device holds
out-of-date credentials: stored ones win over the build-time dev credentials.

    .venv/bin/python scripts/dev/provision_wifi.py /dev/ttyUSB0

Needs pyserial and PyYAML (both in the project venv). Close any serial monitor first.
"""
import sys
import time
from pathlib import Path

import serial
import yaml

HEADER = b"IMPROV\x01"
RPC, RPC_RESULT, STATE, ERROR = 3, 4, 1, 2
ERRORS = {1: "invalid request", 2: "unknown request", 3: "could not connect (check the name and password)"}


def frame(kind, data):
    body = HEADER + bytes([kind, len(data)]) + data
    return body + bytes([sum(body) & 0xFF])


def wifi_rpc(ssid, password):
    s, p = ssid.encode(), password.encode()
    payload = bytes([len(s)]) + s + bytes([len(p)]) + p
    return frame(RPC, bytes([1, len(payload)]) + payload)


def frames(buffer):
    """Yield (kind, data) for each complete, valid frame in buffer."""
    at = buffer.find(HEADER)
    while at >= 0 and len(buffer) >= at + 10:
        kind, size = buffer[at + 7], buffer[at + 8]
        end = at + 9 + size
        if len(buffer) > end and sum(buffer[at:end]) & 0xFF == buffer[end]:
            yield kind, buffer[at + 9:end]
        at = buffer.find(HEADER, at + 1)


def strings(data):
    out, at = [], 2  # skip command and total length
    while at < len(data):
        out.append(data[at + 1:at + 1 + data[at]].decode("utf-8", "replace"))
        at += 1 + data[at]
    return out


def wait_for_boot(link, limit=8):
    """Wait for the Improv state frame the firmware sends at boot, or `limit` seconds.

    Opening the port asserts DTR/RTS on Linux, which resets the board through the CP2102, so a
    frame written straight away lands in the ROM bootloader and is lost. A device that did not
    reset sends no boot frame; then this simply waits out the limit.
    """
    seen, deadline = bytearray(), time.time() + limit
    while time.time() < deadline:
        seen += link.read(256)
        if any(kind == STATE for kind, _ in frames(bytes(seen))):
            time.sleep(0.5)  # let setup() finish before the RPC arrives
            break
    link.reset_input_buffer()


def main(port):
    secrets = yaml.safe_load((Path(__file__).resolve().parents[2] / "secrets.yaml").read_text()) or {}
    ssid, password = secrets.get("wifi_ssid"), secrets.get("wifi_password", "")
    if not ssid:
        sys.exit("secrets.yaml has no wifi_ssid")
    with serial.Serial(port, 115200, timeout=0.2) as link:
        link.dtr = link.rts = False  # release reset; on Linux opening the port has already pulsed it
        wait_for_boot(link)
        link.write(wifi_rpc(ssid, password))
        seen, deadline = bytearray(), time.time() + 25
        while time.time() < deadline:
            seen += link.read(256)
            for kind, data in frames(bytes(seen)):
                if kind == ERROR and data and data[0]:
                    sys.exit(f"device refused: {ERRORS.get(data[0], data[0])}")
                if kind == RPC_RESULT or (kind == STATE and data and data[0] == 4):
                    urls = strings(data) if kind == RPC_RESULT else []
                    print("provisioned" + (f": {urls[0]}" if urls else ""))
                    return
    sys.exit("no answer from the device within 25 s")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__))

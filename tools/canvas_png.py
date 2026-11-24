#!/usr/bin/env python3
"""Convert a canvas dump to a PNG scaled up (default 4x) so people and agents can look at it.

Input is either a binary PBM (P4, as written by test/support/CanvasDump.h) or a raw
3904-byte /api/screen dump (250x122, 32 bytes per row, MSB first, 1 = black).
Pure standard library: no Pillow, no ImageMagick.

  python3 tools/canvas_png.py in.pbm out.png [--scale 4]
  curl http://desk-gadget.local/api/screen > s.bin; python3 tools/canvas_png.py s.bin s.png
"""
import argparse
import struct
import sys
import zlib

W, H, ROW_BYTES = 250, 122, 32


def load(data):
    """Return (width, height, rows) where rows is a list of lists of 0/1 (1 = black)."""
    if data[:2] == b"P4":
        # Header: "P4", width, height separated by whitespace, one whitespace byte, then bits.
        parts, pos = [], 2
        while len(parts) < 2:
            while data[pos:pos + 1].isspace():
                pos += 1
            start = pos
            while not data[pos:pos + 1].isspace():
                pos += 1
            parts.append(int(data[start:pos]))
        pos += 1
        width, height = parts
        stride = (width + 7) // 8
        bits = data[pos:]
    elif len(data) == ROW_BYTES * H:
        width, height, stride, bits = W, H, ROW_BYTES, data
    else:
        raise ValueError("not a P4 PBM and not a 3904-byte raw screen dump (got %d bytes)" % len(data))
    if len(bits) < stride * height:
        raise ValueError("truncated bitmap")
    rows = []
    for y in range(height):
        row = bits[y * stride:(y + 1) * stride]
        rows.append([(row[x >> 3] >> (7 - (x & 7))) & 1 for x in range(width)])
    return width, height, rows


def chunk(tag, body):
    crc = zlib.crc32(tag + body) & 0xFFFFFFFF
    return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", crc)


def write_png(path, width, height, rows, scale):
    raw = bytearray()
    for row in rows:
        line = bytearray()
        for v in row:
            line += (b"\x00" if v else b"\xff") * scale  # 8-bit grey: black ink, white paper
        for _ in range(scale):
            raw += b"\x00" + line  # filter type 0
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width * scale, height * scale, 8, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--scale", type=int, default=4)
    args = ap.parse_args()
    with open(args.input, "rb") as f:
        width, height, rows = load(f.read())
    write_png(args.output, width, height, rows, args.scale)
    return 0


if __name__ == "__main__":
    sys.exit(main())

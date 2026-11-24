#!/usr/bin/env python3
"""Generate the weather icons (lib/epaper/src/IconsData.cpp) from original geometry.

Requirements: Python 3 standard library only (no Pillow, no Inkscape).
Output is deterministic: running it twice gives identical files.

  python3 scripts/dev/make_icons.py            # rewrite lib/epaper/src/IconsData.cpp
  python3 scripts/dev/make_icons.py --check    # exit 1 if the committed file is stale
  python3 scripts/dev/make_icons.py --preview sheet.png   # contact sheet to look at

Each icon is described once on a 48-unit grid with signed-distance shapes, rasterised at
4x4 supersampling per pixel and thresholded at 50 % coverage (no anti-aliasing is kept: the
panel has two colours). At 24 px every stroke is widened to at least 2 px. Where two shapes
overlap (sun behind cloud, bolt over cloud) a white halo is cut around the front shape so
the two stay readable. See assets/icons/README.md for the art's provenance and licence.
"""
import argparse
import math
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "lib", "epaper", "src", "IconsData.cpp")
SS = 4  # subsamples per pixel axis
ICONS = ["ClearDay", "ClearNight", "PartlyCloudyDay", "PartlyCloudyNight", "Cloudy", "Fog",
         "Drizzle", "Rain", "Snow", "Thunder", "Unknown"]

# ---- signed-distance shapes (negative inside), in 48-unit design coordinates ----


def circle(cx, cy, r):
    return lambda x, y: math.hypot(x - cx, y - cy) - r


def capsule(x1, y1, x2, y2, hw):
    dx, dy = x2 - x1, y2 - y1
    ll = dx * dx + dy * dy

    def f(x, y):
        t = 0.0 if ll == 0 else max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / ll))
        return math.hypot(x - (x1 + t * dx), y - (y1 + t * dy)) - hw
    return f


def rrect(cx, cy, hw, hh, r):
    def f(x, y):
        qx, qy = abs(x - cx) - (hw - r), abs(y - cy) - (hh - r)
        return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - r
    return f


def polygon(pts):
    def f(x, y):
        inside, best = False, 1e9
        n = len(pts)
        for i in range(n):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
            if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
                inside = not inside
            best = min(best, capsule(ax, ay, bx, by, 0)(x, y))
        return -best if inside else best
    return f


def union(*shapes):
    return lambda x, y: min(s(x, y) for s in shapes)


def grow(shape, d):
    return lambda x, y: shape(x, y) - d


def minus(a, b):
    return lambda x, y: max(a(x, y), -b(x, y))


# ---- building blocks; each takes the output size so strokes can be widened at 24 px ----


class Ctx:
    def __init__(self, n):
        self.n = n
        self.px = 48.0 / n  # design units per output pixel

    def w(self, hw):
        """Half stroke width in units, never thinner than a 2 px stroke (1 px half width)."""
        return max(hw, self.px)


def cloud(ox, oy, k=1.0):
    """A 40x26 unit cloud scaled by k with its top-left corner at (ox, oy)."""
    parts = [rrect(20, 18, 20, 8, 8), circle(11, 15, 8), circle(22, 11, 11), circle(31, 16, 7)]

    def f(x, y):
        return k * union(*parts)((x - ox) / k, (y - oy) / k)
    return f


def sun(c, cx, cy, r, r1, r2, hw):
    shapes = [circle(cx, cy, r)]
    for i in range(8):
        a = i * math.pi / 4
        shapes.append(capsule(cx + r1 * math.cos(a), cy + r1 * math.sin(a),
                              cx + r2 * math.cos(a), cy + r2 * math.sin(a), c.w(hw)))
    return union(*shapes)


def moon(cx, cy, r, cutx, cuty, cutr):
    return minus(circle(cx, cy, r), circle(cutx, cuty, cutr))


def star(c, cx, cy, r):
    return union(capsule(cx - r, cy, cx + r, cy, c.w(0.9)), capsule(cx, cy - r, cx, cy + r, c.w(0.9)))


def drops(c, pts, length, hw):
    # Slanted dashes, top-left to bottom-right is wrong for rain: they lean like "/".
    return union(*[capsule(x + length * 0.35, y, x - length * 0.35, y + length, c.w(hw)) for x, y in pts])


def flake(c, cx, cy, r):
    arms = 3 if c.n >= 48 else 2
    shapes = []
    for i in range(arms):
        a = (math.pi / 2 if arms == 2 else math.pi / 2) + i * math.pi / arms
        shapes.append(capsule(cx - r * math.cos(a), cy - r * math.sin(a),
                              cx + r * math.cos(a), cy + r * math.sin(a), c.w(0.9)))
    if arms == 2:  # the plus sign: vertical and horizontal
        shapes[1] = capsule(cx - r, cy, cx + r, cy, c.w(0.9))
    return union(*shapes)


# ---- the icons: a list of (operation, shape); "add" paints black, "cut" paints white ----


def build(name, c):
    cl = cloud(4, 1)
    low = cloud(2, 20)
    side = cloud(8, 22, 0.92)  # smaller cloud tucked under a sun or moon
    ops = []
    add = lambda s: ops.append(("add", s))  # noqa: E731
    cut = lambda s: ops.append(("cut", s))  # noqa: E731
    if name == "ClearDay":
        add(sun(c, 24, 24, 10, 15, 21.5, 1.75))
    elif name == "ClearNight":
        add(moon(24, 26, 20, 35, 15, 25))
        add(star(c, 38, 9, 4.5))
        add(star(c, 43, 24, 3))
    elif name == "PartlyCloudyDay":
        add(sun(c, 16, 16, 7.5, 11.5, 15.5, 1.5))
        cut(grow(side, 2.5))
        add(side)
    elif name == "PartlyCloudyNight":
        add(moon(17, 16, 15, 26, 23, 14))
        cut(grow(side, 2.5))
        add(side)
    elif name == "Cloudy":
        add(cloud(15, 2, 0.82))
        cut(grow(low, 2.5))
        add(low)
    elif name == "Fog":
        add(cloud(4, 0, 0.85))
        for y, x1, x2 in ((31, 6, 38), (38, 12, 43), (45, 6, 34)):
            add(capsule(x1, y, x2, y, c.w(1.9)))
    elif name in ("Drizzle", "Rain"):
        add(cl)
        if name == "Rain":
            pts = [(15, 33), (26, 33), (37, 33), (20, 41), (31, 41)] if c.n >= 48 else [(15, 33), (26, 33), (37, 33)]
            add(drops(c, pts, 7 if c.n >= 48 else 9, 1.8))
        else:
            add(drops(c, [(14, 36), (25, 39), (36, 36)], 5, 1.8))
    elif name == "Snow":
        add(cl)
        if c.n >= 48:
            for x, y in ((13, 36), (25, 40), (37, 36)):
                add(flake(c, x, y, 5))
        else:  # flakes would clog at 12 units per pixel pair: round dots read better
            for x, y in ((12, 37), (24, 42), (36, 37)):
                add(circle(x, y, 3.9))
    elif name == "Thunder":
        add(cloud(4, 0))
        bolt = grow(polygon([(31, 22), (17, 37), (25, 37), (20, 47.5), (39, 28), (30, 28), (36, 22)]),
                    0 if c.n >= 48 else 0.9)  # fatter bolt at 24 px
        cut(grow(bolt, 2))
        add(bolt)
    elif name == "Unknown":
        add(minus(circle(24, 23, 21), circle(24, 23, 21 - c.w(2.2) * 2)))
        pts = [(17.5, 18.5), (19, 13.5), (24, 11), (29, 13.5), (30.5, 18.5), (27, 23), (24, 25.5), (24, 28.5)]
        for a, b in zip(pts, pts[1:]):
            add(capsule(a[0], a[1], b[0], b[1], c.w(1.9)))
        add(circle(24, 34.5, c.w(2.6)))
    return ops


def render(name, n):
    c = Ctx(n)
    ops = build(name, c)
    grid = [[False] * (n * SS) for _ in range(n * SS)]
    step = 48.0 / (n * SS)
    for op, shape in ops:
        val = op == "add"
        for j in range(n * SS):
            y = (j + 0.5) * step
            row = grid[j]
            for i in range(n * SS):
                if shape((i + 0.5) * step, y) <= 0:
                    row[i] = val
    rows = []
    for py in range(n):
        rows.append([sum(grid[py * SS + j][px * SS + i] for j in range(SS) for i in range(SS))
                     * 2 >= SS * SS for px in range(n)])
    return rows


def xbm(rows):
    out = []
    for row in rows:
        for b in range(0, len(row), 8):
            out.append(sum(1 << k for k in range(8) if b + k < len(row) and row[b + k]))
    return out


def source():
    lines = [
        "// GENERATED by scripts/dev/make_icons.py - do not edit; change the script and re-run it.",
        "// Pure data: 1-bit XBM arrays (row-major, LSB first, rows padded to whole bytes), 24x24 and 48x48.",
        '#include "Icons.h"',
        "",
        "namespace {",
    ]
    for name in ICONS:
        for n in (24, 48):
            data = xbm(render(name, n))
            lines.append("const uint8_t k%s%d[] = {" % (name, n))
            for i in range(0, len(data), 12):
                lines.append("    " + ", ".join("0x%02x" % v for v in data[i:i + 12]) + ",")
            lines.append("};")
    lines += ["", "// Indexed by Icon; columns are S24, S48.", "const uint8_t* const kTable[%d][2] = {" % len(ICONS)]
    lines += ["    {k%s24, k%s48}," % (n, n) for n in ICONS]
    lines += ["};", "}  // namespace", "", "namespace icons {",
              "const uint8_t* bits(Icon icon, IconSize size) {",
              "  uint8_t i = static_cast<uint8_t>(icon);",
              "  if (i >= ICON_COUNT) i = static_cast<uint8_t>(Icon::Unknown);",
              "  return kTable[i][size == IconSize::S24 ? 0 : 1];",
              "}", "}  // namespace icons", ""]
    return "\n".join(lines)


def preview(path):
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import canvas_png
    gap = 6
    width = len(ICONS) * (48 + gap) + gap
    sheet = [[0] * width for _ in range(48 + 24 + gap * 3)]
    for k, name in enumerate(ICONS):
        for n, oy in ((48, gap), (24, 48 + 2 * gap)):
            for y, row in enumerate(render(name, n)):
                for x, v in enumerate(row):
                    sheet[oy + y][gap + k * (48 + gap) + x] = 1 if v else 0
    canvas_png.write_png(path, width, len(sheet), sheet, 4)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--preview", metavar="PNG")
    args = ap.parse_args()
    if args.preview:
        preview(args.preview)
        return 0
    text = source()
    if args.check:
        with open(OUT) as f:
            return 0 if f.read() == text else 1
    with open(OUT, "w") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())

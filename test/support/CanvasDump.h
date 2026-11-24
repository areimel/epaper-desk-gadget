#pragma once
// Host-only helpers for native tests and tools: write a Canvas as a PBM (P4) file and measure ink.
// View a PBM with tools/canvas_png.py. Header-only; not part of the firmware.
#include <stdio.h>

#include "Canvas.h"

namespace dump {

// Writes the canvas as a binary PBM (1 = black). Returns false if the file cannot be opened.
inline bool writePbm(const Canvas& canvas, const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  fprintf(f, "P4\n%d %d\n", Canvas::W, Canvas::H);
  fwrite(canvas.rows(), 1, Canvas::PACKED_BYTES, f);  // same row-major MSB-first layout as PBM
  fclose(f);
  return true;
}

// Black pixels inside the rectangle [x0,x1) x [y0,y1) and their bounding box (min > max if none).
struct Ink {
  int count = 0;
  int minX = 1 << 20, minY = 1 << 20, maxX = -1, maxY = -1;
};

inline Ink ink(const Canvas& canvas, int x0, int y0, int x1, int y1) {
  Ink r;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      if (canvas.pixel(x, y)) {
        r.count++;
        if (x < r.minX) r.minX = x;
        if (x > r.maxX) r.maxX = x;
        if (y < r.minY) r.minY = y;
        if (y > r.maxY) r.maxY = y;
      }
  return r;
}

inline Ink inkAll(const Canvas& canvas) { return ink(canvas, 0, 0, Canvas::W, Canvas::H); }

}  // namespace dump

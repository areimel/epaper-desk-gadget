#pragma once
// Host-only harness for the page suites: renders a page twice, checks the invariants every
// screen shares, writes a PBM to look at and compares the packed bytes with a golden file.
// UPDATE_GOLDEN=1 rewrites the goldens instead of comparing. PAGES_DIR overrides .pio/pages.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unity.h>

#include "CanvasDump.h"

namespace harness {

inline const char* outDir() {
  const char* d = getenv("PAGES_DIR");
  return d ? d : ".pio/pages";
}

inline void checkInvariants(const char* name, const Canvas& c) {
  char msg[160];
  for (int y = 0; y < 14; y++) {
    bool bar = c.pixel(0, y) && c.pixel(1, y) && c.pixel(Canvas::W - 1, y);
    snprintf(msg, sizeof msg, "%s: title bar broken on row %d", name, y);
    TEST_ASSERT_TRUE_MESSAGE(bar, msg);
  }
  for (int y = 0; y < Canvas::H; y++) {
    snprintf(msg, sizeof msg, "%s: padding bits set on row %d", name, y);
    TEST_ASSERT_EQUAL_MESSAGE(0, c.rows()[y * Canvas::ROW_BYTES + 31] & 0x3F, msg);
  }
}

inline void checkGolden(const char* name, const uint8_t* packed) {
  char path[200], msg[200];
  snprintf(path, sizeof path, "test/fixtures/golden/%s.bin", name);
  if (getenv("UPDATE_GOLDEN")) {
    mkdir("test/fixtures/golden", 0755);
    FILE* f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    fwrite(packed, 1, Canvas::PACKED_BYTES, f);
    fclose(f);
    return;
  }
  static uint8_t want[Canvas::PACKED_BYTES + 1];
  FILE* f = fopen(path, "rb");
  snprintf(msg, sizeof msg, "missing golden %s (run with UPDATE_GOLDEN=1)", path);
  TEST_ASSERT_NOT_NULL_MESSAGE(f, msg);
  size_t n = fread(want, 1, sizeof want, f);
  fclose(f);
  snprintf(msg, sizeof msg, "golden %s has %u bytes", path, static_cast<unsigned>(n));
  TEST_ASSERT_EQUAL_MESSAGE(Canvas::PACKED_BYTES, n, msg);
  for (size_t i = 0; i < Canvas::PACKED_BYTES; i++) {
    if (packed[i] == want[i]) continue;
    int y = static_cast<int>(i / Canvas::ROW_BYTES);
    int x = static_cast<int>(i % Canvas::ROW_BYTES) * 8;
    uint8_t d = packed[i] ^ want[i];
    while (!(d & 0x80)) { d <<= 1; x++; }
    snprintf(msg, sizeof msg, "%s differs from golden, first at x=%d y=%d", name, x, y);
    TEST_FAIL_MESSAGE(msg);
  }
}

// render(canvas) must clear and draw the whole page. It runs twice on the same canvas to prove
// the output is deterministic and independent of what was there before.
template <class F>
void check(const char* name, F render) {
  static Canvas canvas;
  static uint8_t first[Canvas::PACKED_BYTES], second[Canvas::PACKED_BYTES];
  render(canvas);
  canvas.packRows(first);
  render(canvas);
  canvas.packRows(second);
  char msg[120];
  snprintf(msg, sizeof msg, "%s: second render differs", name);
  TEST_ASSERT_EQUAL_MEMORY_MESSAGE(first, second, Canvas::PACKED_BYTES, msg);
  checkInvariants(name, canvas);
  mkdir(".pio", 0755);
  mkdir(outDir(), 0755);
  snprintf(msg, sizeof msg, "%s/%s.pbm", outDir(), name);
  TEST_ASSERT_TRUE_MESSAGE(dump::writePbm(canvas, msg), msg);
  checkGolden(name, first);
}

// The packed bytes of a render, for "these two differ" assertions.
template <class F>
void pack(F render, uint8_t* out) {
  static Canvas canvas;
  render(canvas);
  canvas.packRows(out);
}

}  // namespace harness

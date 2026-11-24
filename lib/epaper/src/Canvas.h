#pragma once
#include <clib/u8g2.h>
#include <stddef.h>
#include <stdint.h>

// 1-bit framebuffer for the 250x122 panel, drawn with the U8g2 C core (fonts, lines, XBM).
// Pure: no Arduino or hardware includes, so views and tests run on the host too.
// Storage is row-major, 32 bytes per row, most significant bit first, 1 = black. The first
// PACKED_BYTES of it are exactly the /api/screen format.
class Canvas {
 public:
  static constexpr int W = 250;
  static constexpr int H = 122;
  static constexpr int ROW_BYTES = 32;
  static constexpr size_t PACKED_BYTES = ROW_BYTES * H;  // 3904

  Canvas();
  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  u8g2_t* g() { return &u8g2_; }  // draw with u8g2_* calls; never call u8g2_SendBuffer
  void clear();                   // all white, draw colour black
  bool pixel(int x, int y) const;  // true = black; false outside the canvas
  const uint8_t* rows() const { return buffer_; }
  size_t packRows(uint8_t* out) const;  // copies PACKED_BYTES, returns that count

 private:
  static constexpr int TILE_ROWS = 16;  // 128 px tall buffer; rows 122..127 are never shown
  u8g2_t u8g2_;
  uint8_t buffer_[ROW_BYTES * TILE_ROWS * 8];
};

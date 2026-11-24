#pragma once
#include <clib/u8g2.h>

#include "Measure.h"

// Measure backed by the real U8g2 font data. Owns a private u8g2_t, so measuring never changes
// the font selected on any Canvas. Widths match u8g2_GetStrWidth (origin to last ink column).
class U8g2Measure : public Measure {
 public:
  U8g2Measure();
  int width(Font font, const char* text, size_t n) const override;

 private:
  mutable u8g2_t u8g2_;
};

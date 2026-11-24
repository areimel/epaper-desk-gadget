#pragma once
#include <stddef.h>

#include "Font.h"

// How wide a run of Latin-1 text is in a font. lib/epaper implements it with U8g2; tests use a
// fixed-width fake. Wrapping and fitting depend only on this.
struct Measure {
  virtual ~Measure() = default;
  virtual int width(Font font, const char* text, size_t n) const = 0;  // pixels
};

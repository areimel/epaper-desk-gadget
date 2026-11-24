#include "U8g2Measure.h"

#include <string.h>

#include "Fonts.h"

U8g2Measure::U8g2Measure() { memset(&u8g2_, 0, sizeof(u8g2_)); }

// Same arithmetic as u8g2_GetStrWidth (sum of advances, last glyph trimmed to its ink), but
// over a length-delimited run so no copy is needed. Only the font is used; no display is set up.
int U8g2Measure::width(Font font, const char* text, size_t n) const {
  if (!text || n == 0) return 0;
  u8g2_SetFont(&u8g2_, fonts::data(font));
  u8g2_.font_decode.glyph_width = 0;
  int w = 0, dx = 0;
  for (size_t i = 0; i < n; i++) {
    dx = u8g2_GetGlyphWidth(&u8g2_, static_cast<uint8_t>(text[i]));
    w += dx;
  }
  if (u8g2_.font_decode.glyph_width != 0) {
    w -= dx;
    w += u8g2_.font_decode.glyph_width;
    w += u8g2_.glyph_x_offset;
  }
  return w;
}

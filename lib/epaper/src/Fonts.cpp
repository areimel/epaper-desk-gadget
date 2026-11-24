#include "Fonts.h"

#include <clib/u8g2.h>

namespace {
struct Face {
  const uint8_t* data;
  int8_t ascent;
  int8_t lineHeight;
};
// Indexed by Font. Ascent is the capital-letter height; line height adds 4-5 px of leading.
const Face kFaces[FONT_COUNT] = {
    {u8g2_font_helvR08_tf, 8, 11},      // Small
    {u8g2_font_helvB08_tf, 8, 11},      // SmallBold
    {u8g2_font_helvR10_tf, 11, 15},     // Body
    {u8g2_font_helvB10_tf, 11, 15},     // BodyBold
    {u8g2_font_ncenB14_tf, 14, 19},     // Quote14
    {u8g2_font_ncenB18_tf, 18, 24},     // Quote18
    {u8g2_font_logisoso32_tn, 32, 38},  // Num32
    {u8g2_font_logisoso42_tn, 42, 48},  // NumBig
};
}  // namespace

namespace fonts {
const uint8_t* data(Font font) { return kFaces[static_cast<uint8_t>(font)].data; }
int ascent(Font font) { return kFaces[static_cast<uint8_t>(font)].ascent; }
int lineHeight(Font font) { return kFaces[static_cast<uint8_t>(font)].lineHeight; }
}  // namespace fonts

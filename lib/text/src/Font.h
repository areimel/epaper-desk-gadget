#pragma once
#include <stdint.h>

// The fixed set of typefaces the dashboard uses. Pure names only: lib/epaper maps each to
// U8g2 font data, lib/text measures through Measure.h without knowing what a font is.
enum class Font : uint8_t {
  Small,      // helvR08: metadata
  SmallBold,  // helvB08: title bar
  Body,       // helvR10
  BodyBold,   // helvB10: headlines
  Quote14,    // ncenB14
  Quote18,    // ncenB18
  Num32,      // logisoso32, digits and punctuation only
  NumBig,     // logisoso42, digits and punctuation only
};
constexpr uint8_t FONT_COUNT = 8;

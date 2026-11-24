#pragma once
#include <stdint.h>

#include "Font.h"

// U8g2 font data for each Font. Text is Latin-1 (codes 32-255); the Num fonts hold only
// digits and the punctuation a clock or temperature needs.
namespace fonts {
const uint8_t* data(Font font);
int ascent(Font font);      // pixels above the baseline of a capital letter
int lineHeight(Font font);  // baseline-to-baseline distance for running text
}  // namespace fonts

#pragma once
// Shared by Draw.cpp and DrawText.cpp only.
#include <stddef.h>

#include "Canvas.h"
#include "Font.h"
#include "U8g2Measure.h"

namespace draw {
namespace detail {

constexpr size_t RUN_MAX = 120;  // longest run copied for drawing (a line never needs more)

// The shared measurer for wrapping and alignment.
const U8g2Measure& measure();

// A run of Latin-1 text of n characters, optionally followed by "...".
int runWidth(Font font, const char* s, size_t n, bool dots);
void drawRun(Canvas& canvas, Font font, int x, int baselineY, const char* s, size_t n, bool dots);

// Longest prefix length of s (NUL-terminated) that fits maxW with "..." appended, or the full
// length with *dots false when the whole string fits.
size_t clipToWidth(Font font, const char* s, int maxW, bool* dots);

// Descender allowance below the baseline, derived from the font metrics.
int descent(Font font);
// How many lines of font fit in a box of height h.
int linesThatFit(Font font, int h);

}  // namespace detail
}  // namespace draw

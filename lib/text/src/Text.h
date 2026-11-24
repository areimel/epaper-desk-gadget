#pragma once
#include <stddef.h>
#include <time.h>

#include "Measure.h"

// Pure text helpers for the dashboard: no Arduino, no allocation.
namespace text {

// UTF-8 in, Latin-1 (ISO 8859-1) out, always NUL-terminated, never more than cap-1 bytes
// written. Returns the number of bytes written (excluding the NUL).
size_t fold(const char* utf8, char* out, size_t cap);

// A line of wrapped text: points into the original string (no copying).
struct Line {
  const char* start;
  size_t len;
  bool ellipsis;  // true: draw "..." after the len characters
};

// Greedy word wrap of Latin-1 text into at most maxLines lines no wider than maxWidth pixels.
// Returns the number of lines produced (0 for empty or all-space text).
int wrap(const Measure& measure, Font font, const char* s, int maxWidth, int maxLines, Line* out);

// Human age of a timestamp. Writes NUL-terminated text into out (cap >= 16).
void age(time_t now, time_t then, char* out, size_t cap);

}  // namespace text

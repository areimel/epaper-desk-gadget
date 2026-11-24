#include "Text.h"

namespace text {
namespace {

const char* skipSpaces(const char* p) {
  while (*p == ' ') p++;
  return p;
}

const char* wordEnd(const char* p) {
  while (*p && *p != ' ') p++;
  return p;
}

// End of the longest line starting at start that fits: whole words while they fit, else (a
// first word wider than the line) as many characters as fit, and always at least one.
const char* lineEnd(const Measure& m, Font f, const char* start, int maxWidth) {
  const char* fit = nullptr;
  const char* w = wordEnd(start);
  while (m.width(f, start, static_cast<size_t>(w - start)) <= maxWidth) {
    fit = w;
    const char* next = skipSpaces(w);
    if (!*next) break;
    w = wordEnd(next);
  }
  if (fit) return fit;
  size_t k = 1;  // the word is too wide: split it by character
  while (start[k] && start[k] != ' ' && m.width(f, start, k + 1) <= maxWidth) k++;
  return start + k;
}

}  // namespace

int wrap(const Measure& m, Font f, const char* s, int maxWidth, int maxLines, Line* out) {
  if (!s || maxWidth <= 0 || maxLines <= 0) return 0;
  int count = 0;
  const char* p = skipSpaces(s);
  while (*p && count < maxLines) {
    const char* end = lineEnd(m, f, p, maxWidth);
    size_t len = static_cast<size_t>(end - p);
    const char* rest = skipSpaces(end);
    bool ellipsis = *rest != '\0' && count == maxLines - 1;
    if (ellipsis) {
      // Shorten until "..." fits too; len may reach 0 if even the dots barely fit.
      int dots = m.width(f, "...", 3);
      while (len > 0 && m.width(f, p, len) + dots > maxWidth) len--;
      while (len > 0 && p[len - 1] == ' ') len--;
    }
    out[count++] = Line{p, len, ellipsis};
    p = rest;
  }
  return count;
}

}  // namespace text

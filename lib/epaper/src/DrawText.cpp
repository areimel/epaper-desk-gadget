#include <string.h>

#include "Draw.h"
#include "DrawInternal.h"
#include "Fonts.h"
#include "Text.h"

namespace draw {
namespace {
constexpr int MAX_LINES = 16;  // 122 px tall panel: no font gives more lines than this

// Wraps s into box; returns lines (0 when nothing fits or s is empty).
int wrapInto(Font font, Box box, const char* s, int maxLines, text::Line* lines) {
  if (!s || box.w <= 0) return 0;
  int cap = detail::linesThatFit(font, box.h);
  if (maxLines > cap) maxLines = cap;
  if (maxLines > MAX_LINES) maxLines = MAX_LINES;
  if (maxLines <= 0) return 0;
  return text::wrap(detail::measure(), font, s, box.w, maxLines, lines);
}
}  // namespace

int paragraph(Canvas& canvas, Font font, Box box, const char* s, int maxLines) {
  text::Line lines[MAX_LINES];
  int n = wrapInto(font, box, s, maxLines, lines);
  int lh = fonts::lineHeight(font);
  for (int i = 0; i < n; i++) {
    detail::drawRun(canvas, font, box.x, box.y + fonts::ascent(font) + i * lh, lines[i].start,
                    lines[i].len, lines[i].ellipsis);
  }
  return n;
}

Font fitted(Canvas& canvas, const Font* ladder, int n, Box box, const char* s) {
  if (!ladder || n <= 0) return Font::Body;
  text::Line lines[MAX_LINES];
  int count = 0;
  int pick = n - 1;
  for (int i = 0; i < n; i++) {
    count = wrapInto(ladder[i], box, s, MAX_LINES, lines);
    if (count > 0 && !lines[count - 1].ellipsis) {
      pick = i;
      break;
    }
    if (i == n - 1) pick = i;
  }
  Font font = ladder[pick];
  if (pick == n - 1) count = wrapInto(font, box, s, MAX_LINES, lines);  // last font, maybe cut
  if (count == 0) return font;
  int lh = fonts::lineHeight(font);
  int a = fonts::ascent(font);
  int blockH = a + detail::descent(font) + (count - 1) * lh;
  int baseline = box.y + (box.h - blockH) / 2 + a;
  for (int i = 0; i < count; i++) {
    int w = detail::runWidth(font, lines[i].start, lines[i].len, lines[i].ellipsis);
    detail::drawRun(canvas, font, box.x + (box.w - w) / 2, baseline + i * lh, lines[i].start,
                    lines[i].len, lines[i].ellipsis);
  }
  return font;
}

}  // namespace draw

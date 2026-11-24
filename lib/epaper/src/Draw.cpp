#include "Draw.h"

#include <string.h>

#include "DrawInternal.h"
#include "Fonts.h"
#include "U8g2Measure.h"

namespace draw {
namespace detail {
const U8g2Measure& measure() {
  static const U8g2Measure m;
  return m;
}

namespace {
// Copies n characters (clamped) plus optional dots into buf; returns the length.
size_t compose(char* buf, const char* s, size_t n, bool dots) {
  if (n > RUN_MAX) n = RUN_MAX;
  memcpy(buf, s, n);
  if (dots) {
    memcpy(buf + n, "...", 3);
    n += 3;
  }
  buf[n] = '\0';
  return n;
}
}  // namespace

int runWidth(Font font, const char* s, size_t n, bool dots) {
  char buf[RUN_MAX + 4];
  size_t len = compose(buf, s, n, dots);
  return measure().width(font, buf, len);
}

void drawRun(Canvas& canvas, Font font, int x, int baselineY, const char* s, size_t n, bool dots) {
  char buf[RUN_MAX + 4];
  compose(buf, s, n, dots);
  u8g2_SetFont(canvas.g(), fonts::data(font));
  u8g2_DrawStr(canvas.g(), x, baselineY, buf);
}

size_t clipToWidth(Font font, const char* s, int maxW, bool* dots) {
  size_t n = strlen(s);
  if (n <= RUN_MAX && runWidth(font, s, n, false) <= maxW) {
    *dots = false;
    return n;
  }
  *dots = true;
  if (n > RUN_MAX) n = RUN_MAX;
  while (n > 0 && runWidth(font, s, n, true) > maxW) n--;
  while (n > 0 && s[n - 1] == ' ') n--;
  return n;
}

int descent(Font font) { return fonts::lineHeight(font) - fonts::ascent(font) - 1; }

int linesThatFit(Font font, int h) {
  int used = fonts::ascent(font) + descent(font);
  if (h < used) return 0;
  return 1 + (h - used) / fonts::lineHeight(font);
}

}  // namespace detail

namespace {
constexpr int MARGIN = 4;
constexpr int NOTE_GAP = 8;  // minimum space between the title and the note

// Draws s clipped to maxW at x (align 0 left, 1 centre) with a dotted cut if needed.
void drawClipped(Canvas& canvas, Font font, int x, int baselineY, const char* s, int maxW,
                 bool centre) {
  bool dots;
  size_t n = detail::clipToWidth(font, s, maxW, &dots);
  if (n == 0) return;
  if (centre) x -= detail::runWidth(font, s, n, dots) / 2;
  detail::drawRun(canvas, font, x, baselineY, s, n, dots);
}
}  // namespace

int textWidth(Font font, const char* s) {
  if (!s) return 0;
  return detail::runWidth(font, s, strlen(s), false);
}

void text(Canvas& canvas, Font font, int x, int baselineY, const char* s) {
  if (!s || !*s) return;
  u8g2_SetFont(canvas.g(), fonts::data(font));
  u8g2_DrawStr(canvas.g(), x, baselineY, s);
}

void textRight(Canvas& canvas, Font font, int rightX, int baselineY, const char* s) {
  text(canvas, font, rightX - textWidth(font, s), baselineY, s);
}

void textCenter(Canvas& canvas, Font font, int centerX, int baselineY, const char* s) {
  text(canvas, font, centerX - textWidth(font, s) / 2, baselineY, s);
}

void titleBar(Canvas& canvas, const char* title, const char* right) {
  u8g2_t* g = canvas.g();
  u8g2_SetDrawColor(g, 1);
  u8g2_DrawBox(g, 0, 0, Canvas::W, TITLE_BAR_H);
  u8g2_SetDrawColor(g, 0);
  constexpr int baseline = 10;  // SmallBold caps span rows 3-10, descenders reach row 12
  int titleEnd = MARGIN;
  if (title && *title) {
    bool dots;
    size_t n = detail::clipToWidth(Font::SmallBold, title, Canvas::W - 2 * MARGIN, &dots);
    detail::drawRun(canvas, Font::SmallBold, MARGIN, baseline, title, n, dots);
    titleEnd = MARGIN + detail::runWidth(Font::SmallBold, title, n, dots);
  }
  if (right && *right) {
    int w = textWidth(Font::Small, right);
    int left = Canvas::W - MARGIN - w;
    if (left - titleEnd >= NOTE_GAP) text(canvas, Font::Small, left, baseline, right);
  }
  u8g2_SetDrawColor(g, 1);
}

void checkbox(Canvas& canvas, int x, int y, bool done) {
  u8g2_t* g = canvas.g();
  u8g2_DrawFrame(g, x, y, 11, 11);
  u8g2_DrawFrame(g, x + 1, y + 1, 9, 9);  // second ring makes the outline 2 px
  if (!done) return;
  // A tick in the 7x7 interior: short stroke down-right, long stroke up-right, 2 px tall.
  for (int dy = 0; dy < 2; dy++) {
    u8g2_DrawLine(g, x + 2, y + 4 + dy, x + 4, y + 6 + dy);
    u8g2_DrawLine(g, x + 4, y + 6 + dy, x + 8, y + 2 + dy);
  }
}

int degree(Canvas& canvas, Font font, int x, int baselineY) {
  int d = fonts::ascent(font) / 5;
  if (d < 6) d = 6;
  int top = baselineY - fonts::ascent(font);  // digits start this row
  u8g2_DrawFrame(canvas.g(), x, top, d, d);
  u8g2_DrawFrame(canvas.g(), x + 1, top + 1, d - 2, d - 2);
  return d;
}

void emptyState(Canvas& canvas, const char* line1, const char* line2) {
  constexpr int cx = Canvas::W / 2;
  constexpr int cy = TITLE_BAR_H + (Canvas::H - TITLE_BAR_H) / 2;  // middle of the free area
  constexpr int maxW = Canvas::W - 2 * MARGIN;
  bool two = line2 && *line2;
  int a = fonts::ascent(Font::BodyBold);
  int b = fonts::ascent(Font::Small);
  int gap = 7;
  int total = two ? a + gap + b : a;
  int base1 = cy - total / 2 + a;
  if (line1) drawClipped(canvas, Font::BodyBold, cx, base1, line1, maxW, true);
  if (two) drawClipped(canvas, Font::Small, cx, base1 + gap + b, line2, maxW, true);
}

void hairline(Canvas& canvas, int x, int y, int w) {
  if (w > 0) u8g2_DrawHLine(canvas.g(), x, y, w);
}

}  // namespace draw

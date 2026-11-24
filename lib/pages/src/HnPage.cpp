#include <stdio.h>

#include "PageCommon.h"
#include "Text.h"
#include "U8g2Measure.h"

namespace pages {
namespace {
// Three two-line headlines with a meta line each do not fit in the 108 px below the bar, so:
// headlines use a 14 px pitch (the font's own is 15), and the meta line sits right-aligned on
// the headline's last line when there is room, else on a row of its own. If the blocks still
// do not fit, the last stories give up their second headline line.
constexpr int PITCH = 14;
constexpr int CAP = 11;         // BodyBold cap height
constexpr int DESC = 3;         // BodyBold descender
constexpr int META_DROP = 12;   // last headline baseline to the meta baseline on its own row
constexpr int META_BELOW = 2;   // Small descender allowance under that baseline
constexpr int META_GAP = 10;    // minimum space between a headline's last line and inline meta
constexpr int PAD = 2;
constexpr int MIN_GAP = 3;      // between blocks

struct Block {
  int lines;
  bool inlineMeta;  // chosen: meta shares the last headline row
  bool canInline;   // the meta fits beside the last headline line
  int height() const {
    return CAP + (lines - 1) * PITCH + (inlineMeta ? DESC : META_DROP + META_BELOW);
  }
};

void metaText(const HnStory& s, char* out, size_t cap) {
  snprintf(out, cap, "%u point%s, %u comment%s", s.points, s.points == 1 ? "" : "s", s.comments,
           s.comments == 1 ? "" : "s");
}

// Lays out one story with at most maxLines headline lines.
Block layout(const HnStory& s, int maxLines) {
  static const U8g2Measure measure;
  text::Line ln[2];
  int n = text::wrap(measure, Font::BodyBold, s.title, BODY_W, maxLines, ln);
  if (n < 1) return Block{1, false, false};
  const text::Line& last = ln[n - 1];
  int lastW = measure.width(Font::BodyBold, last.start, last.len) + (last.ellipsis ? measure.width(Font::BodyBold, "...", 3) : 0);
  char meta[40];
  metaText(s, meta, sizeof meta);
  int metaW = draw::textWidth(Font::Small, meta);
  return Block{n, false, lastW + META_GAP + metaW <= BODY_W};
}

int total(const Block* b, int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) sum += b[i].height();
  return sum;
}
}  // namespace

void renderHn(const PageContext& ctx, Canvas& canvas) {
  const HnSnap& hn = ctx.hn;
  char note[32];
  dataNote(ctx.feed, hn.status, ctx.now, note, sizeof note);
  int per = limits::HN_PER_VISIT;
  int count = hn.count < limits::HN_STORIES ? hn.count : limits::HN_STORIES;
  int screens = (count + per - 1) / per;
  int screen = screens > 0 ? ctx.visit % screens : 0;
  char title[32] = "Hacker News";
  if (screens > 1) snprintf(title, sizeof title, "Hacker News %d/%d", screen + 1, screens);
  begin(canvas, title, note);
  if (fetchEmptyState(canvas, hn.status, "Headlines are loading", "Headlines didn't load")) return;
  if (count == 0) {
    draw::emptyState(canvas, "No stories right now", nullptr);
    return;
  }
  const HnStory* first = &hn.stories[screen * per];
  int n = count - screen * per < per ? count - screen * per : per;
  Block blocks[limits::HN_PER_VISIT];
  for (int i = 0; i < n; i++) blocks[i] = layout(first[i], 2);
  const int room = Canvas::H - BODY_TOP - 2 * PAD - (n - 1) * MIN_GAP;
  // Only when the plain layout (meta on its own row) overflows: share rows, then cut lines.
  for (int i = n - 1; i >= 0 && total(blocks, n) > room; i--) blocks[i].inlineMeta = blocks[i].canInline;
  for (int i = n - 1; i >= 0 && total(blocks, n) > room; i--) {
    if (blocks[i].lines > 1) blocks[i] = layout(first[i], 1);
    blocks[i].inlineMeta = blocks[i].canInline;
  }
  int slack = Canvas::H - BODY_TOP - 2 * PAD - total(blocks, n);
  int gap = n > 1 ? slack / (n - 1) : 0;
  if (gap > 8) gap = 8;
  int top = BODY_TOP + PAD;
  for (int i = 0; i < n; i++) {
    const Block& b = blocks[i];
    int base = top + CAP;
    lines(canvas, Font::BodyBold, MARGIN, base, BODY_W, PITCH, b.lines, first[i].title);
    char meta[40];
    metaText(first[i], meta, sizeof meta);
    int lastBase = base + (b.lines - 1) * PITCH;
    if (b.inlineMeta) {
      draw::textRight(canvas, Font::Small, Canvas::W - MARGIN, lastBase, meta);
    } else {
      draw::text(canvas, Font::Small, MARGIN, lastBase + META_DROP, meta);
    }
    top += b.height() + gap;
  }
}

}  // namespace pages

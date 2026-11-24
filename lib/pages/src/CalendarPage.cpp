#include <stdio.h>

#include "PageCommon.h"

namespace pages {
namespace {
constexpr int ROWS = 4;
constexpr int PITCH = 24;
constexpr int FIRST_BASELINE = 31;
constexpr int GAP = 8;  // between the time column and the title

struct Cell {
  char text[20];
  Font font;
};

Cell timeCell(const FeedConfig& feed, const CalEvent& e) {
  Cell c{{0}, Font::BodyBold};
  if (e.startMin >= 0) {
    formatMinutes(use12h(feed), e.startMin, c.text, sizeof c.text);
    return c;
  }
  c.font = Font::Small;  // "all day" is metadata, not a time
  if (e.endMin < 1440) {
    char t[16];
    formatMinutes(use12h(feed), e.endMin, t, sizeof t);
    snprintf(c.text, sizeof c.text, "until %s", t);
  } else {
    snprintf(c.text, sizeof c.text, "all day");
  }
  return c;
}

bool finished(const CalEvent& e, int minutesNow) { return e.startMin >= 0 && e.endMin <= minutesNow; }

// Which events to list. With more events than rows, finished ones are left out (the latest few
// stay if too little remains to fill the rows). Fills idx in order and returns how many.
int visibleEvents(const CalSnap& cal, int minutesNow, uint8_t* idx) {
  int count = cal.count < limits::CAL_EVENTS_MAX ? cal.count : limits::CAL_EVENTS_MAX;
  bool keep[limits::CAL_EVENTS_MAX];
  int live = 0;
  for (int i = 0; i < count; i++) {
    keep[i] = count <= ROWS || !finished(cal.events[i], minutesNow);
    live += keep[i];
  }
  for (int i = count - 1; i >= 0 && live < ROWS && live < count; i--) {
    if (!keep[i]) keep[i] = true, live++;
  }
  int n = 0;
  for (int i = 0; i < count; i++) {
    if (keep[i]) idx[n++] = static_cast<uint8_t>(i);
  }
  return n;
}
}  // namespace

void renderCalendar(const PageContext& ctx, Canvas& canvas) {
  const CalSnap& cal = ctx.cal;
  char note[32];
  dataNote(ctx.feed, cal.status, ctx.now, note, sizeof note);
  if (ctx.feed.calendarUrl[0] == '\0') {
    begin(canvas, "Today", nullptr);
    draw::emptyState(canvas, "No calendar yet", "Add a link in the web app");
    return;
  }
  tm lt;
  localTime(ctx.now, lt);
  uint8_t idx[limits::CAL_EVENTS_MAX];
  int total = visibleEvents(cal, lt.tm_hour * 60 + lt.tm_min, idx);
  int pageCount = (total + ROWS - 1) / ROWS;
  int page = pageCount > 0 ? ctx.visit % pageCount : 0;
  int first = page * ROWS;
  int shown = total - first < ROWS ? total - first : ROWS;
  char title[32] = "Today";
  if (pageCount > 1) snprintf(title, sizeof title, "Today %d/%d", page + 1, pageCount);
  begin(canvas, title, note);
  if (fetchEmptyState(canvas, cal.status, "Calendar is loading", "Calendar didn't load")) return;
  char more[40] = "";
  if (cal.skipped > 0) snprintf(more, sizeof more, "%u more not shown (see web app)", cal.skipped);
  if (shown == 0) {
    draw::emptyState(canvas, "Nothing on today", more);
    return;
  }
  Cell cells[ROWS];
  int colW = 0;
  for (int i = 0; i < shown; i++) {
    cells[i] = timeCell(ctx.feed, cal.events[idx[first + i]]);
    int w = draw::textWidth(cells[i].font, cells[i].text);
    if (w > colW) colW = w;
  }
  int titleX = MARGIN + colW + GAP;
  for (int i = 0; i < shown; i++) {
    int base = FIRST_BASELINE + i * PITCH;
    draw::textRight(canvas, cells[i].font, MARGIN + colW, base, cells[i].text);
    oneLine(canvas, Font::BodyBold, titleX, base, Canvas::W - MARGIN - titleX, cal.events[idx[first + i]].title);
  }
  if (more[0]) draw::text(canvas, Font::Small, MARGIN, Canvas::H - 5, more);
}

}  // namespace pages

#include "PageCommon.h"

#include <stdio.h>
#include <string.h>

#include "Text.h"
#include "U8g2Measure.h"

namespace pages {
namespace {
const char* const kDays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char* const kMonths[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};
constexpr int LINES_MAX = 6;
}  // namespace

bool use12h(const FeedConfig& feed) { return feed.weather.units == Units::Fahrenheit; }

void formatMinutes(bool h12, int minutes, char* out, size_t cap, const char** suffix) {
  int h = (minutes / 60) % 24, m = minutes % 60;
  if (!h12) {
    snprintf(out, cap, "%02d:%02d", h, m);
    if (suffix) *suffix = "";
    return;
  }
  const char* ampm = h < 12 ? "am" : "pm";
  int hour = h % 12 == 0 ? 12 : h % 12;
  if (suffix) {
    snprintf(out, cap, "%d:%02d", hour, m);
    *suffix = ampm;
  } else {
    snprintf(out, cap, "%d:%02d %s", hour, m, ampm);
  }
}

void localTime(time_t t, tm& out) {
  memset(&out, 0, sizeof out);
  localtime_r(&t, &out);
}

void formatTime(const FeedConfig& feed, time_t t, char* out, size_t cap) {
  tm lt;
  localTime(t, lt);
  formatMinutes(use12h(feed), lt.tm_hour * 60 + lt.tm_min, out, cap);
}

const char* weekdayName(int wday, bool full) {
  static const char* const shortNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (wday < 0 || wday > 6) wday = 0;
  return full ? kDays[wday] : shortNames[wday];
}

const char* monthName(int month) { return kMonths[month < 0 || month > 11 ? 0 : month]; }

void begin(Canvas& canvas, const char* title, const char* right) {
  canvas.clear();
  draw::titleBar(canvas, title, right);
}

void dataNote(const FeedConfig& feed, const SourceStatus& status, time_t now, char* out, size_t cap) {
  out[0] = '\0';
  if (status.state == SourceState::Ok) {
    char t[16];
    formatTime(feed, status.updated, t, sizeof t);
    snprintf(out, cap, "updated %s", t);
  } else if (status.state == SourceState::Stale) {
    char a[20];
    text::age(now, status.updated, a, sizeof a);
    snprintf(out, cap, "old data, %s", a);
  }
}

bool fetchEmptyState(Canvas& canvas, const SourceStatus& status, const char* never, const char* failed) {
  if (status.state == SourceState::Never) {
    draw::emptyState(canvas, never, nullptr);
    return true;
  }
  if (status.state == SourceState::Error) {
    draw::emptyState(canvas, failed, status.error);
    return true;
  }
  return false;
}

int lines(Canvas& canvas, Font font, int x, int baselineY, int w, int pitch, int maxLines,
          const char* s, bool centered) {
  static const U8g2Measure measure;
  text::Line ln[LINES_MAX];
  if (maxLines > LINES_MAX) maxLines = LINES_MAX;
  int n = text::wrap(measure, font, s, w, maxLines, ln);
  for (int i = 0; i < n; i++) {
    char buf[128];
    size_t len = ln[i].len < 120 ? ln[i].len : 120;
    memcpy(buf, ln[i].start, len);
    if (ln[i].ellipsis) {
      memcpy(buf + len, "...", 3);
      len += 3;
    }
    buf[len] = '\0';
    int lx = centered ? x + w / 2 - draw::textWidth(font, buf) / 2 : x;
    draw::text(canvas, font, lx, baselineY + i * pitch, buf);
  }
  return n;
}

}  // namespace pages

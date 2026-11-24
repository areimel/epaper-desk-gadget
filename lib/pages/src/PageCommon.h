#pragma once
#include <stddef.h>
#include <time.h>

#include "Canvas.h"
#include "Draw.h"
#include "Pages.h"

// Helpers shared by the page files, and the per-page entry points that Pages.cpp dispatches to.
namespace pages {

void renderClock(const PageContext& ctx, Canvas& canvas);
void renderWeather(const PageContext& ctx, Canvas& canvas);
void renderHn(const PageContext& ctx, Canvas& canvas);
void renderMessages(const PageContext& ctx, Canvas& canvas);
void renderCalendar(const PageContext& ctx, Canvas& canvas);
void renderNotes(const PageContext& ctx, Canvas& canvas);

constexpr int MARGIN = 4;
constexpr int BODY_TOP = draw::TITLE_BAR_H;  // first row below the bar
constexpr int BODY_W = Canvas::W - 2 * MARGIN;

// Owner-approved shortcut: Fahrenheit feeds are American, so they get 12-hour times.
bool use12h(const FeedConfig& feed);

// Minutes after midnight as "09:30" (24 h) or "9:30 am" (12 h). With suffix non-null the
// output is only the digits and *suffix is "am"/"pm" (12 h) or "" (24 h), for big clocks.
void formatMinutes(bool h12, int minutes, char* out, size_t cap, const char** suffix = nullptr);
// The same for a timestamp, in the process time zone.
void formatTime(const FeedConfig& feed, time_t t, char* out, size_t cap);

const char* weekdayName(int wday, bool full);  // 0 = Sunday; "Sun" or "Sunday"
const char* monthName(int month);              // 0 = January; full name
void localTime(time_t t, tm& out);

// Clears the canvas and draws the title bar.
void begin(Canvas& canvas, const char* title, const char* right);
// Title-bar note for a fetched source: "updated 9:30", "old data, 3 h ago", or "" when there is
// no data to date (Never, Error).
void dataNote(const FeedConfig& feed, const SourceStatus& status, time_t now, char* out, size_t cap);
// Empty states shared by every fetched page. Draws "<never>" for Never or "<failed>" plus the
// error for Error and returns true; returns false (drawing nothing) when there is data.
bool fetchEmptyState(Canvas& canvas, const SourceStatus& status, const char* never, const char* failed);

// Word-wrapped Latin-1 text with an explicit line pitch (draw::paragraph is fixed to the font's
// own leading). First baseline at baselineY; at most maxLines, the last cut with "..." if the
// text is longer. Returns the lines drawn.
int lines(Canvas& canvas, Font font, int x, int baselineY, int w, int pitch, int maxLines,
          const char* s, bool centered = false);
// One truncated line.
inline void oneLine(Canvas& canvas, Font font, int x, int baselineY, int w, const char* s) {
  lines(canvas, font, x, baselineY, w, 0, 1, s);
}

}  // namespace pages

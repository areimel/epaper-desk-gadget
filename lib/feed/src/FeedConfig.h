#pragma once
#include <string.h>

#include "Limits.h"

// The feed: everything the owner edits in the web app. Plain data with fixed storage (about
// 8 KB), kept as one static instance; never put one on the stack. Strings are UTF-8 as typed
// and NUL-terminated; they are folded to Latin-1 only when drawn. JSON shape: docs/api.md.
enum class PageId : uint8_t { Clock, Weather, Hn, Messages, Calendar, Notes };

namespace feed {
constexpr const char* PAGE_NAMES[limits::PAGE_COUNT] = {"clock", "weather", "hn", "messages", "calendar", "notes"};
inline const char* pageName(PageId id) { return PAGE_NAMES[static_cast<uint8_t>(id)]; }
inline bool pageFromName(const char* name, PageId& out) {
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++) {
    if (strcmp(name, PAGE_NAMES[i]) == 0) {
      out = static_cast<PageId>(i);
      return true;
    }
  }
  return false;
}
}  // namespace feed

enum class Units : uint8_t { Celsius, Fahrenheit };

struct PageSlot {
  PageId id;
  bool on;
};

struct Message {
  char text[limits::MESSAGE_TEXT_MAX + 1];
  char by[limits::MESSAGE_BY_MAX + 1];  // may be empty
};

struct NoteLine {
  char text[limits::NOTE_TEXT_MAX + 1];
  bool todo;  // draw a checkbox
  bool done;  // only meaningful when todo
};

struct FeedConfig {
  uint16_t dwellSec;
  char tzIana[limits::TZ_MAX + 1];   // e.g. "America/New_York"; compared with calendar TZIDs
  char tzPosix[limits::TZ_MAX + 1];  // e.g. "EST5EDT,M3.2.0,M11.1.0"; what the clock uses
  PageSlot pages[limits::PAGE_COUNT];  // rotation order; each PageId exactly once
  struct {
    bool set;  // false until a place is chosen
    char place[limits::PLACE_MAX + 1];
    float lat;
    float lon;
    Units units;
  } weather;
  char calendarUrl[limits::URL_MAX + 1];  // secret; empty = no calendar; never sent back by the API
  uint8_t messageCount;
  Message messages[limits::MESSAGES_MAX];
  struct {
    char title[limits::NOTE_TITLE_MAX + 1];
    uint8_t lineCount;
    NoteLine lines[limits::NOTE_LINES_MAX];
  } notes;
};

namespace feed {
// All six pages on in the default order, 5-minute dwell, UTC, Celsius, nothing else set.
inline void setDefaults(FeedConfig& f) {
  memset(&f, 0, sizeof(f));
  f.dwellSec = limits::DWELL_DEFAULT_S;
  strcpy(f.tzIana, "Etc/UTC");
  strcpy(f.tzPosix, "UTC0");
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++) f.pages[i] = {static_cast<PageId>(i), true};
  f.weather.units = Units::Celsius;
  strcpy(f.notes.title, "Today");
}
}  // namespace feed

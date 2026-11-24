#pragma once
// Realistic fixed data for the page suites. Everything is static (the feed is 8 KB), time is
// fixed and the zone is America/New_York so goldens never depend on the machine.
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "FeedConfig.h"
#include "Pages.h"
#include "Snapshots.h"

namespace data {

struct Set {
  FeedConfig feed;
  WeatherSnap weather;
  HnSnap hn;
  CalSnap cal;
};

inline void setZone() {
  setenv("TZ", "America/New_York", 1);
  tzset();
}

// Tuesday 24 November 2026, 09:41 local.
inline time_t fixedNow() {
  setZone();
  tm t{};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 10;
  t.tm_mday = 24;
  t.tm_hour = 9;
  t.tm_min = 41;
  t.tm_isdst = -1;
  return mktime(&t);
}

inline void status(SourceStatus& s, SourceState state, int minutesAgo, const char* error = "") {
  s.state = state;
  s.updated = state == SourceState::Never ? 0 : fixedNow() - minutesAgo * 60;
  strncpy(s.error, error, sizeof s.error - 1);
  s.error[sizeof s.error - 1] = '\0';
}

inline void fillWeather(WeatherSnap& w, int now, int feels, int hi, int lo, uint8_t code) {
  memset(&w, 0, sizeof w);
  status(w.status, SourceState::Ok, 11);
  w.tempNow = now;
  w.feels = feels;
  w.hi = hi;
  w.lo = lo;
  w.rainPct = 60;
  w.code = code;
  w.isDay = true;
  const uint8_t codes[] = {2, 95, 73};
  for (int i = 0; i < limits::FORECAST_DAYS; i++) {
    w.days[i] = {static_cast<uint8_t>((3 + i) % 7), static_cast<int16_t>(hi - 1 - i), static_cast<int16_t>(lo + 1 - i), codes[i]};
  }
}

inline void fillHn(HnSnap& h) {
  static const char* const titles[limits::HN_STORIES] = {
      "Show HN: I built a tiny e-paper desk gadget that edits itself",
      "Ask HN: What are you working on this weekend?",
      "A Brief History of the Typeface, From Gutenberg to Variable Fonts and Everything Since",
      "SQLite is not a toy database",
      "Why I stopped using microservices and went back to a boring monolith for good",
      "Linux 7.0 released",
      "The surprising maths of folding paper into a perfect twelve-sided polygon, explained",
      "Rust in the kernel: two years on",
      "Show HN: A 1-bit font that stays readable at 8 pixels",
  };
  static const uint16_t points[] = {412, 230, 1, 988, 57, 1203, 144, 301, 89};
  static const uint16_t comments[] = {96, 41, 0, 312, 1, 640, 22, 118, 7};
  memset(&h, 0, sizeof h);
  status(h.status, SourceState::Ok, 6);
  h.count = limits::HN_STORIES;
  for (int i = 0; i < h.count; i++) {
    strcpy(h.stories[i].title, titles[i]);
    h.stories[i].points = points[i];
    h.stories[i].comments = comments[i];
  }
}

inline void addEvent(CalSnap& c, const char* title, int start, int end) {
  CalEvent& e = c.events[c.count++];
  strncpy(e.title, title, sizeof e.title - 1);
  e.startMin = static_cast<int16_t>(start);
  e.endMin = static_cast<int16_t>(end);
}

inline void fillCal(CalSnap& c, int events) {
  memset(&c, 0, sizeof c);
  status(c.status, SourceState::Ok, 38);
  static const char* const titles[] = {"Team offsite", "Standup", "Design review: e-paper pages",
                                       "Lunch with Maria", "Dentist, Dr. Okafor, second floor, room 4",
                                       "Plumber arrives", "Pick up the kids"};
  static const int starts[] = {-1, 9 * 60 + 30, 11 * 60, 12 * 60 + 15, 14 * 60, 16 * 60 + 30, 17 * 60 + 45};
  static const int ends[] = {1440, 9 * 60 + 45, 12 * 60, 13 * 60, 15 * 60, 17 * 60, 18 * 60};
  for (int i = 0; i < events && i < limits::CAL_EVENTS_MAX; i++) addEvent(c, titles[i], starts[i], ends[i]);
}

// Defaults: Celsius (24 h), London, a calendar link, notes and messages filled in.
inline Set& fresh() {
  static Set s;
  memset(&s, 0, sizeof s);
  feed::setDefaults(s.feed);
  strcpy(s.feed.tzIana, "America/New_York");
  strcpy(s.feed.tzPosix, "EST5EDT,M3.2.0,M11.1.0");
  s.feed.weather.set = true;
  strcpy(s.feed.weather.place, "London");
  strcpy(s.feed.calendarUrl, "https://calendar.example/private/basic.ics");
  fillWeather(s.weather, 12, 9, 14, 7, 61);
  fillHn(s.hn);
  fillCal(s.cal, 4);
  return s;
}

inline PageContext context(const Set& s, uint8_t visit = 0, bool synced = true) {
  return PageContext{s.feed, s.weather, s.hn, s.cal, fixedNow(), synced, visit};
}

inline void addMessage(FeedConfig& f, const char* text, const char* by) {
  Message& m = f.messages[f.messageCount++];
  strncpy(m.text, text, sizeof m.text - 1);
  strncpy(m.by, by, sizeof m.by - 1);
}

inline void addNote(FeedConfig& f, const char* text, bool todo, bool done) {
  NoteLine& n = f.notes.lines[f.notes.lineCount++];
  strncpy(n.text, text, sizeof n.text - 1);
  n.todo = todo;
  n.done = done;
}

}  // namespace data

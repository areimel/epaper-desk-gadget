#pragma once
#include <stdint.h>
#include <time.h>

#include "Limits.h"

// What each fetched source last produced. Plain data: parsers fill them, the fetcher owns them
// and hands out copies, page views only read them. Strings are already folded to Latin-1.
enum class SourceState : uint8_t {
  Never,  // nothing fetched yet (or the source is not configured)
  Ok,     // data is from the latest attempt
  Stale,  // the latest attempt failed; data is from an earlier one
  Error,  // every attempt so far failed; there is no data
};

struct SourceStatus {
  SourceState state;
  time_t updated;  // when the data was fetched; 0 if never
  char error[32];  // short reason for Stale/Error, e.g. "no network", "HTTP 503", "bad data"
};

struct WeatherDay {
  uint8_t wday;  // 0 = Sunday
  int16_t hi;
  int16_t lo;
  uint8_t code;  // WMO weather code
};

struct WeatherSnap {
  SourceStatus status;
  int16_t tempNow;  // whole degrees in the feed's units, as are feels/hi/lo
  int16_t feels;
  int16_t hi;  // today
  int16_t lo;
  uint8_t rainPct;  // today's highest precipitation probability, 0-100
  uint8_t code;     // WMO weather code now
  bool isDay;
  WeatherDay days[limits::FORECAST_DAYS];  // the days after today
};

struct HnStory {
  char title[limits::HN_TITLE_MAX + 1];
  uint16_t points;
  uint16_t comments;
};

struct HnSnap {
  SourceStatus status;
  uint8_t count;
  HnStory stories[limits::HN_STORIES];  // front-page order
};

struct CalEvent {
  char title[limits::CAL_TITLE_MAX + 1];
  int16_t startMin;  // minutes after local midnight today; -1 = all day (or began before today)
  int16_t endMin;    // minutes after local midnight; 1440 = runs past today; unused when all day
};

struct CalSnap {
  SourceStatus status;
  uint8_t count;
  CalEvent events[limits::CAL_EVENTS_MAX];  // today's events: all-day first, then by start time
  uint16_t skipped;  // events left out: foreign time zone or unsupported recurrence
};

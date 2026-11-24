#pragma once
#include <stdint.h>

#include "IcsTime.h"

// RRULE subset: FREQ=DAILY|WEEKLY|YEARLY with INTERVAL, COUNT, UNTIL, WKST, plain BYDAY (WEEKLY
// only) and, for YEARLY, a single BYMONTH (optionally with BYMONTHDAY) repeating DTSTART's.
// Everything else, including YEARLY;BYMONTHDAY without BYMONTH, is Freq::Unsupported. Occurrence tests are O(1) per day (YEARLY Feb 29 COUNT: O(years/interval)).
namespace ics {

enum class Freq : uint8_t { None, Daily, Weekly, Yearly, Unsupported };

struct Rule {
  Freq freq = Freq::None;
  uint16_t interval = 1;
  uint32_t count = 0;  // 0 = no COUNT
  Time until;          // kind None = no UNTIL; Date, Local (floating) or Utc
  uint8_t byday = 0;   // bit w set = weekday w (0 = Sunday); 0 = DTSTART's weekday
  uint8_t wkst = 1;    // week start, 1 = Monday (the RFC default)
  uint8_t bymonth = 0;     // YEARLY only; must equal DTSTART's month
  uint8_t bymonthday = 0;  // YEARLY only; must equal DTSTART's day
};

// Parses an RRULE value such as "FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE,FR".
Rule parseRule(const char* value);

// False when the rule cannot be evaluated for a series starting on civil day d0.
bool supported(const Rule& r, int32_t d0);

// True when a series starting on civil day d0 has an occurrence starting on civil day d, counting
// COUNT but not UNTIL (the caller compares UNTIL with the occurrence's start time). DTSTART is
// always the first occurrence, even if it does not match BYDAY (as libical and Google do).
bool occursOn(const Rule& r, int32_t d0, int32_t d);

}  // namespace ics

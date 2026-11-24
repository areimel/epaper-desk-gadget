#pragma once
// Streaming iCalendar (RFC 5545) parser that keeps only TODAY'S events. Pure C++17 + libc time.
//
// Design
// ------
// Bytes -> LineReader (IcsLine): CR bytes and NULs are dropped, LF ends a line, and LF followed by
//   a space or tab is a fold, removed at the byte level (so a UTF-8 sequence split by a fold is
//   rejoined). A logical line longer than ics::LINE_CAP keeps its first LINE_CAP bytes and is
//   flagged truncated; the rest is dropped without buffering, so sync is never lost.
// Lines -> component state machine (this class): BEGIN:VCALENDAR arms the parser; BEGIN:VEVENT
//   opens an event (a second BEGIN:VEVENT, END:VCALENDAR or end of input drops an unterminated
//   one, counted in `skipped` if what was read places it on today, and if it was an override its
//   series is uncertain today); any BEGIN inside an event
//   (VALARM...) bumps a nesting counter and properties are only read at nesting 0; VTIMEZONE,
//   VTODO, VJOURNAL, VFREEBUSY are never opened as events. END:VEVENT at nesting 0 evaluates.
// Event properties -> ics::Event (IcsEvent): UID is kept as a 64-bit FNV-1a hash, SUMMARY is
//   unescaped and folded to Latin-1 immediately, DTSTART/DTEND/DURATION/RRULE/RECURRENCE-ID are
//   parsed into small structs, EXDATE keeps only values near today. Nothing else is stored.
// Evaluation (IcsEvent, IcsRule, IcsTime): a single event is tested against today's window; a
//   series is tested on each candidate start day in [today-8, today+1] with O(1) arithmetic per
//   day (COUNT is computed in closed form, see IcsRule.cpp), so history costs nothing.
// Collection (IcsCollect): today's occurrences go to a candidate table; overrides record the
//   (UID hash, original instant) they replace in a suppression table. finish() removes suppressed
//   series occurrences, sorts (all-day, start minute, title) and keeps the first CAL_EVENTS_MAX.
//
// Bounded tables (sizes in IcsCollect.h / IcsEvent.h) and overflow policy
//   candidates  CAND_MAX=24: when full, the latest-sorting candidate is evicted. If after
//               reconciliation fewer than CAL_EVENTS_MAX remain, the evictions that could have
//               filled the gap are counted in `skipped` (missing, never misplaced).
//   suppress    SUPPRESS_MAX=32 overrides whose original day is in [today-9, today+2]. If it
//               overflows, EVERY series occurrence is dropped and counted in `skipped`, because
//               any of them might have been moved.
//   EXDATE      EXDATE_MAX=12 per event within [today-9, today+2]; overflow, a truncated EXDATE
//               line or a foreign-zone EXDATE makes today's occurrence of that event uncertain:
//               it is dropped and counted.
//   series span a recurring event longer than SPAN_MAX_DAYS=7 is never shown; it is counted on
//               days an occurrence covers (start days walked up to 400; longer: always counted).
//   undated     a VEVENT without a readable DTSTART cannot be placed or judged: not counted,
//               unless it is an override of an instance near today (then counted).
//
// Day-window arithmetic
//   today = civil day number of `now` in the process zone; todayStart/todayEnd = mktime of local
//   midnight today / tomorrow (so 23 h and 25 h days are exact). A timed occurrence [s,e) shows
//   when s < todayEnd and (e > todayStart, or e == s >= todayStart). startMin/endMin are wall
//   clock minutes from localtime_r (09:00 is 540 on a DST change day), -1 / 1440 when outside.
//   All-day events use civil day numbers only: shown when start <= today < end (end exclusive).
//   Series in a local zone follow the wall clock (mktime per occurrence); `Z` series follow UTC.
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "ByteSink.h"
#include "IcsCollect.h"
#include "IcsEvent.h"
#include "IcsLine.h"
#include "Snapshots.h"

class IcsParser : public ByteSink {
 public:
  // now: any instant inside the local day of interest. deviceTzIana: the device's zone name, e.g.
  // "America/New_York"; the process time zone (TZ) is already set to that zone.
  IcsParser(time_t now, const char* deviceTzIana);
  bool write(const uint8_t* data, size_t n) override;  // any chunking; false only if unusable
  // Call once after the last byte. Fills snap.count, snap.events and snap.skipped; never touches
  // snap.status. Returns false if no BEGIN:VCALENDAR was seen.
  bool finish(CalSnap& snap);

 private:
  void onLine(char* line, size_t len, bool truncated);
  void beginEvent();
  void dropEvent();

  ics::Day day_;
  ics::LineReader reader_;
  ics::Event event_;
  ics::Collector collector_;
  bool seenCalendar_ = false;
  bool inEvent_ = false;
  uint8_t nest_ = 0;  // components open inside the current VEVENT (VALARM...)
  bool finished_ = false;
};

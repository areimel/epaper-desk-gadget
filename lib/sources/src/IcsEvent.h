#pragma once
#include <stdint.h>
#include <time.h>

#include "IcsCollect.h"
#include "IcsLine.h"
#include "IcsRule.h"
#include "IcsTime.h"

// One VEVENT: the few properties needed, gathered line by line (IcsEvent.cpp), then evaluated
// against today (IcsEval.cpp).
namespace ics {

constexpr uint8_t EXDATE_MAX = 12;      // EXDATE values kept per event (only those near today)
constexpr int32_t SPAN_MAX_DAYS = 7;    // longest occurrence of a series that is evaluated
constexpr int32_t NEAR_BEFORE = SPAN_MAX_DAYS + 2;  // EXDATE / RECURRENCE-ID kept from today-9
constexpr int32_t NEAR_AFTER = 2;                   // ... to today+2 (covers UTC offsets)

// The local day of interest.
struct Day {
  int32_t today;   // civil day number
  int64_t start;   // instant of local midnight starting today
  int64_t end;     // instant of local midnight ending today (23 or 25 h later on DST days)
  const char* tz;  // device zone name (IANA), compared with TZID parameters
  bool near(int32_t day) const { return day >= today - NEAR_BEFORE && day <= today + NEAR_AFTER; }
};
Day makeDay(time_t now, const char* tz);

class Event {
 public:
  void reset();
  // A property directly inside the VEVENT (not in a VALARM). truncated: the line was longer
  // than LINE_CAP and only its prefix is here.
  void property(Prop& p, bool truncated, const Day& day);
  void evaluate(const Day& day, Collector& out) const;

 private:
  Time readTime(const Prop& p, bool truncated, const Day& day) const;
  void readExdate(const Prop& p, bool truncated, const Day& day);
  void readRdate(const Prop& p, bool truncated, const Day& day);

  // IcsEval.cpp
  void recordOverride(const Day& day, Collector& out) const;
  bool plausible(const Day& day, bool series) const;
  int32_t allDayLength() const;
  int64_t endOf(int64_t start) const;
  bool excluded(const Key& k) const;
  void single(const Day& day, Collector& out) const;
  void series(const Day& day, Collector& out) const;
  void emit(const Day& day, Collector& out, int64_t s, int64_t e, const Key& k, bool fromSeries) const;
  void fill(Candidate& c, const Key& k, bool fromSeries) const;
  void emitAllDay(Collector& out, const Key& k, bool fromSeries) const;

  char title_[limits::CAL_TITLE_MAX + 1];
  uint64_t uid_;
  Time start_, end_, rid_;
  Duration dur_;
  Rule rule_;
  Key ex_[EXDATE_MAX];
  uint8_t exCount_;
  bool hasTitle_, hasEnd_, hasDur_, hasRule_;
  bool cancelled_;
  bool bad_;          // DTEND/DURATION present but unreadable: the event cannot be placed
  bool extraRule_;    // a second RRULE, an RDATE or an EXRULE: unsupported
  bool rdateNear_;    // an RDATE (unsupported) near today
  bool exUncertain_;  // an EXDATE near today could not be kept or read
  bool ridRange_;     // RECURRENCE-ID;RANGE=THISANDFUTURE (changes every later instance)
};

}  // namespace ics

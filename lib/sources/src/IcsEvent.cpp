// Gathering one VEVENT's properties. Only what evaluation needs is kept; see IcsEval.cpp.
#include <string.h>

#include "IcsEvent.h"
#include "Text.h"

namespace ics {
namespace {

// 64-bit FNV-1a of the UID; never 0 (0 means "no UID"). Collisions between two UIDs are
// negligible at calendar sizes and would only hide one occurrence.
uint64_t hashUid(const char* s) {
  uint64_t h = 14695981039346656037ull;
  for (; *s; s++) {
    h ^= static_cast<uint8_t>(*s);
    h *= 1099511628211ull;
  }
  return h ? h : 1;
}

// The TZID class of a date-time property. A TZID too long to copy is certainly not the
// device's zone (TZ_MAX), so it is foreign.
Zone zoneOf(const Prop& p, const Day& day) {
  char tz[limits::TZ_MAX + 1];
  if (param(p, "TZID", tz, sizeof tz)) return classifyZone(tz, day.tz);
  return hasParam(p, "TZID") ? Zone::Foreign : Zone::Local;
}

void trimRight(char* s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = '\0';
}

}  // namespace

Day makeDay(time_t now, const char* tz) {
  Day d;
  d.today = localDay(static_cast<int64_t>(now), nullptr);
  d.start = localInstant(d.today, 0);
  d.end = localInstant(d.today + 1, 0);
  if (d.end <= d.start) d.end = d.start + SECS_PER_DAY;  // mktime failure: keep a sane window
  d.tz = tz ? tz : "";
  return d;
}

void Event::reset() { *this = Event{}; }

Time Event::readTime(const Prop& p, bool truncated, const Day& day) const {
  Time t;
  if (truncated) {
    t.kind = Kind::Bad;
    return t;
  }
  t = parseTime(p.value, nullptr);
  applyZone(t, zoneOf(p, day));
  return t;
}

void Event::readExdate(const Prop& p, bool truncated, const Day& day) {
  // Only values near today matter (an occurrence starting there could cover today). Any doubt
  // about one of those makes today's occurrence uncertain rather than wrong.
  if (truncated) exUncertain_ = true;  // the cut-off tail may hold today
  Zone z = zoneOf(p, day);
  const char* s = p.value;
  while (*s) {
    Time t = parseTime(s, &s);
    applyZone(t, z);
    if (t.kind == Kind::Bad) {
      exUncertain_ = true;
      continue;
    }
    if (!day.near(t.day)) continue;
    if (t.kind == Kind::Foreign || exCount_ >= EXDATE_MAX) {
      exUncertain_ = true;
      continue;
    }
    ex_[exCount_++] = Key{t.day, t.kind == Kind::Date ? NO_INSTANT : instantOf(t)};
  }
}

void Event::readRdate(const Prop& p, bool truncated, const Day& day) {
  // RDATE is unsupported; remember only whether it adds an instance near today, so the event
  // is counted as skipped when it matters and not for every historic RDATE.
  extraRule_ = true;
  if (truncated) rdateNear_ = true;
  const char* s = p.value;
  while (*s) {
    Time t = parseTime(s, &s);  // a PERIOD value ("a/b") parses as Bad: assume near
    if (t.kind == Kind::Bad || day.near(t.day)) rdateNear_ = true;
  }
}

void Event::property(Prop& p, bool truncated, const Day& day) {
  if (is(p, "UID")) {
    uid_ = hashUid(p.value);
  } else if (is(p, "SUMMARY")) {
    // An over-long SUMMARY keeps its LINE_CAP-byte prefix, far more than CAL_TITLE_MAX.
    unescapeText(p.value);
    text::fold(p.value, title_, sizeof title_);
    hasTitle_ = true;
  } else if (is(p, "STATUS")) {
    trimRight(p.value);
    cancelled_ = equalsUpper(p.value, "CANCELLED");
  } else if (is(p, "DTSTART")) {
    start_ = readTime(p, truncated, day);
  } else if (is(p, "DTEND")) {
    hasEnd_ = true;
    end_ = readTime(p, truncated, day);
    if (end_.kind == Kind::Bad) bad_ = true;
  } else if (is(p, "DURATION")) {
    hasDur_ = true;
    trimRight(p.value);
    if (truncated || !parseDuration(p.value, dur_)) bad_ = true;
  } else if (is(p, "RRULE")) {
    if (hasRule_) extraRule_ = true;  // several RRULEs: unsupported
    hasRule_ = true;
    if (truncated) rule_.freq = Freq::Unsupported;
    else rule_ = parseRule(p.value);
  } else if (is(p, "EXRULE")) {
    extraRule_ = true;
  } else if (is(p, "RDATE")) {
    readRdate(p, truncated, day);
  } else if (is(p, "EXDATE")) {
    readExdate(p, truncated, day);
  } else if (is(p, "RECURRENCE-ID")) {
    rid_ = readTime(p, truncated, day);
    char range[16];
    ridRange_ = param(p, "RANGE", range, sizeof range) && equalsUpper(range, "THISANDFUTURE");
  }
}

}  // namespace ics

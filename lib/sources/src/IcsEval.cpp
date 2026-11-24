// Evaluating one VEVENT against today: which of its occurrences overlap the local day, and what
// to record for overrides. See the design note in IcsParse.h.
#include <string.h>

#include "IcsEvent.h"

namespace ics {
namespace {

// A timed occurrence [s, e) is on today when it starts before tomorrow and ends after
// midnight; a zero-length one when its instant is inside today. Ending at 00:00 is not today.
bool overlaps(const Day& day, int64_t s, int64_t e) {
  return s < day.end && (e > day.start || (e == s && s >= day.start));
}

int16_t minutesOf(int64_t inst) {
  int m;
  localDay(inst, &m);
  return static_cast<int16_t>(m);
}

}  // namespace

void Event::evaluate(const Day& day, Collector& out) const {
  bool override = rid_.kind != Kind::None;
  // Recorded first, even when cancelled or undatable: the original instance is still replaced.
  if (override) recordOverride(day, out);
  if (start_.kind == Kind::None || start_.kind == Kind::Bad) {
    // An override whose new time is unreadable replaced an instance near today: count it. Any
    // other undatable event cannot be judged and is not counted.
    if (override && !cancelled_ && (rid_.kind == Kind::Bad || day.near(rid_.day))) out.skip();
    return;
  }
  if (cancelled_) return;
  bool isSeries = !override && (hasRule_ || extraRule_);  // an override is a single instance
  bool foreign = start_.kind == Kind::Foreign || (hasEnd_ && end_.kind == Kind::Foreign);
  bool unsupported = isSeries && (extraRule_ || !supported(rule_, start_.day));
  if (foreign || unsupported || bad_) {
    if (plausible(day, isSeries)) out.skip();
    return;
  }
  if (isSeries) series(day, out);
  else single(day, out);
}

void Event::recordOverride(const Day& day, Collector& out) const {
  if (uid_ == 0) return;  // nothing to match it with
  if (rid_.kind == Kind::Bad) {
    out.suppress(uid_, Key{0, NO_INSTANT}, true);  // the replaced instance is unknown
  } else if (ridRange_) {
    // THISANDFUTURE changes every instance from the original on.
    if (rid_.day <= day.today + NEAR_AFTER) out.suppress(uid_, Key{0, NO_INSTANT}, true);
  } else if (day.near(rid_.day)) {
    if (rid_.kind == Kind::Foreign) out.suppress(uid_, Key{0, NO_INSTANT}, true);
    else out.suppress(uid_, Key{rid_.day, rid_.kind == Kind::Date ? NO_INSTANT : instantOf(rid_)}, false);
  }
}

// Whether an event that cannot be interpreted might have been on today (then it is counted).
bool Event::plausible(const Day& day, bool isSeries) const {
  const int32_t today = day.today;
  const int64_t span = spanCivilDays();
  if (isSeries && hasRule_) {
    // Ended once the last start UNTIL allows, plus one occurrence's span, is before today.
    bool ended = rule_.until.kind != Kind::None && rule_.until.day + span < today - 1;
    return rdateNear_ || (!ended && start_.day <= today + 1);
  }
  if (rdateNear_) return true;
  // A single event (or RDATE/EXRULE without RRULE): within a day of today, by civil dates.
  return start_.day <= today + 1 && start_.day + span >= today - 1;
}

// Civil days from DTSTART's date to the end's date (0 for a same-day or unknown end).
int64_t Event::spanCivilDays() const {
  int64_t endDay = start_.day;
  if (hasEnd_ && end_.kind != Kind::Bad) endDay = end_.day;
  else if (hasDur_ && !bad_) endDay = start_.day + dur_.days + dur_.secs / SECS_PER_DAY;
  return endDay > start_.day ? endDay - start_.day : 0;
}

// Days covered by an all-day event: DTEND (exclusive) or DURATION, at least one.
int32_t Event::allDayLength() const {
  int64_t len = 1;
  if (hasEnd_ && end_.kind == Kind::Date) {
    len = static_cast<int64_t>(end_.day) - start_.day;
  } else if (hasEnd_ && (end_.kind == Kind::Local || end_.kind == Kind::Utc)) {
    // A DATE-TIME end: up to its date, plus that date if the end is after its midnight.
    len = static_cast<int64_t>(end_.day) - start_.day + (end_.secs > 0 ? 1 : 0);
  } else if (hasDur_) len = dur_.days + (dur_.secs + SECS_PER_DAY - 1) / SECS_PER_DAY;
  if (len < 1) len = 1;
  return len > DURATION_DAYS_MAX ? DURATION_DAYS_MAX : static_cast<int32_t>(len);
}

// End of a timed occurrence starting at `start` (DTEND for a single event, or DURATION).
int64_t Event::endOf(int64_t start) const {
  int64_t e = start;
  if (hasEnd_) e = instantOf(end_);
  else if (hasDur_) e = addDuration(start, start_.kind == Kind::Local, dur_);
  return e < start ? start : e;
}

bool Event::excluded(const Key& k) const {
  for (uint8_t i = 0; i < exCount_; i++) {
    if (keysMatch(ex_[i], k)) return true;
  }
  return false;
}

void Event::fill(Candidate& c, const Key& k, bool fromSeries) const {
  if (hasTitle_ && title_[0]) memcpy(c.title, title_, sizeof c.title);
  else strcpy(c.title, "(No title)");
  c.uid = uid_;
  c.key = k;
  c.fromSeries = fromSeries;
}

void Event::emit(const Day& day, Collector& out, int64_t s, int64_t e, const Key& k,
                 bool fromSeries) const {
  if (!overlaps(day, s, e)) return;
  Candidate c;
  fill(c, k, fromSeries);
  c.allDay = false;
  c.startMin = s < day.start ? -1 : minutesOf(s);
  c.endMin = e >= day.end ? 1440 : minutesOf(e);
  out.add(c);
}

void Event::emitAllDay(Collector& out, const Key& k, bool fromSeries) const {
  Candidate c;
  fill(c, k, fromSeries);
  c.allDay = true;
  c.startMin = -1;
  c.endMin = 1440;
  out.add(c);
}

void Event::single(const Day& day, Collector& out) const {
  const int32_t today = day.today;
  if (start_.kind == Kind::Date) {
    int32_t len = allDayLength();
    if (start_.day <= today && today < static_cast<int64_t>(start_.day) + len) {
      emitAllDay(out, Key{start_.day, NO_INSTANT}, false);
    }
    return;
  }
  // Cheap rejection on civil days first (UTC dates are within one day of local dates), so the
  // years of history in a feed cost no mktime calls.
  int64_t endDay = start_.day;
  if (hasEnd_) endDay = end_.day;
  else if (hasDur_) endDay = start_.day + dur_.days + dur_.secs / SECS_PER_DAY + 1;
  if (start_.day > today + 1 || endDay < today - 1) return;
  int64_t s = instantOf(start_);
  emit(day, out, s, endOf(s), Key{start_.day, s}, false);
}

// A series whose occurrences last longer than SPAN_MAX_DAYS is outside the EXDATE / override
// window, so it is never shown; it is counted only when an occurrence could cover today (by
// civil days, walking at most SPAN_WALK_MAX_DAYS start days; beyond that, counted always).
void Event::longSeries(const Day& day, Collector& out, int64_t spanDays) const {
  const int32_t today = day.today;
  if (spanDays > SPAN_WALK_MAX_DAYS) {
    out.skip();
    return;
  }
  int32_t lo = today - static_cast<int32_t>(spanDays) - 1;
  if (lo < start_.day) lo = start_.day;
  const bool allDay = start_.kind == Kind::Date;
  for (int32_t d = lo; d <= today + 1; d++) {
    if (!occursOn(rule_, start_.day, d)) continue;
    if (rule_.until.kind != Kind::None && d > rule_.until.day + 1) continue;  // +1: UTC skew
    bool covers = allDay ? (d <= today && today < d + spanDays) : (d + spanDays >= today);
    if (covers) {
      out.skip();
      return;
    }
  }
}

// A supported RRULE. Candidate start days are the few whose occurrence could reach today:
// [today - span - 1, today + 1] for timed series (the extra day absorbs UTC offsets), and
// [today - len + 1, today] for all-day ones. Each is tested in O(1) by occursOn().
void Event::series(const Day& day, Collector& out) const {
  const int32_t today = day.today;
  const Rule& r = rule_;
  if (start_.day > today + 1) return;  // not begun
  const bool allDay = start_.kind == Kind::Date;
  int32_t len = 1;
  int64_t offset = 0;  // DTEND - DTSTART, exact, applied to every occurrence
  int64_t spanDays;    // civil days an occurrence can touch
  if (allDay) {
    len = allDayLength();
    spanDays = len;
  } else {
    int64_t s0 = instantOf(start_);
    offset = endOf(s0) - s0;
    spanDays = offset / SECS_PER_DAY + 1;
  }
  // Ended: even the last start UNTIL allows, plus one occurrence's span, is before today.
  if (r.until.kind != Kind::None && r.until.day + spanDays < today - 1) return;
  if (spanDays > SPAN_MAX_DAYS) {
    longSeries(day, out, spanDays);
    return;
  }
  int32_t lo = allDay ? today - len + 1 : today - static_cast<int32_t>(spanDays) - 1;
  const int32_t hi = allDay ? today : today + 1;
  if (lo < start_.day) lo = start_.day;
  // A timed UNTIL two or more civil days after the last candidate is after every candidate
  // start, so it is only converted (mktime) when it could matter.
  const bool untilTimed =
      (r.until.kind == Kind::Local || r.until.kind == Kind::Utc) && r.until.day <= hi + 2;
  const int64_t untilInst = untilTimed ? instantOf(r.until) : NO_INSTANT;
  for (int32_t d = lo; d <= hi; d++) {
    if (!occursOn(r, start_.day, d)) continue;
    if (r.until.kind == Kind::Date && d > r.until.day) continue;  // UNTIL is inclusive
    if (allDay) {
      if (untilTimed && localInstant(d, 0) > untilInst) continue;
      Key k{d, NO_INSTANT};
      if (excluded(k) || !(d <= today && today < static_cast<int64_t>(d) + len)) continue;
      if (exUncertain_) out.skip();
      else emitAllDay(out, k, true);
      continue;
    }
    // The wall clock of DTSTART is kept: a local series is placed with mktime on each day, so
    // 09:00 stays 09:00 across DST changes; a UTC series keeps its UTC time instead.
    int64_t s = start_.kind == Kind::Utc ? utcInstant(d, start_.secs) : localInstant(d, start_.secs);
    if (untilTimed && s > untilInst) continue;
    Key k{d, s};
    if (excluded(k)) continue;
    int64_t e = hasDur_ && !hasEnd_ ? endOf(s) : s + offset;
    if (!overlaps(day, s, e)) continue;
    if (exUncertain_) out.skip();
    else emit(day, out, s, e, k, true);
  }
}

}  // namespace ics

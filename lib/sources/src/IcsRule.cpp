#include "IcsRule.h"

#include <string.h>

#include "IcsLine.h"

namespace ics {
namespace {

constexpr uint32_t INTERVAL_MAX = 10000;
constexpr uint32_t COUNT_MAX = 1000000;

// Parses a whole decimal field of at most 7 digits (no overflow possible). False if empty,
// longer, or not all digits.
bool number(const char* s, size_t n, uint32_t& out) {
  if (n == 0 || n > 7) return false;
  out = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    out = out * 10 + static_cast<uint32_t>(s[i] - '0');
  }
  return true;
}

// "SU".."SA" -> 0..6, or -1. Ordinals such as "2MO" or "-1FR" are not accepted.
int dayCode(const char* s, size_t n) {
  static const char* const names[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  for (int i = 0; i < 7; i++) {
    if (equalsUpper(s, n, names[i])) return i;
  }
  return -1;
}

bool applyPart(Rule& r, const char* k, size_t kn, const char* v, size_t vn) {
  uint32_t n;
  if (equalsUpper(k, kn, "FREQ")) {
    if (r.freq != Freq::None) return false;  // FREQ twice
    if (equalsUpper(v, vn, "DAILY")) r.freq = Freq::Daily;
    else if (equalsUpper(v, vn, "WEEKLY")) r.freq = Freq::Weekly;
    else if (equalsUpper(v, vn, "YEARLY")) r.freq = Freq::Yearly;
    else return false;
  } else if (equalsUpper(k, kn, "INTERVAL")) {
    if (!number(v, vn, n) || n == 0 || n > INTERVAL_MAX) return false;
    r.interval = static_cast<uint16_t>(n);
  } else if (equalsUpper(k, kn, "COUNT")) {
    if (!number(v, vn, n) || n == 0 || n > COUNT_MAX) return false;
    r.count = n;
  } else if (equalsUpper(k, kn, "UNTIL")) {
    char buf[24];
    if (vn >= sizeof buf) return false;
    memcpy(buf, v, vn);
    buf[vn] = '\0';
    r.until = parseTime(buf, nullptr);
    if (r.until.kind == Kind::Bad) {
      r.until = Time();  // unknown end: never treated as "ended"
      return false;
    }
  } else if (equalsUpper(k, kn, "WKST")) {
    int w = dayCode(v, vn);
    if (w < 0) return false;
    r.wkst = static_cast<uint8_t>(w);
  } else if (equalsUpper(k, kn, "BYDAY")) {
    for (size_t i = 0; i < vn;) {
      size_t j = i;
      while (j < vn && v[j] != ',') j++;
      int w = dayCode(v + i, j - i);
      if (w < 0) return false;
      r.byday = static_cast<uint8_t>(r.byday | (1 << w));
      i = j + 1;
    }
    if (r.byday == 0) return false;
  } else if (equalsUpper(k, kn, "BYMONTH")) {
    if (!number(v, vn, n) || n < 1 || n > 12) return false;  // a list fails here too
    r.bymonth = static_cast<uint8_t>(n);
  } else if (equalsUpper(k, kn, "BYMONTHDAY")) {
    if (!number(v, vn, n) || n < 1 || n > 31) return false;
    r.bymonthday = static_cast<uint8_t>(n);
  } else {
    return false;  // BYSETPOS, BYHOUR, BYWEEKNO, BYYEARDAY, RSCALE, ...
  }
  return true;
}

// Offset of weekday w inside a week starting on wkst: 0..6.
int offsetIn(int w, int wkst) { return (w - wkst + 7) % 7; }

// Number of BYDAY days whose in-week offset is in [lo, hi).
int64_t daysInRange(uint8_t mask, int wkst, int lo, int hi) {
  int64_t c = 0;
  for (int o = lo; o < hi; o++) c += (mask >> ((o + wkst) % 7)) & 1;
  return c;
}

// WEEKLY: weeks begin on WKST; week k (k = 0 holds DTSTART) is active when k % INTERVAL == 0.
// The occurrence index of d (0 = DTSTART) is computed in closed form:
//   DTSTART itself, plus BYDAY days after DTSTART in week 0, plus m days for each full active
//   week between, plus BYDAY days before d in d's own week.
bool weekly(const Rule& r, int32_t d0, int32_t d) {
  if (d == d0) return true;
  uint8_t mask = r.byday ? r.byday : static_cast<uint8_t>(1 << weekday(d0));
  int wd = weekday(d);
  if (!((mask >> wd) & 1)) return false;
  int off0 = offsetIn(weekday(d0), r.wkst);
  int offd = offsetIn(wd, r.wkst);
  int64_t weeks = (static_cast<int64_t>(d - offd) - (d0 - off0)) / 7;  // week starts are 7 apart
  if (weeks % r.interval != 0) return false;
  if (r.count == 0) return true;
  int64_t n;
  if (weeks == 0) {
    n = 1 + daysInRange(mask, r.wkst, off0 + 1, offd);
  } else {
    int64_t m = daysInRange(mask, r.wkst, 0, 7);
    n = 1 + daysInRange(mask, r.wkst, off0 + 1, 7) + (weeks / r.interval - 1) * m +
        daysInRange(mask, r.wkst, 0, offd);
  }
  return n < r.count;
}

// YEARLY on DTSTART's month and day. Years without that date (Feb 29 outside leap years) have
// no occurrence and do not count towards COUNT (RFC 5545: invalid dates are ignored).
bool yearly(const Rule& r, int32_t d0, int32_t d) {
  int y0, m0, dd0, y, m, dd;
  civilFromDays(d0, y0, m0, dd0);
  civilFromDays(d, y, m, dd);
  if (m != m0 || dd != dd0 || (y - y0) % r.interval != 0) return false;
  if (r.count == 0) return true;
  int64_t k = (y - y0) / r.interval;  // candidate years before this one
  if (!(m0 == 2 && dd0 == 29)) return k < r.count;
  int64_t n = 0;  // leap years among y0, y0+i, ... before y; at most 10000 years / interval
  for (int yy = y0; yy < y && n < r.count; yy += r.interval) n += isLeap(yy);
  return n < r.count;
}

}  // namespace

Rule parseRule(const char* value) {
  Rule r;
  bool ok = true;
  const char* s = value;
  // Every part is read even after an unsupported one, so that UNTIL is known: an unsupported
  // series that ended long ago is not counted as skipped.
  while (*s) {
    const char* end = strchr(s, ';');
    size_t n = end ? static_cast<size_t>(end - s) : strlen(s);
    const char* eq = static_cast<const char*>(memchr(s, '=', n));
    if (!eq) ok = ok && n == 0;  // tolerate an empty part (";;" or a trailing ';')
    else {
      size_t kn = static_cast<size_t>(eq - s);
      if (!applyPart(r, s, kn, eq + 1, n - kn - 1)) ok = false;
    }
    s += n + (end ? 1 : 0);
  }
  if (!ok || r.freq == Freq::None) r.freq = Freq::Unsupported;
  if (r.byday && r.freq != Freq::Weekly) r.freq = Freq::Unsupported;
  if ((r.bymonth || r.bymonthday) && r.freq != Freq::Yearly) r.freq = Freq::Unsupported;
  return r;
}

bool supported(const Rule& r, int32_t d0) {
  if (r.freq != Freq::Daily && r.freq != Freq::Weekly && r.freq != Freq::Yearly) return false;
  int y, m, d;
  civilFromDays(d0, y, m, d);
  if (r.bymonth && r.bymonth != m) return false;
  if (r.bymonthday && r.bymonthday != d) return false;
  return true;
}

bool occursOn(const Rule& r, int32_t d0, int32_t d) {
  if (d < d0) return false;
  switch (r.freq) {
    case Freq::Daily: {
      int64_t diff = static_cast<int64_t>(d) - d0;
      if (diff % r.interval != 0) return false;
      return r.count == 0 || diff / r.interval < r.count;
    }
    case Freq::Weekly: return weekly(r, d0, d);
    case Freq::Yearly: return yearly(r, d0, d);
    default: return false;
  }
}

}  // namespace ics

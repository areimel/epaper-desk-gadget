#include "IcsTime.h"

#include <string.h>
#include <time.h>

namespace ics {
namespace {

// Reads exactly n decimal digits; false if any is not a digit.
bool digits(const char*& s, int n, int& out) {
  out = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    out = out * 10 + (s[i] - '0');
  }
  s += n;
  return true;
}

void toTm(int32_t day, int32_t secs, struct tm& t) {
  int y, m, d;
  civilFromDays(day, y, m, d);
  memset(&t, 0, sizeof t);
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = secs / 3600;
  t.tm_min = (secs / 60) % 60;
  t.tm_sec = secs % 60;
  t.tm_isdst = -1;  // let mktime decide whether DST applies on that wall-clock time
}

}  // namespace

// days_from_civil / civil_from_days: http://howardhinnant.github.io/date_algorithms.html
int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yoe = y - era * 400;                                   // [0, 399]
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
  return era * 146097 + doe - 719468;
}

void civilFromDays(int32_t z, int& y, int& m, int& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int32_t doe = z - era * 146097;
  const int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int32_t mp = (5 * doy + 2) / 153;
  d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  y = static_cast<int>(yoe + era * 400 + (m <= 2));
}

int weekday(int32_t day) { return static_cast<int>(((day + 4) % 7 + 7) % 7); }  // 1970-01-01: Thu

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const uint8_t len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (m == 2 && isLeap(y)) ? 29 : len[m - 1];
}

Time parseTime(const char* s, const char** next) {
  Time t;
  t.kind = Kind::Bad;
  int y, mo, d;
  const char* p = s;
  bool ok = digits(p, 4, y) && digits(p, 2, mo) && digits(p, 2, d) && mo >= 1 && mo <= 12 &&
            y >= 1 && d >= 1 && d <= daysInMonth(y, mo);
  Kind kind = Kind::Date;
  int hh = 0, mi = 0, ss = 0;
  if (ok && *p == 'T') {
    p++;
    ok = digits(p, 2, hh) && digits(p, 2, mi) && digits(p, 2, ss) && hh < 24 && mi < 60 &&
         ss <= 60;
    kind = Kind::Local;
    if (ok && *p == 'Z') {
      kind = Kind::Utc;
      p++;
    }
  }
  while (*p == ' ' || *p == '\t') p++;
  if (ok && *p != ',' && *p != '\0') ok = false;
  // Resynchronise on the next ',' so a malformed value in a list does not hide the rest.
  while (*p && *p != ',') p++;
  if (*p == ',') p++;
  if (next) *next = p;
  if (!ok) return t;
  t.kind = kind;
  t.day = daysFromCivil(y, mo, d);
  t.secs = hh * 3600 + mi * 60 + ss;
  return t;
}

Zone classifyZone(const char* tzid, const char* deviceTz) {
  if (!tzid || !*tzid) return Zone::Local;
  if (deviceTz && strcmp(tzid, deviceTz) == 0) return Zone::Local;
  static const char* const utc[] = {"UTC", "Etc/UTC", "GMT", "Etc/GMT"};
  for (const char* u : utc) {
    if (strcmp(tzid, u) == 0) return Zone::Utc;
  }
  return Zone::Foreign;
}

void applyZone(Time& t, Zone z) {
  if (t.kind != Kind::Local) return;
  if (z == Zone::Utc) t.kind = Kind::Utc;
  else if (z == Zone::Foreign) t.kind = Kind::Foreign;
}

int64_t utcInstant(int32_t day, int32_t secs) {
  return static_cast<int64_t>(day) * SECS_PER_DAY + secs;
}

int64_t localInstant(int32_t day, int32_t secs) {
  struct tm t;
  toTm(day, secs, t);
  return static_cast<int64_t>(mktime(&t));
}

int64_t instantOf(const Time& t) {
  switch (t.kind) {
    case Kind::Local: return localInstant(t.day, t.secs);
    case Kind::Utc: return utcInstant(t.day, t.secs);
    case Kind::Date: return localInstant(t.day, 0);
    default: return NO_INSTANT;
  }
}

int32_t localDay(int64_t inst, int* minutes) {
  time_t tt = static_cast<time_t>(inst);
  struct tm t;
  if (!localtime_r(&tt, &t)) {
    if (minutes) *minutes = 0;
    return 0;
  }
  if (minutes) *minutes = t.tm_hour * 60 + t.tm_min;
  return daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

bool parseDuration(const char* s, Duration& d) {
  d = Duration();
  bool neg = false;
  if (*s == '+' || *s == '-') neg = *s++ == '-';
  if (*s++ != 'P') return false;
  // Units must appear at most once and in order W, D, then after T: H, M, S. Each field has at
  // most 7 digits, so with each unit once the totals fit easily; days are also capped.
  static const char units[] = "WDHMS";
  int lastRank = -1;
  bool inTime = false, timeAny = false;
  int64_t days = 0, secs = 0;
  while (*s) {
    if (*s == 'T') {
      if (inTime || lastRank > 1) return false;
      inTime = true;
      s++;
      continue;
    }
    int64_t n = 0;
    int nd = 0;
    while (*s >= '0' && *s <= '9') {
      if (++nd > 7) return false;
      n = n * 10 + (*s++ - '0');
    }
    if (nd == 0 || !*s) return false;
    const char* u = strchr(units, *s++);
    if (!u) return false;
    int rank = static_cast<int>(u - units);
    if (rank <= lastRank || (rank >= 2) != inTime) return false;  // repeated, out of order, or T
    lastRank = rank;
    if (rank == 0) days += n * 7;
    else if (rank == 1) days += n;
    else if (rank == 2) secs += n * 3600;
    else if (rank == 3) secs += n * 60;
    else secs += n;
    if (inTime) timeAny = true;
  }
  if (lastRank < 0 || (inTime && !timeAny)) return false;
  if (days + secs / SECS_PER_DAY > DURATION_DAYS_MAX) return false;  // checked before the cast
  if (!neg) {
    d.days = static_cast<int32_t>(days);
    d.secs = secs;
  }
  return true;
}

int64_t addDuration(int64_t start, bool wallClock, const Duration& d) {
  if (d.days == 0 || !wallClock) return start + static_cast<int64_t>(d.days) * SECS_PER_DAY + d.secs;
  time_t tt = static_cast<time_t>(start);
  struct tm t;
  if (!localtime_r(&tt, &t)) return start + d.secs;
  // Same wall-clock time d.days later, then the exact part.
  int32_t day = daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) + d.days;
  return localInstant(day, t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec) + d.secs;
}

}  // namespace ics

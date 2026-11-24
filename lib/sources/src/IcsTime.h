#pragma once
#include <stddef.h>
#include <stdint.h>

// iCalendar DATE / DATE-TIME / DURATION values and civil-day arithmetic. Pure: uses only libc
// localtime_r/mktime for the process zone (no timegm, which ESP32 newlib may lack).
namespace ics {

constexpr int64_t NO_INSTANT = INT64_MIN;
constexpr int32_t SECS_PER_DAY = 86400;
constexpr int32_t DURATION_DAYS_MAX = 1000000;  // longer DURATIONs are rejected as malformed

// Proleptic Gregorian civil date <-> days since 1970-01-01 (H. Hinnant's algorithms).
int32_t daysFromCivil(int y, int m, int d);
void civilFromDays(int32_t z, int& y, int& m, int& d);
int weekday(int32_t day);  // 0 = Sunday
bool isLeap(int y);
int daysInMonth(int y, int m);

enum class Kind : uint8_t {
  None,     // property absent
  Date,     // VALUE=DATE: a civil day (all-day)
  Local,    // floating time or TZID equal to the device zone: device wall clock
  Utc,      // ...Z (or a TZID that is plainly UTC)
  Foreign,  // TZID of another zone: never guessed
  Bad,      // present but malformed or truncated
};

// As written: the civil day and the seconds after midnight of the value (0 for a Date).
struct Time {
  Kind kind = Kind::None;
  int32_t day = 0;
  int32_t secs = 0;
};

// Parses one value at s: "YYYYMMDD", "YYYYMMDDTHHMMSS" or "YYYYMMDDTHHMMSSZ", ending at ',' or
// NUL. Kind is Date, Local or Utc (Bad if malformed). *next is set past the value and its ','.
Time parseTime(const char* s, const char** next);

enum class Zone : uint8_t { Local, Utc, Foreign };
// No TZID (null or empty) or exactly the device zone -> Local; "UTC", "Etc/UTC", "GMT",
// "Etc/GMT" -> Utc; anything else -> Foreign.
Zone classifyZone(const char* tzid, const char* deviceTz);
// Applies a TZID class to a parsed value: only Local values change (Z and dates ignore TZID).
void applyZone(Time& t, Zone z);

int64_t utcInstant(int32_t day, int32_t secs);
int64_t localInstant(int32_t day, int32_t secs);  // device wall clock -> instant (mktime)
// Instant of a Local, Utc or Date (local midnight) value; NO_INSTANT otherwise.
int64_t instantOf(const Time& t);
// Local civil day of an instant; *minutes (optional) = wall-clock minutes after midnight.
int32_t localDay(int64_t inst, int* minutes);

// RFC 5545 DURATION: weeks and days are nominal (added on the wall clock), H/M/S are exact.
struct Duration {
  int32_t days = 0;
  int64_t secs = 0;
};
// False if malformed (a unit repeated or out of order, a field over 7 digits, or longer than
// DURATION_DAYS_MAX). A negative duration parses as zero (an event cannot end before it starts).
bool parseDuration(const char* s, Duration& d);
// start + d: wallClock adds days on the device wall clock, otherwise (UTC) as 86400 s each.
int64_t addDuration(int64_t start, bool wallClock, const Duration& d);

}  // namespace ics

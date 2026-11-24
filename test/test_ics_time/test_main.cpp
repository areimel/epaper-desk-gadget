#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

#include "IcsTime.h"

using ics::Kind;

void setUp() {}
void tearDown() {}

// Epoch values below were computed independently with `date -u -d ... +%s` and
// `TZ=America/New_York date -d ... +%s`.
void test_civil_days() {
  TEST_ASSERT_EQUAL(0, ics::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL(-1, ics::daysFromCivil(1969, 12, 31));
  TEST_ASSERT_EQUAL(11017, ics::daysFromCivil(2000, 3, 1));
  TEST_ASSERT_EQUAL(20780, ics::daysFromCivil(2026, 11, 23));
  int y, m, d;
  ics::civilFromDays(20780, y, m, d);
  TEST_ASSERT_EQUAL(2026, y);
  TEST_ASSERT_EQUAL(11, m);
  TEST_ASSERT_EQUAL(23, d);
  for (int32_t z = -800000; z < 800000; z += 997) {  // round trip over ~4400 years
    ics::civilFromDays(z, y, m, d);
    TEST_ASSERT_EQUAL(z, ics::daysFromCivil(y, m, d));
  }
}

void test_weekday_and_leap() {
  TEST_ASSERT_EQUAL(1, ics::weekday(ics::daysFromCivil(2026, 11, 23)));  // Monday
  TEST_ASSERT_EQUAL(0, ics::weekday(ics::daysFromCivil(2026, 3, 8)));    // Sunday
  TEST_ASSERT_EQUAL(4, ics::weekday(ics::daysFromCivil(2024, 2, 29)));   // Thursday
  TEST_ASSERT_EQUAL(3, ics::weekday(ics::daysFromCivil(1969, 12, 31)));  // Wednesday
  TEST_ASSERT_TRUE(ics::isLeap(2028));
  TEST_ASSERT_FALSE(ics::isLeap(2026));
  TEST_ASSERT_FALSE(ics::isLeap(2100));
  TEST_ASSERT_TRUE(ics::isLeap(2000));
  TEST_ASSERT_EQUAL(28, ics::daysInMonth(2026, 2));
  TEST_ASSERT_EQUAL(29, ics::daysInMonth(2028, 2));
}

void test_parse_forms() {
  const char* next;
  ics::Time t = ics::parseTime("20261123", &next);
  TEST_ASSERT_EQUAL(Kind::Date, t.kind);
  TEST_ASSERT_EQUAL(20780, t.day);
  t = ics::parseTime("20261123T093015", &next);
  TEST_ASSERT_EQUAL(Kind::Local, t.kind);
  TEST_ASSERT_EQUAL(9 * 3600 + 30 * 60 + 15, t.secs);
  t = ics::parseTime("20261123T140000Z,20261124T140000Z", &next);
  TEST_ASSERT_EQUAL(Kind::Utc, t.kind);
  TEST_ASSERT_EQUAL(14 * 3600, t.secs);
  TEST_ASSERT_EQUAL_STRING("20261124T140000Z", next);
  t = ics::parseTime(next, &next);
  TEST_ASSERT_EQUAL(20781, t.day);
  TEST_ASSERT_EQUAL_STRING("", next);
}

void test_parse_rejects_malformed() {
  const char* bad[] = {"", "2026112", "20261323", "20260230", "20261123T2500",
                       "20261123T250000", "20261123T096000", "20261123X", "2026-11-23",
                       "20261123T090000Q", "\xff\xfe"};
  const char* next;
  for (const char* s : bad) TEST_ASSERT_EQUAL_MESSAGE(Kind::Bad, ics::parseTime(s, &next).kind, s);
}

void test_zones() {
  const char* dev = "America/New_York";
  TEST_ASSERT_EQUAL(ics::Zone::Local, ics::classifyZone(nullptr, dev));
  TEST_ASSERT_EQUAL(ics::Zone::Local, ics::classifyZone("", dev));
  TEST_ASSERT_EQUAL(ics::Zone::Local, ics::classifyZone("America/New_York", dev));
  TEST_ASSERT_EQUAL(ics::Zone::Foreign, ics::classifyZone("America/new_york", dev));
  TEST_ASSERT_EQUAL(ics::Zone::Foreign, ics::classifyZone("Europe/Berlin", dev));
  TEST_ASSERT_EQUAL(ics::Zone::Utc, ics::classifyZone("UTC", dev));
  TEST_ASSERT_EQUAL(ics::Zone::Utc, ics::classifyZone("Etc/UTC", dev));
  const char* next;
  ics::Time t = ics::parseTime("20261123T090000", &next);
  ics::applyZone(t, ics::Zone::Foreign);
  TEST_ASSERT_EQUAL(Kind::Foreign, t.kind);
  t = ics::parseTime("20261123T090000Z", &next);
  ics::applyZone(t, ics::Zone::Foreign);  // Z wins over a TZID
  TEST_ASSERT_EQUAL(Kind::Utc, t.kind);
  t = ics::parseTime("20261123", &next);
  ics::applyZone(t, ics::Zone::Foreign);  // a date has no zone
  TEST_ASSERT_EQUAL(Kind::Date, t.kind);
}

void test_instants_new_york() {
  setenv("TZ", "America/New_York", 1);
  tzset();
  TEST_ASSERT_EQUAL_INT64(1795442400, ics::localInstant(ics::daysFromCivil(2026, 11, 23), 9 * 3600));
  TEST_ASSERT_EQUAL_INT64(1772974800, ics::localInstant(ics::daysFromCivil(2026, 3, 8), 9 * 3600));
  TEST_ASSERT_EQUAL_INT64(1772946000, ics::localInstant(ics::daysFromCivil(2026, 3, 8), 0));
  TEST_ASSERT_EQUAL_INT64(1773028800, ics::localInstant(ics::daysFromCivil(2026, 3, 9), 0));
  TEST_ASSERT_EQUAL_INT64(1793505600, ics::localInstant(ics::daysFromCivil(2026, 11, 1), 0));
  TEST_ASSERT_EQUAL_INT64(1793595600, ics::localInstant(ics::daysFromCivil(2026, 11, 2), 0));
  TEST_ASSERT_EQUAL_INT64(1795392000, ics::utcInstant(20780, 0));
  int minutes = -5;
  TEST_ASSERT_EQUAL(ics::daysFromCivil(2026, 3, 8), ics::localDay(1772974800, &minutes));
  TEST_ASSERT_EQUAL(540, minutes);
  // 2026-11-01 05:30Z and 06:30Z are both 01:30 local (EDT, then EST).
  TEST_ASSERT_EQUAL(ics::daysFromCivil(2026, 11, 1), ics::localDay(1793491200 + 5 * 3600 + 1800, &minutes));
  TEST_ASSERT_EQUAL(90, minutes);
  ics::localDay(1793491200 + 6 * 3600 + 1800, &minutes);
  TEST_ASSERT_EQUAL(90, minutes);
  const char* next;
  TEST_ASSERT_EQUAL_INT64(1795410000, ics::instantOf(ics::parseTime("20261123", &next)));
  TEST_ASSERT_EQUAL_INT64(1795442400, ics::instantOf(ics::parseTime("20261123T140000Z", &next)));
  ics::Time f = ics::parseTime("20261123T090000", &next);
  ics::applyZone(f, ics::Zone::Foreign);
  TEST_ASSERT_EQUAL_INT64(ics::NO_INSTANT, ics::instantOf(f));
}

void test_durations() {
  ics::Duration d;
  TEST_ASSERT_TRUE(ics::parseDuration("PT1H30M", d));
  TEST_ASSERT_EQUAL(0, d.days);
  TEST_ASSERT_EQUAL_INT64(5400, d.secs);
  TEST_ASSERT_TRUE(ics::parseDuration("P1D", d));
  TEST_ASSERT_EQUAL(1, d.days);
  TEST_ASSERT_EQUAL_INT64(0, d.secs);
  TEST_ASSERT_TRUE(ics::parseDuration("P2W", d));
  TEST_ASSERT_EQUAL(14, d.days);
  TEST_ASSERT_TRUE(ics::parseDuration("+P1DT2H3M4S", d));
  TEST_ASSERT_EQUAL(1, d.days);
  TEST_ASSERT_EQUAL_INT64(2 * 3600 + 3 * 60 + 4, d.secs);
  TEST_ASSERT_TRUE(ics::parseDuration("PT45S", d));
  TEST_ASSERT_EQUAL_INT64(45, d.secs);
  TEST_ASSERT_TRUE(ics::parseDuration("-PT15M", d));  // negative: clamped to zero
  TEST_ASSERT_EQUAL(0, d.days);
  TEST_ASSERT_EQUAL_INT64(0, d.secs);
  const char* bad[] = {"", "P", "PT", "1H", "PT1X", "P1H", "PTH", "P1DT", "P99999999999D",
                       "P1W1W",              // repeated unit
                       "P9999999W9999999W",  // repeated unit that would overflow the total
                       "PT1M1H",             // out of order
                       "P1DT1H1H", "P1D1W", "PT1D", "P1S",
                       "P200000W",            // 1.4 million days: over the limit
                       "P999999DT9999999H"};  // each field fits, the total is too long
  for (const char* s : bad) TEST_ASSERT_FALSE_MESSAGE(ics::parseDuration(s, d), s);
  // A hostile long string: thousands of repeated fields.
  static char hostile[8192];
  hostile[0] = 'P';
  for (size_t i = 1; i + 9 < sizeof hostile; i += 8) memcpy(hostile + i, "9999999W", 8);
  hostile[sizeof hostile - 1] = '\0';
  TEST_ASSERT_FALSE(ics::parseDuration(hostile, d));
  TEST_ASSERT_TRUE(ics::parseDuration("P1W2DT3H4M5S", d));  // ordered week + day is accepted
  TEST_ASSERT_EQUAL(9, d.days);
  TEST_ASSERT_TRUE(ics::parseDuration("P142857W", d));  // 999999 days: at the limit
  TEST_ASSERT_EQUAL(999999, d.days);
}

void test_add_duration_follows_wall_clock() {
  setenv("TZ", "America/New_York", 1);
  tzset();
  ics::Duration d;
  ics::parseDuration("P1D", d);
  // 2026-03-07 09:00 + P1D = 2026-03-08 09:00 local (23 h later, across the spring change).
  TEST_ASSERT_EQUAL_INT64(1772974800, ics::addDuration(1772892000, true, d));
  TEST_ASSERT_EQUAL_INT64(1772892000 + 86400, ics::addDuration(1772892000, false, d));
  ics::parseDuration("PT24H", d);  // exact: 24 h
  TEST_ASSERT_EQUAL_INT64(1772892000 + 86400, ics::addDuration(1772892000, true, d));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_civil_days);
  RUN_TEST(test_weekday_and_leap);
  RUN_TEST(test_parse_forms);
  RUN_TEST(test_parse_rejects_malformed);
  RUN_TEST(test_zones);
  RUN_TEST(test_instants_new_york);
  RUN_TEST(test_durations);
  RUN_TEST(test_add_duration_follows_wall_clock);
  return UNITY_END();
}

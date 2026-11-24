#include <stdio.h>
#include <unity.h>

#include "IcsRule.h"

using ics::Freq;

void setUp() {}
void tearDown() {}

static int32_t D(int y, int m, int d) { return ics::daysFromCivil(y, m, d); }

static bool occurs(const char* rule, int32_t d0, int32_t d) {
  ics::Rule r = ics::parseRule(rule);
  TEST_ASSERT_TRUE_MESSAGE(ics::supported(r, d0), rule);
  return ics::occursOn(r, d0, d);
}

void test_parse_supported() {
  ics::Rule r = ics::parseRule("FREQ=WEEKLY;WKST=SU;INTERVAL=2;BYDAY=MO,WE,FR;COUNT=10");
  TEST_ASSERT_EQUAL(Freq::Weekly, r.freq);
  TEST_ASSERT_EQUAL(2, r.interval);
  TEST_ASSERT_EQUAL(10, r.count);
  TEST_ASSERT_EQUAL(0, r.wkst);
  TEST_ASSERT_EQUAL((1 << 1) | (1 << 3) | (1 << 5), r.byday);
  r = ics::parseRule("FREQ=DAILY;UNTIL=20261123T235959Z");
  TEST_ASSERT_EQUAL(Freq::Daily, r.freq);
  TEST_ASSERT_EQUAL(ics::Kind::Utc, r.until.kind);
  TEST_ASSERT_EQUAL(D(2026, 11, 23), r.until.day);
  r = ics::parseRule("FREQ=YEARLY;UNTIL=20301123");
  TEST_ASSERT_EQUAL(Freq::Yearly, r.freq);
  TEST_ASSERT_EQUAL(ics::Kind::Date, r.until.kind);
  r = ics::parseRule("freq=yearly;bymonth=11;bymonthday=23");
  TEST_ASSERT_EQUAL(Freq::Yearly, r.freq);
  TEST_ASSERT_TRUE(ics::supported(r, D(1990, 11, 23)));
  TEST_ASSERT_FALSE(ics::supported(r, D(1990, 12, 23)));
  TEST_ASSERT_FALSE(ics::supported(r, D(1990, 11, 24)));
}

void test_parse_unsupported() {
  const char* rules[] = {
      "FREQ=MONTHLY;BYMONTHDAY=23",  "FREQ=HOURLY",       "FREQ=WEEKLY;BYDAY=2MO",
      "FREQ=WEEKLY;BYDAY=-1FR",      "FREQ=YEARLY;BYMONTH=1,2", "FREQ=WEEKLY;BYSETPOS=1;BYDAY=MO",
      "FREQ=WEEKLY;BYMONTHDAY=3",    "FREQ=DAILY;BYDAY=MO,TU",  "FREQ=DAILY;INTERVAL=0",
      "FREQ=DAILY;COUNT=0",          "FREQ=DAILY;UNTIL=2026",   "FREQ=DAILY;BYHOUR=9",
      "INTERVAL=2",                  "",                        "FREQ=WEEKLY;BYDAY=XX",
      "FREQ=DAILY;FREQ=WEEKLY",      "garbage\xff",             "FREQ=YEARLY;BYDAY=MO",
      // Numeric overflow guards: over-long or over-limit INTERVAL / COUNT are rejected.
      "FREQ=YEARLY;BYMONTHDAY=23", "FREQ=YEARLY;BYMONTHDAY=23;COUNT=2",
      "FREQ=DAILY;INTERVAL=4294967297", "FREQ=DAILY;INTERVAL=10001", "FREQ=DAILY;COUNT=1000001",
      "FREQ=WEEKLY;COUNT=99999999999999999999", "FREQ=DAILY;INTERVAL=-1",
  };
  for (const char* s : rules) TEST_ASSERT_EQUAL_MESSAGE(Freq::Unsupported, ics::parseRule(s).freq, s);
}

void test_unsupported_rule_keeps_until() {
  // So that an unsupported series that ended long ago is not counted as skipped.
  ics::Rule r = ics::parseRule("FREQ=MONTHLY;UNTIL=20250101T000000Z;BYMONTHDAY=1");
  TEST_ASSERT_EQUAL(Freq::Unsupported, r.freq);
  TEST_ASSERT_EQUAL(ics::Kind::Utc, r.until.kind);
  TEST_ASSERT_EQUAL(D(2025, 1, 1), r.until.day);
  r = ics::parseRule("FREQ=DAILY;UNTIL=garbage");
  TEST_ASSERT_EQUAL(Freq::Unsupported, r.freq);
  TEST_ASSERT_EQUAL(ics::Kind::None, r.until.kind);
}

void test_daily_interval_and_count() {
  int32_t d0 = D(2026, 11, 1);
  TEST_ASSERT_TRUE(occurs("FREQ=DAILY;INTERVAL=3", d0, D(2026, 11, 22)));
  TEST_ASSERT_FALSE(occurs("FREQ=DAILY;INTERVAL=3", d0, D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs("FREQ=DAILY", d0, D(2026, 10, 31)));
  // COUNT=5 from 11-18: the 18th..22nd, exhausted the day before the 23rd; COUNT=6 ends today.
  TEST_ASSERT_FALSE(occurs("FREQ=DAILY;COUNT=5", D(2026, 11, 18), D(2026, 11, 23)));
  TEST_ASSERT_TRUE(occurs("FREQ=DAILY;COUNT=6", D(2026, 11, 18), D(2026, 11, 23)));
}

void test_weekly_interval_byday() {
  int32_t d0 = D(2026, 11, 9);  // Monday; weeks of 11-09 and 11-23 are active, 11-16 is not
  const char* r = "FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE,FR";
  TEST_ASSERT_TRUE(occurs(r, d0, D(2026, 11, 13)));
  TEST_ASSERT_FALSE(occurs(r, d0, D(2026, 11, 16)));
  TEST_ASSERT_FALSE(occurs(r, d0, D(2026, 11, 18)));
  TEST_ASSERT_TRUE(occurs(r, d0, D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs(r, d0, D(2026, 11, 24)));
  // No BYDAY: DTSTART's weekday.
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY", d0, D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs("FREQ=WEEKLY", d0, D(2026, 11, 25)));
}

void test_weekly_wkst_matters_with_interval() {
  // RFC 5545 style: DTSTART Sunday 11-08, every 2 weeks on SU,MO.
  int32_t d0 = D(2026, 11, 8);
  TEST_ASSERT_FALSE(occurs("FREQ=WEEKLY;INTERVAL=2;BYDAY=SU,MO", d0, D(2026, 11, 23)));
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;INTERVAL=2;BYDAY=SU,MO;WKST=SU", d0, D(2026, 11, 23)));
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;INTERVAL=2;BYDAY=SU,MO", d0, D(2026, 11, 16)));
}

void test_weekly_count() {
  int32_t d0 = D(2026, 11, 2);  // Monday: 02 04 06 09 11 13 16 18 20 | 23
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;BYDAY=MO,WE,FR;COUNT=9", d0, D(2026, 11, 20)));
  TEST_ASSERT_FALSE(occurs("FREQ=WEEKLY;BYDAY=MO,WE,FR;COUNT=9", d0, D(2026, 11, 23)));
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;BYDAY=MO,WE,FR;COUNT=10", d0, D(2026, 11, 23)));
  // DTSTART Tuesday, not in BYDAY, still counts: 11-03, 11-04, 11-09 | 11-11.
  int32_t t0 = D(2026, 11, 3);
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;BYDAY=MO,WE;COUNT=3", t0, D(2026, 11, 3)));
  TEST_ASSERT_TRUE(occurs("FREQ=WEEKLY;BYDAY=MO,WE;COUNT=3", t0, D(2026, 11, 9)));
  TEST_ASSERT_FALSE(occurs("FREQ=WEEKLY;BYDAY=MO,WE;COUNT=3", t0, D(2026, 11, 11)));
}

// Naive reference: walks every day from d0, counting week boundaries at WKST.
static bool bruteWeekly(int32_t d0, int32_t d, uint8_t mask, int wkst, int interval, uint32_t count) {
  uint32_t n = 0;
  int week = 0;
  for (int32_t x = d0; x <= d; x++) {
    if (x > d0 && ics::weekday(x) == wkst) week++;
    bool occ = x == d0 || ((mask >> ics::weekday(x) & 1) && week % interval == 0);
    if (!occ) continue;
    if (count && n >= count) return false;
    if (x == d) return true;
    n++;
  }
  return false;
}

void test_weekly_count_matches_brute_force() {
  static const char* names[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
  uint32_t seed = 12345;
  auto rnd = [&seed]() { return (seed = seed * 1103515245u + 12345u) >> 8; };
  for (int i = 0; i < 400; i++) {
    uint8_t mask = static_cast<uint8_t>(rnd() % 127 + 1);
    int wkst = static_cast<int>(rnd() % 7);
    int interval = static_cast<int>(rnd() % 4 + 1);
    uint32_t count = rnd() % 300 + 1;
    int32_t d0 = D(2000, 1, 1) + static_cast<int32_t>(rnd() % 3000);
    char rule[96];
    int n = snprintf(rule, sizeof rule, "FREQ=WEEKLY;INTERVAL=%d;WKST=%s;COUNT=%u;BYDAY=",
                     interval, names[wkst], static_cast<unsigned>(count));
    bool first = true;
    for (int w = 0; w < 7; w++) {
      if (mask >> w & 1) {
        n += snprintf(rule + n, sizeof rule - n, "%s%s", first ? "" : ",", names[w]);
        first = false;
      }
    }
    ics::Rule r = ics::parseRule(rule);
    TEST_ASSERT_EQUAL_MESSAGE(Freq::Weekly, r.freq, rule);
    for (int32_t d = d0 - 3; d < d0 + 3 * 7 * 4 * 3; d += static_cast<int32_t>(rnd() % 5 + 1)) {
      TEST_ASSERT_EQUAL_MESSAGE(bruteWeekly(d0, d, mask, wkst, interval, count),
                                ics::occursOn(r, d0, d), rule);
    }
  }
}

void test_limits_do_not_overflow() {
  // Largest accepted values, far from DTSTART: the arithmetic stays in range.
  int32_t d0 = ics::daysFromCivil(1, 1, 1);
  TEST_ASSERT_FALSE(occurs("FREQ=WEEKLY;INTERVAL=10000;COUNT=1000000;BYDAY=MO,TU,WE,TH,FR,SA,SU",
                           d0, D(9999, 12, 31)));
  TEST_ASSERT_TRUE(occurs("FREQ=DAILY;COUNT=1000000", d0, d0 + 999999));
  TEST_ASSERT_FALSE(occurs("FREQ=DAILY;COUNT=1000000", d0, d0 + 1000000));
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;COUNT=1000000", D(1, 2, 28), D(9999, 2, 28)));
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;COUNT=1000000", D(4, 2, 29), D(9996, 2, 29)));
}

void test_yearly() {
  int32_t b = D(1990, 11, 23);
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY", b, D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY", b, D(2026, 11, 24)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY", b, D(1989, 11, 23)));
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;INTERVAL=2", b, D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY;INTERVAL=2", D(1991, 11, 23), D(2026, 11, 23)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY;COUNT=36", b, D(2026, 11, 23)));  // 1990..2025
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;COUNT=37", b, D(2026, 11, 23)));
}

void test_yearly_feb_29() {
  int32_t l = D(2024, 2, 29);
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY", l, D(2027, 2, 28)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY", l, D(2027, 3, 1)));
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY", l, D(2028, 2, 29)));
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;COUNT=2", l, D(2028, 2, 29)));  // 2024, 2028
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY;COUNT=2", l, D(2032, 2, 29)));
  // Every 3 years from 2024: 2027 and 2030 have no Feb 29 and do not count; 2036 is the 2nd.
  TEST_ASSERT_TRUE(occurs("FREQ=YEARLY;INTERVAL=3;COUNT=2", l, D(2036, 2, 29)));
  TEST_ASSERT_FALSE(occurs("FREQ=YEARLY;INTERVAL=3;COUNT=2", l, D(2048, 2, 29)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_supported);
  RUN_TEST(test_parse_unsupported);
  RUN_TEST(test_unsupported_rule_keeps_until);
  RUN_TEST(test_daily_interval_and_count);
  RUN_TEST(test_weekly_interval_byday);
  RUN_TEST(test_weekly_wkst_matters_with_interval);
  RUN_TEST(test_weekly_count);
  RUN_TEST(test_weekly_count_matches_brute_force);
  RUN_TEST(test_limits_do_not_overflow);
  RUN_TEST(test_yearly);
  RUN_TEST(test_yearly_feb_29);
  return UNITY_END();
}

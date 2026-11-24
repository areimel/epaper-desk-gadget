// Edge cases from an independent review: YEARLY;BYMONTHDAY, series ended by UNTIL whose last
// occurrence still covers today, overrides without a usable DTSTART, long-span series, broken
// structure, all-day events with a DATE-TIME end, empty TZID and finish() twice.
// Monday 2026-11-23 in America/New_York (UTC-5) unless stated.
#include <unity.h>

#include <initializer_list>

#include "../support/IcsFeed.h"

using namespace icsfeed;

static const char* NY = "America/New_York";

void setUp() { setZone(NY); }
void tearDown() {}

static Result feed(const char* body, time_t now = 0) {
  static char buf[8192];
  snprintf(buf, sizeof buf, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n%s", body);
  return parseAllWays(reinterpret_cast<const uint8_t*>(buf), strlen(buf),
                      now ? now : at(2026, 11, 23), NY);
}

void test_yearly_bymonthday_is_unsupported() {
  // RFC 5545: BYMONTHDAY under YEARLY expands to every month (11-23, then 12-23).
  const char* withCount =
      "BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261123\r\nRRULE:FREQ=YEARLY;BYMONTHDAY=23;COUNT=2\r\n"
      "SUMMARY:Monthly really\r\nEND:VEVENT\r\n";
  const char* without =
      "BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261123\r\nRRULE:FREQ=YEARLY;BYMONTHDAY=23\r\n"
      "SUMMARY:Monthly really\r\nEND:VEVENT\r\n";
  for (const char* body : {withCount, without}) {
    for (time_t now : {at(2026, 11, 23), at(2027, 11, 23)}) {
      Result r = feed(body, now);
      TEST_ASSERT_EQUAL(0, r.snap.count);
      TEST_ASSERT_EQUAL(1, r.snap.skipped);
    }
  }
  // BYMONTH + BYMONTHDAY repeating DTSTART stays supported.
  Result r = feed("BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20101123\r\n"
                 "RRULE:FREQ=YEARLY;BYMONTH=11;BYMONTHDAY=23\r\nSUMMARY:Anniversary\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(1, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
}

void test_until_on_last_start_of_multi_day_series() {
  // Saturday 3-day all-day series; last start 11-21 covers 21, 22, 23.
  Result r = feed("BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261107\r\nDTEND;VALUE=DATE:20261110\r\n"
                 "RRULE:FREQ=WEEKLY;UNTIL=20261121\r\nSUMMARY:Long weekend\r\nEND:VEVENT\r\n"
                 // Fri 18:00 - Mon 09:00 timed series; last start Fri 11-20.
                 "BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:20261106T180000\r\n"
                 "DTEND;TZID=America/New_York:20261109T090000\r\n"
                 "RRULE:FREQ=WEEKLY;UNTIL=20261120T235959Z\r\nSUMMARY:On call\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(2, r.snap.count);
  expectEvent(r.snap, 0, "Long weekend", -1, 1440);
  expectEvent(r.snap, 1, "On call", -1, 540);
  r = feed("BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261107\r\nDTEND;VALUE=DATE:20261110\r\n"
          "RRULE:FREQ=WEEKLY;UNTIL=20261121\r\nSUMMARY:Long weekend\r\nEND:VEVENT\r\n",
          at(2026, 11, 24));
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  // A far-future UNTIL (not converted with mktime) does not end the series.
  r = feed("BEGIN:VEVENT\r\nDTSTART:20261101T150000Z\r\nRRULE:FREQ=DAILY;UNTIL=99991231T000000Z\r\n"
           "SUMMARY:Forever\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "Forever", 600, 600);
}

void test_override_without_usable_dtstart() {
  Result r = feed(
      "BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:20261101T100000\r\nRRULE:FREQ=DAILY\r\n"
      "UID:a@google.com\r\nSUMMARY:Series A\r\nEND:VEVENT\r\n"
      "BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:20261101T110000\r\nRRULE:FREQ=DAILY\r\n"
      "UID:b@google.com\r\nSUMMARY:Series B\r\nEND:VEVENT\r\n"
      // A: override without DTSTART (not cancelled): original suppressed, counted.
      "BEGIN:VEVENT\r\nRECURRENCE-ID;TZID=America/New_York:20261123T100000\r\n"
      "UID:a@google.com\r\nSUMMARY:A moved somewhere\r\nEND:VEVENT\r\n"
      // B: cancelled override with a malformed DTSTART: suppressed, not counted.
      "BEGIN:VEVENT\r\nRECURRENCE-ID;TZID=America/New_York:20261123T110000\r\n"
      "DTSTART:2026112\r\nSTATUS:CANCELLED\r\nUID:b@google.com\r\nSUMMARY:B\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(1, r.snap.skipped);
}

void test_long_span_series_counted_only_when_covering_today() {
  const char* body = "BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20260701\r\nDTEND;VALUE=DATE:20260711\r\n"
                     "RRULE:FREQ=YEARLY\r\nSUMMARY:Summer camp\r\nEND:VEVENT\r\n";
  Result r = feed(body);
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  r = feed(body, at(2027, 7, 5));
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(1, r.snap.skipped);
}

void test_broken_structure_counted_when_today() {
  // A second BEGIN:VEVENT discards an unterminated event: counted if it was for today.
  Result r = feed("BEGIN:VEVENT\r\nDTSTART:20261123T150000Z\r\nSUMMARY:Lost\r\n"
                 "BEGIN:VEVENT\r\nDTSTART:20101123T150000Z\r\nSUMMARY:Old lost\r\n"
                 "BEGIN:VEVENT\r\nDTSTART:20261123T160000Z\r\nSUMMARY:Kept\r\nEND:VEVENT\r\n"
                 "BEGIN:VEVENT\r\nDTSTART:20261123T170000Z\r\nSUMMARY:Cut by END\r\nEND:VCALENDAR\r\n"
                 "BEGIN:VEVENT\r\nDTSTART:20261123T180000Z\r\nSUMMARY:Cut by EOF\r\n");
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "Kept", 660, 660);
  TEST_ASSERT_EQUAL(3, r.snap.skipped);  // Lost, Cut by END, Cut by EOF; not Old lost
  // No DTSTART at all: cannot be dated, so not counted (documented limitation).
  r = feed("BEGIN:VEVENT\r\nSUMMARY:Undated\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
}

void test_all_day_with_date_time_end() {
  const char* body = "BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261120\r\nDTEND:20261125T000000\r\n"
                     "SUMMARY:Mixed\r\nEND:VEVENT\r\n";
  TEST_ASSERT_EQUAL(1, feed(body).snap.count);
  TEST_ASSERT_EQUAL(1, feed(body, at(2026, 11, 24)).snap.count);
  TEST_ASSERT_EQUAL(0, feed(body, at(2026, 11, 25)).snap.count);
}

void test_empty_tzid_is_foreign() {
  Result r = feed("BEGIN:VEVENT\r\nDTSTART;TZID=:20261123T100000\r\nSUMMARY:Empty zone\r\nEND:VEVENT\r\n");
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(1, r.snap.skipped);
}

void test_finish_twice_is_idempotent() {
  const char* s = "BEGIN:VCALENDAR\r\n"
                  "BEGIN:VEVENT\r\nDTSTART:20261101T150000Z\r\nRRULE:FREQ=DAILY\r\nUID:tf@x\r\n"
                  "SUMMARY:Daily\r\nEND:VEVENT\r\n"
                  "BEGIN:VEVENT\r\nRECURRENCE-ID;RANGE=THISANDFUTURE:20261116T150000Z\r\n"
                  "DTSTART:20261116T160000Z\r\nUID:tf@x\r\nSUMMARY:Later\r\nEND:VEVENT\r\n"
                  "BEGIN:VEVENT\r\nDTSTART:20261123T150000Z\r\nSUMMARY:One\r\nEND:VEVENT\r\n";
  alignas(IcsParser) static unsigned char storage[sizeof(IcsParser)];
  IcsParser* p = new (storage) IcsParser(at(2026, 11, 23), NY);
  p->write(reinterpret_cast<const uint8_t*>(s), strlen(s));
  CalSnap a, b;
  TEST_ASSERT_TRUE(p->finish(a));
  TEST_ASSERT_TRUE(p->finish(b));
  TEST_ASSERT_EQUAL(1, a.count);
  TEST_ASSERT_EQUAL(1, a.skipped);
  TEST_ASSERT_EQUAL(a.count, b.count);
  TEST_ASSERT_EQUAL(a.skipped, b.skipped);
  TEST_ASSERT_EQUAL_STRING(a.events[0].title, b.events[0].title);
  p->~IcsParser();
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_yearly_bymonthday_is_unsupported);
  RUN_TEST(test_until_on_last_start_of_multi_day_series);
  RUN_TEST(test_override_without_usable_dtstart);
  RUN_TEST(test_long_span_series_counted_only_when_covering_today);
  RUN_TEST(test_broken_structure_counted_when_today);
  RUN_TEST(test_all_day_with_date_time_end);
  RUN_TEST(test_empty_tzid_is_foreign);
  RUN_TEST(test_finish_twice_is_idempotent);
  return UNITY_END();
}

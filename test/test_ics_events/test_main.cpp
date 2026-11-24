// Single (non-recurring) events: forms of DTSTART/DTEND, all-day spans, nesting, long lines,
// foreign zones and the CAL_EVENTS_MAX cut. Expected minutes were worked out by hand from the
// fixture text (America/New_York is UTC-5 on 2026-11-23) and checked with `TZ=... date -d`.
#include <unity.h>

#include "../support/IcsFeed.h"

using namespace icsfeed;

static const char* NY = "America/New_York";

void setUp() { setZone(NY); }
void tearDown() {}

void test_singles() {
  Result r = fixture("singles.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(5, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  expectEvent(r.snap, 0, "Late show", -1, 90);        // began yesterday 23:00
  expectEvent(r.snap, 1, "Standup", 570, 600);        // 14:30Z
  expectEvent(r.snap, 2, "Review", 660, 720);         // TZID = device zone
  expectEvent(r.snap, 3, "Dentist", 960, 1020);       // floating
  expectEvent(r.snap, 4, "Overnight shift", 1320, 1440);  // runs past midnight
}

void test_singles_next_day() {
  Result r = fixture("singles.ics", at(2026, 11, 24, 0, 5), NY);
  TEST_ASSERT_EQUAL(2, r.snap.count);
  expectEvent(r.snap, 0, "Overnight shift", -1, 360);
  expectEvent(r.snap, 1, "Tomorrow", 540, 600);
}

void test_details_lf_nested_long_lines() {
  Result r = fixture("details.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(7, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  expectEvent(r.snap, 0, "Quarterly planning session with the extended pro", 420, 450);
  expectEvent(r.snap, 1, "Reminder", 480, 480);  // no DTEND, no DURATION: zero length
  expectEvent(r.snap, 2, "Caf\xE9 cr\xE8me tasting", 600, 660);  // fold inside UTF-8
  expectEvent(r.snap, 3, "Duration talk", 780, 870);  // PT1H30M; 10 KB DESCRIPTION, ATTENDEE
  expectEvent(r.snap, 4, "(No title)", 900, 930);
  expectEvent(r.snap, 5, "Plan, budget; review notes", 990, 1020);
  expectEvent(r.snap, 6, "Gym", 1080, 1140);  // VALARM SUMMARY/DTSTART ignored
}

void test_all_day_spans() {
  Result r = fixture("allday.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_EQUAL(7, r.snap.count);
  expectEvent(r.snap, 0, "Book fair", -1, 1440);   // DURATION:P2D from yesterday
  expectEvent(r.snap, 1, "Conference", -1, 1440);  // first day
  expectEvent(r.snap, 2, "Holiday", -1, 1440);
  expectEvent(r.snap, 3, "Pay rent", -1, 1440);    // no DTEND: one day
  expectEvent(r.snap, 4, "Trip", -1, 1440);        // middle day
  expectEvent(r.snap, 5, "Workshop", -1, 1440);    // last day
  expectEvent(r.snap, 6, "Morning run", 420, 480); // timed events after all-day ones
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Retreat"));  // its exclusive end day
}

void test_all_day_later_days() {
  Result r = fixture("allday.ics", at(2026, 11, 24), NY);
  TEST_ASSERT_EQUAL(2, r.snap.count);
  expectEvent(r.snap, 0, "Conference", -1, 1440);
  expectEvent(r.snap, 1, "Trip", -1, 1440);
  r = fixture("allday.ics", at(2026, 11, 26), NY);  // Conference's exclusive end
  TEST_ASSERT_EQUAL(0, r.snap.count);
}

void test_foreign_zones_are_skipped_and_counted() {
  Result r = fixture("foreign.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "UTC zone", 720, 780);  // TZID=UTC is plain UTC
  // "Berlin call" (today) and "Berlin weekly" (active series); not the old or ended ones, and
  // not "Berlin later" (three days ahead).
  TEST_ASSERT_EQUAL(2, r.snap.skipped);
}

void test_more_than_max_keeps_earliest() {
  Result r = fixture("many.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_EQUAL(limits::CAL_EVENTS_MAX, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);  // overflow is not "skipped"
  expectEvent(r.snap, 0, "All day item", -1, 1440);
  const char* slots[] = {"Slot 07", "Slot 08", "Slot 09", "Slot 10", "Slot 11", "Slot 12", "Slot 13"};
  for (uint8_t i = 0; i < 7; i++) expectEvent(r.snap, i + 1, slots[i], (7 + i) * 60, (7 + i) * 60 + 30);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_singles);
  RUN_TEST(test_singles_next_day);
  RUN_TEST(test_details_lf_nested_long_lines);
  RUN_TEST(test_all_day_spans);
  RUN_TEST(test_all_day_later_days);
  RUN_TEST(test_foreign_zones_are_skipped_and_counted);
  RUN_TEST(test_more_than_max_keeps_earliest);
  return UNITY_END();
}

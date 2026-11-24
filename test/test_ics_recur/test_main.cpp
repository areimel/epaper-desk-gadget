// Recurring events, EXDATE and RECURRENCE-ID overrides (in both file orders). Monday
// 2026-11-23 in America/New_York unless stated; expectations worked out by hand from the
// fixture text.
#include <unity.h>

#include "../support/IcsFeed.h"

using namespace icsfeed;

static const char* NY = "America/New_York";

void setUp() { setZone(NY); }
void tearDown() {}

void test_series_today() {
  Result r = fixture("recur.ics", at(2026, 11, 23), NY);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(5, r.snap.count);
  expectEvent(r.snap, 0, "Robin's birthday", -1, 1440);  // YEARLY all-day since 1990
  expectEvent(r.snap, 1, "Daily check", 495, 510);       // COUNT=6 ends today
  expectEvent(r.snap, 2, "Weekdays", 510, 525);
  expectEvent(r.snap, 3, "Planning", 540, 570);          // every 2 weeks MO,WE,FR
  expectEvent(r.snap, 4, "Until today", 600, 630);       // UNTIL equals today's start
  // Not shown: "Old daily" (COUNT exhausted yesterday), "Until yesterday", "Exercise" and
  // "Yoga" (EXDATE today, the latter in a list), "Reading" (EXDATE in UTC), "Old monthly".
  // Skipped and counted: "Monthly review" (MONTHLY) and "Fourth Monday" (BYDAY=4MO).
  TEST_ASSERT_EQUAL(2, r.snap.skipped);
}

void test_series_other_days() {
  // Tuesday: Planning is MO,WE,FR; Exercise and Reading are back (no EXDATE on the 24th for
  // Exercise; Reading's 2nd EXDATE is the 23rd's instance).
  Result r = fixture("recur.ics", at(2026, 11, 24), NY);
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Planning"));
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Daily check"));
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Exercise"));
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Reading"));
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Weekdays"));
  // Monday 11-16 is in an inactive week for Planning; Yoga's 11-16 is not excluded.
  r = fixture("recur.ics", at(2026, 11, 16), NY);
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Planning"));
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Yoga"));
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Reading"));
  r = fixture("recur.ics", at(2026, 11, 15), NY);  // Reading's first EXDATE (01:00Z on the 16th)
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Reading"));
}

void test_feb_29_yearly() {
  TEST_ASSERT_FALSE(hasTitle(fixture("recur.ics", at(2027, 2, 28), NY).snap, "Leap day party"));
  TEST_ASSERT_FALSE(hasTitle(fixture("recur.ics", at(2027, 3, 1), NY).snap, "Leap day party"));
  Result r = fixture("recur.ics", at(2028, 2, 29), NY);
  TEST_ASSERT_TRUE(hasTitle(r.snap, "Leap day party"));
  TEST_ASSERT_EQUAL(-1, r.snap.events[0].startMin);
}

static void checkOverridesToday(const char* file) {
  Result r = fixture(file, at(2026, 11, 23), NY);
  TEST_ASSERT_EQUAL_MESSAGE(2, r.snap.count, file);
  TEST_ASSERT_EQUAL_MESSAGE(0, r.snap.skipped, file);
  // Lunch: override at the same time replaces today's instance (shown once, new title).
  expectEvent(r.snap, 0, "Lunch with guests", 720, 780);
  // 1:1 moved from tomorrow 14:00 to today 15:00.
  expectEvent(r.snap, 1, "1:1 moved", 900, 930);
  // Team sync moved from today to tomorrow; Design review's instance today is cancelled.
}

static void checkOverridesTomorrow(const char* file) {
  Result r = fixture(file, at(2026, 11, 24), NY);
  TEST_ASSERT_EQUAL_MESSAGE(2, r.snap.count, file);
  expectEvent(r.snap, 0, "Team sync moved", 600, 630);  // moved in from Monday
  expectEvent(r.snap, 1, "Lunch", 720, 780);            // the master again
  // 1:1's Tuesday instance moved to Monday: suppressed.
}

void test_overrides_before_master() {
  checkOverridesToday("overrides_before.ics");
  checkOverridesTomorrow("overrides_before.ics");
}

void test_overrides_after_master() {
  checkOverridesToday("overrides_after.ics");
  checkOverridesTomorrow("overrides_after.ics");
}

void test_overrides_next_week() {
  // Nothing is moved on 11-30: the masters show normally.
  Result r = fixture("overrides_after.ics", at(2026, 11, 30), NY);
  TEST_ASSERT_EQUAL(3, r.snap.count);
  expectEvent(r.snap, 0, "Team sync", 600, 630);
  expectEvent(r.snap, 1, "Lunch", 720, 780);
  expectEvent(r.snap, 2, "Design review", 960, 1020);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_series_today);
  RUN_TEST(test_series_other_days);
  RUN_TEST(test_feb_29_yearly);
  RUN_TEST(test_overrides_before_master);
  RUN_TEST(test_overrides_after_master);
  RUN_TEST(test_overrides_next_week);
  return UNITY_END();
}

// Time zones: the two 2026 US daylight-saving change days, a device in Europe/Berlin and one in
// UTC. UTC conversions were checked with `TZ=America/New_York date -d '2026-03-08 13:00Z'` etc.
#include <unity.h>

#include "../support/IcsFeed.h"

using namespace icsfeed;

void setUp() {}
void tearDown() {}

void test_spring_forward_day() {
  setZone("America/New_York");
  Result r = fixture("dst.ics", at(2026, 3, 8), "America/New_York");  // 23-hour day
  TEST_ASSERT_EQUAL(5, r.snap.count);
  expectEvent(r.snap, 0, "Night owl", 90, 240);      // floating 01:30-04:00 wall clock
  expectEvent(r.snap, 1, "Spring UTC", 540, 600);    // 13:00Z = 09:00 EDT
  expectEvent(r.snap, 2, "Weekly 9am", 540, 600);    // wall clock kept across the change
  expectEvent(r.snap, 3, "UTC series", 600, 630);    // 14:00Z series: 10:00 EDT now
  expectEvent(r.snap, 4, "Late spring", 1380, 1440);
}

void test_fall_back_day() {
  setZone("America/New_York");
  Result r = fixture("dst.ics", at(2026, 11, 1), "America/New_York");  // 25-hour day
  TEST_ASSERT_EQUAL(4, r.snap.count);
  expectEvent(r.snap, 0, "Fall back", 90, 150);
  expectEvent(r.snap, 1, "Fall UTC", 540, 600);   // 14:00Z = 09:00 EST
  expectEvent(r.snap, 2, "UTC series", 540, 570); // 14:00Z = 09:00 EST again
  expectEvent(r.snap, 3, "Weekly 9am", 540, 600);
}

void test_day_before_spring_forward() {
  setZone("America/New_York");
  Result r = fixture("dst.ics", at(2026, 3, 7), "America/New_York");
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "UTC series", 540, 570);  // 14:00Z = 09:00 EST
}

void test_berlin_device() {
  setZone("Europe/Berlin");
  Result r = fixture("foreign.ics", at(2026, 11, 23), "Europe/Berlin");
  TEST_ASSERT_EQUAL(3, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  expectEvent(r.snap, 0, "Berlin weekly", 540, 600);
  expectEvent(r.snap, 1, "Berlin call", 900, 960);
  expectEvent(r.snap, 2, "UTC zone", 1080, 1140);  // 17:00Z = 18:00 CET
  r = fixture("singles.ics", at(2026, 11, 23), "Europe/Berlin");
  TEST_ASSERT_EQUAL(2, r.snap.count);
  expectEvent(r.snap, 0, "Standup", 930, 960);  // 14:30Z = 15:30 CET
  expectEvent(r.snap, 1, "Dentist", 960, 1020);  // floating: device wall clock
  // Review, Late show and Overnight shift are in America/New_York: foreign here.
  TEST_ASSERT_EQUAL(3, r.snap.skipped);
}

void test_utc_device() {
  setZone("UTC");
  Result r = fixture("singles.ics", at(2026, 11, 23), "Etc/UTC");
  TEST_ASSERT_EQUAL(2, r.snap.count);
  expectEvent(r.snap, 0, "Standup", 870, 900);
  expectEvent(r.snap, 1, "Dentist", 960, 1020);
  TEST_ASSERT_EQUAL(3, r.snap.skipped);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_spring_forward_day);
  RUN_TEST(test_fall_back_day);
  RUN_TEST(test_day_before_spring_forward);
  RUN_TEST(test_berlin_device);
  RUN_TEST(test_utc_device);
  return UNITY_END();
}

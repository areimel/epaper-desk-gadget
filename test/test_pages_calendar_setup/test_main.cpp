// Calendar and setup screens. Goldens: test/fixtures/golden. Pictures: .pio/pages/*.pbm.
#include <string.h>
#include <unity.h>

#include "../support/PageData.h"
#include "../support/PagesHarness.h"
#include "Pages.h"

void setUp() { data::setZone(); }
void tearDown() {}

static void renderPage(const data::Set& s, uint8_t visit, Canvas& c) {
  pages::render(PageId::Calendar, data::context(s, visit), c);
}

#define CHECK(name, set, visit) harness::check(name, [&](Canvas& c) { renderPage(set, visit, c); })

void test_calendar_main() {
  data::Set& s = data::fresh();
  CHECK("calendar", s, 0);  // four events, one all-day
  s.feed.weather.units = Units::Fahrenheit;
  CHECK("calendar-12h", s, 0);
  data::fillCal(s.cal, 2);
  CHECK("calendar-two", s, 0);
  s.cal.skipped = 3;
  CHECK("calendar-skipped", s, 0);
  // An all-day event that ends today shows "until"; it sits among the timed ones.
  s.cal.count = 0;
  data::addEvent(s.cal, "Conference, day two", -1, 17 * 60);
  data::addEvent(s.cal, "Standup", 9 * 60 + 30, 9 * 60 + 45);
  CHECK("calendar-until", s, 0);
}

void test_calendar_paging() {
  data::Set& s = data::fresh();
  data::fillCal(s.cal, 7);  // all-day, then six timed events; now is 09:41
  CHECK("calendar-many", s, 0);
  CHECK("calendar-many-2", s, 1);
  uint8_t a[Canvas::PACKED_BYTES], b[Canvas::PACKED_BYTES];
  harness::pack([&](Canvas& c) { renderPage(s, 0, c); }, a);
  harness::pack([&](Canvas& c) { renderPage(s, 1, c); }, b);
  TEST_ASSERT_TRUE(memcmp(a, b, sizeof a) != 0);
  harness::pack([&](Canvas& c) { renderPage(s, 2, c); }, b);
  TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof a);
  // Later in the day the finished events drop off the first screen.
  s.cal.events[1].endMin = 9 * 60 + 40;  // standup is over at 09:41
  s.cal.events[2].endMin = 9 * 60 + 41;
  harness::pack([&](Canvas& c) { renderPage(s, 0, c); }, b);
  TEST_ASSERT_TRUE(memcmp(a, b, sizeof a) != 0);
}

void test_calendar_states() {
  data::Set& s = data::fresh();
  s.feed.calendarUrl[0] = '\0';
  CHECK("calendar-nourl", s, 0);
  strcpy(s.feed.calendarUrl, "https://calendar.example/private/basic.ics");
  data::status(s.cal.status, SourceState::Never, 0);
  CHECK("calendar-never", s, 0);
  data::status(s.cal.status, SourceState::Error, 0, "HTTP 404");
  CHECK("calendar-error", s, 0);
  data::fillCal(s.cal, 0);
  CHECK("calendar-empty", s, 0);
  s.cal.skipped = 2;
  CHECK("calendar-empty-skipped", s, 0);
  data::fillCal(s.cal, 4);
  data::status(s.cal.status, SourceState::Stale, 3 * 60, "no network");
  CHECK("calendar-stale", s, 0);
}

void test_setup() {
  static const struct { SetupState st; const char* detail; const char* name; } cases[] = {
      {SetupState::NoWifi, "Use the installer page over USB", "setup-nowifi"},
      {SetupState::Connecting, "Joining the network. This can take a few seconds.", "setup-connecting"},
      {SetupState::NoPages, "Open http://192.168.100.200 in a browser on the same WiFi and switch a page on",
       "setup-nopages"},
      {SetupState::NoWifi, nullptr, "setup-nodetail"},
  };
  for (const auto& k : cases)
    harness::check(k.name, [&](Canvas& cv) { pages::renderSetup(k.st, k.detail, cv); });
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calendar_main);
  RUN_TEST(test_calendar_paging);
  RUN_TEST(test_calendar_states);
  RUN_TEST(test_setup);
  return UNITY_END();
}

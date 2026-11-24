// Weather and Hacker News pages. Goldens: test/fixtures/golden. Pictures: .pio/pages/*.pbm.
#include <string.h>
#include <unity.h>

#include "../support/PageData.h"
#include "../support/PagesHarness.h"
#include "Pages.h"

void setUp() { data::setZone(); }
void tearDown() {}

static void renderPage(PageId id, const data::Set& s, uint8_t visit, Canvas& c) {
  pages::render(id, data::context(s, visit), c);
}

#define CHECK(name, id, set, visit) harness::check(name, [&](Canvas& c) { renderPage(id, set, visit, c); })

static void fail(SourceStatus& st, SourceState state, int minutesAgo, const char* err) {
  data::status(st, state, minutesAgo, err);
}

void test_weather_main_and_extremes() {
  data::Set& s = data::fresh();
  CHECK("weather", PageId::Weather, s, 0);
  data::fillWeather(s.weather, -12, -19, -8, -15, 73);
  CHECK("weather-cold", PageId::Weather, s, 0);
  s.feed.weather.units = Units::Fahrenheit;
  data::fillWeather(s.weather, 104, 109, 106, 88, 95);
  s.weather.rainPct = 100;
  s.weather.days[0] = {3, 104, -12, 2};
  strcpy(s.feed.weather.place, "Marseille-en-Provence-Alpes-C\xC3\xB4te-d'Azur");
  CHECK("weather-hot", PageId::Weather, s, 0);
  data::fillWeather(s.weather, 7, 4, 9, 2, 56);
  strcpy(s.feed.weather.place, "Z\xC3\xBCrich");
  CHECK("weather-drizzle", PageId::Weather, s, 0);
}

void test_weather_states() {
  data::Set& s = data::fresh();
  s.feed.weather.set = false;
  CHECK("weather-noplace", PageId::Weather, s, 0);
  s.feed.weather.set = true;
  fail(s.weather.status, SourceState::Never, 0, "");
  CHECK("weather-never", PageId::Weather, s, 0);
  fail(s.weather.status, SourceState::Error, 0, "no network");
  CHECK("weather-error", PageId::Weather, s, 0);
  data::fillWeather(s.weather, 12, 9, 14, 7, 61);
  fail(s.weather.status, SourceState::Stale, 3 * 60, "HTTP 503");
  CHECK("weather-stale", PageId::Weather, s, 0);
}

void test_hn() {
  data::Set& s = data::fresh();
  CHECK("hn", PageId::Hn, s, 0);
  CHECK("hn-2", PageId::Hn, s, 1);
  CHECK("hn-3", PageId::Hn, s, 2);
  uint8_t a[Canvas::PACKED_BYTES], b[Canvas::PACKED_BYTES];
  harness::pack([&](Canvas& c) { renderPage(PageId::Hn, s, 0, c); }, a);
  harness::pack([&](Canvas& c) { renderPage(PageId::Hn, s, 1, c); }, b);
  TEST_ASSERT_TRUE(memcmp(a, b, sizeof a) != 0);
  harness::pack([&](Canvas& c) { renderPage(PageId::Hn, s, 3, c); }, b);  // wraps to the first screen
  TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof a);
  s.hn.count = 2;  // fewer than a screenful: no marker, no stepping
  harness::pack([&](Canvas& c) { renderPage(PageId::Hn, s, 0, c); }, a);
  harness::pack([&](Canvas& c) { renderPage(PageId::Hn, s, 1, c); }, b);
  TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof a);
  CHECK("hn-two", PageId::Hn, s, 0);
}

void test_hn_states() {
  data::Set& s = data::fresh();
  s.hn.count = 0;
  CHECK("hn-empty", PageId::Hn, s, 0);
  fail(s.hn.status, SourceState::Never, 0, "");
  CHECK("hn-never", PageId::Hn, s, 0);
  fail(s.hn.status, SourceState::Error, 0, "HTTP 503");
  CHECK("hn-error", PageId::Hn, s, 0);
  data::fillHn(s.hn);
  fail(s.hn.status, SourceState::Stale, 95, "no network");
  CHECK("hn-stale", PageId::Hn, s, 0);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_weather_main_and_extremes);
  RUN_TEST(test_weather_states);
  RUN_TEST(test_hn);
  RUN_TEST(test_hn_states);
  return UNITY_END();
}

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "../support/Fixture.h"
#include "WeatherParse.h"

void setUp() {}
void tearDown() {}

static FeedConfig cfg;
static WeatherSnap snap;

static void sentinel() {
  memset(&snap, 0xA5, sizeof snap);
  snap.status.state = SourceState::Stale;
  snap.status.updated = 1234;
}

static void assertUntouched() {
  WeatherSnap want;
  memset(&want, 0xA5, sizeof want);
  want.status.state = SourceState::Stale;
  want.status.updated = 1234;
  TEST_ASSERT_EQUAL_MEMORY(&want, &snap, sizeof want);
}

static void assertStatusKept() {
  TEST_ASSERT_EQUAL(SourceState::Stale, snap.status.state);
  TEST_ASSERT_EQUAL(1234, snap.status.updated);
  TEST_ASSERT_EQUAL_HEX8(0xA5, static_cast<uint8_t>(snap.status.error[5]));
}

static const char* LONDON_URL_C =
    "https://api.open-meteo.com/v1/forecast?latitude=51.5074&longitude=-0.1278"
    "&current=temperature_2m,apparent_temperature,weather_code,is_day"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&forecast_days=4&temperature_unit=celsius&timezone=Europe%2FLondon";

static void setPlace(float lat, float lon, Units u, const char* zone) {
  feed::setDefaults(cfg);
  cfg.weather.set = true;
  cfg.weather.lat = lat;
  cfg.weather.lon = lon;
  cfg.weather.units = u;
  strcpy(cfg.tzIana, zone);
}

void test_url_celsius_with_encoded_zone() {
  setPlace(51.5074f, -0.1278f, Units::Celsius, "Europe/London");
  char url[400];
  size_t n = weatherUrl(cfg, url, sizeof url);
  TEST_ASSERT_EQUAL_STRING(LONDON_URL_C, url);
  TEST_ASSERT_EQUAL(strlen(LONDON_URL_C), n);
}

void test_url_fahrenheit_negative_coordinates_plus_zone() {
  setPlace(-33.8688f, -70.25f, Units::Fahrenheit, "Etc/GMT+3");
  char url[400];
  weatherUrl(cfg, url, sizeof url);
  TEST_ASSERT_NOT_NULL(strstr(url, "latitude=-33.8688&longitude=-70.2500&"));
  TEST_ASSERT_NOT_NULL(strstr(url, "&temperature_unit=fahrenheit&timezone=Etc%2FGMT%2B3"));
}

void test_url_plain_zone_keeps_unreserved() {
  setPlace(0, 0, Units::Celsius, "UTC");
  char url[400];
  weatherUrl(cfg, url, sizeof url);
  TEST_ASSERT_NOT_NULL(strstr(url, "latitude=0.0000&longitude=0.0000&"));
  TEST_ASSERT_TRUE(strcmp(strstr(url, "timezone="), "timezone=UTC") == 0);
}

void test_url_no_place_or_small_buffer() {
  setPlace(51.5074f, -0.1278f, Units::Celsius, "Europe/London");
  char url[400];
  cfg.weather.set = false;
  TEST_ASSERT_EQUAL(0, weatherUrl(cfg, url, sizeof url));
  cfg.weather.set = true;
  TEST_ASSERT_EQUAL(0, weatherUrl(cfg, url, strlen(LONDON_URL_C)));      // no room for NUL
  TEST_ASSERT_EQUAL(strlen(LONDON_URL_C), weatherUrl(cfg, url, strlen(LONDON_URL_C) + 1));
  TEST_ASSERT_EQUAL(0, weatherUrl(cfg, url, 10));
  TEST_ASSERT_EQUAL(0, weatherUrl(cfg, url, 0));
}

static void day(const WeatherDay& d, uint8_t wday, int16_t hi, int16_t lo, uint8_t code) {
  TEST_ASSERT_EQUAL(wday, d.wday);
  TEST_ASSERT_EQUAL(hi, d.hi);
  TEST_ASSERT_EQUAL(lo, d.lo);
  TEST_ASSERT_EQUAL(code, d.code);
}

void test_fixture_london_celsius() {
  Fixture f("weather_london_c.json");
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(f.buf, f.len, snap));
  TEST_ASSERT_EQUAL(3, snap.tempNow);
  TEST_ASSERT_EQUAL(-1, snap.feels);
  TEST_ASSERT_EQUAL(7, snap.hi);
  TEST_ASSERT_EQUAL(3, snap.lo);
  TEST_ASSERT_EQUAL(4, snap.rainPct);
  TEST_ASSERT_EQUAL(0, snap.code);
  TEST_ASSERT_FALSE(snap.isDay);
  day(snap.days[0], 3, 7, 4, 3);  // 2026-11-25 Wednesday
  day(snap.days[1], 4, 4, 3, 3);  // Thursday
  day(snap.days[2], 5, 6, 1, 51);  // Friday
  assertStatusKept();
}

void test_fixture_london_fahrenheit() {
  Fixture f("weather_london_f.json");
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(f.buf, f.len, snap));
  TEST_ASSERT_EQUAL(37, snap.tempNow);
  TEST_ASSERT_EQUAL(31, snap.feels);
  TEST_ASSERT_EQUAL(45, snap.hi);
  TEST_ASSERT_EQUAL(37, snap.lo);
  TEST_ASSERT_EQUAL(4, snap.rainPct);
  day(snap.days[0], 3, 44, 39, 3);
  day(snap.days[1], 4, 40, 37, 3);
  day(snap.days[2], 5, 42, 34, 51);
  assertStatusKept();
}

void test_fixture_yakutsk_negative_day() {
  Fixture f("weather_yakutsk_c.json");
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(f.buf, f.len, snap));
  TEST_ASSERT_EQUAL(-31, snap.tempNow);
  TEST_ASSERT_EQUAL(-36, snap.feels);
  TEST_ASSERT_EQUAL(-24, snap.hi);
  TEST_ASSERT_EQUAL(-35, snap.lo);
  TEST_ASSERT_EQUAL(86, snap.rainPct);
  TEST_ASSERT_EQUAL(3, snap.code);
  TEST_ASSERT_TRUE(snap.isDay);
  day(snap.days[0], 3, -24, -31, 71);
  day(snap.days[1], 4, -28, -32, 2);
  day(snap.days[2], 5, -28, -31, 3);
  assertStatusKept();
}

void test_fixture_error_reply_rejected() {
  Fixture f("weather_error.json");
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(f.buf, f.len, snap));
  assertUntouched();
}

// A reply with chosen current temperature, rain, codes and first daily dates.
static size_t synth(char* out, size_t cap, const char* temp, const char* rain, const char* code, const char* d1,
                    const char* d2, const char* d3) {
  return snprintf(out, cap,
                  "{\"current\":{\"temperature_2m\":%s,\"apparent_temperature\":%s,\"weather_code\":%s,"
                  "\"is_day\":1},\"daily\":{\"time\":[\"2026-01-01\",\"%s\",\"%s\",\"%s\"],"
                  "\"weather_code\":[1,2,3,4],\"temperature_2m_max\":[%s,%s,%s,%s],"
                  "\"temperature_2m_min\":[0,0,0,0],\"precipitation_probability_max\":[%s,0,0,0]}}",
                  temp, temp, code, d1, d2, d3, temp, temp, temp, temp, rain);
}

static int16_t roundOf(const char* temp) {
  char b[600];
  size_t n = synth(b, sizeof b, temp, "0", "0", "2026-01-02", "2026-01-03", "2026-01-04");
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(snap.tempNow, snap.hi);
  return snap.tempNow;
}

void test_rounding_half_away_from_zero() {
  TEST_ASSERT_EQUAL(-1, roundOf("-0.5"));
  TEST_ASSERT_EQUAL(1, roundOf("0.5"));
  TEST_ASSERT_EQUAL(1, roundOf("1.49"));
  TEST_ASSERT_EQUAL(-2, roundOf("-1.5"));
  TEST_ASSERT_EQUAL(0, roundOf("-0.4"));
}

void test_rain_and_code_edge_cases() {
  char b[600];
  size_t n = synth(b, sizeof b, "1", "null", "null", "2026-01-02", "2026-01-03", "2026-01-04");
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(0, snap.rainPct);
  TEST_ASSERT_EQUAL(255, snap.code);
  n = synth(b, sizeof b, "1", "130", "999", "2026-01-02", "2026-01-03", "2026-01-04");
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(100, snap.rainPct);
  TEST_ASSERT_EQUAL(255, snap.code);
  n = synth(b, sizeof b, "1", "-5", "95", "2026-01-02", "2026-01-03", "2026-01-04");
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(0, snap.rainPct);
  TEST_ASSERT_EQUAL(95, snap.code);
}

void test_required_number_null_or_text_rejected() {
  char b[600];
  size_t n = synth(b, sizeof b, "null", "0", "0", "2026-01-02", "2026-01-03", "2026-01-04");
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(b, n, snap));
  assertUntouched();
  n = synth(b, sizeof b, "\"warm\"", "0", "0", "2026-01-02", "2026-01-03", "2026-01-04");
  TEST_ASSERT_FALSE(parseWeather(b, n, snap));
  assertUntouched();
}

void test_weekday_known_dates() {
  char b[600];
  // 2028-02-29 Tue(2), 2028-03-01 Wed(3), 2028-03-02 Thu(4)
  size_t n = synth(b, sizeof b, "1", "0", "0", "2028-02-29", "2028-03-01", "2028-03-02");
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(2, snap.days[0].wday);
  TEST_ASSERT_EQUAL(3, snap.days[1].wday);
  TEST_ASSERT_EQUAL(4, snap.days[2].wday);
  // Year boundary: 2027-12-31 Fri(5), 2028-01-01 Sat(6), 2028-01-02 Sun(0)
  n = synth(b, sizeof b, "1", "0", "0", "2027-12-31", "2028-01-01", "2028-01-02");
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(5, snap.days[0].wday);
  TEST_ASSERT_EQUAL(6, snap.days[1].wday);
  TEST_ASSERT_EQUAL(0, snap.days[2].wday);
  // Century non-leap year: 2100-03-01 is a Monday(1)
  n = synth(b, sizeof b, "1", "0", "0", "2100-03-01", "2100-03-02", "2100-03-03");
  TEST_ASSERT_TRUE(parseWeather(b, n, snap));
  TEST_ASSERT_EQUAL(1, snap.days[0].wday);
}

void test_bad_date_rejected() {
  char b[600];
  size_t n = synth(b, sizeof b, "1", "0", "0", "tomorrow", "2026-01-03", "2026-01-04");
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(b, n, snap));
  assertUntouched();
}

void test_unusable_replies_leave_snap_untouched() {
  Fixture f("weather_london_c.json");
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(f.buf, f.len / 2, snap));  // truncated
  assertUntouched();
  TEST_ASSERT_FALSE(parseWeather("{}", 2, snap));
  assertUntouched();
  TEST_ASSERT_FALSE(parseWeather("not json", 8, snap));
  assertUntouched();
  TEST_ASSERT_FALSE(parseWeather("", 0, snap));
  assertUntouched();
  TEST_ASSERT_FALSE(parseWeather("[1,2]", 5, snap));
  assertUntouched();
  const char* noDaily = "{\"current\":{\"temperature_2m\":1,\"apparent_temperature\":1,\"weather_code\":0,\"is_day\":1}}";
  TEST_ASSERT_FALSE(parseWeather(noDaily, strlen(noDaily), snap));
  assertUntouched();
  const char* noCurrent = "{\"daily\":{\"time\":[\"2026-01-01\"]}}";
  TEST_ASSERT_FALSE(parseWeather(noCurrent, strlen(noCurrent), snap));
  assertUntouched();
}

void test_three_daily_entries_rejected() {
  const char* j =
      "{\"current\":{\"temperature_2m\":1,\"apparent_temperature\":1,\"weather_code\":0,\"is_day\":1},"
      "\"daily\":{\"time\":[\"2026-01-01\",\"2026-01-02\",\"2026-01-03\"],\"weather_code\":[1,2,3],"
      "\"temperature_2m_max\":[1,2,3],\"temperature_2m_min\":[0,0,0],\"precipitation_probability_max\":[0,0,0]}}";
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(j, strlen(j), snap));
  assertUntouched();
}

void test_respects_length_without_nul() {
  Fixture f("weather_london_c.json");
  static char exact[Fixture::CAP + 64];
  memset(exact, 'x', sizeof exact);
  memcpy(exact, f.buf, f.len);  // garbage (not even a NUL) follows the JSON
  sentinel();
  TEST_ASSERT_TRUE(parseWeather(exact, f.len, snap));
  TEST_ASSERT_EQUAL(3, snap.tempNow);
  // One byte short must fail rather than read on.
  sentinel();
  TEST_ASSERT_FALSE(parseWeather(exact, f.len - 3, snap));
  assertUntouched();
}

void test_wmo_text() {
  TEST_ASSERT_EQUAL_STRING("Clear", wmoText(0));
  TEST_ASSERT_EQUAL_STRING("Partly cloudy", wmoText(2));
  TEST_ASSERT_EQUAL_STRING("Fog", wmoText(48));
  TEST_ASSERT_EQUAL_STRING("Light rain", wmoText(61));
  TEST_ASSERT_EQUAL_STRING("Snow showers", wmoText(86));
  TEST_ASSERT_EQUAL_STRING("Storm, hail", wmoText(99));
  TEST_ASSERT_EQUAL_STRING("Unknown", wmoText(4));
  TEST_ASSERT_EQUAL_STRING("Unknown", wmoText(255));
  for (int c = 0; c < 256; c++) TEST_ASSERT_TRUE(strlen(wmoText(static_cast<uint8_t>(c))) <= 16);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_url_celsius_with_encoded_zone);
  RUN_TEST(test_url_fahrenheit_negative_coordinates_plus_zone);
  RUN_TEST(test_url_plain_zone_keeps_unreserved);
  RUN_TEST(test_url_no_place_or_small_buffer);
  RUN_TEST(test_fixture_london_celsius);
  RUN_TEST(test_fixture_london_fahrenheit);
  RUN_TEST(test_fixture_yakutsk_negative_day);
  RUN_TEST(test_fixture_error_reply_rejected);
  RUN_TEST(test_rounding_half_away_from_zero);
  RUN_TEST(test_rain_and_code_edge_cases);
  RUN_TEST(test_required_number_null_or_text_rejected);
  RUN_TEST(test_weekday_known_dates);
  RUN_TEST(test_bad_date_rejected);
  RUN_TEST(test_unusable_replies_leave_snap_untouched);
  RUN_TEST(test_three_daily_entries_rejected);
  RUN_TEST(test_respects_length_without_nul);
  RUN_TEST(test_wmo_text);
  return UNITY_END();
}

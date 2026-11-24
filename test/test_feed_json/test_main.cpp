#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "FeedJson.h"

void setUp() {}
void tearDown() {}

static FeedConfig out, cur, again;
static char buf[limits::FEED_JSON_MAX + limits::URL_MAX];

static const char* SAMPLE =
    "{\"v\":1,\"dwellSec\":300,\"tz\":{\"iana\":\"America/New_York\",\"posix\":\"EST5EDT,M3.2.0,M11.1.0\"},"
    "\"pages\":[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
    "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":false},{\"id\":\"notes\",\"on\":true}],"
    "\"weather\":{\"place\":\"Philadelphia\",\"lat\":39.95,\"lon\":-75.16,\"units\":\"f\"},"
    "\"calendar\":{\"set\":false},"
    "\"messages\":[{\"text\":\"Hello\",\"by\":\"me\"},{\"text\":\"No by\"}],"
    "\"notes\":{\"title\":\"Today\",\"lines\":[{\"text\":\"a\",\"todo\":true,\"done\":true},"
    "{\"text\":\"b\",\"todo\":false,\"done\":true}]}}";

static const char* HEAD =
    "{\"v\":1,\"dwellSec\":300,\"tz\":{\"iana\":\"UTC\",\"posix\":\"UTC0\"},\"pages\":["
    "{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
    "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}],";
static const char* TAIL = "\"messages\":[],\"notes\":{\"title\":\"\",\"lines\":[]}}";

static feed::Result parse(const char* j) { return feed::fromJson(j, strlen(j), cur, out); }

static void same(const FeedConfig& a, const FeedConfig& b) {
  TEST_ASSERT_EQUAL(a.dwellSec, b.dwellSec);
  TEST_ASSERT_EQUAL_STRING(a.tzIana, b.tzIana);
  TEST_ASSERT_EQUAL_STRING(a.tzPosix, b.tzPosix);
  for (int i = 0; i < limits::PAGE_COUNT; i++) {
    TEST_ASSERT_EQUAL(a.pages[i].id, b.pages[i].id);
    TEST_ASSERT_EQUAL(a.pages[i].on, b.pages[i].on);
  }
  TEST_ASSERT_EQUAL(a.weather.set, b.weather.set);
  TEST_ASSERT_EQUAL_STRING(a.weather.place, b.weather.place);
  TEST_ASSERT_EQUAL_FLOAT(a.weather.lat, b.weather.lat);
  TEST_ASSERT_EQUAL_FLOAT(a.weather.lon, b.weather.lon);
  TEST_ASSERT_EQUAL(a.weather.units, b.weather.units);
  TEST_ASSERT_EQUAL_STRING(a.calendarUrl, b.calendarUrl);
  TEST_ASSERT_EQUAL(a.messageCount, b.messageCount);
  for (int i = 0; i < a.messageCount; i++) {
    TEST_ASSERT_EQUAL_STRING(a.messages[i].text, b.messages[i].text);
    TEST_ASSERT_EQUAL_STRING(a.messages[i].by, b.messages[i].by);
  }
  TEST_ASSERT_EQUAL_STRING(a.notes.title, b.notes.title);
  TEST_ASSERT_EQUAL(a.notes.lineCount, b.notes.lineCount);
  for (int i = 0; i < a.notes.lineCount; i++) {
    TEST_ASSERT_EQUAL_STRING(a.notes.lines[i].text, b.notes.lines[i].text);
    TEST_ASSERT_EQUAL(a.notes.lines[i].todo, b.notes.lines[i].todo);
    TEST_ASSERT_EQUAL(a.notes.lines[i].done, b.notes.lines[i].done);
  }
}

void test_parses_a_valid_feed() {
  feed::setDefaults(cur);
  auto r = parse(SAMPLE);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL_STRING("", r.path);
  TEST_ASSERT_EQUAL_STRING("", r.error);
  TEST_ASSERT_EQUAL(300, out.dwellSec);
  TEST_ASSERT_EQUAL_STRING("America/New_York", out.tzIana);
  TEST_ASSERT_EQUAL_STRING("EST5EDT,M3.2.0,M11.1.0", out.tzPosix);
  TEST_ASSERT_FALSE(out.pages[4].on);
  TEST_ASSERT_EQUAL(PageId::Calendar, out.pages[4].id);
  TEST_ASSERT_TRUE(out.weather.set);
  TEST_ASSERT_EQUAL_STRING("Philadelphia", out.weather.place);
  TEST_ASSERT_EQUAL_FLOAT(39.95f, out.weather.lat);
  TEST_ASSERT_EQUAL_FLOAT(-75.16f, out.weather.lon);
  TEST_ASSERT_EQUAL(Units::Fahrenheit, out.weather.units);
  TEST_ASSERT_EQUAL(2, out.messageCount);
  TEST_ASSERT_EQUAL_STRING("me", out.messages[0].by);
  TEST_ASSERT_EQUAL_STRING("", out.messages[1].by);
  TEST_ASSERT_EQUAL_STRING("Today", out.notes.title);
  TEST_ASSERT_EQUAL(2, out.notes.lineCount);
}

void test_done_is_forced_false_without_todo() {
  feed::setDefaults(cur);
  TEST_ASSERT_TRUE(parse(SAMPLE).ok);
  TEST_ASSERT_TRUE(out.notes.lines[0].done);
  TEST_ASSERT_FALSE(out.notes.lines[1].done);
}

void test_weather_null_or_absent_clears_weather() {
  static char j[1024];
  feed::setDefaults(cur);
  for (int variant = 0; variant < 2; variant++) {
    snprintf(j, sizeof j, "%s%s%s", HEAD, variant ? "\"weather\":null," : "", TAIL);
    out.weather.set = true;
    strcpy(out.weather.place, "x");
    out.weather.lat = 5;
    out.weather.units = Units::Fahrenheit;
    TEST_ASSERT_TRUE(parse(j).ok);
    TEST_ASSERT_FALSE(out.weather.set);
    TEST_ASSERT_EQUAL_STRING("", out.weather.place);
    TEST_ASSERT_EQUAL_FLOAT(0, out.weather.lat);
    TEST_ASSERT_EQUAL(Units::Celsius, out.weather.units);
  }
}

void test_calendar_url_rules() {
  static char j[1024];
  feed::setDefaults(cur);
  strcpy(cur.calendarUrl, "https://old.example/x.ics");
  TEST_ASSERT_TRUE(parse(SAMPLE).ok);  // {"set":false} keeps the stored link
  TEST_ASSERT_EQUAL_STRING("https://old.example/x.ics", out.calendarUrl);
  snprintf(j, sizeof j, "%s\"calendar\":{\"url\":\"\"},%s", HEAD, TAIL);
  TEST_ASSERT_TRUE(parse(j).ok);
  TEST_ASSERT_EQUAL_STRING("", out.calendarUrl);
  snprintf(j, sizeof j, "%s\"calendar\":{\"url\":\"webcal://x.example/a.ics\"},%s", HEAD, TAIL);
  TEST_ASSERT_TRUE(parse(j).ok);
  TEST_ASSERT_EQUAL_STRING("webcal://x.example/a.ics", out.calendarUrl);
  snprintf(j, sizeof j, "%s%s", HEAD, TAIL);  // calendar absent
  TEST_ASSERT_TRUE(parse(j).ok);
  TEST_ASSERT_EQUAL_STRING("https://old.example/x.ics", out.calendarUrl);
}

static void fillFull(FeedConfig& f) {
  feed::setDefaults(f);
  f.dwellSec = limits::DWELL_MAX_S;
  memset(f.tzIana, 'i', limits::TZ_MAX);
  memset(f.tzPosix, 'p', limits::TZ_MAX);
  f.tzPosix[limits::TZ_MAX - 1] = '0';  // a zone string needs an offset digit
  f.pages[0] = {PageId::Notes, true};
  f.pages[5] = {PageId::Clock, false};
  f.weather.set = true;
  memset(f.weather.place, 'w', limits::PLACE_MAX);
  f.weather.lat = -33.865143f;
  f.weather.lon = 151.2099f;
  f.weather.units = Units::Fahrenheit;
  strcpy(f.calendarUrl, "https://");
  memset(f.calendarUrl + 8, '~', limits::URL_MAX - 8);
  f.messageCount = limits::MESSAGES_MAX;
  for (int i = 0; i < limits::MESSAGES_MAX; i++) {
    memset(f.messages[i].text, 'a' + i % 26, limits::MESSAGE_TEXT_MAX);
    f.messages[i].text[limits::MESSAGE_TEXT_MAX] = 0;
    memset(f.messages[i].by, 'A' + i % 26, limits::MESSAGE_BY_MAX);
    f.messages[i].by[limits::MESSAGE_BY_MAX] = 0;
  }
  memset(f.notes.title, 't', limits::NOTE_TITLE_MAX);
  f.notes.lineCount = limits::NOTE_LINES_MAX;
  for (int i = 0; i < limits::NOTE_LINES_MAX; i++) {
    memset(f.notes.lines[i].text, 'n', limits::NOTE_TEXT_MAX);
    f.notes.lines[i].text[limits::NOTE_TEXT_MAX] = 0;
    f.notes.lines[i].todo = i % 2 == 0;
    f.notes.lines[i].done = i % 4 == 0;
  }
}

void test_round_trip_full_feed_in_storage_shape() {
  static FeedConfig full;
  fillFull(full);
  size_t n = feed::toJson(full, false, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL(strlen(buf), n);
  // Worst case without escapes (40 x 140-byte messages, 32-byte `by`, full notes, 256-byte URL)
  // measured 8823 bytes (printed below); it must stay under FEED_JSON_MAX. Quotes and backslashes
  // are escaped as two bytes, so a feed full of them is longer, but never longer than the
  // request that carried it.
  TEST_ASSERT_LESS_THAN(limits::FEED_JSON_MAX, n);
  printf("worst-case storage JSON: %u bytes\n", (unsigned)n);
  feed::setDefaults(cur);
  auto r = feed::fromJson(buf, n, cur, again);
  TEST_ASSERT_TRUE_MESSAGE(r.ok, r.error);
  same(full, again);
}

void test_redacted_shape_hides_the_url() {
  static FeedConfig full;
  fillFull(full);
  size_t n = feed::toJson(full, true, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_NULL(strstr(buf, "https://"));
  TEST_ASSERT_NULL(strstr(buf, "~~~~"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"calendar\":{\"set\":true}"));
  full.calendarUrl[0] = 0;
  feed::toJson(full, true, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"calendar\":{\"set\":false}"));
  // the redacted shape parses back and keeps the stored link
  feed::setDefaults(cur);
  strcpy(cur.calendarUrl, "https://keep.example/");
  TEST_ASSERT_TRUE(feed::fromJson(buf, strlen(buf), cur, again).ok);
  TEST_ASSERT_EQUAL_STRING("https://keep.example/", again.calendarUrl);
}

void test_shape_and_key_order() {
  feed::setDefaults(cur);
  TEST_ASSERT_TRUE(parse(SAMPLE).ok);
  size_t n = feed::toJson(out, true, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  const char* expect =
      "{\"v\":1,\"dwellSec\":300,\"tz\":{\"iana\":\"America/New_York\",\"posix\":\"EST5EDT,M3.2.0,M11.1.0\"},"
      "\"pages\":[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
      "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":false},{\"id\":\"notes\",\"on\":true}],"
      "\"weather\":{\"place\":\"Philadelphia\",\"lat\":39.95,\"lon\":-75.16,\"units\":\"f\"},"
      "\"calendar\":{\"set\":false},"
      "\"messages\":[{\"text\":\"Hello\",\"by\":\"me\"},{\"text\":\"No by\",\"by\":\"\"}],"
      "\"notes\":{\"title\":\"Today\",\"lines\":[{\"text\":\"a\",\"todo\":true,\"done\":true},"
      "{\"text\":\"b\",\"todo\":false,\"done\":false}]}}";
  TEST_ASSERT_EQUAL_STRING(expect, buf);
}

void test_weather_unset_is_null_and_escapes() {
  feed::setDefaults(cur);
  cur.messageCount = 1;
  strcpy(cur.messages[0].text, "say \"hi\" \\ \xC3\xA9");
  feed::toJson(cur, true, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"weather\":null"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "say \\\"hi\\\" \\\\ \xC3\xA9"));
  TEST_ASSERT_TRUE(feed::fromJson(buf, strlen(buf), cur, again).ok);
  TEST_ASSERT_EQUAL_STRING(cur.messages[0].text, again.messages[0].text);
}

void test_to_json_reports_too_small_buffer() {
  static FeedConfig full;
  fillFull(full);
  size_t n = feed::toJson(full, false, buf, sizeof buf);
  TEST_ASSERT_EQUAL(0, feed::toJson(full, false, buf, n));  // no room for the NUL
  TEST_ASSERT_EQUAL(n, feed::toJson(full, false, buf, n + 1));
  TEST_ASSERT_EQUAL(0, feed::toJson(full, false, buf, 10));
  TEST_ASSERT_EQUAL(0, feed::toJson(full, false, buf, 0));
}

// Counts the peak heap of a worst-case parse (full feed, every string at its limit).
struct Counting : ArduinoJson::Allocator {
  size_t live = 0, peak = 0;
  void* allocate(size_t n) override {
    char* p = (char*)malloc(n + sizeof(size_t));
    *(size_t*)p = n;
    live += n;
    if (live > peak) peak = live;
    return p + sizeof(size_t);
  }
  void deallocate(void* p) override {
    if (!p) return;
    char* b = (char*)p - sizeof(size_t);
    live -= *(size_t*)b;
    free(b);
  }
  void* reallocate(void* p, size_t n) override {
    char* b = p ? (char*)p - sizeof(size_t) : nullptr;
    size_t old = b ? *(size_t*)b : 0;
    char* nb = (char*)realloc(b, n + sizeof(size_t));
    if (!nb) return nullptr;
    *(size_t*)nb = n;
    live = live - old + n;
    if (live > peak) peak = live;
    return nb + sizeof(size_t);
  }
};

void test_worst_case_parse_heap_is_bounded() {
  static FeedConfig full;
  fillFull(full);
  size_t n = feed::toJson(full, false, buf, sizeof buf);
  Counting c;
  feed::setDefaults(cur);
  TEST_ASSERT_TRUE(feed::fromJson(buf, n, cur, again, &c).ok);
  printf("worst-case parse heap peak: %u bytes for %u bytes of JSON\n", (unsigned)c.peak, (unsigned)n);
  TEST_ASSERT_EQUAL(0, c.live);  // everything released
  TEST_ASSERT_LESS_THAN(limits::FEED_JSON_MAX * 2, c.peak);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_a_valid_feed);
  RUN_TEST(test_done_is_forced_false_without_todo);
  RUN_TEST(test_weather_null_or_absent_clears_weather);
  RUN_TEST(test_calendar_url_rules);
  RUN_TEST(test_round_trip_full_feed_in_storage_shape);
  RUN_TEST(test_redacted_shape_hides_the_url);
  RUN_TEST(test_shape_and_key_order);
  RUN_TEST(test_weather_unset_is_null_and_escapes);
  RUN_TEST(test_to_json_reports_too_small_buffer);
  RUN_TEST(test_worst_case_parse_heap_is_bounded);
  return UNITY_END();
}

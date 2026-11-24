// Validation added after review: the time zone string and the calendar link are passed to
// setenv("TZ") and to the HTTPS client, so their shape is checked, not just their length.
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "FeedJson.h"

void setUp() {}
void tearDown() {}

static FeedConfig current, out;
static char doc[limits::FEED_JSON_MAX + limits::URL_MAX + 64];

static feed::Result parse(const char* posix, const char* calendar, size_t maxBytes = limits::FEED_JSON_MAX) {
  const int n = snprintf(doc, sizeof doc,
                         "{\"v\":1,\"dwellSec\":300,\"tz\":{\"iana\":\"Etc/UTC\",\"posix\":\"%s\"},"
                         "\"pages\":[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
                         "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}],"
                         "\"calendar\":%s,\"messages\":[],\"notes\":{\"title\":\"\",\"lines\":[]}}",
                         posix, calendar);
  feed::setDefaults(current);
  feed::setDefaults(out);
  return feed::fromJson(doc, static_cast<size_t>(n), current, out, nullptr, maxBytes);
}

void test_posix_zone_shapes() {
  for (const char* good : {"UTC0", "EST5EDT,M3.2.0,M11.1.0", "<+03>-3", "CET-1CEST,M3.5.0,M10.5.0/3"}) {
    TEST_ASSERT_TRUE_MESSAGE(parse(good, "{}").ok, good);
  }
  for (const char* bad : {"x", "UTC", "UTC0;rm", "UTC 0", "A=B0", "UTC0%"}) {
    const feed::Result r = parse(bad, "{}");
    TEST_ASSERT_FALSE_MESSAGE(r.ok, bad);
    TEST_ASSERT_EQUAL_STRING("tz.posix", r.path);
  }
}

void test_calendar_link_shapes() {
  for (const char* good : {"{\"url\":\"https://calendar.google.com/calendar/ical/a%40b/private-x/basic.ics\"}",
                           "{\"url\":\"webcal://example.com/c.ics?x=1\"}", "{\"url\":\"\"}"}) {
    TEST_ASSERT_TRUE_MESSAGE(parse("UTC0", good).ok, good);
  }
  for (const char* bad : {"{\"url\":\"https://\"}", "{\"url\":\"https:///path\"}", "{\"url\":\"https://user:pw@example.com/c.ics\"}",
                          "{\"url\":\"https://example.com/a b.ics\"}", "{\"url\":\"https://example.com/a\\u007f.ics\"}",
                          "{\"url\":\"http://example.com/c.ics\"}", "{\"url\":\"HTTPS://example.com/c.ics\"}"}) {
    const feed::Result r = parse("UTC0", bad);
    TEST_ASSERT_FALSE_MESSAGE(r.ok, bad);
    TEST_ASSERT_EQUAL_STRING("calendar.url", r.path);
  }
}

void test_size_limit_is_the_callers() {
  // A document just over the request limit is refused by default but accepted with the larger
  // storage limit, so a feed that was saved can always be loaded again.
  static char big[limits::FEED_JSON_MAX + limits::URL_MAX];
  const feed::Result small = parse("UTC0", "{}");
  TEST_ASSERT_TRUE(small.ok);
  const size_t base = strlen(doc);
  memcpy(big, doc, base);
  memset(big + base, ' ', limits::FEED_JSON_MAX + 10 - base);  // trailing spaces are valid JSON
  const size_t n = limits::FEED_JSON_MAX + 10;
  TEST_ASSERT_FALSE(feed::fromJson(big, n, current, out).ok);
  TEST_ASSERT_TRUE(feed::fromJson(big, n, current, out, nullptr, limits::FEED_JSON_MAX + limits::URL_MAX).ok);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_posix_zone_shapes);
  RUN_TEST(test_calendar_link_shapes);
  RUN_TEST(test_size_limit_is_the_callers);
  return UNITY_END();
}

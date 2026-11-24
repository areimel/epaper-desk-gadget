#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "FeedJson.h"

void setUp() {}
void tearDown() {}

static FeedConfig out, before, cur;
static char doc[limits::FEED_JSON_MAX + 64];

// A valid document with each top-level member replaceable. A null slot uses the valid default;
// "-" drops the member entirely.
static const char* D_V = "1";
static const char* D_DWELL = "300";
static const char* D_TZ = "{\"iana\":\"UTC\",\"posix\":\"UTC0\"}";
static const char* D_PAGES =
    "[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
    "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}]";
static const char* D_WEATHER = "{\"place\":\"X\",\"lat\":1.5,\"lon\":2.5,\"units\":\"c\"}";
static const char* D_CAL = "{\"set\":false}";
static const char* D_MSG = "[{\"text\":\"hi\"}]";
static const char* D_NOTES = "{\"title\":\"T\",\"lines\":[{\"text\":\"a\",\"todo\":true,\"done\":false}]}";

static void mk(const char* v, const char* dwell, const char* tz, const char* pages, const char* weather,
               const char* cal, const char* msg, const char* notes, const char* extra = "") {
  const char* name[] = {"v", "dwellSec", "tz", "pages", "weather", "calendar", "messages", "notes"};
  const char* val[] = {v ? v : D_V,           dwell ? dwell : D_DWELL, tz ? tz : D_TZ,   pages ? pages : D_PAGES,
                       weather ? weather : D_WEATHER, cal ? cal : D_CAL, msg ? msg : D_MSG, notes ? notes : D_NOTES};
  size_t n = 0;
  doc[n++] = '{';
  for (int i = 0; i < 8; i++) {
    if (strcmp(val[i], "-") == 0) continue;
    n += snprintf(doc + n, sizeof doc - n, "%s\"%s\":%s", n > 1 ? "," : "", name[i], val[i]);
  }
  if (*extra) n += snprintf(doc + n, sizeof doc - n, ",%s", extra);
  strcpy(doc + n, "}");
}

static void expectFail(const char* path, const char* errorContains = nullptr) {
  feed::setDefaults(out);
  strcpy(out.calendarUrl, "https://before.example/");
  before = out;
  feed::setDefaults(cur);
  feed::Result r = feed::fromJson(doc, strlen(doc), cur, out);
  TEST_ASSERT_FALSE_MESSAGE(r.ok, doc);
  TEST_ASSERT_EQUAL_STRING(path, r.path);
  TEST_ASSERT_TRUE(strlen(r.error) > 0);
  TEST_ASSERT_EQUAL_CHAR('.', r.error[strlen(r.error) - 1]);
  if (errorContains) TEST_ASSERT_NOT_NULL_MESSAGE(strstr(r.error, errorContains), r.error);
  TEST_ASSERT_EQUAL_MEMORY(&before, &out, sizeof out);  // all or nothing
}

static bool valid() {
  feed::setDefaults(cur);
  return feed::fromJson(doc, strlen(doc), cur, out).ok;
}

void test_defaults_document_is_valid() {
  mk(0, 0, 0, 0, 0, 0, 0, 0);
  TEST_ASSERT_TRUE(valid());
}

void test_not_json_and_wrong_root() {
  strcpy(doc, "not json");
  expectFail("");
  strcpy(doc, "[1,2]");
  expectFail("");
  strcpy(doc, "");
  expectFail("");
  strcpy(doc, "{\"v\":1");  // truncated
  expectFail("");
}

void test_too_large_is_not_parsed() {
  feed::setDefaults(cur);
  feed::setDefaults(out);
  before = out;
  static char big[limits::FEED_JSON_MAX + 2];
  memset(big, ' ', sizeof big);
  big[sizeof big - 1] = 0;
  feed::Result r = feed::fromJson(big, limits::FEED_JSON_MAX + 1, cur, out);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("Feed is too large.", r.error);
  TEST_ASSERT_EQUAL_STRING("", r.path);
  TEST_ASSERT_EQUAL_MEMORY(&before, &out, sizeof out);
}

void test_version() {
  mk("2", 0, 0, 0, 0, 0, 0, 0);
  expectFail("v");
  mk("\"1\"", 0, 0, 0, 0, 0, 0, 0);
  expectFail("v");
  mk("1.5", 0, 0, 0, 0, 0, 0, 0);
  expectFail("v");
}

void test_unknown_top_level_key() {
  mk(0, 0, 0, 0, 0, 0, 0, 0, "\"bogus\":1");
  expectFail("bogus");
}

void test_missing_required_keys() {
  const char* keys[] = {"v", "dwellSec", "tz", "pages", "messages", "notes"};
  for (int i = 0; i < 6; i++) {
    const char* s[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int slot[] = {0, 1, 2, 3, 6, 7};
    s[slot[i]] = "-";
    mk(s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]);
    expectFail(keys[i]);
  }
  // weather and calendar are optional
  mk(0, 0, 0, 0, "-", "-", 0, 0);
  TEST_ASSERT_TRUE(valid());
}

void test_dwell() {
  char b[16];
  snprintf(b, sizeof b, "%u", limits::DWELL_MIN_S - 1);
  mk(0, b, 0, 0, 0, 0, 0, 0);
  expectFail("dwellSec", "between 3 and 60 minutes");
  snprintf(b, sizeof b, "%u", limits::DWELL_MAX_S + 1);
  mk(0, b, 0, 0, 0, 0, 0, 0);
  expectFail("dwellSec");
  mk(0, "\"300\"", 0, 0, 0, 0, 0, 0);
  expectFail("dwellSec");
  mk(0, "300.5", 0, 0, 0, 0, 0, 0);
  expectFail("dwellSec");
  mk(0, "99999999999", 0, 0, 0, 0, 0, 0);
  expectFail("dwellSec");
  snprintf(b, sizeof b, "%u", limits::DWELL_MIN_S);
  mk(0, b, 0, 0, 0, 0, 0, 0);
  TEST_ASSERT_TRUE(valid());
  snprintf(b, sizeof b, "%u", limits::DWELL_MAX_S);
  mk(0, b, 0, 0, 0, 0, 0, 0);
  TEST_ASSERT_TRUE(valid());
}

void test_tz() {
  mk(0, 0, "\"UTC\"", 0, 0, 0, 0, 0);
  expectFail("tz");
  mk(0, 0, "{\"posix\":\"UTC0\"}", 0, 0, 0, 0, 0);
  expectFail("tz.iana");
  mk(0, 0, "{\"iana\":\"UTC\",\"posix\":\"\"}", 0, 0, 0, 0, 0);
  expectFail("tz.posix");
  mk(0, 0, "{\"iana\":5,\"posix\":\"UTC0\"}", 0, 0, 0, 0, 0);
  expectFail("tz.iana");
  char tz[200];
  snprintf(tz, sizeof tz, "{\"iana\":\"%s\",\"posix\":\"UTC0\"}", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  mk(0, 0, tz, 0, 0, 0, 0, 0);
  expectFail("tz.iana", "too long");
}

void test_pages() {
  mk(0, 0, 0, "{}", 0, 0, 0, 0);
  expectFail("pages");
  mk(0, 0, 0, "[{\"id\":\"clock\",\"on\":true}]", 0, 0, 0, 0);
  expectFail("pages", "exactly 6");
  mk(0, 0, 0,
     "[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"clock\",\"on\":true},"
     "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}]",
     0, 0, 0, 0);
  expectFail("pages[2].id", "listed twice");
  mk(0, 0, 0,
     "[{\"id\":\"clock\",\"on\":true},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
     "{\"id\":\"news\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}]",
     0, 0, 0, 0);
  expectFail("pages[3].id");
  mk(0, 0, 0,
     "[{\"id\":\"clock\",\"on\":1},{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},"
     "{\"id\":\"messages\",\"on\":true},{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}]",
     0, 0, 0, 0);
  expectFail("pages[0].on");
  mk(0, 0, 0,
     "[5,{\"id\":\"weather\",\"on\":true},{\"id\":\"hn\",\"on\":true},{\"id\":\"messages\",\"on\":true},"
     "{\"id\":\"calendar\",\"on\":true},{\"id\":\"notes\",\"on\":true}]",
     0, 0, 0, 0);
  expectFail("pages[0]");
}

void test_weather() {
  mk(0, 0, 0, 0, "\"rain\"", 0, 0, 0); expectFail("weather");
  mk(0, 0, 0, 0, "{\"lat\":1,\"lon\":2,\"units\":\"c\"}", 0, 0, 0); expectFail("weather.place");
  mk(0, 0, 0, 0, "{\"place\":\"\",\"lat\":1,\"lon\":2,\"units\":\"c\"}", 0, 0, 0); expectFail("weather.place");
  mk(0, 0, 0, 0, "{\"place\":\"X\",\"lat\":90.5,\"lon\":2,\"units\":\"c\"}", 0, 0, 0); expectFail("weather.lat");
  mk(0, 0, 0, 0, "{\"place\":\"X\",\"lat\":1,\"lon\":-180.1,\"units\":\"c\"}", 0, 0, 0); expectFail("weather.lon");
  mk(0, 0, 0, 0, "{\"place\":\"X\",\"lat\":\"1\",\"lon\":2,\"units\":\"c\"}", 0, 0, 0); expectFail("weather.lat");
  mk(0, 0, 0, 0, "{\"place\":\"X\",\"lat\":1,\"lon\":2,\"units\":\"k\"}", 0, 0, 0); expectFail("weather.units");
  mk(0, 0, 0, 0, "{\"place\":\"X\",\"lat\":90,\"lon\":-180,\"units\":\"f\"}", 0, 0, 0);  // edges are fine
  TEST_ASSERT_TRUE(valid());
}

void test_calendar() {
  mk(0, 0, 0, 0, 0, "\"https://x\"", 0, 0); expectFail("calendar");
  mk(0, 0, 0, 0, 0, "{\"url\":\"http://x.example/\"}", 0, 0); expectFail("calendar.url", "https:// or webcal://");
  mk(0, 0, 0, 0, 0, "{\"url\":5}", 0, 0); expectFail("calendar.url");
  static char cal[400];
  strcpy(cal, "{\"url\":\"https://");
  memset(cal + strlen(cal), 'a', limits::URL_MAX - 7);  // 8 + (URL_MAX-7) = URL_MAX + 1
  strcpy(cal + strlen(cal), "\"}");
  mk(0, 0, 0, 0, 0, cal, 0, 0);
  expectFail("calendar.url", "too long");
}

void test_messages() {
  mk(0, 0, 0, 0, 0, 0, "{}", 0); expectFail("messages");
  mk(0, 0, 0, 0, 0, 0, "[\"hi\"]", 0); expectFail("messages[0]");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"ok\"},{\"by\":\"me\"}]", 0); expectFail("messages[1].text");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"\"}]", 0); expectFail("messages[0].text");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":7}]", 0); expectFail("messages[0].text");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"a\",\"by\":3}]", 0); expectFail("messages[0].by");
  static char m[400];
  snprintf(m, sizeof m, "[{\"text\":\"a\"},{\"text\":\"a\"},{\"text\":\"a\"},{\"text\":\"%.*s\"}]",
           (int)limits::MESSAGE_TEXT_MAX + 1,
           "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  mk(0, 0, 0, 0, 0, 0, m, 0);
  expectFail("messages[3].text", "Text is too long.");
  snprintf(m, sizeof m, "[{\"text\":\"a\",\"by\":\"%.*s\"}]", (int)limits::MESSAGE_BY_MAX + 1,
           "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
  mk(0, 0, 0, 0, 0, 0, m, 0);
  expectFail("messages[0].by");
}

void test_too_many_messages() {
  static char m[2000];
  size_t n = 0;
  m[n++] = '[';
  for (int i = 0; i <= limits::MESSAGES_MAX; i++) n += snprintf(m + n, sizeof m - n, "%s{\"text\":\"a\"}", i ? "," : "");
  strcpy(m + n, "]");
  mk(0, 0, 0, 0, 0, 0, m, 0);
  expectFail("messages", "40");
}

void test_notes() {
  mk(0, 0, 0, 0, 0, 0, 0, "[]"); expectFail("notes");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"lines\":[]}"); expectFail("notes.title");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"title\":\"This title is longer than 24 bytes\",\"lines\":[]}"); expectFail("notes.title", "too long");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"title\":\"\"}"); expectFail("notes.lines");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"title\":\"\",\"lines\":[{\"text\":\"a\",\"todo\":true}]}"); expectFail("notes.lines[0].done");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"title\":\"\",\"lines\":[{\"text\":\"a\",\"done\":false}]}"); expectFail("notes.lines[0].todo");
  mk(0, 0, 0, 0, 0, 0, 0, "{\"title\":\"\",\"lines\":[{\"text\":\"\",\"todo\":false,\"done\":false}]}"); expectFail("notes.lines[0].text");
  mk(0, 0, 0, 0, 0, 0, 0,
     "{\"title\":\"\",\"lines\":[{\"text\":\"a\",\"todo\":false,\"done\":false},{\"text\":\"a\",\"todo\":false,"
     "\"done\":false},{\"text\":\"a\",\"todo\":false,\"done\":false},{\"text\":\"a\",\"todo\":false,\"done\":false},"
     "{\"text\":\"a\",\"todo\":false,\"done\":false},{\"text\":\"a\",\"todo\":false,\"done\":false}]}");
  expectFail("notes.lines", "5");
}

void test_bytes_not_characters_and_control_chars() {
  // 24 two-byte characters = 48 bytes: a title limit of 24 bytes rejects it
  mk(0, 0, 0, 0, 0, 0, 0,
     "{\"title\":\"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\","
     "\"lines\":[]}");
  expectFail("notes.title");
  // exactly NOTE_TITLE_MAX bytes of two-byte characters is accepted
  mk(0, 0, 0, 0, 0, 0, 0,
     "{\"title\":\"\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\","
     "\"lines\":[]}");
  TEST_ASSERT_TRUE(valid());
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"a\\u0000b\"}]", 0); expectFail("messages[0].text");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"a\\nb\"}]", 0); expectFail("messages[0].text");
  mk(0, 0, 0, 0, 0, 0, "[{\"text\":\"ok\",\"by\":\"a\\tb\"}]", 0); expectFail("messages[0].by");
  mk(0, 0, "{\"iana\":\"U\\u0001TC\",\"posix\":\"UTC0\"}", 0, 0, 0, 0, 0);
  expectFail("tz.iana");
  mk(0, 0, 0, 0, 0, "{\"url\":\"https://a\\u001f\"}", 0, 0); expectFail("calendar.url");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_document_is_valid);
  RUN_TEST(test_not_json_and_wrong_root);
  RUN_TEST(test_too_large_is_not_parsed);
  RUN_TEST(test_version);
  RUN_TEST(test_unknown_top_level_key);
  RUN_TEST(test_missing_required_keys);
  RUN_TEST(test_dwell);
  RUN_TEST(test_tz);
  RUN_TEST(test_pages);
  RUN_TEST(test_weather);
  RUN_TEST(test_calendar);
  RUN_TEST(test_messages);
  RUN_TEST(test_too_many_messages);
  RUN_TEST(test_notes);
  RUN_TEST(test_bytes_not_characters_and_control_chars);
  return UNITY_END();
}

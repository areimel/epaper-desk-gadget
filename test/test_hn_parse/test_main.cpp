#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "../support/Fixture.h"
#include "HnParse.h"

void setUp() {}
void tearDown() {}

static HnSnap snap;

static void sentinel() {
  memset(&snap, 0xA5, sizeof snap);
  snap.status.state = SourceState::Error;
  snap.status.updated = 777;
}

static void assertUntouched() {
  HnSnap want;
  memset(&want, 0xA5, sizeof want);
  want.status.state = SourceState::Error;
  want.status.updated = 777;
  TEST_ASSERT_EQUAL_MEMORY(&want, &snap, sizeof want);
}

static void assertStatusKept() {
  TEST_ASSERT_EQUAL(SourceState::Error, snap.status.state);
  TEST_ASSERT_EQUAL(777, snap.status.updated);
  TEST_ASSERT_EQUAL_HEX8(0xA5, static_cast<uint8_t>(snap.status.error[7]));
}

static bool parse(const char* j) { return parseHn(j, strlen(j), snap); }

void test_url_exact_and_small_buffer() {
  const char* want =
      "https://hn.algolia.com/api/v1/search?tags=front_page&hitsPerPage=9"
      "&attributesToRetrieve=title,points,num_comments&attributesToHighlight=";
  char url[200];
  TEST_ASSERT_EQUAL(strlen(want), hnUrl(url, sizeof url));
  TEST_ASSERT_EQUAL_STRING(want, url);
  TEST_ASSERT_EQUAL(0, hnUrl(url, strlen(want)));  // no room for the NUL
  TEST_ASSERT_EQUAL(strlen(want), hnUrl(url, strlen(want) + 1));
  TEST_ASSERT_EQUAL(0, hnUrl(url, 0));
}

void test_fixture_front_page() {
  Fixture f("hn_front.json");
  sentinel();
  TEST_ASSERT_TRUE(parseHn(f.buf, f.len, snap));
  TEST_ASSERT_EQUAL(9, snap.count);
  TEST_ASSERT_EQUAL_STRING("Nvidia Reinstates Jensen Huang as CEO", snap.stories[0].title);
  TEST_ASSERT_EQUAL(1951, snap.stories[0].points);
  TEST_ASSERT_EQUAL(3044, snap.stories[0].comments);
  TEST_ASSERT_EQUAL_STRING("Ladybird is now in alpha", snap.stories[1].title);
  TEST_ASSERT_EQUAL(1314, snap.stories[1].points);
  TEST_ASSERT_EQUAL(315, snap.stories[1].comments);
  TEST_ASSERT_EQUAL_STRING("Per Arm's request, this site will be discontinued on 4 December 2026 :(",
                           snap.stories[5].title);
  TEST_ASSERT_EQUAL(789, snap.stories[5].points);
  TEST_ASSERT_EQUAL(160, snap.stories[5].comments);
  TEST_ASSERT_EQUAL_STRING("Spacecraft reaches Mercury on second-ever mission to enter its orbit",
                           snap.stories[8].title);
  TEST_ASSERT_EQUAL(279, snap.stories[8].points);
  TEST_ASSERT_EQUAL(88, snap.stories[8].comments);
  assertStatusKept();
}

void test_titles_are_folded() {
  // emoji, CJK, curly quotes (as raw UTF-8), and a JSON \u escape for a curly double quote.
  const char* j =
      "{\"hits\":[{\"title\":\"Hi \xF0\x9F\x98\x80 there\",\"points\":1,\"num_comments\":2},"
      "{\"title\":\"\xE6\x97\xA5\xE6\x9C\xAC\",\"points\":1,\"num_comments\":2},"
      "{\"title\":\"It\xE2\x80\x99s \xE2\x80\x9Cquoted\xE2\x80\x9D\",\"points\":1,\"num_comments\":2},"
      "{\"title\":\"\\u201cesc\\u201d caf\\u00e9\",\"points\":1,\"num_comments\":2}]}";
  sentinel();
  TEST_ASSERT_TRUE(parse(j));
  TEST_ASSERT_EQUAL(4, snap.count);
  TEST_ASSERT_EQUAL_STRING("Hi ? there", snap.stories[0].title);
  TEST_ASSERT_EQUAL_STRING("?", snap.stories[1].title);  // runs of unmappable characters collapse
  TEST_ASSERT_EQUAL_STRING("It's \"quoted\"", snap.stories[2].title);
  TEST_ASSERT_EQUAL_STRING("\"esc\" caf\xE9", snap.stories[3].title);
  assertStatusKept();
}

void test_long_title_is_cut_with_nul() {
  char j[400];
  char title[200];
  memset(title, 'a', 150);
  title[150] = 0;
  snprintf(j, sizeof j, "{\"hits\":[{\"title\":\"%s\",\"points\":1,\"num_comments\":1}]}", title);
  sentinel();
  TEST_ASSERT_TRUE(parse(j));
  TEST_ASSERT_EQUAL(1, snap.count);
  TEST_ASSERT_EQUAL(limits::HN_TITLE_MAX, strlen(snap.stories[0].title));
  TEST_ASSERT_EQUAL(0, snap.stories[0].title[limits::HN_TITLE_MAX]);
}

void test_twelve_hits_give_nine_stories() {
  char j[1200] = "{\"hits\":[";
  for (int i = 0; i < 12; i++) {
    char one[80];
    snprintf(one, sizeof one, "%s{\"title\":\"T%d\",\"points\":%d,\"num_comments\":%d}", i ? "," : "", i, i * 10, i);
    strcat(j, one);
  }
  strcat(j, "]}");
  sentinel();
  TEST_ASSERT_TRUE(parse(j));
  TEST_ASSERT_EQUAL(9, snap.count);
  TEST_ASSERT_EQUAL_STRING("T0", snap.stories[0].title);
  TEST_ASSERT_EQUAL_STRING("T8", snap.stories[8].title);
  TEST_ASSERT_EQUAL(80, snap.stories[8].points);
}

void test_counts_null_missing_negative_and_huge() {
  const char* j =
      "{\"hits\":[{\"title\":\"a\",\"points\":null,\"num_comments\":5},"
      "{\"title\":\"b\",\"points\":7},"
      "{\"title\":\"c\",\"points\":-3,\"num_comments\":70000},"
      "{\"title\":\"d\",\"points\":65535,\"num_comments\":\"x\"}]}";
  sentinel();
  TEST_ASSERT_TRUE(parse(j));
  TEST_ASSERT_EQUAL(4, snap.count);
  TEST_ASSERT_EQUAL(0, snap.stories[0].points);
  TEST_ASSERT_EQUAL(5, snap.stories[0].comments);
  TEST_ASSERT_EQUAL(7, snap.stories[1].points);
  TEST_ASSERT_EQUAL(0, snap.stories[1].comments);
  TEST_ASSERT_EQUAL(0, snap.stories[2].points);
  TEST_ASSERT_EQUAL(65535, snap.stories[2].comments);
  TEST_ASSERT_EQUAL(65535, snap.stories[3].points);
  TEST_ASSERT_EQUAL(0, snap.stories[3].comments);
}

void test_hits_without_usable_title_are_skipped() {
  const char* j =
      "{\"hits\":[{\"points\":1},{\"title\":null,\"points\":2},{\"title\":\"\",\"points\":3},"
      "{\"title\":42,\"points\":4},{\"title\":\"ok\",\"points\":5,\"num_comments\":6}]}";
  sentinel();
  TEST_ASSERT_TRUE(parse(j));
  TEST_ASSERT_EQUAL(1, snap.count);
  TEST_ASSERT_EQUAL_STRING("ok", snap.stories[0].title);
  TEST_ASSERT_EQUAL(5, snap.stories[0].points);
  TEST_ASSERT_EQUAL(6, snap.stories[0].comments);
}

void test_replaces_earlier_content() {
  sentinel();
  TEST_ASSERT_TRUE(parse("{\"hits\":[{\"title\":\"one\"},{\"title\":\"two\"}]}"));
  TEST_ASSERT_EQUAL(2, snap.count);
  TEST_ASSERT_TRUE(parse("{\"hits\":[{\"title\":\"three\"}]}"));
  TEST_ASSERT_EQUAL(1, snap.count);
  TEST_ASSERT_EQUAL_STRING("three", snap.stories[0].title);
}

void test_empty_hits_is_usable() {
  sentinel();
  TEST_ASSERT_TRUE(parse("{\"hits\":[]}"));
  TEST_ASSERT_EQUAL(0, snap.count);
  assertStatusKept();
}

void test_unusable_replies_leave_snap_untouched() {
  Fixture f("hn_front.json");
  sentinel();
  TEST_ASSERT_FALSE(parseHn(f.buf, f.len / 2, snap));  // truncated
  assertUntouched();
  TEST_ASSERT_FALSE(parse("{}"));
  assertUntouched();
  TEST_ASSERT_FALSE(parse("{\"hits\":\"nope\"}"));
  assertUntouched();
  TEST_ASSERT_FALSE(parse("{\"hits\":null}"));
  assertUntouched();
  TEST_ASSERT_FALSE(parse("<html>502 Bad Gateway</html>"));
  assertUntouched();
  TEST_ASSERT_FALSE(parseHn("", 0, snap));
  assertUntouched();
}

void test_respects_length_without_nul() {
  Fixture f("hn_front.json");
  static char exact[Fixture::CAP + 64];
  memset(exact, 'x', sizeof exact);
  memcpy(exact, f.buf, f.len);
  sentinel();
  TEST_ASSERT_TRUE(parseHn(exact, f.len, snap));
  TEST_ASSERT_EQUAL(9, snap.count);
  sentinel();
  TEST_ASSERT_FALSE(parseHn(exact, f.len - 3, snap));
  assertUntouched();
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_url_exact_and_small_buffer);
  RUN_TEST(test_fixture_front_page);
  RUN_TEST(test_titles_are_folded);
  RUN_TEST(test_long_title_is_cut_with_nul);
  RUN_TEST(test_twelve_hits_give_nine_stories);
  RUN_TEST(test_counts_null_missing_negative_and_huge);
  RUN_TEST(test_hits_without_usable_title_are_skipped);
  RUN_TEST(test_replaces_earlier_content);
  RUN_TEST(test_empty_hits_is_usable);
  RUN_TEST(test_unusable_replies_leave_snap_untouched);
  RUN_TEST(test_respects_length_without_nul);
  return UNITY_END();
}

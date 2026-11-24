#include <unity.h>

#include "FeedConfig.h"
#include "Snapshots.h"

void setUp() {}
void tearDown() {}

static FeedConfig config;

void test_page_names_round_trip() {
  const char* names[] = {"clock", "weather", "hn", "messages", "calendar", "notes"};
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++) {
    PageId id;
    TEST_ASSERT_TRUE(feed::pageFromName(names[i], id));
    TEST_ASSERT_EQUAL(i, static_cast<uint8_t>(id));
    TEST_ASSERT_EQUAL_STRING(names[i], feed::pageName(id));
  }
}

void test_unknown_page_name_is_rejected() {
  PageId id;
  TEST_ASSERT_FALSE(feed::pageFromName("news", id));
  TEST_ASSERT_FALSE(feed::pageFromName("", id));
}

void test_defaults() {
  feed::setDefaults(config);
  TEST_ASSERT_EQUAL(300, config.dwellSec);
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++) {
    TEST_ASSERT_TRUE(config.pages[i].on);
    TEST_ASSERT_EQUAL(i, static_cast<uint8_t>(config.pages[i].id));
  }
  TEST_ASSERT_EQUAL_STRING("UTC0", config.tzPosix);
  TEST_ASSERT_FALSE(config.weather.set);
  TEST_ASSERT_EQUAL(0, config.messageCount);
  TEST_ASSERT_EQUAL_STRING("", config.calendarUrl);
  TEST_ASSERT_EQUAL_STRING("Today", config.notes.title);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_page_names_round_trip);
  RUN_TEST(test_unknown_page_name_is_rejected);
  RUN_TEST(test_defaults);
  return UNITY_END();
}

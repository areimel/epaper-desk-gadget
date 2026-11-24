#include <string.h>
#include <unity.h>

#include "Text.h"

void setUp() {}
void tearDown() {}

static const time_t NOW = 1000000;

static void expectAge(long long secondsAgo, const char* want) {
  char buf[16];
  text::age(NOW, NOW - static_cast<time_t>(secondsAgo), buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING(want, buf);
}

void test_never() {
  char buf[16];
  text::age(NOW, 0, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("never", buf);
}
void test_just_now() {
  expectAge(0, "just now");
  expectAge(59, "just now");
}
void test_future_is_just_now() {
  expectAge(-30, "just now");
  expectAge(-60, "just now");
  expectAge(-5000, "just now");
}
void test_minutes() {
  expectAge(60, "1 min ago");
  expectAge(59 * 60 + 59, "59 min ago");
}
void test_hours() {
  expectAge(3600, "1 h ago");
  expectAge(23 * 3600 + 59 * 60, "23 h ago");
}
void test_yesterday() {
  expectAge(24 * 3600, "yesterday");
  expectAge(47 * 3600 + 3599, "yesterday");
}
void test_days() {
  expectAge(48 * 3600, "2 days ago");
  expectAge(10 * 86400, "10 days ago");
}
void test_fits_in_16() {
  char buf[16];
  text::age(NOW + 999 * 86400, NOW, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("999 days ago", buf);
  TEST_ASSERT_TRUE(strlen(buf) < 16);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_never);
  RUN_TEST(test_just_now);
  RUN_TEST(test_future_is_just_now);
  RUN_TEST(test_minutes);
  RUN_TEST(test_hours);
  RUN_TEST(test_yesterday);
  RUN_TEST(test_days);
  RUN_TEST(test_fits_in_16);
  return UNITY_END();
}

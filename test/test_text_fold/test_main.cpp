#include <string.h>
#include <unity.h>

#include "Text.h"

void setUp() {}
void tearDown() {}

static void check(const char* in, const char* want) {
  char buf[128];
  size_t n = text::fold(in, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING(want, buf);
  TEST_ASSERT_EQUAL(strlen(want), n);
}

void test_ascii_and_empty() {
  check("Hello, World ~!", "Hello, World ~!");
  check("", "");
}
void test_latin1() {
  check("caf\xC3\xA9", "caf\xE9");
  check("20\xC2\xB0", "20\xB0");
  check("\xC3\xBF", "\xFF");
}
void test_punctuation() {
  check("\xE2\x80\x98x\xE2\x80\x99\xE2\x80\x9A\xE2\x80\xB2", "'x'''");
  check("\xE2\x80\x9Cx\xE2\x80\x9D\xE2\x80\x9E\xE2\x80\xB3", "\"x\"\"\"");
  check("a\xE2\x80\x93" "b\xE2\x80\x94" "c\xE2\x88\x92" "d", "a-b-c-d");
  check("wait\xE2\x80\xA6", "wait...");
  check("\xE2\x80\xA2 item", "* item");
}
void test_spaces() {
  check("a\xC2\xA0" "b\xE2\x80\x89" "c\xE2\x80\x8A" "d\xE2\x80\xAF" "e", "a b c d e");
  check("a\xE2\x80\x8B" "b\xE2\x80\x8C" "c\xE2\x80\x8D" "d\xEF\xBB\xBF" "e", "abcde");
  check("a\tb\r\nc", "a b c");
  check("  a    b  ", "a b");
  check("   ", "");
  check("a \xE2\x80\x8B b", "a b");
}
void test_controls_dropped() {
  check("a\x01" "b\x1F" "c\x7F" "d", "abcd");
  check("a\xC2\x80" "b\xC2\x9F" "c", "abc");
}
void test_unmappable() {
  check("hi \xF0\x9F\x98\x80 there", "hi ? there");
  check("\xF0\x9F\x98\x80\xF0\x9F\x98\x80", "?");
  check("\xE4\xB8\xAD\xE6\x96\x87", "?");
  check("a\xE4\xB8\xAD" "b", "a?b");
  check("\xE2\x82\xAC", "?");  // euro sign is not Latin-1
}
void test_malformed() {
  check("a\x80" "b", "a?b");
  check("a\xC3", "a?");               // truncated at NUL
  check("a\xE2\x82", "a?");           // truncated 3-byte
  check("a\xC3(b", "a?(b");           // bad continuation is not swallowed
  check("a\xC0\xAF" "b", "a?b");      // overlong "/"
  check("a\xE0\x80\xAF" "b", "a?b");  // overlong 3-byte
  check("\xED\xA0\x80", "?");         // surrogate
  check("\xF5\x80\x80\x80", "?");
}
void test_truncation() {
  char buf[8];
  memset(buf, 'X', sizeof buf);
  TEST_ASSERT_EQUAL(4, text::fold("abcdefgh", buf, 5));
  TEST_ASSERT_EQUAL_STRING("abcd", buf);
  TEST_ASSERT_EQUAL(0, text::fold("abc", buf, 1));
  TEST_ASSERT_EQUAL('\0', buf[0]);
  memset(buf, 'X', sizeof buf);
  TEST_ASSERT_EQUAL(0, text::fold("abc", buf, 0));
  TEST_ASSERT_EQUAL('X', buf[0]);
  TEST_ASSERT_EQUAL(2, text::fold("ab cd", buf, 4));  // cut after the space, which is trimmed
  TEST_ASSERT_EQUAL_STRING("ab", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ascii_and_empty);
  RUN_TEST(test_latin1);
  RUN_TEST(test_punctuation);
  RUN_TEST(test_spaces);
  RUN_TEST(test_controls_dropped);
  RUN_TEST(test_unmappable);
  RUN_TEST(test_malformed);
  RUN_TEST(test_truncation);
  return UNITY_END();
}

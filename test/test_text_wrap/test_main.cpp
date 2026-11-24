#include <string.h>
#include <unity.h>

#include "Text.h"

void setUp() {}
void tearDown() {}

// Every character is 6 px wide.
struct Fixed : Measure {
  int width(Font, const char*, size_t n) const override { return static_cast<int>(n) * 6; }
};
static Fixed fixed;

static void expectLine(const text::Line& l, const char* want, bool ellipsis) {
  TEST_ASSERT_EQUAL(strlen(want), l.len);
  TEST_ASSERT_EQUAL_STRING_LEN(want, l.start, l.len);
  TEST_ASSERT_EQUAL(ellipsis, l.ellipsis);
}

void test_one_word() {
  text::Line out[4];
  TEST_ASSERT_EQUAL(1, text::wrap(fixed, Font::Body, "hello", 100, 4, out));
  expectLine(out[0], "hello", false);
}
void test_empty_and_spaces() {
  text::Line out[4];
  TEST_ASSERT_EQUAL(0, text::wrap(fixed, Font::Body, "", 100, 4, out));
  TEST_ASSERT_EQUAL(0, text::wrap(fixed, Font::Body, "    ", 100, 4, out));
}
void test_bad_limits() {
  text::Line out[4];
  TEST_ASSERT_EQUAL(0, text::wrap(fixed, Font::Body, "abc", 0, 4, out));
  TEST_ASSERT_EQUAL(0, text::wrap(fixed, Font::Body, "abc", -5, 4, out));
  TEST_ASSERT_EQUAL(0, text::wrap(fixed, Font::Body, "abc", 100, 0, out));
}
void test_breaks_at_space() {
  text::Line out[4];
  const char* s = "hello world";  // 11 chars, 60 px holds 10
  TEST_ASSERT_EQUAL(2, text::wrap(fixed, Font::Body, s, 60, 4, out));
  expectLine(out[0], "hello", false);
  expectLine(out[1], "world", false);
  TEST_ASSERT_EQUAL_PTR(s, out[0].start);
  TEST_ASSERT_EQUAL_PTR(s + 6, out[1].start);
}
void test_fits_exactly_no_ellipsis() {
  text::Line out[2];
  TEST_ASSERT_EQUAL(2, text::wrap(fixed, Font::Body, "aaaaa bbbbb ccccc", 66, 2, out));
  expectLine(out[0], "aaaaa bbbbb", false);
  expectLine(out[1], "ccccc", false);
}
void test_long_word_split() {
  char word[91];
  memset(word, 'x', 90);
  word[90] = '\0';
  text::Line out[12];
  TEST_ASSERT_EQUAL(9, text::wrap(fixed, Font::Body, word, 60, 12, out));
  for (int i = 0; i < 9; i++) {
    TEST_ASSERT_EQUAL(10, out[i].len);
    TEST_ASSERT_EQUAL_PTR(word + i * 10, out[i].start);
    TEST_ASSERT_FALSE(out[i].ellipsis);
  }
}
void test_ellipsis() {
  const char* s = "aaaaaaaaa bbbbbbbbb ccccccccc ddddddddd eeeeeeeee";  // 5 lines at 60 px
  text::Line out[2];
  TEST_ASSERT_EQUAL(2, text::wrap(fixed, Font::Body, s, 60, 2, out));
  expectLine(out[0], "aaaaaaaaa", false);
  TEST_ASSERT_TRUE(out[1].ellipsis);
  TEST_ASSERT_TRUE(static_cast<int>(out[1].len) * 6 + 18 <= 60);
  TEST_ASSERT_EQUAL_PTR(s + 10, out[1].start);
  TEST_ASSERT_EQUAL_STRING_LEN("bbbbbbb", out[1].start, out[1].len);
}
void test_ellipsis_drops_trailing_space() {
  text::Line out[1];
  // Cutting "ab cde" (36 px) to leave room for "..." (18 px) in 40 px lands after "ab ".
  TEST_ASSERT_EQUAL(1, text::wrap(fixed, Font::Body, "ab cde fghij", 40, 1, out));
  expectLine(out[0], "ab", true);
}
void test_narrower_than_one_char() {
  text::Line out[8];
  TEST_ASSERT_EQUAL(3, text::wrap(fixed, Font::Body, "abc", 3, 8, out));
  for (int i = 0; i < 3; i++) TEST_ASSERT_EQUAL(1, out[i].len);
  TEST_ASSERT_EQUAL(2, text::wrap(fixed, Font::Body, "a b", 1, 8, out));
}
void test_multiple_spaces() {
  text::Line out[4];
  TEST_ASSERT_EQUAL(1, text::wrap(fixed, Font::Body, "   aa     bb   ", 100, 4, out));
  expectLine(out[0], "aa     bb", false);  // inner spaces are kept, outer ones are not
  TEST_ASSERT_EQUAL(3, text::wrap(fixed, Font::Body, "   aa     bb   cc   ", 24, 4, out));
  expectLine(out[0], "aa", false);
  expectLine(out[1], "bb", false);
  expectLine(out[2], "cc", false);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_one_word);
  RUN_TEST(test_empty_and_spaces);
  RUN_TEST(test_bad_limits);
  RUN_TEST(test_breaks_at_space);
  RUN_TEST(test_fits_exactly_no_ellipsis);
  RUN_TEST(test_long_word_split);
  RUN_TEST(test_ellipsis);
  RUN_TEST(test_ellipsis_drops_trailing_space);
  RUN_TEST(test_narrower_than_one_char);
  RUN_TEST(test_multiple_spaces);
  return UNITY_END();
}

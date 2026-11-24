#include <initializer_list>
#include <string.h>
#include <unity.h>

#include "../support/CanvasDump.h"
#include "Draw.h"
#include "Fonts.h"

void setUp() {}
void tearDown() {}

static Canvas canvas;

// Asserts every black pixel lies inside the box (and that there is some).
static void assertInkInside(const draw::Box& b) {
  dump::Ink ink = dump::inkAll(canvas);
  TEST_ASSERT_GREATER_THAN(0, ink.count);
  TEST_ASSERT_TRUE(ink.minX >= b.x && ink.maxX < b.x + b.w);
  TEST_ASSERT_TRUE(ink.minY >= b.y && ink.maxY < b.y + b.h);
}

void test_paragraph_stays_in_box_and_counts_lines() {
  canvas.clear();
  draw::Box box{10, 20, 120, 60};
  int n = draw::paragraph(canvas, Font::Body, box,
                          "The quick brown fox jumps over the lazy dog and keeps running", 3);
  TEST_ASSERT_EQUAL(3, n);
  assertInkInside(box);
}

void test_paragraph_short_text_is_one_line() {
  canvas.clear();
  draw::Box box{10, 20, 200, 60};
  TEST_ASSERT_EQUAL(1, draw::paragraph(canvas, Font::Body, box, "Hello there", 4));
  assertInkInside(box);
}

void test_paragraph_ellipsis_on_overflow() {
  Canvas cut, whole;
  draw::Box box{4, 20, 100, 60};
  const char* s = "alpha beta gamma delta epsilon zeta eta theta iota kappa";
  TEST_ASSERT_EQUAL(2, draw::paragraph(cut, Font::Body, box, s, 2));
  // The text fits in more lines than 2, so the second line was cut: it must differ from the same
  // two lines drawn when three are allowed (where line 2 is not shortened).
  draw::paragraph(whole, Font::Body, box, s, 4);
  TEST_ASSERT_TRUE(memcmp(cut.rows(), whole.rows(), Canvas::PACKED_BYTES) != 0);
  // Dots at the end of line 2: ink near the right edge of the box on its baseline.
  int base2 = 20 + fonts::ascent(Font::Body) + fonts::lineHeight(Font::Body);
  TEST_ASSERT_GREATER_THAN(0, dump::ink(cut, 80, base2 - 3, 104, base2 + 1).count);
}

void test_paragraph_limited_by_box_height() {
  canvas.clear();
  draw::Box box{4, 20, 200, 40};  // Body lineHeight 15: three lines of ~15 do not fit in 40
  int n = draw::paragraph(canvas, Font::Body, box,
                          "one two three four five six seven eight nine ten eleven twelve "
                          "thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty",
                          10);
  TEST_ASSERT_TRUE(n >= 2 && n <= 3);
  assertInkInside(box);
}

void test_paragraph_box_too_small_draws_nothing() {
  canvas.clear();
  TEST_ASSERT_EQUAL(0, draw::paragraph(canvas, Font::Quote18, draw::Box{0, 20, 100, 10}, "Hi", 3));
  TEST_ASSERT_EQUAL(0, dump::inkAll(canvas).count);
}

void test_paragraph_null_empty_and_zero_lines() {
  canvas.clear();
  draw::Box box{0, 20, 100, 60};
  TEST_ASSERT_EQUAL(0, draw::paragraph(canvas, Font::Body, box, nullptr, 3));
  TEST_ASSERT_EQUAL(0, draw::paragraph(canvas, Font::Body, box, "", 3));
  TEST_ASSERT_EQUAL(0, draw::paragraph(canvas, Font::Body, box, "text", 0));
  TEST_ASSERT_EQUAL(0, dump::inkAll(canvas).count);
}

void test_paragraph_long_unbroken_word_stays_in_box() {
  canvas.clear();
  draw::Box box{10, 20, 80, 50};
  draw::paragraph(canvas, Font::Body, box, "Supercalifragilisticexpialidocious_and_more", 3);
  assertInkInside(box);
}

static const Font kLadder[] = {Font::Quote18, Font::Quote14, Font::BodyBold};

void test_fitted_short_text_uses_largest_font() {
  canvas.clear();
  draw::Box box{10, 20, 230, 90};
  Font f = draw::fitted(canvas, kLadder, 3, box, "Stay curious about everything");
  TEST_ASSERT_EQUAL(static_cast<int>(Font::Quote18), static_cast<int>(f));
  assertInkInside(box);
}

void test_fitted_long_text_steps_down() {
  canvas.clear();
  draw::Box box{10, 20, 230, 90};
  const char* s =
      "You are doing better than you think, and the small steps you take every single day add up "
      "to something truly remarkable over time, so keep going, one step at a time, always.";
  TEST_ASSERT_GREATER_OR_EQUAL(140, strlen(s));
  Font f = draw::fitted(canvas, kLadder, 3, box, s);
  TEST_ASSERT_TRUE(f != Font::Quote18);
  assertInkInside(box);
}

void test_fitted_overflow_uses_last_font_with_ellipsis() {
  canvas.clear();
  draw::Box box{10, 20, 230, 40};
  char s[400];
  memset(s, 0, sizeof s);
  for (int i = 0; i < 60; i++) strcat(s, "word ");
  Font f = draw::fitted(canvas, kLadder, 3, box, s);
  TEST_ASSERT_EQUAL(static_cast<int>(Font::BodyBold), static_cast<int>(f));
  assertInkInside(box);
}

void test_fitted_is_centred() {
  canvas.clear();
  draw::Box box{10, 20, 230, 90};
  draw::fitted(canvas, kLadder, 3, box, "Hello");
  dump::Ink ink = dump::inkAll(canvas);
  TEST_ASSERT_INT_WITHIN(3, 2 * 125, ink.minX + ink.maxX);   // horizontally
  TEST_ASSERT_INT_WITHIN(12, 2 * 65, ink.minY + ink.maxY);   // vertically (ascender/descender slack)
}

void test_fitted_edge_cases() {
  canvas.clear();
  draw::Box box{10, 20, 230, 90};
  draw::fitted(canvas, kLadder, 3, box, nullptr);
  draw::fitted(canvas, kLadder, 3, box, "");
  TEST_ASSERT_EQUAL(0, dump::inkAll(canvas).count);
  Font f = draw::fitted(canvas, kLadder, 1, box, "One font only");
  TEST_ASSERT_EQUAL(static_cast<int>(Font::Quote18), static_cast<int>(f));
}

void test_checkbox_done_differs_and_stays_in_11x11() {
  Canvas open, done;
  draw::checkbox(open, 20, 30, false);
  draw::checkbox(done, 20, 30, true);
  TEST_ASSERT_TRUE(memcmp(open.rows(), done.rows(), Canvas::PACKED_BYTES) != 0);
  for (Canvas* c : {&open, &done}) {
    dump::Ink ink = dump::inkAll(*c);
    TEST_ASSERT_EQUAL(20, ink.minX);
    TEST_ASSERT_EQUAL(30, ink.minY);
    TEST_ASSERT_EQUAL(30, ink.maxX);
    TEST_ASSERT_EQUAL(40, ink.maxY);
  }
  // 2 px outline: both rings are black on the empty box, the middle is white.
  TEST_ASSERT_TRUE(open.pixel(20, 35) && open.pixel(21, 35) && !open.pixel(22, 35));
  TEST_ASSERT_FALSE(open.pixel(25, 35));
  TEST_ASSERT_GREATER_THAN(0, dump::ink(done, 22, 32, 29, 39).count);
}

void test_empty_state_with_and_without_second_line() {
  Canvas one, two;
  draw::emptyState(one, "Nothing else today", nullptr);
  draw::emptyState(two, "Nothing else today", "Enjoy the quiet");
  dump::Ink a = dump::inkAll(one), b = dump::inkAll(two);
  TEST_ASSERT_GREATER_THAN(0, a.count);
  TEST_ASSERT_GREATER_THAN(a.count, b.count);
  for (const dump::Ink& i : {a, b}) {
    TEST_ASSERT_TRUE(i.minY >= draw::TITLE_BAR_H && i.maxY < Canvas::H);
    TEST_ASSERT_INT_WITHIN(3, 2 * 125, i.minX + i.maxX);
  }
  // Oversized text is cut rather than drawn off the panel.
  Canvas big;
  draw::emptyState(big, "An absurdly long line of text that cannot fit on the little panel ever", "x");
  dump::Ink c = dump::inkAll(big);
  TEST_ASSERT_TRUE(c.minX >= 0 && c.maxX <= 249);
}

void test_degree_ring_is_aligned_with_numerals() {
  for (Font f : {Font::Num32, Font::NumBig}) {
    canvas.clear();
    draw::text(canvas, f, 10, 80, "1");
    int w = draw::degree(canvas, f, 40, 80);
    dump::Ink ring = dump::ink(canvas, 38, 0, 80, Canvas::H);
    dump::Ink digit = dump::ink(canvas, 0, 0, 38, Canvas::H);
    TEST_ASSERT_EQUAL(digit.minY, ring.minY);  // top of the ring = top of the digits
    TEST_ASSERT_EQUAL(40, ring.minX);
    TEST_ASSERT_EQUAL(40 + w - 1, ring.maxX);
    TEST_ASSERT_TRUE(w >= 6 && w <= 10);
    TEST_ASSERT_FALSE(canvas.pixel(40 + w / 2, ring.minY + w / 2));  // hollow
    TEST_ASSERT_TRUE(canvas.pixel(40, ring.minY + w / 2) && canvas.pixel(41, ring.minY + w / 2));  // 2 px stroke
  }
}

void test_hairline_is_one_pixel() {
  canvas.clear();
  draw::hairline(canvas, 10, 50, 100);
  dump::Ink ink = dump::inkAll(canvas);
  TEST_ASSERT_EQUAL(100, ink.count);
  TEST_ASSERT_EQUAL(50, ink.minY);
  TEST_ASSERT_EQUAL(50, ink.maxY);
  TEST_ASSERT_EQUAL(10, ink.minX);
  TEST_ASSERT_EQUAL(109, ink.maxX);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_paragraph_stays_in_box_and_counts_lines);
  RUN_TEST(test_paragraph_short_text_is_one_line);
  RUN_TEST(test_paragraph_ellipsis_on_overflow);
  RUN_TEST(test_paragraph_limited_by_box_height);
  RUN_TEST(test_paragraph_box_too_small_draws_nothing);
  RUN_TEST(test_paragraph_null_empty_and_zero_lines);
  RUN_TEST(test_paragraph_long_unbroken_word_stays_in_box);
  RUN_TEST(test_fitted_short_text_uses_largest_font);
  RUN_TEST(test_fitted_long_text_steps_down);
  RUN_TEST(test_fitted_overflow_uses_last_font_with_ellipsis);
  RUN_TEST(test_fitted_is_centred);
  RUN_TEST(test_fitted_edge_cases);
  RUN_TEST(test_checkbox_done_differs_and_stays_in_11x11);
  RUN_TEST(test_empty_state_with_and_without_second_line);
  RUN_TEST(test_degree_ring_is_aligned_with_numerals);
  RUN_TEST(test_hairline_is_one_pixel);
  return UNITY_END();
}

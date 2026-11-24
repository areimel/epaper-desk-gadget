#include <string.h>
#include <unity.h>

#include "../support/CanvasDump.h"
#include "Draw.h"
#include "Fonts.h"
#include "U8g2Measure.h"

void setUp() {}
void tearDown() {}

static Canvas canvas;

// Number of black pixels in row y.
static int rowInk(int y) { return dump::ink(canvas, 0, y, Canvas::W, y + 1).count; }

void test_title_bar_is_black_with_white_glyphs() {
  canvas.clear();
  draw::titleBar(canvas, "Weather", "updated 9:30");
  for (int y = 0; y < 14; y++) TEST_ASSERT_GREATER_THAN(150, rowInk(y));  // mostly black
  TEST_ASSERT_EQUAL(0, rowInk(14));                                      // row 14 stays white
  TEST_ASSERT_EQUAL(0, dump::ink(canvas, 0, 15, Canvas::W, Canvas::H).count);
  // Some white pixels exist inside the bar (the text), and the bar edges are solid black.
  int white = 0;
  for (int y = 0; y < 14; y++)
    for (int x = 0; x < Canvas::W; x++) white += !canvas.pixel(x, y);
  TEST_ASSERT_GREATER_THAN(60, white);
  TEST_ASSERT_TRUE(canvas.pixel(0, 0));
  TEST_ASSERT_TRUE(canvas.pixel(249, 13));
  // The draw colour is back to black for whatever comes next.
  TEST_ASSERT_EQUAL(1, u8g2_GetDrawColor(canvas.g()));
}

void test_title_bar_without_note_and_with_null_title() {
  canvas.clear();
  draw::titleBar(canvas, "Notes", nullptr);
  TEST_ASSERT_EQUAL(0, rowInk(14));
  canvas.clear();
  draw::titleBar(canvas, nullptr, nullptr);
  TEST_ASSERT_EQUAL(250 * 14, dump::ink(canvas, 0, 0, 250, 14).count);  // solid bar
}

void test_title_bar_right_note_is_right_aligned() {
  canvas.clear();
  draw::titleBar(canvas, "A", "updated 9:30");
  // White ink of the note ends close to the right margin (3-4 px) and is not at the far left.
  int lastWhite = -1;
  for (int x = 0; x < Canvas::W; x++)
    for (int y = 0; y < 14; y++)
      if (!canvas.pixel(x, y)) lastWhite = x;
  TEST_ASSERT_TRUE(lastWhite >= 244 && lastWhite <= 247);
}

void test_title_bar_drops_note_on_collision() {
  Canvas withNote, without;
  const char* title = "A fairly long title for the bar that leaves no room";  // leaves < gap to the note
  draw::titleBar(withNote, title, "updated 9:30");
  draw::titleBar(without, title, nullptr);
  TEST_ASSERT_EQUAL_MEMORY(without.rows(), withNote.rows(), Canvas::PACKED_BYTES);
  // A short title keeps the note.
  Canvas a, b;
  draw::titleBar(a, "Hi", "updated 9:30");
  draw::titleBar(b, "Hi", nullptr);
  TEST_ASSERT_TRUE(memcmp(a.rows(), b.rows(), Canvas::PACKED_BYTES) != 0);
}

void test_title_bar_truncates_long_title() {
  Canvas longTitle, cut;
  const char* title =
      "An extremely long title that cannot possibly fit in the title bar of this tiny panel at all";
  draw::titleBar(longTitle, title, "updated 9:30");
  // Nothing outside the bar; right margin of 3 px stays black bar but no white glyph pixel at x >= 247.
  for (int y = 0; y < 14; y++)
    for (int x = 247; x < Canvas::W; x++) TEST_ASSERT_TRUE(longTitle.pixel(x, y));
  TEST_ASSERT_EQUAL(0, dump::ink(longTitle, 0, 14, Canvas::W, Canvas::H).count);
  // Truncated title must differ from a title that merely fits; note is dropped, so no white text
  // sits in the last 20 columns except possibly the dots near the end of the title.
  draw::titleBar(cut, "Short", nullptr);
  TEST_ASSERT_TRUE(memcmp(cut.rows(), longTitle.rows(), Canvas::PACKED_BYTES) != 0);
  // The last glyph of the truncated title ends with dots: a lone 2x2... just check some white at x>230.
  int white = 0;
  for (int y = 0; y < 14; y++)
    for (int x = 225; x < 247; x++) white += !longTitle.pixel(x, y);
  TEST_ASSERT_GREATER_THAN(2, white);
}

void test_text_right_ends_at_right_x() {
  const char* samples[] = {"Wed", "12:45", "abc", "-7\xB0"};
  for (const char* s : samples) {
    canvas.clear();
    draw::textRight(canvas, Font::Body, 200, 30, s);
    dump::Ink ink = dump::inkAll(canvas);
    TEST_ASSERT_EQUAL(199, ink.maxX);
  }
}

void test_text_center_is_centered_within_one_pixel() {
  const char* samples[] = {"Hello", "Weather", "12:45", "mm"};
  for (const char* s : samples) {
    canvas.clear();
    draw::textCenter(canvas, Font::BodyBold, 125, 40, s);
    dump::Ink ink = dump::inkAll(canvas);
    int mid2 = ink.minX + ink.maxX;  // twice the ink centre
    TEST_ASSERT_INT_WITHIN(2, 250, mid2);
  }
}

void test_text_draws_at_baseline() {
  canvas.clear();
  draw::text(canvas, Font::Body, 10, 50, "HEH");  // caps only: no descender
  dump::Ink ink = dump::inkAll(canvas);
  TEST_ASSERT_EQUAL(50 - fonts::ascent(Font::Body), ink.minY);
  TEST_ASSERT_EQUAL(49, ink.maxY);  // bottom row sits just above the baseline row
}

void test_text_null_and_empty_draw_nothing() {
  canvas.clear();
  draw::text(canvas, Font::Body, 10, 20, nullptr);
  draw::textRight(canvas, Font::Body, 100, 20, nullptr);
  draw::textCenter(canvas, Font::Body, 100, 20, nullptr);
  draw::text(canvas, Font::Body, 10, 20, "");
  draw::textRight(canvas, Font::Body, 100, 20, "");
  draw::textCenter(canvas, Font::Body, 100, 20, "");
  TEST_ASSERT_EQUAL(0, dump::inkAll(canvas).count);
  TEST_ASSERT_EQUAL(0, draw::textWidth(Font::Body, nullptr));
  TEST_ASSERT_EQUAL(0, draw::textWidth(Font::Body, ""));
}

void test_latin1_bytes_use_latin1_glyphs() {
  canvas.clear();
  draw::text(canvas, Font::Body, 10, 30, "\xE9");  // e acute: one glyph, not a UTF-8 error
  dump::Ink ink = dump::inkAll(canvas);
  TEST_ASSERT_GREATER_THAN(8, ink.count);
  TEST_ASSERT_TRUE(ink.maxX < 20);
}

void test_measure_agrees_with_text_width() {
  U8g2Measure m;
  for (uint8_t f = 0; f < FONT_COUNT; f++) {
    Font font = static_cast<Font>(f);
    const char* samples[] = {"12:45", "-7\xB0", "Hello, world", "Wed 13/6", ""};
    for (const char* s : samples) {
      TEST_ASSERT_EQUAL(draw::textWidth(font, s), m.width(font, s, strlen(s)));
    }
  }
  // A prefix measured by length equals the width of that prefix as its own string.
  TEST_ASSERT_EQUAL(draw::textWidth(Font::Body, "Hell"), m.width(Font::Body, "Hello", 4));
  TEST_ASSERT_EQUAL(0, m.width(Font::Body, "Hello", 0));
}

void test_measure_matches_u8g2_on_a_canvas() {
  U8g2Measure m;
  Canvas c;
  const char* s = "Quick brown fox \xE9\xFC";
  u8g2_SetFont(c.g(), fonts::data(Font::Quote14));
  TEST_ASSERT_EQUAL(u8g2_GetStrWidth(c.g(), s), m.width(Font::Quote14, s, strlen(s)));
}

void test_measure_does_not_disturb_canvas_font() {
  U8g2Measure m;
  Canvas c;
  u8g2_SetFont(c.g(), u8g2_font_ncenB18_tf);
  const uint8_t* before = c.g()->font;
  m.width(Font::Small, "abc", 3);
  m.width(Font::NumBig, "12", 2);
  TEST_ASSERT_TRUE(c.g()->font == before);
  // Drawing helpers also leave a caller's measuring alone: textWidth does not touch c either.
  draw::textWidth(Font::Body, "x");
  TEST_ASSERT_TRUE(c.g()->font == before);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_title_bar_is_black_with_white_glyphs);
  RUN_TEST(test_title_bar_without_note_and_with_null_title);
  RUN_TEST(test_title_bar_right_note_is_right_aligned);
  RUN_TEST(test_title_bar_drops_note_on_collision);
  RUN_TEST(test_title_bar_truncates_long_title);
  RUN_TEST(test_text_right_ends_at_right_x);
  RUN_TEST(test_text_center_is_centered_within_one_pixel);
  RUN_TEST(test_text_draws_at_baseline);
  RUN_TEST(test_text_null_and_empty_draw_nothing);
  RUN_TEST(test_latin1_bytes_use_latin1_glyphs);
  RUN_TEST(test_measure_agrees_with_text_width);
  RUN_TEST(test_measure_matches_u8g2_on_a_canvas);
  RUN_TEST(test_measure_does_not_disturb_canvas_font);
  return UNITY_END();
}

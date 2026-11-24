#include <unity.h>

#include "Canvas.h"

void setUp() {}
void tearDown() {}

static Canvas canvas;

void test_starts_white() {
  canvas.clear();
  TEST_ASSERT_FALSE(canvas.pixel(0, 0));
  TEST_ASSERT_FALSE(canvas.pixel(249, 121));
}

void test_frame_touches_all_four_edges() {
  canvas.clear();
  u8g2_DrawFrame(canvas.g(), 0, 0, Canvas::W, Canvas::H);
  TEST_ASSERT_TRUE(canvas.pixel(0, 0));
  TEST_ASSERT_TRUE(canvas.pixel(249, 0));
  TEST_ASSERT_TRUE(canvas.pixel(0, 121));
  TEST_ASSERT_TRUE(canvas.pixel(249, 121));
  TEST_ASSERT_FALSE(canvas.pixel(125, 60));
}

void test_outside_reads_white() {
  canvas.clear();
  u8g2_DrawBox(canvas.g(), 0, 0, Canvas::W, Canvas::H);
  TEST_ASSERT_FALSE(canvas.pixel(-1, 0));
  TEST_ASSERT_FALSE(canvas.pixel(250, 0));
  TEST_ASSERT_FALSE(canvas.pixel(0, 122));
}

void test_text_draws_ink_inside_its_box() {
  canvas.clear();
  u8g2_SetFont(canvas.g(), u8g2_font_helvB10_tf);
  u8g2_DrawStr(canvas.g(), 4, 20, "Ag");
  int ink = 0;
  for (int y = 0; y < Canvas::H; y++)
    for (int x = 0; x < Canvas::W; x++)
      if (canvas.pixel(x, y)) {
        ink++;
        TEST_ASSERT_TRUE(x >= 4 && x < 30 && y >= 6 && y < 26);
      }
  TEST_ASSERT_GREATER_THAN(30, ink);
}

void test_pack_is_row_major_msb_first() {
  canvas.clear();
  u8g2_DrawPixel(canvas.g(), 0, 0);
  u8g2_DrawPixel(canvas.g(), 9, 1);
  static uint8_t out[Canvas::PACKED_BYTES];
  TEST_ASSERT_EQUAL(3904, canvas.packRows(out));
  TEST_ASSERT_EQUAL_HEX8(0x80, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x40, out[Canvas::ROW_BYTES + 1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_starts_white);
  RUN_TEST(test_frame_touches_all_four_edges);
  RUN_TEST(test_outside_reads_white);
  RUN_TEST(test_text_draws_ink_inside_its_box);
  RUN_TEST(test_pack_is_row_major_msb_first);
  return UNITY_END();
}

#include <initializer_list>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "../support/CanvasDump.h"
#include "Icons.h"

void setUp() {}
void tearDown() {}

static void expectIcon(int code, bool day, Icon want) {
  char msg[48];
  snprintf(msg, sizeof msg, "wmo %d day=%d", code, day);
  TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(want), static_cast<int>(iconForWmo(code, day)), msg);
}

void test_wmo_clear_and_partly_cloudy_have_day_night_variants() {
  expectIcon(0, true, Icon::ClearDay);
  expectIcon(0, false, Icon::ClearNight);
  for (int code : {1, 2}) {
    expectIcon(code, true, Icon::PartlyCloudyDay);
    expectIcon(code, false, Icon::PartlyCloudyNight);
  }
}

void test_wmo_groups_ignore_day_night() {
  struct Row {
    int code;
    Icon icon;
  };
  const Row table[] = {{3, Icon::Cloudy},    {45, Icon::Fog},     {48, Icon::Fog},
                       {51, Icon::Drizzle},  {53, Icon::Drizzle}, {55, Icon::Drizzle},
                       {56, Icon::Drizzle},  {57, Icon::Drizzle}, {61, Icon::Rain},
                       {63, Icon::Rain},     {65, Icon::Rain},    {66, Icon::Rain},
                       {67, Icon::Rain},     {80, Icon::Rain},    {81, Icon::Rain},
                       {82, Icon::Rain},     {71, Icon::Snow},    {73, Icon::Snow},
                       {75, Icon::Snow},     {77, Icon::Snow},    {85, Icon::Snow},
                       {86, Icon::Snow},     {95, Icon::Thunder}, {96, Icon::Thunder},
                       {97, Icon::Thunder}, {98, Icon::Thunder}, {99, Icon::Thunder}};
  for (const Row& r : table) {
    expectIcon(r.code, true, r.icon);
    expectIcon(r.code, false, r.icon);
  }
}

void test_wmo_unknown_codes() {
  // Codes just outside each documented range, and others Open-Meteo never sends.
  for (int code : {4, 10, 44, 46, 49, 50, 58, 59, 60, 68, 69, 70, 78, 79, 83, 84, 87, 90, 94, 100, 200, 255}) {
    expectIcon(code, true, Icon::Unknown);
    expectIcon(code, false, Icon::Unknown);
  }
}

void test_icons_have_ink_only_inside_their_box() {
  for (uint8_t i = 0; i < ICON_COUNT; i++) {
    for (IconSize size : {IconSize::S24, IconSize::S48}) {
      Canvas c;
      int s = static_cast<int>(size);
      draw::icon(c, static_cast<Icon>(i), 40, 30, size);
      dump::Ink ink = dump::inkAll(c);
      char msg[40];
      snprintf(msg, sizeof msg, "icon %d size %d", i, s);
      TEST_ASSERT_GREATER_THAN_MESSAGE(s * s / 16, ink.count, msg);  // a real picture
      TEST_ASSERT_LESS_THAN_MESSAGE(s * s * 3 / 4, ink.count, msg);  // not a black square
      TEST_ASSERT_TRUE_MESSAGE(ink.minX >= 40 && ink.maxX < 40 + s, msg);
      TEST_ASSERT_TRUE_MESSAGE(ink.minY >= 30 && ink.maxY < 30 + s, msg);
    }
  }
}

void test_icons_fill_most_of_the_box() {
  // Icons should use the box: no more than 4 px of padding on any side at 48, 2 at 24.
  for (uint8_t i = 0; i < ICON_COUNT; i++) {
    for (IconSize size : {IconSize::S24, IconSize::S48}) {
      Canvas c;
      int s = static_cast<int>(size);
      int slack = s / 12;
      draw::icon(c, static_cast<Icon>(i), 0, 0, size);
      dump::Ink ink = dump::inkAll(c);
      TEST_ASSERT_TRUE(ink.minX <= slack && ink.minY <= slack);
      TEST_ASSERT_TRUE(ink.maxX >= s - 1 - slack && ink.maxY >= s - 1 - slack);
    }
  }
}

void test_icons_are_all_different() {
  for (IconSize size : {IconSize::S24, IconSize::S48}) {
    size_t bytes = static_cast<size_t>(size) * ((static_cast<size_t>(size) + 7) / 8);
    for (uint8_t a = 0; a < ICON_COUNT; a++)
      for (uint8_t b = a + 1; b < ICON_COUNT; b++)
        TEST_ASSERT_TRUE(memcmp(icons::bits(static_cast<Icon>(a), size),
                                icons::bits(static_cast<Icon>(b), size), bytes) != 0);
  }
}

void test_icon_is_transparent_over_existing_ink() {
  Canvas c;
  u8g2_DrawBox(c.g(), 0, 0, 100, 100);  // black underneath
  draw::icon(c, Icon::Cloudy, 10, 10, IconSize::S48);
  TEST_ASSERT_TRUE(c.pixel(5, 5));  // untouched outside
  // Icon pixels set to white are not erased: the black stays black everywhere.
  TEST_ASSERT_EQUAL(100 * 100, dump::ink(c, 0, 0, 100, 100).count);
  TEST_ASSERT_EQUAL(1, u8g2_GetDrawColor(c.g()));
}

void test_icon_out_of_range_draws_unknown() {
  Canvas a, b;
  draw::icon(a, static_cast<Icon>(200), 0, 0, IconSize::S24);
  draw::icon(b, Icon::Unknown, 0, 0, IconSize::S24);
  TEST_ASSERT_EQUAL_MEMORY(a.rows(), b.rows(), Canvas::PACKED_BYTES);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wmo_clear_and_partly_cloudy_have_day_night_variants);
  RUN_TEST(test_wmo_groups_ignore_day_night);
  RUN_TEST(test_wmo_unknown_codes);
  RUN_TEST(test_icons_have_ink_only_inside_their_box);
  RUN_TEST(test_icons_fill_most_of_the_box);
  RUN_TEST(test_icons_are_all_different);
  RUN_TEST(test_icon_is_transparent_over_existing_ink);
  RUN_TEST(test_icon_out_of_range_draws_unknown);
  return UNITY_END();
}

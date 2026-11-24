// Draws type and icon specimens to PBM files so a person can look at them. Not an assertion
// suite: it only checks the files were written. Output goes to $SPECIMEN_DIR (default
// .pio/specimen); convert with tools/canvas_png.py. See docs/images/specimen-*.png.
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unity.h>

#include "../support/CanvasDump.h"
#include "Draw.h"
#include "Fonts.h"
#include "Icons.h"

void setUp() {}
void tearDown() {}

static const char* outDir() {
  const char* d = getenv("SPECIMEN_DIR");
  return d ? d : ".pio/specimen";
}

static void save(const Canvas& c, const char* name) {
  mkdir(".pio", 0755);
  mkdir(outDir(), 0755);
  char path[256];
  snprintf(path, sizeof path, "%s/%s.pbm", outDir(), name);
  TEST_ASSERT_TRUE_MESSAGE(dump::writePbm(c, path), path);
}

void test_fonts() {
  Canvas c;
  draw::titleBar(c, "Specimen: fonts", "updated 9:30");
  const Font list[] = {Font::Small, Font::SmallBold, Font::Body, Font::BodyBold, Font::Quote14, Font::Quote18};
  const char* names[] = {"Small", "SmallBold", "Body", "BodyBold", "Quote14", "Quote18"};
  int y = draw::TITLE_BAR_H + 2;
  for (int i = 0; i < 6; i++) {
    y += fonts::ascent(list[i]);
    char line[48];
    snprintf(line, sizeof line, "%s 13/6 \xE9\xFC\xB0 gjpqy", names[i]);
    draw::text(c, list[i], 4, y, line);
    y += fonts::lineHeight(list[i]) - fonts::ascent(list[i]);
  }
  save(c, "specimen-1-fonts");
}

void test_numbers() {
  Canvas c;
  draw::titleBar(c, "Specimen: numerals", nullptr);
  draw::text(c, Font::Num32, 4, 14 + 4 + 32, "12:45");
  int x = 130;
  draw::text(c, Font::Num32, x, 14 + 4 + 32, "-7");
  draw::degree(c, Font::Num32, x + draw::textWidth(Font::Num32, "-7") + 3, 14 + 4 + 32);
  draw::text(c, Font::NumBig, 4, 14 + 4 + 32 + 8 + 42, "12:45");
  x = 150;
  draw::text(c, Font::NumBig, x, 14 + 4 + 32 + 8 + 42, "-7");
  draw::degree(c, Font::NumBig, x + draw::textWidth(Font::NumBig, "-7") + 3, 14 + 4 + 32 + 8 + 42);
  save(c, "specimen-2-numerals");
}

void test_layout() {
  Canvas c;
  draw::titleBar(c, "Specimen: layout", "updated 9:30");
  draw::paragraph(c, Font::Body, {4, 18, 140, 62},
                  "Show HN: I built a tiny e-paper desk gadget that edits itself and does much more", 4);
  draw::hairline(c, 4, 84, 242);
  draw::checkbox(c, 4, 90, false);
  draw::text(c, Font::Body, 20, 100, "Water the plants");
  draw::checkbox(c, 4, 106, true);
  draw::text(c, Font::Body, 20, 116, "Order filament");
  draw::textRight(c, Font::Small, 246, 100, "right aligned");
  draw::textCenter(c, Font::Small, 190, 116, "centred");
  save(c, "specimen-3-layout");

  static const Font ladder[] = {Font::Quote18, Font::Quote14, Font::BodyBold};
  Canvas q1, q2, q3;
  draw::titleBar(q1, "Note to self", nullptr);
  draw::fitted(q1, ladder, 3, {4, 18, 242, 100}, "Stay curious.");
  save(q1, "specimen-4-quote-short");
  draw::titleBar(q2, "Note to self", nullptr);
  draw::fitted(q2, ladder, 3, {4, 18, 242, 100},
               "You are doing better than you think, and the small steps you take every day add up to "
               "something remarkable over time.");
  save(q2, "specimen-4-quote-long");
  draw::titleBar(q3, "Calendar", "updated 9:30");
  draw::emptyState(q3, "Nothing else today", "Enjoy the quiet");
  save(q3, "specimen-4-empty");
}

static const char* kNames[] = {"ClearD", "ClearN", "PartlyD", "PartlyN", "Cloudy", "Fog",
                               "Drizzle",  "Rain",       "Snow",      "Thunder",     "Unknown"};

static void iconPage(const char* file, int first, int count) {
  Canvas c;
  draw::titleBar(c, "Specimen: icons", nullptr);
  for (int i = 0; i < count; i++) {
    int x = 4 + i * 50;
    draw::icon(c, static_cast<Icon>(first + i), x, 18, IconSize::S48);
    draw::icon(c, static_cast<Icon>(first + i), x + 12, 74, IconSize::S24);
    draw::textCenter(c, Font::Small, x + 24, 117, kNames[first + i]);
  }
  save(c, file);
}

void test_icons() {
  iconPage("specimen-5-icons-a", 0, 5);
  iconPage("specimen-5-icons-b", 5, 5);
  iconPage("specimen-5-icons-c", 10, 1);
}

// A weather page assembled from the helpers: the way a page author would use them.
void test_weather_mock() {
  Canvas c;
  draw::titleBar(c, "Weather", "updated 9:30");
  draw::icon(c, Icon::Rain, 6, 20, IconSize::S48);
  draw::text(c, Font::Num32, 66, 56, "12");
  draw::degree(c, Font::Num32, 66 + draw::textWidth(Font::Num32, "12") + 3, 56);
  draw::text(c, Font::BodyBold, 140, 36, "Light rain");
  draw::text(c, Font::Small, 140, 50, "feels 9\xB0  rain 60%");
  draw::text(c, Font::Small, 140, 62, "H 14\xB0  L 7\xB0");
  draw::hairline(c, 4, 78, 242);
  const char* day[] = {"Wed", "Thu", "Fri"};
  const Icon ic[] = {Icon::PartlyCloudyDay, Icon::Thunder, Icon::Snow};
  const char* t[] = {"13/6", "9/4", "10/5"};
  for (int i = 0; i < 3; i++) {
    int x = 8 + i * 82;
    draw::text(c, Font::SmallBold, x, 96, day[i]);
    draw::icon(c, ic[i], x + 26, 84, IconSize::S24);
    draw::text(c, Font::Small, x, 112, t[i]);
  }
  save(c, "specimen-6-weather");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fonts);
  RUN_TEST(test_numbers);
  RUN_TEST(test_layout);
  RUN_TEST(test_icons);
  RUN_TEST(test_weather_mock);
  return UNITY_END();
}

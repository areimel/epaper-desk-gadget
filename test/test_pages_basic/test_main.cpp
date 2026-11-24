// Clock, messages and notes pages plus the shared time helper. Goldens: test/fixtures/golden.
// Look at the pictures: .pio/pages/*.pbm, converted with tools/canvas_png.py.
#include <string.h>
#include <unity.h>

#include "../support/PageData.h"
#include "../support/PagesHarness.h"
#include "PageCommon.h"
#include "Pages.h"

void setUp() { data::setZone(); }
void tearDown() {}

static void fmt(bool h12, int minutes, const char* want) {
  char out[16];
  pages::formatMinutes(h12, minutes, out, sizeof out);
  TEST_ASSERT_EQUAL_STRING(want, out);
}

void test_time_format() {
  fmt(true, 5, "12:05 am");
  fmt(true, 12 * 60, "12:00 pm");
  fmt(true, 23 * 60 + 59, "11:59 pm");
  fmt(true, 9 * 60 + 30, "9:30 am");
  fmt(false, 5, "00:05");
  fmt(false, 23 * 60 + 59, "23:59");
  fmt(false, 9 * 60 + 30, "09:30");
  const char* suffix = nullptr;
  char out[16];
  pages::formatMinutes(true, 13 * 60 + 7, out, sizeof out, &suffix);
  TEST_ASSERT_EQUAL_STRING("1:07", out);
  TEST_ASSERT_EQUAL_STRING("pm", suffix);
  pages::formatMinutes(false, 13 * 60 + 7, out, sizeof out, &suffix);
  TEST_ASSERT_EQUAL_STRING("13:07", out);
  TEST_ASSERT_EQUAL_STRING("", suffix);
}

void test_convention_follows_units() {
  data::Set& s = data::fresh();
  TEST_ASSERT_FALSE(pages::use12h(s.feed));
  s.feed.weather.units = Units::Fahrenheit;
  TEST_ASSERT_TRUE(pages::use12h(s.feed));
  char out[16];
  pages::formatTime(s.feed, data::fixedNow(), out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("9:41 am", out);
}

void test_titles_and_clock_flag() {
  TEST_ASSERT_EQUAL_STRING("Clock", pages::title(PageId::Clock));
  TEST_ASSERT_EQUAL_STRING("Hacker News", pages::title(PageId::Hn));
  TEST_ASSERT_EQUAL_STRING("Note to self", pages::title(PageId::Messages));
  TEST_ASSERT_EQUAL_STRING("Today", pages::title(PageId::Calendar));
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++)
    TEST_ASSERT_EQUAL(i == 0, pages::usesClock(static_cast<PageId>(i)));
}

static void renderPage(PageId id, const data::Set& s, uint8_t visit, bool synced, Canvas& c) {
  pages::render(id, data::context(s, visit, synced), c);
}

#define CHECK(name, id, set, visit, synced) \
  harness::check(name, [&](Canvas& c) { renderPage(id, set, visit, synced, c); })

void test_clock() {
  data::Set& s = data::fresh();
  CHECK("clock", PageId::Clock, s, 0, true);
  CHECK("clock-unsynced", PageId::Clock, s, 0, false);
  s.feed.weather.units = Units::Fahrenheit;
  CHECK("clock-12h", PageId::Clock, s, 0, true);
}

void test_messages() {
  data::Set& s = data::fresh();
  CHECK("messages-none", PageId::Messages, s, 0, true);
  data::addMessage(s.feed, "You are doing better than you think.", "your desk");
  data::addMessage(s.feed, "Breathe in. Out.", "");
  data::addMessage(s.feed,
                   "Don't wait for the perfect moment \xE2\x80\x94 take the moment and make it perfect, one small "
                   "honest step at a time, and call it good enough tonight.",
                   "Zig Ziglar, or somebody");
  data::addMessage(s.feed, "Drink some water.", "Mum");
  CHECK("messages", PageId::Messages, s, 0, true);
  CHECK("messages-short", PageId::Messages, s, 1, true);
  CHECK("messages-long", PageId::Messages, s, 2, true);
  CHECK("messages-by", PageId::Messages, s, 3, true);
  uint8_t a[Canvas::PACKED_BYTES], b[Canvas::PACKED_BYTES];
  harness::pack([&](Canvas& c) { renderPage(PageId::Messages, s, 0, true, c); }, a);
  harness::pack([&](Canvas& c) { renderPage(PageId::Messages, s, 1, true, c); }, b);
  TEST_ASSERT_TRUE(memcmp(a, b, sizeof a) != 0);
  harness::pack([&](Canvas& c) { renderPage(PageId::Messages, s, 4, true, c); }, b);  // wraps to the first
  TEST_ASSERT_EQUAL_MEMORY(a, b, sizeof a);
}

void test_notes() {
  data::Set& s = data::fresh();
  CHECK("notes-none", PageId::Notes, s, 0, true);
  strcpy(s.feed.notes.title, "Saturday jobs");
  data::addNote(s.feed, "Water the plants", true, true);
  data::addNote(s.feed, "Order filament", true, false);
  data::addNote(s.feed, "Caf\xC3\xA9 with Zo\xC3\xAB \xF0\x9F\x98\x80", false, false);
  data::addNote(s.feed, "Call the plumber about the kitchen tap before noon", true, false);
  data::addNote(s.feed, "Pay invoice 4471", true, true);
  CHECK("notes", PageId::Notes, s, 0, true);
  s.feed.notes.title[0] = '\0';
  s.feed.notes.lineCount = 0;
  data::addNote(s.feed, "Plain line one", false, false);
  data::addNote(s.feed, "Plain line two, a little longer than the first", false, false);
  CHECK("notes-plain", PageId::Notes, s, 0, true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_time_format);
  RUN_TEST(test_convention_follows_units);
  RUN_TEST(test_titles_and_clock_flag);
  RUN_TEST(test_clock);
  RUN_TEST(test_messages);
  RUN_TEST(test_notes);
  return UNITY_END();
}

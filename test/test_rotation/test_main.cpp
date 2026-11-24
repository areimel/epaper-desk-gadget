#include <unity.h>

#include "FeedConfig.h"
#include "Rotation.h"

void setUp() {}
void tearDown() {}

static const PageId ABC[] = {PageId::Clock, PageId::Weather, PageId::Hn};

static PageId cur(const Rotation& r) {
  PageId p = PageId::Notes;
  TEST_ASSERT_TRUE(r.current(p));
  return p;
}

static Rotation three(uint32_t t = 0) {
  Rotation r;
  r.setDwell(1000);
  r.setPages(ABC, 3, t);
  return r;
}

void test_zero_pages() {
  Rotation r;
  r.setDwell(1000);
  r.setPages(nullptr, 0, 0);
  PageId p;
  TEST_ASSERT_FALSE(r.current(p));
  TEST_ASSERT_FALSE(r.tick(5000));
  TEST_ASSERT_FALSE(r.next(5000));
}

void test_starts_on_first_enabled_page() {
  Rotation r = three();
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Clock));
}

void test_tick_turns_exactly_at_the_dwell() {
  Rotation r = three(100);
  TEST_ASSERT_FALSE(r.tick(1099));
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
  TEST_ASSERT_TRUE(r.tick(1100));
  TEST_ASSERT_EQUAL(PageId::Weather, cur(r));
  TEST_ASSERT_FALSE(r.tick(2099));  // the timer restarted at 1100
  TEST_ASSERT_TRUE(r.tick(2100));
  TEST_ASSERT_EQUAL(PageId::Hn, cur(r));
  TEST_ASSERT_TRUE(r.tick(3100));
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));  // wraps around
}

void test_next_advances_and_restarts_the_dwell() {
  Rotation r = three(0);
  TEST_ASSERT_TRUE(r.next(900));
  TEST_ASSERT_EQUAL(PageId::Weather, cur(r));
  TEST_ASSERT_FALSE(r.tick(1500));  // 900 + 1000 not reached
  TEST_ASSERT_TRUE(r.tick(1900));
}

void test_one_page_never_turns_but_the_timer_restarts() {
  Rotation r;
  r.setDwell(1000);
  const PageId one[] = {PageId::Notes};
  r.setPages(one, 1, 0);
  TEST_ASSERT_FALSE(r.tick(1000));
  TEST_ASSERT_FALSE(r.next(1200));
  TEST_ASSERT_EQUAL(PageId::Notes, cur(r));
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Notes));  // no increase
  // next() at 1200 restarted the timer, so a tick at 2199 is still inside the dwell
  TEST_ASSERT_FALSE(r.tick(2199));
  TEST_ASSERT_FALSE(r.tick(2200));
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Notes));
}

void test_visit_counts_repeat_showings() {
  Rotation r = three(0);
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Clock));
  r.next(10);  // Weather first showing
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Weather));
  r.next(20);  // Hn
  r.next(30);  // Clock again
  TEST_ASSERT_EQUAL(1, r.visit(PageId::Clock));
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Weather));  // not shown again yet
  r.tick(1030);                                    // Weather again
  TEST_ASSERT_EQUAL(1, r.visit(PageId::Weather));
  r.show(PageId::Weather, 1040);  // showing it again counts too
  TEST_ASSERT_EQUAL(2, r.visit(PageId::Weather));
}

void test_visit_wraps_at_255() {
  Rotation r;
  r.setDwell(1);
  const PageId two[] = {PageId::Clock, PageId::Hn};
  r.setPages(two, 2, 0);
  for (uint32_t i = 1; i <= 2 * 256; i++) r.next(i);
  // 512 turns: Clock shown 1 + 256 times, so the count wrapped past 255 back to 0
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Clock));
  r.next(10000);
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Hn));  // 257 showings -> 256 -> wraps to 0
}

void test_show_a_page_that_is_not_enabled() {
  Rotation r = three(0);
  r.next(10);  // Weather
  r.show(PageId::Calendar, 100);
  TEST_ASSERT_EQUAL(PageId::Calendar, cur(r));
  TEST_ASSERT_FALSE(r.tick(1099));
  TEST_ASSERT_TRUE(r.tick(1100));  // dwell restarted by show
  TEST_ASSERT_EQUAL(PageId::Hn, cur(r));  // follows the previously current enabled position (Weather)
}

void test_show_unlisted_then_next_goes_to_following_page() {
  Rotation r = three(0);  // showing Clock
  r.show(PageId::Notes, 5);
  TEST_ASSERT_TRUE(r.next(6));
  TEST_ASSERT_EQUAL(PageId::Weather, cur(r));
}

void test_show_unlisted_after_last_wraps_to_first() {
  Rotation r = three(0);
  r.next(1);
  r.next(2);  // Hn, the last
  r.show(PageId::Notes, 3);
  TEST_ASSERT_TRUE(r.next(4));
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
}

void test_dwell_change_does_not_restart_the_timer() {
  Rotation r = three(0);
  r.setDwell(5000);
  TEST_ASSERT_FALSE(r.tick(4999));
  TEST_ASSERT_TRUE(r.tick(5000));
  r.setDwell(100);  // a shorter dwell applies to the running timer, measured from 5000
  TEST_ASSERT_TRUE(r.tick(5100));
}

void test_zero_dwell_is_one_ms_and_does_not_spin() {
  Rotation r = three(0);
  r.setDwell(0);
  TEST_ASSERT_FALSE(r.tick(0));
  TEST_ASSERT_TRUE(r.tick(1));
  TEST_ASSERT_FALSE(r.tick(1));  // one turn per millisecond at most
  TEST_ASSERT_TRUE(r.tick(2));
}

void test_clock_wraps_across_0xFFFFFFFF() {
  Rotation r = three(0xFFFFFF00u);
  TEST_ASSERT_FALSE(r.tick(0xFFFFFF00u + 999));  // 0xFFFFFFE7
  TEST_ASSERT_TRUE(r.tick(0xFFFFFF00u + 1000));  // wrapped past zero
  TEST_ASSERT_EQUAL(PageId::Weather, cur(r));
  TEST_ASSERT_FALSE(r.tick(0xFFFFFF00u + 1999));
  TEST_ASSERT_TRUE(r.tick(0xFFFFFF00u + 2000));
}

void test_reorder_keeps_the_current_page() {
  Rotation r = three(0);
  r.next(10);  // Weather
  const PageId order[] = {PageId::Hn, PageId::Weather, PageId::Clock};
  r.setPages(order, 3, 20);
  TEST_ASSERT_EQUAL(PageId::Weather, cur(r));
  TEST_ASSERT_EQUAL(0, r.visit(PageId::Weather));  // not a new showing
  TEST_ASSERT_TRUE(r.next(30));
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));  // follows the new order
}

void test_reorder_does_not_restart_the_timer() {
  Rotation r = three(0);
  const PageId order[] = {PageId::Hn, PageId::Weather, PageId::Clock};
  r.setPages(order, 3, 400);
  TEST_ASSERT_TRUE(r.tick(1000));  // still measured from 0
}

void test_removing_the_current_page_moves_to_the_first_enabled() {
  Rotation r = three(0);
  r.next(10);  // Weather
  const PageId rest[] = {PageId::Hn, PageId::Clock};
  r.setPages(rest, 2, 50);
  TEST_ASSERT_EQUAL(PageId::Hn, cur(r));
  TEST_ASSERT_FALSE(r.tick(1049));  // the move restarts the dwell
  TEST_ASSERT_TRUE(r.tick(1050));
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
}

void test_removing_all_pages_then_adding_back() {
  Rotation r = three(0);
  r.setPages(nullptr, 0, 10);
  PageId p;
  TEST_ASSERT_FALSE(r.current(p));
  TEST_ASSERT_FALSE(r.tick(99999));
  r.setPages(ABC, 3, 20);
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
}

void test_setpages_keeps_an_unlisted_shown_page_out_of_the_list() {
  Rotation r = three(0);
  r.show(PageId::Notes, 5);
  r.setPages(ABC, 3, 6);  // Notes is still not enabled: move to the first enabled
  TEST_ASSERT_EQUAL(PageId::Clock, cur(r));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_zero_pages);
  RUN_TEST(test_starts_on_first_enabled_page);
  RUN_TEST(test_tick_turns_exactly_at_the_dwell);
  RUN_TEST(test_next_advances_and_restarts_the_dwell);
  RUN_TEST(test_one_page_never_turns_but_the_timer_restarts);
  RUN_TEST(test_visit_counts_repeat_showings);
  RUN_TEST(test_visit_wraps_at_255);
  RUN_TEST(test_show_a_page_that_is_not_enabled);
  RUN_TEST(test_show_unlisted_then_next_goes_to_following_page);
  RUN_TEST(test_show_unlisted_after_last_wraps_to_first);
  RUN_TEST(test_dwell_change_does_not_restart_the_timer);
  RUN_TEST(test_zero_dwell_is_one_ms_and_does_not_spin);
  RUN_TEST(test_clock_wraps_across_0xFFFFFFFF);
  RUN_TEST(test_reorder_keeps_the_current_page);
  RUN_TEST(test_reorder_does_not_restart_the_timer);
  RUN_TEST(test_removing_the_current_page_moves_to_the_first_enabled);
  RUN_TEST(test_removing_all_pages_then_adding_back);
  RUN_TEST(test_setpages_keeps_an_unlisted_shown_page_out_of_the_list);
  return UNITY_END();
}

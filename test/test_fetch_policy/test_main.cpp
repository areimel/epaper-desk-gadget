#include <unity.h>

// lib/net is ignored on native (lib_ignore), so its pure header is included by path. Snapshots.h
// comes first so the library finder pulls in lib/sources.
#include "Snapshots.h"
#include "../../lib/net/src/FetchPolicy.h"

using fetch::BACKOFF_MS;
using fetch::MINUTE_MS;
using fetch::SourcePolicy;

void setUp() {}
void tearDown() {}

void test_state_transitions() {
  TEST_ASSERT_EQUAL(SourceState::Ok, fetch::nextState(SourceState::Never, true));
  TEST_ASSERT_EQUAL(SourceState::Ok, fetch::nextState(SourceState::Error, true));
  TEST_ASSERT_EQUAL(SourceState::Ok, fetch::nextState(SourceState::Stale, true));
  TEST_ASSERT_EQUAL(SourceState::Ok, fetch::nextState(SourceState::Ok, true));
  TEST_ASSERT_EQUAL(SourceState::Stale, fetch::nextState(SourceState::Ok, false));
  TEST_ASSERT_EQUAL(SourceState::Stale, fetch::nextState(SourceState::Stale, false));
  TEST_ASSERT_EQUAL(SourceState::Error, fetch::nextState(SourceState::Never, false));
  TEST_ASSERT_EQUAL(SourceState::Error, fetch::nextState(SourceState::Error, false));
}

void test_offline_state() {
  TEST_ASSERT_EQUAL(SourceState::Stale, fetch::offlineState(SourceState::Ok));
  TEST_ASSERT_EQUAL(SourceState::Stale, fetch::offlineState(SourceState::Stale));
  TEST_ASSERT_EQUAL(SourceState::Never, fetch::offlineState(SourceState::Never));
  TEST_ASSERT_EQUAL(SourceState::Error, fetch::offlineState(SourceState::Error));
}

void test_record_keeps_age_and_truncates_error() {
  SourceStatus s{};
  fetch::record(s, true, nullptr, 1000);
  TEST_ASSERT_EQUAL(SourceState::Ok, s.state);
  TEST_ASSERT_EQUAL(1000, s.updated);
  fetch::record(s, false, "a reason that is far longer than thirty-one characters", 2000);
  TEST_ASSERT_EQUAL(SourceState::Stale, s.state);
  TEST_ASSERT_EQUAL(1000, s.updated);  // the data shown is still from 1000
  TEST_ASSERT_EQUAL(31, strlen(s.error));
  fetch::record(s, true, "ignored", 3000);
  TEST_ASSERT_EQUAL_STRING("", s.error);
  TEST_ASSERT_EQUAL(3000, s.updated);
}

void test_due_at_first_check_then_after_interval() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  TEST_ASSERT_TRUE(p.due(12345));
  p.onSuccess(12345);
  TEST_ASSERT_FALSE(p.due(12345));
  TEST_ASSERT_FALSE(p.due(12345 + 15 * MINUTE_MS - 1));
  TEST_ASSERT_TRUE(p.due(12345 + 15 * MINUTE_MS));
}

void test_intervals() {
  TEST_ASSERT_EQUAL_UINT32(30 * MINUTE_MS, fetch::WEATHER_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT32(15 * MINUTE_MS, fetch::HN_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT32(60 * MINUTE_MS, fetch::CALENDAR_INTERVAL_MS);
}

void test_backoff_ladder_and_reset() {
  SourcePolicy p(fetch::WEATHER_INTERVAL_MS);
  const uint32_t expected[] = {1, 2, 5, 15, 15, 15};
  uint32_t now = 0;
  for (uint32_t minutes : expected) {
    p.onFailure(now);
    TEST_ASSERT_EQUAL_UINT32(minutes * MINUTE_MS, p.waitMs());
    TEST_ASSERT_FALSE(p.due(now + minutes * MINUTE_MS - 1));
    TEST_ASSERT_TRUE(p.due(now + minutes * MINUTE_MS));
    now += minutes * MINUTE_MS;
  }
  p.onSuccess(now);
  TEST_ASSERT_EQUAL(0, p.failures());
  TEST_ASSERT_EQUAL_UINT32(fetch::WEATHER_INTERVAL_MS, p.waitMs());
  p.onFailure(now);
  TEST_ASSERT_EQUAL_UINT32(BACKOFF_MS[0], p.waitMs());  // the ladder starts over
}

void test_wrap_around() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  const uint32_t start = 0xFFFFFFFFu - MINUTE_MS;  // 1 min before millis() wraps
  p.onSuccess(start);
  TEST_ASSERT_FALSE(p.due(start + 10 * MINUTE_MS));  // wrapped past zero
  TEST_ASSERT_TRUE(p.due(start + 15 * MINUTE_MS));
  p.onFailure(0xFFFFFFF0u);
  TEST_ASSERT_FALSE(p.due(0x10u));
  TEST_ASSERT_TRUE(p.due(0xFFFFFFF0u + MINUTE_MS));
}

void test_request_now_during_backoff() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  p.onFailure(0);
  p.onFailure(MINUTE_MS);  // second failure: waiting 2 min
  const uint32_t t = MINUTE_MS + 10;
  TEST_ASSERT_FALSE(p.due(t));
  p.requestNow();
  TEST_ASSERT_TRUE(p.due(t));
  TEST_ASSERT_EQUAL(2, p.failures());  // the count survives the request
  p.onFailure(t);                      // the forced fetch failed: third rung, once
  TEST_ASSERT_FALSE(p.due(t));
  TEST_ASSERT_EQUAL_UINT32(5 * MINUTE_MS, p.waitMs());
  TEST_ASSERT_TRUE(p.due(t + 5 * MINUTE_MS));
}

void test_request_now_success_returns_to_interval() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  p.onSuccess(0);
  p.requestNow();
  TEST_ASSERT_TRUE(p.due(1));
  p.onSuccess(1);
  TEST_ASSERT_FALSE(p.due(2));
  TEST_ASSERT_EQUAL_UINT32(fetch::HN_INTERVAL_MS, p.waitMs());
}

void test_reset() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  p.onFailure(0);
  p.onFailure(1);
  p.reset();
  TEST_ASSERT_TRUE(p.due(2));
  TEST_ASSERT_EQUAL(0, p.failures());
}

// A fake transport driving the policy the way the fetch task does, once per second: the old
// data survives a run of failures as Stale and the attempts follow the ladder.
void test_fake_transport_outage() {
  SourcePolicy p(fetch::HN_INTERVAL_MS);
  SourceStatus s{};
  int attempts = 0;
  const uint32_t outageFrom = 20 * MINUTE_MS, outageTo = 45 * MINUTE_MS;
  uint32_t attemptAt[16];
  for (uint32_t now = 0; now < 60 * MINUTE_MS; now += 1000) {
    if (!p.due(now)) continue;
    const bool ok = now < outageFrom || now >= outageTo;
    if (attempts < 16) attemptAt[attempts] = now;
    attempts++;
    fetch::record(s, ok, ok ? nullptr : "HTTP 503", static_cast<time_t>(now / 1000));
    if (ok) p.onSuccess(now);
    else p.onFailure(now);
    if (!ok) {
      TEST_ASSERT_EQUAL(SourceState::Stale, s.state);
      TEST_ASSERT_EQUAL(900, s.updated);  // the 15-min fetch is the last good data
      TEST_ASSERT_EQUAL_STRING("HTTP 503", s.error);
    }
  }
  // 0, 15 (ok), 30 (fail), +1, +2, +5, +15 = 53 (ok, outage over)
  const uint32_t want[] = {0, 15, 30, 31, 33, 38, 53};
  TEST_ASSERT_EQUAL(7, attempts);
  for (int i = 0; i < 7; i++) TEST_ASSERT_EQUAL_UINT32(want[i] * MINUTE_MS, attemptAt[i]);
  TEST_ASSERT_EQUAL(SourceState::Ok, s.state);
  TEST_ASSERT_EQUAL_STRING("", s.error);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_state_transitions);
  RUN_TEST(test_offline_state);
  RUN_TEST(test_record_keeps_age_and_truncates_error);
  RUN_TEST(test_due_at_first_check_then_after_interval);
  RUN_TEST(test_intervals);
  RUN_TEST(test_backoff_ladder_and_reset);
  RUN_TEST(test_wrap_around);
  RUN_TEST(test_request_now_during_backoff);
  RUN_TEST(test_request_now_success_returns_to_interval);
  RUN_TEST(test_reset);
  RUN_TEST(test_fake_transport_outage);
  return UNITY_END();
}

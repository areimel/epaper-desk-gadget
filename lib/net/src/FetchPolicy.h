#pragma once
#include <stdint.h>
#include <string.h>

#include "Snapshots.h"

// When each source is fetched and what a result does to its state. Pure: no Arduino, no heap.
// Time is nowMs, a wrapping uint32_t compared only by subtraction. Host-tested by
// test/test_fetch_policy (the native env includes this header by relative path).
namespace fetch {
constexpr uint32_t MINUTE_MS = 60UL * 1000UL;
constexpr uint32_t WEATHER_INTERVAL_MS = 30 * MINUTE_MS;
constexpr uint32_t HN_INTERVAL_MS = 15 * MINUTE_MS;
constexpr uint32_t CALENDAR_INTERVAL_MS = 60 * MINUTE_MS;
// Wait after the 1st, 2nd, 3rd and 4th-or-later failure in a row.
constexpr uint32_t BACKOFF_MS[] = {1 * MINUTE_MS, 2 * MINUTE_MS, 5 * MINUTE_MS, 15 * MINUTE_MS};
constexpr uint8_t BACKOFF_STEPS = sizeof(BACKOFF_MS) / sizeof(BACKOFF_MS[0]);

// success -> Ok; a failure keeps older data as Stale, or Error when there never was any.
inline SourceState nextState(SourceState current, bool success) {
  if (success) return SourceState::Ok;
  return (current == SourceState::Ok || current == SourceState::Stale) ? SourceState::Stale
                                                                        : SourceState::Error;
}

// A fetch was wanted but the network is down: fresh data turns Stale; anything else is kept
// (Never stays Never so the boot screen does not flash an error before WiFi is up).
inline SourceState offlineState(SourceState current) {
  return current == SourceState::Ok ? SourceState::Stale : current;
}

// Applies one fetch result to a status: success sets updated = now and clears the error; a
// failure keeps updated (the age of the data still shown) and stores a truncated reason.
inline void record(SourceStatus& s, bool success, const char* error, time_t now) {
  s.state = nextState(s.state, success);
  if (success) s.updated = now;
  const char* text = success || !error ? "" : error;
  strncpy(s.error, text, sizeof(s.error) - 1);
  s.error[sizeof(s.error) - 1] = 0;
}

class SourcePolicy {
 public:
  explicit SourcePolicy(uint32_t intervalMs) : interval_(intervalMs) {}

  // Due on the first check, after the interval (or the backoff wait) since the last attempt,
  // and right after requestNow().
  bool due(uint32_t nowMs) const {
    return !attempted_ || requested_ || static_cast<uint32_t>(nowMs - last_) >= wait_;
  }

  void onSuccess(uint32_t nowMs) { finish(nowMs, interval_, 0); }

  void onFailure(uint32_t nowMs) {
    const uint8_t step = failures_ < BACKOFF_STEPS ? failures_ : BACKOFF_STEPS - 1;
    finish(nowMs, BACKOFF_MS[step], failures_ < 255 ? failures_ + 1 : 255);
  }

  // Button, API refresh or feed change: due now. The failure count is kept until a success,
  // so a failed forced fetch continues the backoff ladder where it was.
  void requestNow() { requested_ = true; }

  // Forget everything (the source was reconfigured): due now with a fresh ladder.
  void reset() { *this = SourcePolicy(interval_); }

  uint8_t failures() const { return failures_; }
  uint32_t waitMs() const { return wait_; }  // current wait after the last attempt

 private:
  void finish(uint32_t nowMs, uint32_t wait, uint8_t failures) {
    last_ = nowMs;
    wait_ = wait;
    failures_ = failures;
    attempted_ = true;
    requested_ = false;
  }

  uint32_t interval_;
  uint32_t wait_ = 0;
  uint32_t last_ = 0;
  uint8_t failures_ = 0;
  bool attempted_ = false;
  bool requested_ = false;
};
}  // namespace fetch

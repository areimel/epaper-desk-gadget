#include "Clock.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "Limits.h"
#include "Log.h"

namespace {
constexpr time_t SANE_EPOCH = 1704067200;  // 2024-01-01T00:00:00Z
bool started = false;
char zone[limits::TZ_MAX + 1] = "";  // "" = never applied
}  // namespace

namespace net {

void setTimezone(const char* posixTz) {
  const char* tz = posixTz && posixTz[0] ? posixTz : "UTC0";
  if (zone[0] && strncmp(tz, zone, sizeof zone - 1) == 0) return;
  strncpy(zone, tz, sizeof zone - 1);
  zone[sizeof zone - 1] = 0;
  setenv("TZ", zone, 1);
  tzset();
  LOG_I("clock: zone %s", zone);
}

void beginClock(const char* posixTz) {
  if (!started) {
    // Starts SNTP (lwIP re-polls hourly) and sets TZ; safe before WiFi is up.
    configTzTime(posixTz && posixTz[0] ? posixTz : "UTC0", "pool.ntp.org", "time.nist.gov");
    started = true;
    zone[0] = 0;  // record the zone below even though configTzTime already applied it
  }
  setTimezone(posixTz);
}

bool timeSynced() { return time(nullptr) >= SANE_EPOCH; }

}  // namespace net
#endif

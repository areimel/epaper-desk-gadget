#pragma once
#if FEATURE_DASHBOARD
#include <time.h>

// Wall-clock time from NTP (pool.ntp.org, time.nist.gov) with the feed's POSIX time zone.
// beginClock/setTimezone: loop thread only. timeSynced(): any task.
namespace net {
// Starts SNTP and applies the zone. Safe to call again; later calls only change the zone.
void beginClock(const char* posixTz);
// Applies a POSIX TZ string (e.g. "EST5EDT,M3.2.0,M11.1.0") to localtime(); empty = UTC0.
// Does nothing when the zone is unchanged.
void setTimezone(const char* posixTz);
// True once the clock reads 2024-01-01 or later (NTP answered; TLS can check certificate dates).
bool timeSynced();
}  // namespace net
#endif

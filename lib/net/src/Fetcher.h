#pragma once
#if FEATURE_DASHBOARD
#include "FeedConfig.h"
#include "Snapshots.h"

// Background fetching of weather and Hacker News (calendar later) in one FreeRTOS task. The task
// never touches the panel, canvas or FeedConfig; everything below is for the loop thread and
// only copies small structs under a mutex. Schedule and backoff: FetchPolicy.h.
namespace net {
void beginFetcher();  // creates the task; idempotent
// Copies what the task needs (weather request, time zone name) and marks affected sources due.
// Also applies feed.tzPosix to the clock (net::setTimezone). Call at boot and after every save.
void onFeedChanged(const FeedConfig& feed);
void requestRefresh();  // every configured source due now
// Copy the latest snapshot; true if it changed since the last call that returned true.
bool copyWeather(WeatherSnap& out);
bool copyHn(HnSnap& out);
bool copyCalendar(CalSnap& out);  // calendar fetching is not implemented yet: always Never
// Read-only view for /api/state: does not consume the "changed" flags above.
void sourceStatuses(SourceStatus& weather, SourceStatus& hn, SourceStatus& calendar);
}  // namespace net
#endif

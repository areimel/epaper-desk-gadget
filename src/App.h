#pragma once
#include <stdint.h>

// The dashboard: owns the feed, the page rotation and what is on the panel. main.cpp only
// feeds it time and button events. Needs FEATURE_DASHBOARD and FEATURE_EPAPER.
namespace app {
void begin();              // panel, stored feed, clock, fetcher, web API; draws the first screen
void update(uint32_t now);  // serve the API, take fetched data, turn pages, refresh the panel
void onClick(uint32_t now);  // next page
void onHold(uint32_t now);   // refetch everything and show the web app's address
}  // namespace app

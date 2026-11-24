#pragma once
#include <stdint.h>
#include <time.h>

#include "Canvas.h"
#include "FeedConfig.h"
#include "Snapshots.h"

// The dashboard screens: plain data in, pixels on a Canvas out. Pure (no Arduino, no
// allocation), so the same code runs on the device and in host tests.
struct PageContext {
  const FeedConfig& feed;
  const WeatherSnap& weather;
  const HnSnap& hn;
  const CalSnap& cal;
  time_t now;       // current time; converted with localtime_r (the process time zone)
  bool timeSynced;  // false until the clock has been set from the network
  uint8_t visit;    // how many times this page has been shown before (steps HN stories, messages)
};

enum class SetupState : uint8_t { NoWifi, Connecting, NoPages };

namespace pages {
// Clears the canvas, then draws the page.
void render(PageId page, const PageContext& ctx, Canvas& canvas);
// The screen shown when there is nothing to rotate through. detail (may be null) is a hint such
// as the installer page, or the device address once connected.
void renderSetup(SetupState state, const char* detail, Canvas& canvas);
const char* title(PageId page);  // "Clock", "Weather", "Hacker News", "Note to self", "Today", "Notes"
bool usesClock(PageId page);     // true for pages whose pixels change every minute (only Clock)
}  // namespace pages

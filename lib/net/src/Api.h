#pragma once
#if FEATURE_DASHBOARD
#include <stdint.h>

#include "Canvas.h"
#include "FeedConfig.h"

// The HTTP API of docs/api.md and the embedded web app, on the core's synchronous WebServer
// (port 80) with mDNS. Everything, hooks included, runs on the loop thread inside updateApi().
namespace net {
struct ApiHooks {
  FeedConfig* feed;                               // the live feed (loop thread only)
  void (*onFeedSaved)();                          // feed was replaced and stored
  void (*renderPage)(PageId page, Canvas& into);  // draw a page off-panel with current data
  const Canvas* (*liveCanvas)();                  // what the panel shows now; may return nullptr
  bool (*showPage)(PageId page);                  // jump the panel to this page
  const char* (*currentPage)();                   // page id on the panel, or ""
  uint32_t (*refreshCount)();
  uint16_t (*calendarSkipped)();
};

// hostname: mDNS name without ".local"; deviceName: "name" in /api/state. Both must outlive
// the server (string literals from generated_config.h). Any hook may be nullptr.
void beginApi(const char* hostname, const char* deviceName, const ApiHooks& hooks);
void updateApi();         // call from loop(): starts the server once WiFi is up, serves one client
const char* deviceUrl();  // "http://<ip>/" while connected, else ""
}  // namespace net
#endif

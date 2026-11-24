#include "Api.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <time.h>

#include "ApiInternal.h"
#include "Clock.h"
#include "Fetcher.h"
#include "Net.h"
#include "StateJson.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "0.0.0-dev"
#endif
#ifndef BOARD_LABEL
#define BOARD_LABEL ""
#endif

namespace net {
namespace api {

void handleState() {
  const ApiHooks& h = hooks();
  char hostLocal[48];
  snprintf(hostLocal, sizeof hostLocal, "%s.local", hostname());
  StateInfo s = {};
  s.name = deviceName();
  s.version = FIRMWARE_VERSION;
  s.board = BOARD_LABEL;
  s.ip = ipAddress();
  s.host = hostLocal;
  s.time = static_cast<long long>(time(nullptr));
  s.timeSynced = timeSynced();
  s.page = h.currentPage ? h.currentPage() : "";
  if (!s.page) s.page = "";
  sourceStatuses(s.weather, s.hn, s.calendar);  // read-only: the panel's "changed" flags stay
  s.calendarSkipped = h.calendarSkipped ? h.calendarSkipped() : 0;
  s.heapFree = ESP.getFreeHeap();
  s.refreshCount = h.refreshCount ? h.refreshCount() : 0;
  static char out[1024];  // the document is about 700 bytes
  const size_t n = stateJson(s, out, sizeof out);
  if (n) sendJson(200, out, n);
  else sendError(500, "The device is out of memory. Try again.");
}

}  // namespace api
}  // namespace net
#endif

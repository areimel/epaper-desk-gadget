#include "Fetcher.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <new>
#include <stdlib.h>
#include <string.h>

#include "Clock.h"
#include "FetchPolicy.h"
#include "HnParse.h"
#include "HttpGet.h"
#include "IcsParse.h"
#include "Log.h"
#include "Net.h"
#include "WeatherParse.h"

// Threading: the public functions run on the loop thread; fetchTask is the only other user.
// `lock` guards `shared[]` and the three published snapshots; it is held only to copy structs,
// never during network I/O. Policies and the scratch snapshots belong to the task alone.
namespace {
// 12 KB: an mbedTLS handshake with certificate-bundle verification uses roughly 6-8 KB of the
// caller's stack, plus HTTPClient and ArduinoJson frames. The fetch log line prints the
// remaining minimum so this can be trimmed once measured on hardware.
constexpr uint32_t TASK_STACK_BYTES = 12 * 1024;
constexpr UBaseType_t TASK_PRIORITY = 1;  // same as loopTask, but on the other core
constexpr BaseType_t TASK_CORE = 0;       // loopTask runs on core 1
constexpr uint32_t IDLE_CHECK_MS = 5000;  // also woken early by requestRefresh/onFeedChanged
constexpr size_t URL_CAP = 400;

enum Src : uint8_t { WEATHER, HN, CALENDAR, SRC_COUNT };

struct Shared {        // per source, guarded by lock
  char url[URL_CAP];   // "" = not configured
  bool requested;      // fetch now (button, API, feed change)
  bool reset;          // reconfigured: start a fresh backoff ladder
  bool changed;        // snapshot changed since the loop last copied it
};

SemaphoreHandle_t lock = nullptr;
TaskHandle_t task = nullptr;
Shared shared[SRC_COUNT] = {};
WeatherSnap weather = {};  // published, guarded by lock
HnSnap hn = {};
CalSnap cal = {};
char tzIana[limits::TZ_MAX + 1] = "";  // the calendar parser compares event zones with it

struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};

WeatherSnap scratchWeather;  // task only: parse targets, never on the task stack
HnSnap scratchHn;
CalSnap scratchCal;

// Download a small JSON reply into a heap buffer of maxBody bytes and parse it.
bool runBody(const char* url, size_t maxBody, bool (*parse)(const char*, size_t), net::GetResult& r) {
  char* body = static_cast<char*>(malloc(maxBody));
  if (body) r = net::httpsGet(url, body, maxBody);
  else strncpy(r.error, "no memory", sizeof r.error - 1);
  const bool ok = r.ok && parse(body, r.len);
  if (r.ok && !ok) strncpy(r.error, "bad data", sizeof r.error - 1);
  free(body);
  return ok;
}
bool parseWeatherBody(const char* b, size_t n) { return parseWeather(b, n, scratchWeather); }
bool parseHnBody(const char* b, size_t n) { return parseHn(b, n, scratchHn); }
bool runWeather(const char* url, net::GetResult& r) { return runBody(url, 4096, parseWeatherBody, r); }
bool runHn(const char* url, net::GetResult& r) { return runBody(url, 12288, parseHnBody, r); }

// The calendar can be megabytes, so it streams through the parser (about 4.4 KB, on the heap
// for the length of the download) and is never held in memory.
bool runCalendar(const char* url, net::GetResult& r) {
  char zone[limits::TZ_MAX + 1];
  {
    Guard g;
    memcpy(zone, tzIana, sizeof zone);
  }
  IcsParser* parser = new (std::nothrow) IcsParser(time(nullptr), zone);
  if (!parser) {
    strncpy(r.error, "no memory", sizeof r.error - 1);
    return false;
  }
  r = net::httpsGetTo(url, *parser);
  const bool ok = r.ok && parser->finish(scratchCal);
  if (r.ok && !ok) strncpy(r.error, "not a calendar", sizeof r.error - 1);
  delete parser;
  return ok;
}

// Copy parsed data over the published snapshot, keeping its status (set by record()).
void publishWeather() { const SourceStatus s = weather.status; weather = scratchWeather; weather.status = s; }
void publishHn() { const SourceStatus s = hn.status; hn = scratchHn; hn.status = s; }
void publishCal() { const SourceStatus s = cal.status; cal = scratchCal; cal.status = s; }

struct Source {
  const char* name;
  bool (*run)(const char* url, net::GetResult& r);  // fetch and parse into the scratch snapshot
  void (*publish)();                                // called under lock after a good run
  SourceStatus* status;                             // published status, guarded by lock
};

const Source SOURCES[SRC_COUNT] = {
    {"weather", runWeather, publishWeather, &weather.status},
    {"hn", runHn, publishHn, &hn.status},
    {"calendar", runCalendar, publishCal, &cal.status},
};
fetch::SourcePolicy policies[SRC_COUNT] = {fetch::SourcePolicy(fetch::WEATHER_INTERVAL_MS),
                                           fetch::SourcePolicy(fetch::HN_INTERVAL_MS),
                                           fetch::SourcePolicy(fetch::CALENDAR_INTERVAL_MS)};

bool ensureInit() {  // loop thread
  if (!lock) {
    lock = xSemaphoreCreateMutex();
    if (lock) hnUrl(shared[HN].url, URL_CAP);  // HN needs no settings: always configured
  }
  return lock != nullptr;
}

void wake() {
  if (task) xTaskNotifyGive(task);
}

// Takes the loop's requests into the task-owned policy and copies the URL out. Returns false if
// the source is not configured.
bool claim(uint8_t i, char* url) {
  Guard g;
  if (shared[i].reset) policies[i].reset();
  if (shared[i].requested) policies[i].requestNow();
  shared[i].reset = shared[i].requested = false;
  memcpy(url, shared[i].url, URL_CAP);
  return url[0] != 0;
}

void markOffline(uint8_t i) {
  Guard g;
  SourceStatus& s = *SOURCES[i].status;
  const SourceState next = fetch::offlineState(s.state);
  if (next == s.state) return;
  s.state = next;
  strncpy(s.error, "no network", sizeof s.error - 1);
  shared[i].changed = true;
}

void fetchOne(uint8_t i, const char* url) {
  const Source& src = SOURCES[i];
  const uint32_t start = millis();
  net::GetResult r{};
  const bool ok = src.run(url, r);
  bool kept = true;
  {
    Guard g;
    kept = strcmp(url, shared[i].url) == 0;  // settings changed mid-fetch: the result is stale
    if (kept) {
      if (ok) src.publish();
      fetch::record(*src.status, ok, r.error, time(nullptr));
      shared[i].changed = true;
    }
  }
  const uint32_t now = millis();
  if (ok) policies[i].onSuccess(now);
  else policies[i].onFailure(now);
  // Never log the URL: the calendar link is a secret and the weather one carries the location.
  LOG_I("fetch: %s %s%s in %lu ms, %u bytes, heap %u, stack min %u", src.name, ok ? "ok" : r.error,
        kept ? "" : " (discarded)", static_cast<unsigned long>(now - start), static_cast<unsigned>(r.len),
        static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void fetchTask(void*) {
  char url[URL_CAP];  // 400 B of the 12 KB stack
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(IDLE_CHECK_MS));
    const bool online = net::wifiState() == WifiState::Connected;
    for (uint8_t i = 0; i < SRC_COUNT; i++) {
      if (!claim(i, url) || !policies[i].due(millis())) continue;
      if (!online) markOffline(i);              // still due: fetched as soon as WiFi is back
      else if (net::timeSynced()) fetchOne(i, url);  // TLS needs the date; wait for NTP
    }
  }
}

template <typename Snap>
bool copyOut(uint8_t i, const Snap& from, Snap& out) {
  if (!ensureInit()) {
    out = from;  // nothing can have been fetched without the lock
    return false;
  }
  Guard g;
  out = from;
  const bool changed = shared[i].changed;
  shared[i].changed = false;
  return changed;
}
}  // namespace

namespace net {

void beginFetcher() {
  if (task || !ensureInit()) return;
  if (xTaskCreatePinnedToCore(fetchTask, "fetch", TASK_STACK_BYTES, nullptr, TASK_PRIORITY, &task, TASK_CORE) !=
      pdPASS) {
    task = nullptr;
    LOG_E("fetch: task not created (heap %u)", static_cast<unsigned>(ESP.getFreeHeap()));
  }
}

void onFeedChanged(const FeedConfig& feed) {
  if (!ensureInit()) return;
  setTimezone(feed.tzPosix);
  char url[URL_CAP];
  if (!feed.weather.set || weatherUrl(feed, url, sizeof url) == 0) url[0] = 0;
  char calUrl[URL_CAP] = "";  // webcal:// is plain https underneath
  const bool webcal = strncmp(feed.calendarUrl, "webcal://", 9) == 0;
  snprintf(calUrl, sizeof calUrl, "%s%s", webcal ? "https://" : "", feed.calendarUrl + (webcal ? 9 : 0));
  {
    Guard g;
    if (strcmp(url, shared[WEATHER].url) != 0) {
      // New place, units or zone: the old numbers are for another request, so drop them.
      memcpy(shared[WEATHER].url, url, URL_CAP);
      weather = WeatherSnap{};
      shared[WEATHER].reset = shared[WEATHER].requested = shared[WEATHER].changed = true;
    }
    if (strcmp(feed.tzIana, tzIana) != 0) {
      strncpy(tzIana, feed.tzIana, sizeof tzIana - 1);
      shared[CALENDAR].requested = true;  // "today" moved
    }
    if (strcmp(calUrl, shared[CALENDAR].url) != 0) {  // new, changed or removed link
      memcpy(shared[CALENDAR].url, calUrl, URL_CAP);
      cal = CalSnap{};
      shared[CALENDAR].reset = shared[CALENDAR].requested = shared[CALENDAR].changed = true;
    }
  }
  wake();
}

void requestRefresh() {
  if (!ensureInit()) return;
  {
    Guard g;
    for (auto& s : shared) s.requested = true;
  }
  wake();
}

bool copyWeather(WeatherSnap& out) { return copyOut(WEATHER, weather, out); }
bool copyHn(HnSnap& out) { return copyOut(HN, hn, out); }
bool copyCalendar(CalSnap& out) { return copyOut(CALENDAR, cal, out); }

void sourceStatuses(SourceStatus& w, SourceStatus& h, SourceStatus& c) {
  if (!ensureInit()) return;
  Guard g;
  w = weather.status;
  h = hn.status;
  c = cal.status;
}

}  // namespace net
#endif

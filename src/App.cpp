#include "App.h"
#if FEATURE_DASHBOARD && FEATURE_EPAPER
#include <Arduino.h>
#include <string.h>
#include <time.h>

#include "Api.h"
#include "Canvas.h"
#include "Clock.h"
#include "FeedFile.h"
#include "Fetcher.h"
#include "Log.h"
#include "Net.h"
#include "Pages.h"
#include "Panel.h"
#include "Rotation.h"
#include "generated_config.h"

namespace {
constexpr uint32_t ADDRESS_SHOW_MS = 30000;        // how long the "edit me at" screen stays up
constexpr uint32_t FULL_REFRESH_MS = 3600 * 1000;  // a still page is redrawn fully this often

FeedConfig config;  // about 8 KB each: static, never on the stack
WeatherSnap weather;
HnSnap hn;
CalSnap cal;
Canvas live;     // what the panel shows
Canvas scratch;  // drawn first, so an unchanged screen costs no refresh
Rotation rotation;

bool everConnected = false;
bool redraw = true;        // something that may change the pixels happened
bool wantFull = true;      // the next refresh must be a full one
bool addressShown = false;  // the address screen is up
uint32_t addressUntil = 0;
uint32_t lastFullAt = 0;
int shownMinute = -1;
int shownDay = -1;

PageContext context(PageId page) {
  return {config, weather, hn, cal, time(nullptr), net::timeSynced(), rotation.visit(page)};
}

void applyFeed(uint32_t now) {
  PageId enabled[limits::PAGE_COUNT];
  uint8_t n = 0;
  for (const PageSlot& slot : config.pages)
    if (slot.on) enabled[n++] = slot.id;
  rotation.setDwell(static_cast<uint32_t>(config.dwellSec) * 1000);
  rotation.setPages(enabled, n, now);
  redraw = wantFull = true;
}

void firstBootFeed() {
  feed::setDefaults(config);
  config.dwellSec = AppConfig::DEFAULT_DWELL_S;
  for (uint8_t i = 0; i < AppConfig::STARTER_MESSAGE_COUNT && i < limits::MESSAGES_MAX; i++) {
    Message& m = config.messages[config.messageCount++];
    strlcpy(m.text, AppConfig::STARTER_MESSAGES[i].text, sizeof m.text);
    strlcpy(m.by, AppConfig::STARTER_MESSAGES[i].by, sizeof m.by);
  }
}

// Draws what should be on the panel now into `into`. Returns true for the clock page, whose
// minute tick may use a fast refresh.
bool compose(Canvas& into, uint32_t now) {
  const WifiState wifi = net::wifiState();
  PageId page;
  addressShown = false;
  if (wifi == WifiState::NoCredentials) {
    pages::renderSetup(SetupState::NoWifi, "Plug me into a computer and open the installer page to set up WiFi.", into);
  } else if (!everConnected) {
    pages::renderSetup(SetupState::Connecting, "This can take a few seconds.", into);
  } else if (static_cast<int32_t>(addressUntil - now) > 0 && net::deviceUrl()[0]) {
    char detail[96];
    snprintf(detail, sizeof detail, "%s or http://%s.local/", net::deviceUrl(), AppConfig::WIFI_HOSTNAME);
    pages::renderSetup(SetupState::Ready, detail, into);
    addressShown = true;
  } else if (!rotation.current(page)) {
    pages::renderSetup(SetupState::NoPages, net::deviceUrl(), into);
  } else {
    pages::render(page, context(page), into);
    return pages::usesClock(page);
  }
  return false;
}

void refreshPanel(uint32_t now) {
  const bool clockPage = compose(scratch, now);
  redraw = false;
  if (memcmp(scratch.rows(), live.rows(), Canvas::PACKED_BYTES) == 0 && !wantFull) return;
  const Refresh mode = (wantFull || !clockPage) ? Refresh::Full : Refresh::Fast;
  scratch.packRows(const_cast<uint8_t*>(live.rows()));
  panel::show(live, mode);
  if (mode == Refresh::Full) lastFullAt = millis();
  wantFull = false;
}

// --- hooks for the web API (loop thread, inside net::updateApi) ---
void onFeedSaved() { applyFeed(millis()); }
void renderPage(PageId page, Canvas& into) { pages::render(page, context(page), into); }
const Canvas* liveCanvas() { return &live; }
bool showPage(PageId page) {
  rotation.show(page, millis());
  addressUntil = millis();  // a chosen page replaces the address screen
  redraw = wantFull = true;
  return true;
}
const char* currentPage() {
  PageId page;
  return (everConnected && !addressShown && rotation.current(page)) ? feed::pageName(page) : "";
}
uint32_t refreshCount() { return panel::refreshCount(); }
uint16_t calendarSkipped() { return cal.skipped; }
}  // namespace

namespace app {
void begin() {
  panel::begin();
  if (!feedfile::begin() || !feedfile::load(config)) {
    firstBootFeed();
    if (!feedfile::save(config)) LOG_W("app: the feed could not be stored; changes will not survive a restart");
  }
  net::beginClock(config.tzPosix);
  net::onFeedChanged(config);
  net::beginFetcher();
  static const net::ApiHooks hooks = {&config,      onFeedSaved, renderPage,   liveCanvas,
                                      showPage,   currentPage, refreshCount, calendarSkipped};
  net::beginApi(AppConfig::WIFI_HOSTNAME, AppConfig::PROJECT_NAME, hooks);
  applyFeed(millis());
  refreshPanel(millis());
}

void update(uint32_t now) {
  net::updateApi();
  if (!everConnected && net::wifiState() == WifiState::Connected) {
    everConnected = true;
    addressUntil = now + ADDRESS_SHOW_MS;  // first connection since boot: say where the web app is
    redraw = wantFull = true;
  }
  // `|`, not `||`: every source's "changed" flag must be consumed.
  if (net::copyWeather(weather) | net::copyHn(hn) | net::copyCalendar(cal)) redraw = wantFull = true;
  if (rotation.tick(now)) redraw = wantFull = true;
  if (addressShown && static_cast<int32_t>(addressUntil - now) <= 0) redraw = wantFull = true;

  struct tm local;
  const time_t t = time(nullptr);
  localtime_r(&t, &local);
  if (local.tm_min != shownMinute) {  // the clock page ticks; other pages come out identical
    shownMinute = local.tm_min;
    redraw = true;
  }
  if (net::timeSynced() && local.tm_yday != shownDay) {  // a new day: today's calendar changed
    if (shownDay >= 0) net::requestRefresh();
    shownDay = local.tm_yday;
  }
  if (static_cast<uint32_t>(now - lastFullAt) >= FULL_REFRESH_MS) redraw = wantFull = true;
  if (redraw) refreshPanel(now);
}

void onClick(uint32_t now) {
  if (addressShown) addressUntil = now;  // dismiss the address screen
  else rotation.next(now);
  redraw = wantFull = true;
}

void onHold(uint32_t now) {
  net::requestRefresh();
  addressUntil = now + ADDRESS_SHOW_MS;
  redraw = wantFull = true;
}
}  // namespace app
#endif  // FEATURE_DASHBOARD && FEATURE_EPAPER

// Wiring only: board, WiFi (provisioned over Improv Serial), optional OTA, the one button and
// the dashboard app. Click = next page; hold = refetch and show the web app's address.
#include <Arduino.h>

#include "Board.h"
#include "Gesture.h"
#include "Log.h"
#include "generated_config.h"
#define APP_DASHBOARD (FEATURE_DASHBOARD && FEATURE_EPAPER)
#if APP_DASHBOARD
#include "App.h"
#endif
#if FEATURE_WIFI
#include "Net.h"
#endif

namespace {
Gesture button{AppConfig::DEBOUNCE_MS, AppConfig::DOUBLE_CLICK_MS, AppConfig::LONG_PRESS_MS};

#if FEATURE_OTA
bool otaStarted = false;  // set by the start hook; still set after updateOta() only if the transfer failed
// Runs inside updateOta() as a transfer begins; the loop does not run until it fails or restarts.
void onOtaStart() { otaStarted = true; }
#endif

void resetButton(uint32_t now) {
  button.setAssigned(false, true);  // single click and hold; no double click
  button.reset(board::isPressed(0), now);
}
}  // namespace

void setup() {
  Serial.begin(115200);  // never wait for a USB serial host
  board::begin();
  LOG_I("%s %s on %s", AppConfig::PROJECT_NAME, FIRMWARE_VERSION, BOARD_LABEL);
#if FEATURE_WIFI
  net::beginWifi(AppConfig::WIFI_HOSTNAME, AppConfig::DEV_WIFI_SSID, AppConfig::DEV_WIFI_PASSWORD);
  net::beginImprov(AppConfig::PROJECT_NAME, FIRMWARE_VERSION, ESP.getChipModel(), AppConfig::PROJECT_NAME);
#endif
#if FEATURE_OTA
  net::beginOta(AppConfig::WIFI_HOSTNAME, AppConfig::OTA_PASSWORD, onOtaStart);
#endif
#if APP_DASHBOARD
  app::begin();
#endif
  board::update();
  resetButton(millis());
}

void loop() {
#if FEATURE_OTA
  net::updateOta();  // a transfer runs inline; success restarts, so returning after a start means it failed
  if (otaStarted) {
    otaStarted = false;
    board::update();
    resetButton(millis());  // input during the transfer is discarded
  }
#endif
  board::update();
  const uint32_t now = millis();
#if FEATURE_WIFI
  net::updateWifi(now);
  net::updateImprov();
#endif
  switch (button.update(board::isPressed(0), now)) {
#if APP_DASHBOARD
    case GestureEvent::SINGLE: app::onClick(now); break;
    case GestureEvent::HOLD: app::onHold(now); break;
#endif
    default: break;
  }
#if APP_DASHBOARD
  app::update(now);
#endif
  delay(1);
}

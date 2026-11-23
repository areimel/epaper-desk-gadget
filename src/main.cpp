// Desk gadget skeleton: WiFi is provisioned over Improv Serial, OTA is optional, and the one
// button's gestures are logged. Pages, sources and the e-paper panel are wired in by later tasks.
#include <Arduino.h>

#include "Board.h"
#include "Gesture.h"
#include "Log.h"
#include "generated_config.h"
#if FEATURE_EPAPER
#include "Panel.h"
#include "TestCard.h"
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

#if FEATURE_EPAPER
Canvas canvas;
int cardStep = 0;
void showCard(Refresh mode) {
  drawTestCard(canvas, FIRMWARE_VERSION, cardStep);
  const uint32_t start = millis();
  panel::show(canvas, mode);
  LOG_I("panel: step %d %s refresh took %lu ms", cardStep, mode == Refresh::Full ? "full" : "fast",
        static_cast<unsigned long>(millis() - start));
}
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
#if FEATURE_EPAPER
  panel::begin();
  showCard(Refresh::Full);
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
    case GestureEvent::SINGLE:
      LOG_I("click");
#if FEATURE_EPAPER
      cardStep++;
      showCard(Refresh::Fast);
#endif
      break;
    case GestureEvent::HOLD:
      LOG_I("hold");
#if FEATURE_EPAPER
      cardStep = 0;
      showCard(Refresh::Full);
#endif
      break;
    default: break;
  }
  delay(1);
}

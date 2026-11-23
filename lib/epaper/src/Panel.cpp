#include "Panel.h"
#if FEATURE_EPAPER
#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include "Board.h"
#if BOARD_EPD_PANEL_FC1
#include <epd/GxEPD2_213_FC1.h>
using Driver = GxEPD2_213_FC1;  // Wireless Paper V1.1 (JD79656)
#elif BOARD_EPD_PANEL_E0213A367
#include <epd/GxEPD2_213_E0213A367.h>
using Driver = GxEPD2_213_E0213A367;  // V1.1.1 / V1.2 (SSD1682)
#elif BOARD_EPD_PANEL_BN
#include <epd/GxEPD2_213_BN.h>
using Driver = GxEPD2_213_BN;  // V1.0 (SSD1680)
#else
#error "boards.yaml: epaper.panel has no driver in Panel.cpp"
#endif

namespace {
// The controller is portrait, 128 columns (122 visible) by 250 rows, 1 = white.
constexpr int kPanelW = 128;
constexpr int kPanelH = 250;
constexpr int kPanelRowBytes = kPanelW / 8;
constexpr int kColumnOffset = 0;  // first visible controller column
constexpr bool kFlip = false;     // true rotates the image 180 degrees

SPIClass spi(HSPI);  // own bus; the default one is wired to the LoRa radio
Driver epd(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY, spi);
uint8_t frame[kPanelRowBytes * kPanelH];
uint32_t refreshes = 0;
uint8_t fastRun = 0;

void rotate(const Canvas& canvas) {
  memset(frame, 0xFF, sizeof(frame));
  for (int y = 0; y < Canvas::H; y++) {
    for (int x = 0; x < Canvas::W; x++) {
      if (!canvas.pixel(x, y)) continue;
      const int col = kColumnOffset + (kFlip ? Canvas::H - 1 - y : y);
      const int row = kFlip ? x : kPanelH - 1 - x;
      frame[row * kPanelRowBytes + col / 8] &= ~(0x80 >> (col % 8));
    }
  }
}
}  // namespace

namespace panel {
void begin() {
  board::setPeripheralPower(true);
  delay(100);  // let the panel rail settle before talking to the controller
  spi.begin(BOARD_EPD_SCK, -1, BOARD_EPD_MOSI, BOARD_EPD_CS);
  epd.init(0, true, 20, false);  // no serial diagnostics; first refresh is a full one
}

void show(const Canvas& canvas, Refresh mode) {
  const bool full = mode == Refresh::Full || refreshes == 0 || fastRun >= FAST_LIMIT;
  rotate(canvas);
  epd.writeImage(frame, 0, 0, kPanelW, kPanelH);
  epd.refresh(!full);
  epd.writeImageAgain(frame, 0, 0, kPanelW, kPanelH);  // keep "previous" in step for the next fast refresh
  epd.powerOff();  // stop driving the panel between updates; the image stays
  fastRun = full ? 0 : fastRun + 1;
  refreshes++;
}

uint32_t refreshCount() { return refreshes; }
}  // namespace panel
#endif  // FEATURE_EPAPER

#pragma once
#include <Arduino.h>

// Hardware abstraction selected at build time by boards/boards.yaml (BOARD_DRIVER_HELTEC).
// This is the only place that touches button pins, the status LED or peripheral power.
// Nothing here blocks on USB serial.
namespace board {
void begin();              // pinMode setup; peripheral power starts off
void update();             // poll buttons; consumers debounce
uint8_t buttonCount();
bool isPressed(uint8_t i);  // false for an out-of-range index
void setStatusLed(bool on);  // no-op when BOARD_LED_PIN < 0
// Switches the board's peripheral rail (e-paper panel on the Wireless Paper). No-op without one.
void setPeripheralPower(bool on);
}  // namespace board

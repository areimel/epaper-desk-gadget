#include "Board.h"
#if BOARD_DRIVER_HELTEC
namespace {
constexpr uint8_t kPins[] = BOARD_BUTTON_PINS;  // active low, INPUT_PULLUP
}

namespace board {
void begin() {
  for (uint8_t pin : kPins) pinMode(pin, INPUT_PULLUP);
#if BOARD_LED_PIN >= 0
  pinMode(BOARD_LED_PIN, OUTPUT);
  setStatusLed(false);
#endif
#ifdef BOARD_POWER_PIN
  pinMode(BOARD_POWER_PIN, OUTPUT);
  setPeripheralPower(false);
#endif
}

void update() {}
uint8_t buttonCount() { return sizeof(kPins); }
bool isPressed(uint8_t i) { return i < sizeof(kPins) && digitalRead(kPins[i]) == LOW; }

void setStatusLed(bool on) {
#if BOARD_LED_PIN >= 0
  digitalWrite(BOARD_LED_PIN, on ? HIGH : LOW);  // active high
#else
  (void)on;
#endif
}

void setPeripheralPower(bool on) {
#ifdef BOARD_POWER_PIN
  // The pin is driven to its active level to power the rail (active low on the Wireless Paper).
  const bool level = BOARD_POWER_ACTIVE_LOW ? !on : on;
  digitalWrite(BOARD_POWER_PIN, level ? HIGH : LOW);
#else
  (void)on;
#endif
}
}  // namespace board
#endif  // BOARD_DRIVER_HELTEC

#include "led.h"

#include <Arduino.h>

#include "config.h"

namespace {

uint32_t identifyUntilMs;
bool identifyActive = false;
int lastLevel = -1;

void write(bool on) {
  if ((int)on == lastLevel) return;
  digitalWrite(LED_PIN, on == (bool)LED_ACTIVE_HIGH ? HIGH : LOW);
#if ONBOARD_LED_PIN >= 0
  digitalWrite(ONBOARD_LED_PIN, on == (bool)ONBOARD_LED_ACTIVE_HIGH ? HIGH : LOW);
#endif
  lastLevel = on;
}

}  // namespace

void led_begin() {
  pinMode(LED_PIN, OUTPUT);
#if ONBOARD_LED_PIN >= 0
  pinMode(ONBOARD_LED_PIN, OUTPUT);
#endif
  write(false);
}

void led_identify() {
  identifyUntilMs = millis() + IDENTIFY_MS;
  identifyActive = true;
}

void led_update(LedMode mode, int64_t phaseUs) {
  uint32_t now = millis();

  if (identifyActive) {
    if ((int32_t)(now - identifyUntilMs) < 0) {
      write((now / IDENTIFY_BLINK_MS) % 2 == 0);
      return;
    }
    identifyActive = false;
  }

  bool on = false;
  switch (mode) {
    case LED_ON:
      on = true;
      break;
    case LED_FAST:
      on = (now / FAST_BLINK_MS) % 2 == 0;
      break;
    case LED_SLOW:
      on = phaseUs >= 0 && (phaseUs / (SLOW_BLINK_MS * 1000LL)) % 2 == 0;
      break;
    case LED_OFF:
      break;
  }
  write(on);
}

#include "led.h"

#include <Arduino.h>

#include "config.h"

namespace {

uint32_t identifyUntilMs;
bool identifyActive = false;
int lastLevel = -1;

void write(bool on) {
  int level = (on == (bool)LED_ACTIVE_HIGH) ? HIGH : LOW;
  if (level == lastLevel) return;
  digitalWrite(LED_PIN, level);
  lastLevel = level;
}

}  // namespace

void led_begin() {
  pinMode(LED_PIN, OUTPUT);
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

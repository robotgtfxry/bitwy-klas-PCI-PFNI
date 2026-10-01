#include "led.h"

#include <Arduino.h>

#include "config.h"

#if LED_STRIP_PIN >= 0
#include <Adafruit_NeoPixel.h>
#endif

namespace {

uint32_t identifyUntilMs;
bool identifyActive = false;
int lastLevel = -1;

#if LED_STRIP_PIN >= 0
Adafruit_NeoPixel strip(LED_STRIP_COUNT, LED_STRIP_PIN, LED_STRIP_TYPE);
uint32_t stripColor = LED_STRIP_COLOR;

void showStrip(bool on) {
  strip.fill(on ? stripColor : 0);
  strip.show();
}
#endif

void write(bool on) {
  if ((int)on == lastLevel) return;
  digitalWrite(LED_PIN, on == (bool)LED_ACTIVE_HIGH ? HIGH : LOW);
#if ONBOARD_LED_PIN >= 0
  digitalWrite(ONBOARD_LED_PIN, on == (bool)ONBOARD_LED_ACTIVE_HIGH ? HIGH : LOW);
#endif
#if LED_STRIP_PIN >= 0
  showStrip(on);
#endif
  lastLevel = on;
}

}  // namespace

void led_begin() {
  pinMode(LED_PIN, OUTPUT);
#if ONBOARD_LED_PIN >= 0
  pinMode(ONBOARD_LED_PIN, OUTPUT);
#endif
#if LED_STRIP_PIN >= 0
  strip.begin();
#endif
  write(false);
}

void led_identify() {
  identifyUntilMs = millis() + IDENTIFY_MS;
  identifyActive = true;
}

void led_setStripColor(uint32_t color) {
#if LED_STRIP_PIN >= 0
  if (color == stripColor) return;
  stripColor = color;
  if (lastLevel == 1) showStrip(true);
#endif
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

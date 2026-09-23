#include "button.h"

#include <Arduino.h>

#include "config.h"

namespace {

volatile int64_t edgeUs = 0;
volatile bool edgePending = false;
bool down = false;
int64_t releaseStartUs = 0;  // 0 = przycisk nie jest w trakcie puszczania

bool rawPressed() {
  int v = digitalRead(BUTTON_PIN);
  return BUTTON_ACTIVE_LOW ? v == LOW : v == HIGH;
}

void IRAM_ATTR onEdge() {
  // Zapamiętujemy tylko pierwsze zbocze – drgania styków go nie nadpiszą.
  if (!edgePending) {
    edgeUs = esp_timer_get_time();
    edgePending = true;
  }
}

}  // namespace

void button_begin() {
  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  down = rawPressed();  // wciśnięty przy starcie -> najpierw musi zostać puszczony
  edgePending = down;
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onEdge, BUTTON_ACTIVE_LOW ? FALLING : RISING);
}

ButtonEvent button_poll(int64_t& tsUs) {
  int64_t now = esp_timer_get_time();
  bool pressed = rawPressed();

  if (!down) {
    if (!edgePending) return BTN_NONE;
    int64_t age = now - edgeUs;
    if (age < DEBOUNCE_CONFIRM_MS * 1000LL) return BTN_NONE;
    if (pressed) {
      down = true;
      releaseStartUs = 0;
      tsUs = edgeUs;
      return BTN_PRESS;
    }
    if (age > DEBOUNCE_GLITCH_MS * 1000LL) edgePending = false;  // zakłócenie, nie klik
    return BTN_NONE;
  }

  if (pressed) {
    releaseStartUs = 0;
    return BTN_NONE;
  }
  if (releaseStartUs == 0) releaseStartUs = now;
  if (now - releaseStartUs < DEBOUNCE_RELEASE_MS * 1000LL) return BTN_NONE;

  down = false;
  edgePending = false;  // uzbrój ponownie dopiero po stabilnym puszczeniu
  tsUs = now;
  return BTN_RELEASE;
}

bool button_isDown() { return down; }

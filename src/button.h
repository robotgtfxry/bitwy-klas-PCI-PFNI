#pragma once

#include <stdint.h>

enum ButtonEvent : uint8_t { BTN_NONE = 0, BTN_PRESS, BTN_RELEASE };

void button_begin();

// Wołać w każdym obiegu loop(). Dla BTN_PRESS tsUs = moment pierwszego zbocza
// (esp_timer_get_time() złapany w przerwaniu) – to on decyduje, kto był pierwszy.
ButtonEvent button_poll(int64_t& tsUs);

bool button_isDown();

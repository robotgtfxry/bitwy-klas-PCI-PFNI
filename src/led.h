#pragma once

#include <stdint.h>

enum LedMode : uint8_t { LED_OFF = 0, LED_ON, LED_FAST, LED_SLOW };

void led_begin();

// Krótka seria bardzo szybkich mrugnięć – nadpisuje bieżący tryb na IDENTIFY_MS.
void led_identify();

// phaseUs: dla LED_SLOW czas (zegar mastera), od którego liczona jest faza migania.
// Dzięki temu wszystkie płytki migają w tym samym rytmie.
void led_update(LedMode mode, int64_t phaseUs);

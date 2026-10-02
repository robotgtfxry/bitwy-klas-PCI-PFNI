#pragma once

#include <stdint.h>

enum LedMode : uint8_t { LED_OFF = 0, LED_ON, LED_FAST, LED_SLOW };

void led_begin();

// Krótka seria bardzo szybkich mrugnięć – nadpisuje bieżący tryb na IDENTIFY_MS.
void led_identify();

// Taśma LED: kolor (0xRRGGBB), czy jest aktywna i jasność (1..255); świecąca taśma zmienia się od razu.
// Nieaktywna taśma pozostaje zgaszona – dioda z D5 i wbudowana działają bez zmian.
void led_setStrip(uint32_t color, bool enabled, uint8_t brightness);

// phaseUs: dla LED_SLOW czas (zegar mastera), od którego liczona jest faza migania.
// Dzięki temu wszystkie płytki migają w tym samym rytmie.
void led_update(LedMode mode, int64_t phaseUs);

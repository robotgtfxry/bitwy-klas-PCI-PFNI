#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

Preferences prefs;
NodeSettings node;
GameConfig cfg;

}  // namespace

void copyName(char* dst, const char* src) {
  size_t n = 0;
  while (src[n] && n < NAME_LEN) n++;
  // Pierwszy odcięty bajt jest kontynuacją znaku UTF-8 -> cofnij do jego początku.
  if (src[n]) {
    while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

void storage_begin() {
  prefs.begin("bitwy", false);

  // Nazwa z panelu żyje tylko do restartu – po starcie zawsze "Przycisk N" wg kolejności podłączenia.
  node.name[0] = 0;
  if (prefs.isKey("name")) prefs.remove("name");  // sprzątanie po starszych wersjach, które ją zapisywały
  node.enabled = prefs.isKey("en") ? prefs.getBool("en") : true;

  cfg.lockMs = prefs.isKey("lockMs") ? prefs.getUShort("lockMs") : DEFAULT_LOCK_MS;
  cfg.expectedNodes = prefs.isKey("expN") ? prefs.getUChar("expN") : DEFAULT_EXPECTED_NODES;
  cfg.stripColor = prefs.isKey("ledC") ? prefs.getUInt("ledC") : LED_STRIP_COLOR;
  // Włączenie taśmy żyje tylko do restartu – po włączeniu zestawu taśma jest zawsze wyłączona.
  cfg.stripOn = LED_STRIP_DEFAULT_ON;
  if (prefs.isKey("ledOn")) prefs.remove("ledOn");  // sprzątanie po wersji, która to zapisywała
  cfg.stripBright = prefs.isKey("ledB") ? prefs.getUChar("ledB") : LED_STRIP_BRIGHTNESS;
}

NodeSettings& storage_node() { return node; }

void storage_setName(const char* name) {
  copyName(node.name, name);
}

void storage_setEnabled(bool enabled) {
  if (node.enabled == enabled) return;
  node.enabled = enabled;
  prefs.putBool("en", enabled);
}

GameConfig& storage_config() { return cfg; }

void storage_setConfig(uint16_t lockMs, uint8_t expectedNodes) {
  if (cfg.lockMs == lockMs && cfg.expectedNodes == expectedNodes) return;
  cfg.lockMs = lockMs;
  cfg.expectedNodes = expectedNodes;
  prefs.putUShort("lockMs", lockMs);
  prefs.putUChar("expN", expectedNodes);
}

void storage_setStrip(uint32_t color, bool on, uint8_t brightness) {
  if (cfg.stripColor != color) {
    cfg.stripColor = color;
    prefs.putUInt("ledC", color);
  }
  cfg.stripOn = on;  // tylko w RAM: przetrwa zmianę mastera, ale nie restart
  if (cfg.stripBright != brightness) {
    cfg.stripBright = brightness;
    prefs.putUChar("ledB", brightness);
  }
}

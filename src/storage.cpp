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

  node.name[0] = 0;  // brak własnej nazwy = "Przycisk N" wg kolejności podłączenia
  if (prefs.isKey("name")) copyName(node.name, prefs.getString("name").c_str());
  node.enabled = prefs.isKey("en") ? prefs.getBool("en") : true;

  cfg.lockMs = prefs.isKey("lockMs") ? prefs.getUShort("lockMs") : DEFAULT_LOCK_MS;
  cfg.expectedNodes = prefs.isKey("expN") ? prefs.getUChar("expN") : DEFAULT_EXPECTED_NODES;
}

NodeSettings& storage_node() { return node; }

void storage_setName(const char* name) {
  char tmp[NAME_LEN + 1];
  copyName(tmp, name);
  if (strcmp(tmp, node.name) == 0) return;
  strcpy(node.name, tmp);
  if (tmp[0]) {
    prefs.putString("name", tmp);
  } else {
    prefs.remove("name");
  }
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

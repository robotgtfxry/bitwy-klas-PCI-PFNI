#include "storage.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#if __has_include(<esp_mac.h>)
#include <esp_mac.h>
#endif

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

void defaultNodeName(char* dst, const uint8_t* id) {
  snprintf(dst, NAME_LEN + 1, "Przycisk-%02X%02X", id[4], id[5]);
}

void storage_begin() {
  prefs.begin("bitwy", false);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  defaultNodeName(node.name, mac);
  if (prefs.isKey("name")) copyName(node.name, prefs.getString("name").c_str());
  node.enabled = prefs.isKey("en") ? prefs.getBool("en") : true;

  cfg.lockMs = prefs.isKey("lockMs") ? prefs.getUShort("lockMs") : DEFAULT_LOCK_MS;
  cfg.expectedNodes = prefs.isKey("expN") ? prefs.getUChar("expN") : DEFAULT_EXPECTED_NODES;
}

NodeSettings& storage_node() { return node; }

void storage_setName(const char* name) {
  char tmp[NAME_LEN + 1];
  copyName(tmp, name);
  if (!tmp[0] || strcmp(tmp, node.name) == 0) return;
  strcpy(node.name, tmp);
  prefs.putString("name", node.name);
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

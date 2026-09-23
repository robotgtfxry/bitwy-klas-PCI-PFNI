#pragma once

#include <stdint.h>

#include "config.h"

struct NodeSettings {
  char name[NAME_LEN + 1];  // własna nazwa z panelu (tylko w RAM), "" = domyślna "Przycisk N"
  bool enabled;
};

struct GameConfig {
  uint16_t lockMs;
  uint8_t expectedNodes;
};

void storage_begin();

NodeSettings& storage_node();
void storage_setName(const char* name);  // nie jest zapisywana – po restarcie wraca "Przycisk N"
void storage_setEnabled(bool enabled);

GameConfig& storage_config();
void storage_setConfig(uint16_t lockMs, uint8_t expectedNodes);

// Kopiuje nazwę, ucinając do NAME_LEN bajtów bez rozcinania znaku UTF-8.
void copyName(char* dst, const char* src);

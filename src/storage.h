#pragma once

#include <stdint.h>

#include "config.h"

struct NodeSettings {
  char name[NAME_LEN + 1];
  bool enabled;
};

struct GameConfig {
  uint16_t lockMs;
  uint8_t expectedNodes;
};

void storage_begin();

NodeSettings& storage_node();
void storage_setName(const char* name);
void storage_setEnabled(bool enabled);

GameConfig& storage_config();
void storage_setConfig(uint16_t lockMs, uint8_t expectedNodes);

// Kopiuje nazwę, ucinając do NAME_LEN bajtów bez rozcinania znaku UTF-8.
void copyName(char* dst, const char* src);
void defaultNodeName(char* dst, const uint8_t* id);

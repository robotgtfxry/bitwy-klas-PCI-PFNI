#pragma once

#include <Arduino.h>

#include "protocol.h"

enum Role : uint8_t { ROLE_SEARCHING = 0, ROLE_SLAVE, ROLE_MASTER };

// Wpis w tabeli węzłów mastera (master też ma swój wpis, z self = true).
struct NodeInfo {
  bool used;
  bool self;
  uint8_t id[6];    // MAC STA – stałe ID płytki
  uint8_t addr[6];  // adres ESP-NOW, pod który wysyłamy
  uint8_t number;   // kolejność podłączenia: 1, 2, 3...
  char name[NAME_LEN + 1];  // własna nazwa, "" = "Przycisk N"
  bool enabled;
  bool online;
  bool ready;  // kliknięty od dołączenia (do tego czasu miga)
  bool buttonDown;
  uint8_t linkQuality;  // % odebranych beaconów
  uint16_t rttUs;
  uint32_t lastSeenMs;
  uint32_t uptimeMs;
  uint32_t lastPressMs;
  uint16_t lastPressId;
  uint16_t wins;
};

void mesh_begin();
void mesh_loop();

Role mesh_role();
// Slave: ma mastera i zsynchronizowany zegar. Master: jest online co najmniej jeden slave.
bool mesh_connected();

int64_t mesh_masterNowUs();
int64_t mesh_toMasterUs(int64_t localUs);
const uint8_t* mesh_selfId();
uint8_t mesh_selfNumber();  // 0 = jeszcze nie nadany

void mesh_fillHeader(MsgHeader& h, MsgType type);
bool mesh_sendToMaster(const void* data, size_t len);
bool mesh_sendTo(const uint8_t* addr, const void* data, size_t len);
void mesh_beaconNow();  // natychmiastowy beacon (+ krótka seria) po zmianie stanu gry

// Tabela węzłów (tylko master).
NodeInfo* mesh_nodes();  // MAX_NODES elementów
NodeInfo* mesh_findNode(const uint8_t* id);
int mesh_onlineCount();

// Nazwa do wyświetlenia: własna z panelu albo "Przycisk N".
String nodeLabel(const NodeInfo& n);
String macToStr(const uint8_t* mac);
bool strToMac(const char* s, uint8_t* mac);

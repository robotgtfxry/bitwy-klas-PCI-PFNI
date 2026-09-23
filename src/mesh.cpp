#include "mesh.h"

#include <WiFi.h>
#include <esp_arduino_version.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>
#if __has_include(<esp_mac.h>)
#include <esp_mac.h>
#endif

#include "button.h"
#include "game.h"
#include "storage.h"
#include "web.h"

namespace {

const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct RxPacket {
  uint8_t src[6];
  int64_t rxUs;  // czas odbioru złapany w callbacku – potrzebny do synchronizacji
  uint8_t len;
  uint8_t data[250];
};

struct SyncSample {
  int64_t offsetUs;
  int32_t rttUs;
};

QueueHandle_t rxQueue;
bool espNowActive = false;
Role role = ROLE_SEARCHING;
uint8_t selfId[6];

// --- slave ---
uint8_t masterAddr[6];
uint32_t listenUntilMs;
uint32_t lastBeaconMs;
uint32_t joinedMs;
uint32_t nextHelloMs;
uint32_t nextSyncMs;
uint32_t lqWindowStartMs;
uint16_t lqBeacons;
uint8_t linkQuality;
uint32_t nextLogMs;

// --- synchronizacja zegara (slave) ---
SyncSample samples[SYNC_WINDOW];
int sampleCount;
int sampleNext;
int64_t offsetUs;  // czas mastera = czas lokalny + offsetUs
int32_t bestRttUs;
bool synced;

// --- master ---
uint32_t masterSinceMs;
uint32_t nextBeaconMs;
uint8_t beaconBurst;
NodeInfo nodes[MAX_NODES];

// ---------------------------------------------------------------------------

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* src = info->src_addr;
#else
void onRecv(const uint8_t* src, const uint8_t* data, int len) {
#endif
  int64_t now = esp_timer_get_time();
  if (len < (int)sizeof(MsgHeader) || len > (int)sizeof(RxPacket::data)) return;
  const MsgHeader* h = (const MsgHeader*)data;
  if (h->magic != PROTO_MAGIC || h->version != PROTO_VERSION || h->group != GROUP_ID) return;

  // Callback działa w wątku Wi-Fi – tylko kopiujemy do kolejki, obróbka w loop().
  RxPacket p;
  memcpy(p.src, src, 6);
  p.rxUs = now;
  p.len = len;
  memcpy(p.data, data, len);
  xQueueSend(rxQueue, &p, 0);
}

template <typename T>
bool readMsg(const RxPacket& p, T& out) {
  if (p.len < sizeof(T)) return false;
  memcpy(&out, p.data, sizeof(T));
  return true;
}

bool ensurePeer(const uint8_t* addr) {
  if (esp_now_is_peer_exist(addr)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, addr, 6);
  peer.channel = 0;  // bieżący kanał
  peer.ifidx = role == ROLE_MASTER ? WIFI_IF_AP : WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void radioInit(bool asMaster) {
  // esp_now_deinit() przed pierwszym esp_now_init() wysypuje IDF 4.4 (null pointer).
  if (espNowActive) {
    esp_now_deinit();
    espNowActive = false;
  }
  WiFi.persistent(false);
  if (asMaster) {
    // Jedno radio: punkt dostępowy do konfiguracji + ESP-NOW na tym samym kanale.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASSWORD, WIFI_CHANNEL, 0, AP_MAX_CLIENTS);
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
  }
  esp_wifi_set_ps(WIFI_PS_NONE);  // bez oszczędzania energii = najmniejsze opóźnienia

  if (esp_now_init() != ESP_OK) {
    Serial.println("[MESH] Błąd inicjalizacji ESP-NOW – restart");
    delay(100);
    ESP.restart();
  }
  espNowActive = true;
  esp_now_register_recv_cb(onRecv);
  ensurePeer(BROADCAST);
}

void resetSync() {
  sampleCount = 0;
  sampleNext = 0;
  offsetUs = 0;
  bestRttUs = 0;
  synced = false;
}

void startSearching() {
  role = ROLE_SEARCHING;
  resetSync();
  listenUntilMs = millis() + ELECTION_LISTEN_MS + esp_random() % ELECTION_JITTER_MS;
  Serial.printf("[MESH] Szukam mastera (nasłuch do %u ms)...\n", (unsigned)(listenUntilMs - millis()));
  game_onRoleChanged(role);
}

void becomeMaster() {
  role = ROLE_MASTER;
  radioInit(true);
  resetSync();
  synced = true;
  masterSinceMs = millis();
  nextBeaconMs = millis();
  beaconBurst = 0;

  memset(nodes, 0, sizeof(nodes));
  NodeInfo& self = nodes[0];
  self.used = true;
  self.self = true;
  memcpy(self.id, selfId, 6);
  memcpy(self.addr, selfId, 6);
  self.online = true;

  Serial.printf("[MESH] Zostaję MASTEREM. Wi-Fi: \"%s\", panel: http://%s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());
  game_onRoleChanged(role);
  web_begin();
}

void becomeSlave(const uint8_t* addr, const uint8_t* id) {
  role = ROLE_SLAVE;
  memcpy(masterAddr, addr, 6);
  ensurePeer(masterAddr);
  resetSync();
  uint32_t now = millis();
  lastBeaconMs = now;
  joinedMs = now;
  nextHelloMs = now;
  nextSyncMs = now;
  lqWindowStartMs = now;
  lqBeacons = 0;
  linkQuality = 100;
  nextLogMs = now + 3000;
  Serial.printf("[MESH] Dołączam jako SLAVE do mastera %s\n", macToStr(id).c_str());
  game_onRoleChanged(role);
}

NodeInfo* upsertNode(const uint8_t* id, const uint8_t* addr) {
  NodeInfo* n = mesh_findNode(id);
  if (!n) {
    for (auto& x : nodes) {
      if (!x.used) {
        n = &x;
        break;
      }
    }
    if (!n) {  // pełna tabela – zastąp najdawniej widziany węzeł offline
      for (auto& x : nodes) {
        if (!x.self && !x.online && (!n || (int32_t)(x.lastSeenMs - n->lastSeenMs) < 0)) n = &x;
      }
      if (!n) return nullptr;
      esp_now_del_peer(n->addr);
    }
    *n = NodeInfo{};
    n->used = true;
    memcpy(n->id, id, 6);
    defaultNodeName(n->name, id);
    n->enabled = true;
  }
  memcpy(n->addr, addr, 6);
  ensurePeer(addr);
  n->lastSeenMs = millis();
  if (!n->online) {
    n->online = true;
    Serial.printf("[MESH] Węzeł online: %s (%s), online: %d\n", n->name, macToStr(id).c_str(), mesh_onlineCount());
    web_notify();
  }
  return n;
}

// --- obsługa wiadomości -----------------------------------------------------

void handleBeacon(const RxPacket& p) {
  BeaconMsg b;
  if (!readMsg(p, b)) return;

  if (role == ROLE_MASTER) {
    // Dwóch masterów (np. włączone w tej samej chwili): zostaje ten, który jest nim dłużej.
    int64_t mine = millis() - masterSinceMs;
    int64_t theirs = b.masterForMs;
    bool iLose = theirs > mine + MASTER_TIE_MS ||
                 (llabs(theirs - mine) <= MASTER_TIE_MS && memcmp(b.h.id, selfId, 6) < 0);
    if (iLose) {
      Serial.printf("[MESH] Wykryto starszego mastera %s – restart jako slave\n", macToStr(b.h.id).c_str());
      delay(50);
      ESP.restart();
    }
    return;
  }

  if (role == ROLE_SEARCHING) becomeSlave(p.src, b.h.id);
  if (memcmp(p.src, masterAddr, 6) != 0) return;  // beacon innego mastera

  lastBeaconMs = millis();
  lqBeacons++;
  game_onBeacon(b.game);
}

void handleHello(const RxPacket& p) {
  HelloMsg m;
  if (role != ROLE_MASTER || !readMsg(p, m)) return;
  NodeInfo* n = upsertNode(m.h.id, p.src);
  if (!n) return;
  m.name[NAME_LEN] = 0;
  copyName(n->name, m.name);
  n->enabled = m.enabled;
  n->buttonDown = m.buttonDown;
  n->linkQuality = m.linkQuality;
  n->rttUs = m.rttUs;
  n->uptimeMs = m.uptimeMs;
}

void handleSyncReq(const RxPacket& p) {
  SyncReqMsg m;
  if (role != ROLE_MASTER || !readMsg(p, m)) return;
  if (!upsertNode(m.h.id, p.src)) return;
  SyncRespMsg r = {};
  mesh_fillHeader(r.h, MSG_SYNC_RESP);
  r.t1 = m.t1;
  r.t2 = p.rxUs;
  r.t3 = esp_timer_get_time();
  mesh_sendTo(p.src, &r, sizeof(r));
}

void handleSyncResp(const RxPacket& p) {
  SyncRespMsg m;
  if (role != ROLE_SLAVE || !readMsg(p, m) || memcmp(p.src, masterAddr, 6) != 0) return;
  int64_t t4 = p.rxUs;
  int64_t rtt = (t4 - m.t1) - (m.t3 - m.t2);
  if (rtt < 0 || rtt > SYNC_MAX_RTT_US) return;  // retransmisje / śmieci

  samples[sampleNext] = {((m.t2 - m.t1) + (m.t3 - t4)) / 2, (int32_t)rtt};
  sampleNext = (sampleNext + 1) % SYNC_WINDOW;
  if (sampleCount < SYNC_WINDOW) sampleCount++;

  // Najmniejsze RTT = najmniej zakłócony pomiar -> jego offset.
  int best = 0;
  for (int i = 1; i < sampleCount; i++) {
    if (samples[i].rttUs < samples[best].rttUs) best = i;
  }
  offsetUs = samples[best].offsetUs;
  bestRttUs = samples[best].rttUs;

  if (!synced) {
    synced = true;
    Serial.printf("[SYNC] Zegar zsynchronizowany (RTT %d us)\n", (int)bestRttUs);
  }
}

void handlePress(const RxPacket& p) {
  PressMsg m;
  if (role != ROLE_MASTER || !readMsg(p, m)) return;
  NodeInfo* n = upsertNode(m.h.id, p.src);
  if (n) game_onPress(n, m);
}

void handlePressAck(const RxPacket& p) {
  PressAckMsg m;
  if (role != ROLE_SLAVE || !readMsg(p, m) || memcmp(p.src, masterAddr, 6) != 0) return;
  game_onPressAck(m);
}

void handleCmd(const RxPacket& p) {
  CmdMsg m;
  if (role != ROLE_SLAVE || !readMsg(p, m) || memcmp(p.src, masterAddr, 6) != 0) return;
  game_onCmd(m);
}

void dispatch(const RxPacket& p) {
  switch (((const MsgHeader*)p.data)->type) {
    case MSG_BEACON: handleBeacon(p); break;
    case MSG_HELLO: handleHello(p); break;
    case MSG_SYNC_REQ: handleSyncReq(p); break;
    case MSG_SYNC_RESP: handleSyncResp(p); break;
    case MSG_PRESS: handlePress(p); break;
    case MSG_PRESS_ACK: handlePressAck(p); break;
    case MSG_CMD: handleCmd(p); break;
    default: break;
  }
}

// --- okresowe zadania -------------------------------------------------------

void sendBeacon() {
  BeaconMsg b = {};
  mesh_fillHeader(b.h, MSG_BEACON);
  b.masterForMs = millis() - masterSinceMs;
  b.game = game_snapshot();
  b.masterTimeUs = esp_timer_get_time();
  esp_now_send(BROADCAST, (const uint8_t*)&b, sizeof(b));
}

void sendHello() {
  HelloMsg m = {};
  mesh_fillHeader(m.h, MSG_HELLO);
  copyName(m.name, storage_node().name);
  m.enabled = storage_node().enabled;
  m.buttonDown = button_isDown();
  m.linkQuality = linkQuality;
  m.rttUs = bestRttUs > 65535 ? 65535 : bestRttUs;
  m.uptimeMs = millis();
  mesh_sendToMaster(&m, sizeof(m));
}

void sendSyncReq() {
  SyncReqMsg m = {};
  mesh_fillHeader(m.h, MSG_SYNC_REQ);
  m.t1 = esp_timer_get_time();
  mesh_sendToMaster(&m, sizeof(m));
}

void slaveLoop(uint32_t now) {
  if (now - lastBeaconMs > MASTER_TIMEOUT_MS) {
    Serial.println("[MESH] Utracono mastera – nowe wybory");
    esp_now_del_peer(masterAddr);
    startSearching();
    return;
  }
  if ((int32_t)(now - nextHelloMs) >= 0) {
    sendHello();
    nextHelloMs = now + HELLO_INTERVAL_MS;
  }
  if ((int32_t)(now - nextSyncMs) >= 0) {
    sendSyncReq();
    nextSyncMs = now + (now - joinedMs < SYNC_FAST_PERIOD_MS ? SYNC_FAST_INTERVAL_MS : SYNC_INTERVAL_MS);
  }
  if (now - lqWindowStartMs >= LINK_WINDOW_MS) {
    uint32_t q = lqBeacons * 100u / (LINK_WINDOW_MS / BEACON_INTERVAL_MS);
    linkQuality = q > 100 ? 100 : q;
    lqBeacons = 0;
    lqWindowStartMs = now;
  }
  if ((int32_t)(now - nextLogMs) >= 0) {
    Serial.printf("[SYNC] offset=%lld us, RTT=%d us, łącze=%u%%\n", (long long)offsetUs, (int)bestRttUs,
                  linkQuality);
    nextLogMs = now + 10000;
  }
}

void masterLoop(uint32_t now) {
  if ((int32_t)(now - nextBeaconMs) >= 0) {
    sendBeacon();
    if (beaconBurst > 0) {
      beaconBurst--;
      nextBeaconMs = now + BEACON_BURST_GAP_MS;
    } else {
      nextBeaconMs = now + BEACON_INTERVAL_MS;
    }
  }

  NodeInfo& self = nodes[0];
  copyName(self.name, storage_node().name);
  self.enabled = storage_node().enabled;
  self.lastSeenMs = now;
  self.uptimeMs = now;
  self.linkQuality = 100;

  for (auto& n : nodes) {
    if (n.used && !n.self && n.online && now - n.lastSeenMs > NODE_TIMEOUT_MS) {
      n.online = false;
      n.buttonDown = false;
      Serial.printf("[MESH] Węzeł offline: %s, online: %d\n", n.name, mesh_onlineCount());
      web_notify();
    }
  }

  if ((int32_t)(now - nextLogMs) >= 0) {
    Serial.printf("[MESH] MASTER, online %d:", mesh_onlineCount());
    for (auto& n : nodes) {
      if (n.used && n.online) Serial.printf(" [%s RTT %uus %u%%]", n.name, n.rttUs, n.linkQuality);
    }
    Serial.println();
    nextLogMs = now + 10000;
  }
}

}  // namespace

// ---------------------------------------------------------------------------

void mesh_begin() {
  esp_read_mac(selfId, ESP_MAC_WIFI_STA);
  rxQueue = xQueueCreate(24, sizeof(RxPacket));
  Serial.printf("[MESH] ID płytki: %s, nazwa: %s\n", macToStr(selfId).c_str(), storage_node().name);
  role = ROLE_SEARCHING;
  radioInit(false);
  startSearching();
}

void mesh_loop() {
  RxPacket p;
  while (xQueueReceive(rxQueue, &p, 0) == pdTRUE) dispatch(p);

  uint32_t now = millis();
  switch (role) {
    case ROLE_SEARCHING:
      if ((int32_t)(now - listenUntilMs) >= 0) becomeMaster();
      break;
    case ROLE_SLAVE:
      slaveLoop(now);
      break;
    case ROLE_MASTER:
      masterLoop(now);
      break;
  }
}

Role mesh_role() { return role; }

bool mesh_connected() {
  if (role == ROLE_SLAVE) return synced;
  if (role == ROLE_MASTER) return mesh_onlineCount() >= 2;
  return false;
}

int64_t mesh_masterNowUs() { return mesh_toMasterUs(esp_timer_get_time()); }

int64_t mesh_toMasterUs(int64_t localUs) { return role == ROLE_SLAVE ? localUs + offsetUs : localUs; }

const uint8_t* mesh_selfId() { return selfId; }

void mesh_fillHeader(MsgHeader& h, MsgType type) {
  h.magic = PROTO_MAGIC;
  h.version = PROTO_VERSION;
  h.group = GROUP_ID;
  h.type = type;
  memcpy(h.id, selfId, 6);
}

bool mesh_sendToMaster(const void* data, size_t len) {
  if (role != ROLE_SLAVE) return false;
  return esp_now_send(masterAddr, (const uint8_t*)data, len) == ESP_OK;
}

bool mesh_sendTo(const uint8_t* addr, const void* data, size_t len) {
  if (!ensurePeer(addr)) return false;
  return esp_now_send(addr, (const uint8_t*)data, len) == ESP_OK;
}

void mesh_beaconNow() {
  if (role != ROLE_MASTER) return;
  nextBeaconMs = millis();
  beaconBurst = BEACON_BURST;
}

NodeInfo* mesh_nodes() { return nodes; }

NodeInfo* mesh_findNode(const uint8_t* id) {
  if (role != ROLE_MASTER) return nullptr;
  for (auto& n : nodes) {
    if (n.used && memcmp(n.id, id, 6) == 0) return &n;
  }
  return nullptr;
}

int mesh_onlineCount() {
  if (role != ROLE_MASTER) return 0;
  int c = 0;
  for (auto& n : nodes) {
    if (n.used && n.online) c++;
  }
  return c;
}

String macToStr(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool strToMac(const char* s, uint8_t* mac) {
  unsigned v[6];
  if (sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) mac[i] = v[i];
  return true;
}

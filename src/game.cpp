#include "game.h"

#include <Arduino.h>

#include "button.h"
#include "storage.h"
#include "web.h"

namespace {

struct Candidate {
  uint8_t id[6];
  int64_t ts;
};

GameSnapshot snap;

// --- master: arbitraż ---
Candidate cands[MAX_NODES];
int candCount;
int64_t decisionAtUs;

// --- master: historia ---
RoundResult history[HISTORY_SIZE];
int histCount;
int histHead;  // indeks następnego zapisu
uint32_t histVersion;

// --- slave: kliknięcie czekające na potwierdzenie ---
PressMsg pending;
bool pendingActive;
uint8_t pendingTries;
uint32_t pendingNextMs;
uint16_t pressSeq;

void changed() {
  mesh_beaconNow();
  web_notify();
}

void resetSnapshot() {
  snap = GameSnapshot{};
  snap.state = GS_LOBBY;
  snap.lockMs = storage_config().lockMs;
  snap.expectedNodes = storage_config().expectedNodes;
  candCount = 0;
}

const char* nodeName(const uint8_t* id, String& buf) {
  NodeInfo* n = mesh_findNode(id);
  buf = n ? nodeLabel(*n) : macToStr(id);
  return buf.c_str();
}

void startRound(int64_t startAt) {
  snap.state = GS_ARMED;
  snap.round++;
  snap.roundStartAt = startAt;
  snap.lockEndAt = 0;
  memset(snap.winner, 0, 6);
  candCount = 0;
  Serial.printf("[GRA] Runda %u – start\n", snap.round);
  changed();
}

void setState(GameState st) {
  snap.state = st;
  memset(snap.winner, 0, 6);
  candCount = 0;
  changed();
}

// Przejście LOCKED -> nowa runda; identyczne jak w game_effective().
void masterAdvance(int64_t now) {
  if (snap.state != GS_LOCKED || now < snap.lockEndAt) return;
  snap.state = GS_ARMED;
  snap.round++;
  snap.roundStartAt = snap.lockEndAt;
  memset(snap.winner, 0, 6);
  candCount = 0;
  Serial.printf("[GRA] Runda %u – start\n", snap.round);
  changed();
}

void decide(int64_t now) {
  for (int i = 1; i < candCount; i++) {  // sortowanie po czasie kliknięcia
    Candidate c = cands[i];
    int j = i - 1;
    while (j >= 0 && cands[j].ts > c.ts) {
      cands[j + 1] = cands[j];
      j--;
    }
    cands[j + 1] = c;
  }

  const Candidate& w = cands[0];
  snap.state = GS_LOCKED;
  memcpy(snap.winner, w.id, 6);
  snap.lockEndAt = now + (int64_t)snap.lockMs * 1000;

  String buf;
  RoundResult& r = history[histHead];
  r = RoundResult{};
  r.round = snap.round;
  copyName(r.winner, nodeName(w.id, buf));
  r.reactionUs = (int32_t)(w.ts - snap.roundStartAt);
  if (candCount > 1) {
    r.hasSecond = true;
    copyName(r.second, nodeName(cands[1].id, buf));
    r.marginUs = (int32_t)(cands[1].ts - w.ts);
  }
  histHead = (histHead + 1) % HISTORY_SIZE;
  if (histCount < HISTORY_SIZE) histCount++;
  histVersion++;

  NodeInfo* wn = mesh_findNode(w.id);
  if (wn) wn->wins++;

  Serial.printf("[GRA] Runda %u: wygrywa %s, reakcja %.1f ms", r.round, r.winner, r.reactionUs / 1000.0);
  if (r.hasSecond) Serial.printf(", 2. %s (+%.2f ms)", r.second, r.marginUs / 1000.0);
  Serial.println();

  candCount = 0;
  changed();
}

void updateReadyMask() {
  uint16_t mask = 0;
  NodeInfo* nodes = mesh_nodes();
  for (int i = 0; i < MAX_NODES; i++) {
    const NodeInfo& n = nodes[i];
    if (n.used && n.online && n.ready && n.number >= 1 && n.number <= 16) mask |= 1u << (n.number - 1);
  }
  if (mask == snap.readyMask) return;
  snap.readyMask = mask;
  changed();
}

void masterPress(NodeInfo* n, uint16_t round, int64_t ts, bool down) {
  if (!n) return;
  n->buttonDown = down;
  if (down) n->lastPressMs = millis();
  web_notify();
  if (!down) return;

  if (!n->ready) {
    // Pierwszy klik po dołączeniu tylko potwierdza przycisk (przestaje migać) – nie liczy się w grze.
    if (!n->self || mesh_connected()) {
      n->ready = true;
      Serial.printf("[GRA] %s gotowy\n", nodeLabel(*n).c_str());
      updateReadyMask();
    }
    return;
  }

  masterAdvance(mesh_masterNowUs());
  if (snap.state != GS_ARMED || round != snap.round) return;
  if (ts < snap.roundStartAt || !n->enabled) return;  // przed startem rundy / wyłączony z gry

  for (int i = 0; i < candCount; i++) {
    if (memcmp(cands[i].id, n->id, 6) == 0) {
      if (ts < cands[i].ts) cands[i].ts = ts;
      return;
    }
  }
  if (candCount >= MAX_NODES) return;
  // Pierwszy klik otwiera okno arbitrażu – czekamy na pakiety, które mogły się spóźnić w radiu.
  if (candCount == 0) decisionAtUs = mesh_masterNowUs() + ARBITRATION_WINDOW_MS * 1000LL;
  memcpy(cands[candCount].id, n->id, 6);
  cands[candCount].ts = ts;
  candCount++;
  Serial.printf("[GRA] Klik: %s, +%.2f ms od startu rundy\n", nodeLabel(*n).c_str(),
                (ts - snap.roundStartAt) / 1000.0);
}

void masterLoop() {
  int64_t now = mesh_masterNowUs();
  masterAdvance(now);
  int online = mesh_onlineCount();

  // Master bez żadnego slave'a nie jest połączony – po dołączeniu kogoś też musi zostać kliknięty.
  NodeInfo* self = mesh_findNode(mesh_selfId());
  if (self && online < 2) self->ready = false;
  updateReadyMask();
  int ready = __builtin_popcount(snap.readyMask);

  switch (snap.state) {
    case GS_LOBBY:
      if (ready >= 2 && ready >= snap.expectedNodes) {
        Serial.printf("[GRA] Wszystkie przyciski gotowe (%d) – start gry\n", ready);
        startRound(now + ROUND_START_LEAD_MS * 1000LL);
      }
      break;
    case GS_ARMED:
    case GS_LOCKED:
      if (online < 2) {
        Serial.println("[GRA] Brak innych przycisków – powrót do oczekiwania");
        setState(GS_LOBBY);
        break;
      }
      if (snap.state == GS_ARMED && candCount > 0 && now >= decisionAtUs) decide(now);
      break;
    default:
      break;
  }
}

void handleButton(bool down, int64_t tsLocal) {
  Role r = mesh_role();
  if (r == ROLE_MASTER) {
    masterPress(mesh_findNode(mesh_selfId()), snap.round, tsLocal, down);
    return;
  }
  if (r != ROLE_SLAVE || !mesh_connected()) {
    if (down) Serial.println("[PRZYCISK] Klik zignorowany – brak połączenia");
    return;
  }

  GameSnapshot s = game_effective(mesh_masterNowUs());
  PressMsg m = {};
  mesh_fillHeader(m.h, MSG_PRESS);
  if (++pressSeq == 0) pressSeq = 1;
  m.pressId = pressSeq;
  m.round = s.round;
  m.tsMaster = mesh_toMasterUs(tsLocal);
  m.down = down;
  mesh_sendToMaster(&m, sizeof(m));

  if (down) {  // wciśnięcie ponawiamy do potwierdzenia przez mastera
    pending = m;
    pendingActive = true;
    pendingTries = 1;
    pendingNextMs = millis() + PRESS_RETRY_MS;
  }
  if (down) Serial.printf("[PRZYCISK] Klik wysłany (runda %u)\n", s.round);
}

void retryPending() {
  if (!pendingActive) return;
  if (!mesh_connected() || pendingTries >= PRESS_MAX_RETRIES) {
    pendingActive = false;
    return;
  }
  if ((int32_t)(millis() - pendingNextMs) < 0) return;
  mesh_sendToMaster(&pending, sizeof(pending));
  pendingTries++;
  pendingNextMs = millis() + PRESS_RETRY_MS;
}

void sendCmd(const uint8_t* id, CmdType cmd, uint8_t value, const char* name) {
  NodeInfo* n = mesh_findNode(id);
  if (!n || !n->online) return;
  CmdMsg m = {};
  mesh_fillHeader(m.h, MSG_CMD);
  m.cmd = cmd;
  m.value = value;
  if (name) copyName(m.name, name);
  mesh_sendTo(n->addr, &m, sizeof(m));
}

bool isSelf(const uint8_t* id) { return memcmp(id, mesh_selfId(), 6) == 0; }

bool selfReady() {
  uint8_t num = mesh_selfNumber();
  return num >= 1 && num <= 16 && (snap.readyMask & (1u << (num - 1)));
}

}  // namespace

// ---------------------------------------------------------------------------

void game_begin() {
  resetSnapshot();
  pressSeq = esp_random();  // po restarcie płytki master nie weźmie nowych klików za powtórki
}

void game_loop() {
  Role r = mesh_role();
  if (r == ROLE_MASTER) masterAdvance(mesh_masterNowUs());

  int64_t ts;
  ButtonEvent ev = button_poll(ts);
  if (ev != BTN_NONE) handleButton(ev == BTN_PRESS, ts);

  if (r == ROLE_MASTER) {
    masterLoop();
  } else if (r == ROLE_SLAVE) {
    retryPending();
  }
}

void game_onRoleChanged(Role) {
  pendingActive = false;
  resetSnapshot();
}

const GameSnapshot& game_snapshot() { return snap; }

GameSnapshot game_effective(int64_t masterNow) {
  GameSnapshot s = snap;
  if (s.state == GS_LOCKED && masterNow >= s.lockEndAt) {
    s.state = GS_ARMED;
    s.round++;
    s.roundStartAt = s.lockEndAt;
    memset(s.winner, 0, 6);
  }
  return s;
}

LedMode game_ledMode(int64_t& phaseUs) {
  if (!mesh_connected()) return LED_FAST;

  int64_t now = mesh_masterNowUs();
  GameSnapshot s = game_effective(now);
  if (s.state == GS_TEST) return button_isDown() ? LED_ON : LED_OFF;

  if (!selfReady()) {  // dołączył i jeszcze nikt go nie kliknął
    phaseUs = now;     // czas mastera -> niepotwierdzone przyciski migają razem
    return LED_SLOW;
  }

  switch (s.state) {
    case GS_LOBBY:
      return LED_ON;  // gotowy, czeka na pozostałe
    case GS_ARMED:
      // Przed startem rundy chwila ciemności, potem wszystkie zapalają się w tej samej chwili.
      if (!storage_node().enabled || now < s.roundStartAt) return LED_OFF;
      return LED_ON;
    case GS_LOCKED:
      return isSelf(s.winner) ? LED_ON : LED_OFF;
    default:
      return LED_OFF;
  }
}

void game_onBeacon(const GameSnapshot& s) {
  snap = s;
  // Zapamiętaj ustawienia – przydadzą się, gdyby ta płytka została kiedyś masterem.
  storage_setConfig(s.lockMs, s.expectedNodes);
}

void game_onPress(NodeInfo* n, const PressMsg& m) {
  PressAckMsg ack = {};
  mesh_fillHeader(ack.h, MSG_PRESS_ACK);
  ack.pressId = m.pressId;
  mesh_sendTo(n->addr, &ack, sizeof(ack));

  if (m.pressId == n->lastPressId) return;  // powtórzony pakiet
  n->lastPressId = m.pressId;
  masterPress(n, m.round, m.tsMaster, m.down);
}

void game_onPressAck(const PressAckMsg& m) {
  if (pendingActive && m.pressId == pending.pressId) pendingActive = false;
}

void game_onCmd(const CmdMsg& m) {
  switch (m.cmd) {
    case CMD_IDENTIFY:
      led_identify();
      break;
    case CMD_SET_NAME: {
      char name[NAME_LEN + 1];
      memcpy(name, m.name, NAME_LEN);
      name[NAME_LEN] = 0;
      storage_setName(name);
      Serial.printf("[CMD] Nowa nazwa: %s\n", storage_node().name[0] ? storage_node().name : "(domyślna)");
      break;
    }
    case CMD_SET_ENABLED:
      storage_setEnabled(m.value);
      Serial.printf("[CMD] %s z gry\n", m.value ? "Włączony do" : "Wyłączony");
      break;
    default:
      break;
  }
}

void game_cmdStart() {
  if (snap.state == GS_ARMED || snap.state == GS_LOCKED) return;
  startRound(mesh_masterNowUs() + ROUND_START_LEAD_MS * 1000LL);
}

void game_cmdStop() {
  Serial.println("[GRA] Zatrzymano z panelu");
  setState(GS_PAUSED);
}

void game_cmdNext() { startRound(mesh_masterNowUs() + ROUND_START_LEAD_MS * 1000LL); }

void game_cmdTest(bool on) {
  if (on) {
    Serial.println("[GRA] Tryb testu");
    setState(GS_TEST);
  } else if (snap.state == GS_TEST) {
    setState(GS_LOBBY);  // wróci do gry sam, jeśli jest komplet przycisków
  }
}

void game_cmdResetScores() {
  NodeInfo* nodes = mesh_nodes();
  for (int i = 0; i < MAX_NODES; i++) nodes[i].wins = 0;
  histCount = 0;
  histHead = 0;
  histVersion++;
  bool playing = snap.state == GS_ARMED || snap.state == GS_LOCKED;
  snap.round = 0;
  Serial.println("[GRA] Wyniki wyzerowane");
  if (playing) {
    startRound(mesh_masterNowUs() + ROUND_START_LEAD_MS * 1000LL);
  } else {
    changed();
  }
}

void game_cmdIdentify(const uint8_t* id) {
  if (isSelf(id)) {
    led_identify();
  } else {
    sendCmd(id, CMD_IDENTIFY, 0, nullptr);
  }
}

void game_cmdRename(const uint8_t* id, const char* name) {
  NodeInfo* n = mesh_findNode(id);
  if (!n) return;
  copyName(n->name, name);  // "" = powrót do "Przycisk N"
  if (isSelf(id)) {
    storage_setName(name);
  } else {
    sendCmd(id, CMD_SET_NAME, 0, name);
  }
  web_notify();
}

void game_cmdEnable(const uint8_t* id, bool enabled) {
  NodeInfo* n = mesh_findNode(id);
  if (!n) return;
  n->enabled = enabled;
  if (isSelf(id)) {
    storage_setEnabled(enabled);
  } else {
    sendCmd(id, CMD_SET_ENABLED, enabled, nullptr);
  }
  web_notify();
}

void game_cmdConfig(uint32_t lockMs, uint32_t expectedNodes) {
  lockMs = constrain(lockMs, 1000u, 30000u);
  expectedNodes = constrain(expectedNodes, 2u, (uint32_t)MAX_NODES);
  storage_setConfig(lockMs, expectedNodes);
  snap.lockMs = lockMs;
  snap.expectedNodes = expectedNodes;
  Serial.printf("[GRA] Ustawienia: blokada %u ms, start przy %u przyciskach\n", (unsigned)lockMs,
                (unsigned)expectedNodes);
  changed();
}

int game_historyCount() { return histCount; }

const RoundResult& game_historyAt(int i) {
  return history[(histHead - 1 - i + 2 * HISTORY_SIZE) % HISTORY_SIZE];
}

uint32_t game_historyVersion() { return histVersion; }

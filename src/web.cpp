#include "web.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "game.h"
#include "mesh.h"
#include "storage.h"
#include "web_page.h"

namespace {

enum WebCmdType : uint8_t {
  WC_START,
  WC_STOP,
  WC_NEXT,
  WC_TEST,
  WC_RESET,
  WC_IDENTIFY,
  WC_NAME,
  WC_ENABLE,
  WC_CONFIG,
};

struct WebCmd {
  uint8_t type;
  uint8_t id[6];
  int32_t a;
  int32_t b;
  char name[NAME_LEN + 1];
};

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dns;
QueueHandle_t cmdQueue;
bool started = false;
volatile bool dirty = true;
volatile bool histDirty = true;
uint32_t lastPushMs;
uint32_t lastHistVersion;

// Wątek AsyncTCP: tylko parsujemy i wrzucamy do kolejki – stan gry zmienia wyłącznie loop().
void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    dirty = true;
    histDirty = true;
    return;
  }
  if (type != WS_EVT_DATA) return;
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;

  JsonDocument doc;
  if (deserializeJson(doc, (const char*)data, len)) return;
  const char* c = doc["c"] | "";

  WebCmd cmd = {};
  if (!strcmp(c, "start")) {
    cmd.type = WC_START;
  } else if (!strcmp(c, "stop")) {
    cmd.type = WC_STOP;
  } else if (!strcmp(c, "next")) {
    cmd.type = WC_NEXT;
  } else if (!strcmp(c, "test")) {
    cmd.type = WC_TEST;
    cmd.a = doc["on"] | false;
  } else if (!strcmp(c, "reset")) {
    cmd.type = WC_RESET;
  } else if (!strcmp(c, "cfg")) {
    cmd.type = WC_CONFIG;
    cmd.a = doc["lockMs"] | (int)DEFAULT_LOCK_MS;
    cmd.b = doc["expected"] | (int)DEFAULT_EXPECTED_NODES;
  } else if (!strcmp(c, "identify") || !strcmp(c, "name") || !strcmp(c, "enable")) {
    if (!strToMac(doc["id"] | "", cmd.id)) return;
    if (c[0] == 'i') {
      cmd.type = WC_IDENTIFY;
    } else if (c[0] == 'n') {
      cmd.type = WC_NAME;
      copyName(cmd.name, doc["name"] | "");
    } else {
      cmd.type = WC_ENABLE;
      cmd.a = doc["on"] | true;
    }
  } else {
    return;
  }
  xQueueSend(cmdQueue, &cmd, 0);
}

void execute(const WebCmd& cmd) {
  switch (cmd.type) {
    case WC_START: game_cmdStart(); break;
    case WC_STOP: game_cmdStop(); break;
    case WC_NEXT: game_cmdNext(); break;
    case WC_TEST: game_cmdTest(cmd.a); break;
    case WC_RESET: game_cmdResetScores(); break;
    case WC_IDENTIFY: game_cmdIdentify(cmd.id); break;
    case WC_NAME: game_cmdRename(cmd.id, cmd.name); break;
    case WC_ENABLE: game_cmdEnable(cmd.id, cmd.a); break;
    case WC_CONFIG: game_cmdConfig(cmd.a, cmd.b); break;
  }
  dirty = true;
}

const char* nameOf(const uint8_t* id) {
  NodeInfo* n = mesh_findNode(id);
  return n ? n->name : "?";
}

void pushState() {
  int64_t now = mesh_masterNowUs();
  GameSnapshot s = game_effective(now);
  uint32_t ms = millis();

  JsonDocument d;
  d["t"] = "s";
  d["st"] = s.state;
  d["round"] = s.round;
  d["online"] = mesh_onlineCount();
  d["expected"] = s.expectedNodes;
  d["lockMs"] = s.lockMs;
  if (s.state == GS_ARMED) {
    d["elapsed"] = now >= s.roundStartAt ? (long)((now - s.roundStartAt) / 1000) : -1;
    d["startsIn"] = now < s.roundStartAt ? (long)((s.roundStartAt - now) / 1000) : 0;
  }
  if (s.state == GS_LOCKED) {
    d["winnerName"] = nameOf(s.winner);
    d["lockLeft"] = (long)((s.lockEndAt - now) / 1000);
    if (game_historyCount() > 0) d["reaction"] = game_historyAt(0).reactionUs / 1000.0;
  }

  JsonArray arr = d["nodes"].to<JsonArray>();
  NodeInfo* nodes = mesh_nodes();
  for (int i = 0; i < MAX_NODES; i++) {
    const NodeInfo& n = nodes[i];
    if (!n.used) continue;
    JsonObject o = arr.add<JsonObject>();
    o["id"] = macToStr(n.id);
    o["name"] = n.name;
    o["self"] = n.self;
    o["on"] = n.online;
    o["en"] = n.enabled;
    o["down"] = n.buttonDown;
    o["press"] = n.lastPressMs ? (long)(ms - n.lastPressMs) : -1;
    o["q"] = n.linkQuality;
    o["rtt"] = n.rttUs;
    o["wins"] = n.wins;
    o["up"] = n.uptimeMs / 1000;
    o["win"] = s.state == GS_LOCKED && memcmp(n.id, s.winner, 6) == 0;
  }

  String out;
  serializeJson(d, out);
  ws.textAll(out);
}

void pushHistory() {
  JsonDocument d;
  d["t"] = "h";
  JsonArray items = d["items"].to<JsonArray>();
  for (int i = 0; i < game_historyCount(); i++) {
    const RoundResult& r = game_historyAt(i);
    JsonObject o = items.add<JsonObject>();
    o["r"] = r.round;
    o["w"] = r.winner;
    o["rt"] = r.reactionUs / 1000.0;
    if (r.hasSecond) {
      o["s"] = r.second;
      o["m"] = r.marginUs / 1000.0;
    }
  }
  String out;
  serializeJson(d, out);
  ws.textAll(out);
}

}  // namespace

void web_begin() {
  if (started) return;
  started = true;
  cmdQueue = xQueueCreate(16, sizeof(WebCmd));

  // Captive portal: każda nazwa domeny wskazuje na mastera, telefon sam otworzy panel.
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res =
        req->beginResponse(200, "text/html; charset=utf-8", (const uint8_t*)PANEL_HTML, strlen(PANEL_HTML));
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  });
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->redirect(String("http://") + WiFi.softAPIP().toString() + "/");
  });
  server.begin();
}

void web_loop() {
  if (!started) return;
  dns.processNextRequest();

  WebCmd cmd;
  while (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE) execute(cmd);

  ws.cleanupClients();
  if (ws.count() == 0) return;

  uint32_t now = millis();
  if (histDirty || game_historyVersion() != lastHistVersion) {
    histDirty = false;
    lastHistVersion = game_historyVersion();
    pushHistory();
  }
  if (now - lastPushMs >= (dirty ? 30u : 250u)) {
    dirty = false;
    lastPushMs = now;
    pushState();
  }
}

void web_notify() { dirty = true; }

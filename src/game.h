#pragma once

#include "led.h"
#include "mesh.h"
#include "protocol.h"

struct RoundResult {
  uint16_t round;
  char winner[NAME_LEN + 1];
  int32_t reactionUs;  // od startu rundy do kliknięcia zwycięzcy
  bool hasSecond;
  char second[NAME_LEN + 1];
  int32_t marginUs;  // o ile później kliknął drugi
};

void game_begin();
void game_loop();  // przycisk tej płytki + logika mastera
void game_onRoleChanged(Role role);

const GameSnapshot& game_snapshot();
// Stan w chwili masterNow – uwzględnia przejście LOCKED -> nowa runda o lockEndAt,
// więc każda płytka przełącza się dokładnie w tym samym momencie, nawet bez nowego beaconu.
GameSnapshot game_effective(int64_t masterNow);
LedMode game_ledMode(int64_t& phaseUs);

// Z sieci
void game_onBeacon(const GameSnapshot& s);
void game_onPress(NodeInfo* n, const PressMsg& m);
void game_onPressAck(const PressAckMsg& m);
void game_onCmd(const CmdMsg& m);

// Komendy z panelu (tylko master)
void game_cmdStart();
void game_cmdStop();
void game_cmdNext();
void game_cmdTest(bool on);
void game_cmdResetScores();
void game_cmdIdentify(const uint8_t* id);
void game_cmdRename(const uint8_t* id, const char* name);
void game_cmdEnable(const uint8_t* id, bool enabled);
void game_cmdConfig(uint32_t lockMs, uint32_t expectedNodes);

// Historia (tylko master), 0 = najnowsza runda
int game_historyCount();
const RoundResult& game_historyAt(int i);
uint32_t game_historyVersion();

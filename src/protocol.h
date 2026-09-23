#pragma once

#include <stdint.h>

#include "config.h"

#define PROTO_MAGIC 0xB17A
#define PROTO_VERSION 3

enum MsgType : uint8_t {
  MSG_BEACON = 1,  // master -> wszyscy (broadcast)
  MSG_HELLO,       // slave -> master
  MSG_SYNC_REQ,    // slave -> master
  MSG_SYNC_RESP,   // master -> slave
  MSG_PRESS,       // slave -> master
  MSG_PRESS_ACK,   // master -> slave
  MSG_CMD,         // master -> slave
};

enum GameState : uint8_t {
  GS_LOBBY = 0,  // czeka na komplet przycisków
  GS_ARMED,      // runda trwa, czeka na klik
  GS_LOCKED,     // zwycięzca świeci, reszta zablokowana
  GS_TEST,       // tryb testu z panelu
  GS_PAUSED,     // zatrzymane z panelu
};

enum CmdType : uint8_t { CMD_IDENTIFY = 1, CMD_SET_NAME, CMD_SET_ENABLED };

struct __attribute__((packed)) MsgHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t group;
  uint8_t type;
  uint8_t id[6];  // ID nadawcy = MAC interfejsu STA
};

// Wszystkie czasy w mikrosekundach zegara mastera.
struct __attribute__((packed)) GameSnapshot {
  uint8_t state;
  uint16_t round;
  uint8_t winner[6];
  int64_t roundStartAt;
  int64_t lockEndAt;
  uint16_t lockMs;
  uint8_t expectedNodes;
  uint16_t readyMask;  // bit (numer-1): przycisk kliknięty po dołączeniu = gotowy
};

struct __attribute__((packed)) BeaconMsg {
  MsgHeader h;
  int64_t masterTimeUs;
  uint32_t masterForMs;  // jak długo nadawca jest masterem (rozstrzyganie konfliktów)
  GameSnapshot game;
};

// number w HELLO/SYNC_REQ: numer, który płytka dostała wcześniej (0 = jeszcze żaden) –
// dzięki temu po zmianie mastera przyciski zachowują swoje numery.
struct __attribute__((packed)) HelloMsg {
  MsgHeader h;
  uint8_t number;
  char name[NAME_LEN + 1];  // własna nazwa z panelu, "" = domyślna "Przycisk N"
  uint8_t enabled;
  uint8_t buttonDown;
  uint8_t linkQuality;
  uint16_t rttUs;
  uint32_t uptimeMs;
};

struct __attribute__((packed)) SyncReqMsg {
  MsgHeader h;
  uint8_t number;
  int64_t t1;
};

struct __attribute__((packed)) SyncRespMsg {
  MsgHeader h;
  uint8_t number;  // numer nadany tej płytce przez mastera
  int64_t t1;
  int64_t t2;
  int64_t t3;
};

struct __attribute__((packed)) PressMsg {
  MsgHeader h;
  uint16_t pressId;
  uint16_t round;
  int64_t tsMaster;  // moment pierwszego zbocza, przeliczony na czas mastera
  uint8_t down;
};

struct __attribute__((packed)) PressAckMsg {
  MsgHeader h;
  uint16_t pressId;
};

struct __attribute__((packed)) CmdMsg {
  MsgHeader h;
  uint8_t cmd;
  uint8_t value;
  char name[NAME_LEN + 1];
};

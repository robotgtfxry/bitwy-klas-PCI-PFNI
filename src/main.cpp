// Bitwy Klas – bezprzewodowe przyciski quizowe.
// Ten sam program na wszystkich płytkach: pierwsza włączona zostaje masterem
// (Wi-Fi "Bitwy-klas-PFNI" + panel http://192.168.4.1), kolejne dołączają przez ESP-NOW.

#include <Arduino.h>

#include "button.h"
#include "game.h"
#include "led.h"
#include "mesh.h"
#include "storage.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("=== Bitwy Klas – przyciski (ESP-NOW) ===");

  storage_begin();
  led_begin();
  button_begin();
  game_begin();
  mesh_begin();
}

void loop() {
  mesh_loop();
  game_loop();
  if (mesh_role() == ROLE_MASTER) web_loop();

  int64_t phaseUs = 0;
  LedMode mode = game_ledMode(phaseUs);
  led_update(mode, phaseUs);

  delay(1);
}

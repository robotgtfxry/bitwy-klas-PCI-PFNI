#pragma once

// Panel WWW – działa tylko na masterze (Wi-Fi AP + captive portal + WebSocket).
void web_begin();
void web_loop();
void web_notify();  // coś się zmieniło -> wyślij stan do panelu jak najszybciej

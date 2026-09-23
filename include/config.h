#pragma once

// ---------------------------------------------------------------------------
// Sprzęt
// ---------------------------------------------------------------------------
#define BUTTON_PIN 4          // D4 – przycisk
#define LED_PIN 5             // D5 – dioda
#define BUTTON_ACTIVE_LOW 1   // 1: przycisk zwiera pin do GND (wewnętrzny pull-up)
#define LED_ACTIVE_HIGH 1     // 1: stan wysoki = dioda świeci

// ---------------------------------------------------------------------------
// Radio / sieć
// ---------------------------------------------------------------------------
#define WIFI_CHANNEL 1        // ESP-NOW i AP mastera MUSZĄ być na tym samym kanale
#define GROUP_ID 0x42         // inny numer = osobny zestaw (np. druga sala)
#define AP_SSID "Bitwy-klas-PFNI"
#define AP_PASSWORD "szybkihelikopterlatawolno"
#define AP_MAX_CLIENTS 4

#define BEACON_INTERVAL_MS 100    // master -> wszyscy: stan gry + czas
#define BEACON_BURST 2            // dodatkowe beacony tuż po zmianie stanu
#define BEACON_BURST_GAP_MS 10
#define HELLO_INTERVAL_MS 500     // slave -> master: nazwa, stan, jakość łącza
#define NODE_TIMEOUT_MS 2000      // master uznaje slave'a za offline
#define MASTER_TIMEOUT_MS 2500    // slave uznaje mastera za utraconego
#define ELECTION_LISTEN_MS 1500   // nasłuch przed zostaniem masterem...
#define ELECTION_JITTER_MS 1000   // ...plus losowo 0..JITTER
#define MASTER_TIE_MS 200         // dwa mastery "równie stare" -> wygrywa niższy MAC
#define LINK_WINDOW_MS 2000       // okno liczenia jakości łącza

// Synchronizacja zegarów (NTP-owo, próbka z najmniejszym RTT z okna)
#define SYNC_INTERVAL_MS 1000
#define SYNC_FAST_INTERVAL_MS 100 // tuż po dołączeniu – szybka zbieżność
#define SYNC_FAST_PERIOD_MS 2000
#define SYNC_WINDOW 6
#define SYNC_MAX_RTT_US 20000

// ---------------------------------------------------------------------------
// Gra
// ---------------------------------------------------------------------------
#define DEFAULT_LOCK_MS 5000      // jak długo świeci zwycięzca
#define DEFAULT_EXPECTED_NODES 3  // ile przycisków musi być online do auto-startu
#define ARBITRATION_WINDOW_MS 40  // master czeka na spóźnione pakiety i wybiera najwcześniejszy klik
#define ROUND_START_LEAD_MS 300   // start rundy planowany z wyprzedzeniem (wszyscy ruszają naraz)
#define PRESS_RETRY_MS 20
#define PRESS_MAX_RETRIES 10
#define HISTORY_SIZE 50
#define MAX_NODES 10

// ---------------------------------------------------------------------------
// Dioda
// ---------------------------------------------------------------------------
#define FAST_BLINK_MS 100         // brak połączenia
#define SLOW_BLINK_MS 500         // połączony / runda czeka na klik
#define IDENTIFY_MS 2000
#define IDENTIFY_BLINK_MS 50

// ---------------------------------------------------------------------------
// Przycisk
// ---------------------------------------------------------------------------
#define DEBOUNCE_CONFIRM_MS 5     // po pierwszym zboczu musi być wciśnięty...
#define DEBOUNCE_GLITCH_MS 20     // ...najpóźniej w tym czasie, inaczej to zakłócenie
#define DEBOUNCE_RELEASE_MS 30    // stabilne puszczenie przed kolejnym kliknięciem

#define NAME_LEN 20               // bajty UTF-8

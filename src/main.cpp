/***************************************************************************************
 * FILE: src/main.cpp
 * LAST MODIFIED: 2025-11-15 01:20 (Europe/Warsaw)
 * PURPOSE: Arduino entry points – uruchamia config + control + web + OLED
 ***************************************************************************************/
#include <Arduino.h>
#include "config.h"    // <-- DODANE
#include "control.h"
#include "webserver.h"
#include "web_config.h"
#include "display.h"
#include <LittleFS.h>
#include <ESP8266WiFi.h>

#define DO_FS_FORMAT 0   // ← po akcji zmienisz na 0 albo skasujesz

void setup() {
  Serial.begin(115200);
  delay(500);

#if DO_FS_FORMAT
  Serial.println(F("[FS] FORMAT LittleFS..."));
  if (LittleFS.begin()) {
    LittleFS.format();
    Serial.println(F("[FS] FORMAT DONE, reboot..."));
  } else {
    Serial.println(F("[FS] LittleFS.begin() FAIL – brak formatu."));
  }
  delay(1000);
  ESP.restart();   // po jednorazowym flashu będzie się kręcić, ale FS już będzie czysty
#endif
  
  // 1) Wczytaj konfigurację z LittleFS (piny, PID, tryb, itp.)
  cfgLoadOrDefault();

  // 2) Zainicjalizuj piny/I2C/MAX + PID (w środku wywołaj pinsAndBusesInit)
  controlSetup();

  // 3) Wczytaj profil z FS (jeśli istnieje) i ustaw jako aktywny w RAM
  loadProfileFS();

  // 4) Webserver – rejestruje trasy, ładuje profil z FS itp.
  webserverSetup();

  // 4) OLED
  displayInit();

  Serial.println(F("[BOOT] setup done"));
}

void loop() {
  controlLoop();      // pomiary, PID, SSR okno, safety
  webserverLoop();    // obsługa HTTP
  displayLoop();      // odświeżanie ekranu (co ~500 ms)

  yield();            // uniknij WDT
}

/***************************************************************************************
 * FILE: src/control.h
 * LAST MODIFIED: 2025-11-14 09:20 (Europe/Warsaw)
 ***************************************************************************************/
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"


// ────────────────────────────────────────────────────────────────────────────────
// Zmienne procesu
// ────────────────────────────────────────────────────────────────────────────────
extern double   KILN_TEMP;    // °C
extern double   CTRL_TEMP;    // °C – temperatura sterownika (MAX31855 internal)
extern double   PID_SET;      // °C
extern double   PID_OUT;      // 0..100 %


extern bool     RUN_ACTIVE;
extern bool     SAFETY_TRIP;

extern bool     HEATER_ON;
extern bool     SENSOR_OK;

extern uint8_t  PROFILE_ACTIVE;
extern uint8_t  PROFILE_LEN;
extern uint32_t RUN_REV;

// ────────────────────────────────────────────────────────────────────────────────
// PROFIL – kroki + czasy etapu
// ────────────────────────────────────────────────────────────────────────────────
struct ProfileStep {
  double   targetC;   // zadana temperatura
  uint32_t holdSec;   // czas trwania etapu (sekundy, 0 = nieskończoność)
};

extern ProfileStep PROFILE[8];

// czas etapu (w sekundach): ile minęło / ile zostało
extern uint32_t PROFILE_ELAPSED_SEC;
extern uint32_t PROFILE_REMAIN_SEC;

// ────────────────────────────────────────────────────────────────────────────────
// API sterowania
// ────────────────────────────────────────────────────────────────────────────────
void pinsAndBusesInit();
void controlSetup();
void controlLoop();
void runStart();
void runStop();

// Profil z JSON (web_config.cpp → control.cpp)
bool applyProfileFromJson(const JsonArray& arr);

// Ręczne przełączanie kroków (webserver: /profile/next, /profile/prev)
void profileNextStep();
void profilePrevStep();

// eksport próbek do CSV (dla /export)
void buildSamplesCSV(String& out);

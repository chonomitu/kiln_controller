/***************************************************************************************
 * FILE: src/storage.cpp
 * LAST MODIFIED: 2025-11-09 07:05 (Europe/Warsaw)
 * PURPOSE: Minimal stubs for profile persistence (replace with real LittleFS later)
 ***************************************************************************************/
#include "storage.h"
#include "control.h"

// NOTE: Stubs – replace with real LittleFS JSON load/save.
bool loadProfile(ProfileStep* outArray, uint8_t maxLen, uint8_t& outLen) {
  (void)outArray; (void)maxLen;
  outLen = 0;                // no steps loaded
  return true;
}

bool saveProfile(const ProfileStep* inArray, uint8_t len) {
  (void)inArray; (void)len;
  return true;
}

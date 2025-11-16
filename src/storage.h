/***************************************************************************************
 * FILE: src/storage.h
 * LAST MODIFIED: 2025-11-09 07:05 (Europe/Warsaw)
 * PURPOSE: Profile/load/save API (no duplicate struct definitions)
 ***************************************************************************************/
#pragma once
#include <Arduino.h>

// Forward declaration – the full definition lives in control.h
struct ProfileStep;

// Load up to maxLen steps into outArray; write actual count to outLen.
// Return true on success.
bool loadProfile(ProfileStep* outArray, uint8_t maxLen, uint8_t& outLen);

// Save len steps from inArray.
// Return true on success.
bool saveProfile(const ProfileStep* inArray, uint8_t len);

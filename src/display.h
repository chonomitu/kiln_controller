/***************************************************************************************
 * FILE: src/display.h
 * LAST MODIFIED: 2025-11-10 23:58 (Europe/Warsaw)
 * PURPOSE: Interfejs OLED (SSD1306) dla KilnControllerESP8266
 ***************************************************************************************/
#pragma once
#include <Arduino.h>

enum class ViewMode : uint8_t { AUTO = 0, TEMP = 1, GRAPH = 2, BOTH = 3 };

void displayInit();
void displayLoop();
void displaySetMode(ViewMode m);
void displayForceRedraw();

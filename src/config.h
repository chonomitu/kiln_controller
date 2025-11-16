/***************************************************************************************
 * FILE: src/config.h
 * LAST MODIFIED: 2025-11-10 23:58 (Europe/Warsaw)
 * PURPOSE: Konfiguracja uruchomieniowa i stałe sprzętowe (deklaracje)
 ***************************************************************************************/
#pragma once
#include <Arduino.h>

#define USE_QUICKPID 0

// PT100 / MAX31865
static const double CFG_RNOMINAL = 100.0;
static const double CFG_RREF     = 430.0;

// OLED
#ifndef OLED_W
  #define OLED_W 128
#endif
#ifndef OLED_H
  #define OLED_H 64
#endif

struct PinConfig {
  int SSR   = D0;
  int LED   = LED_BUILTIN;
  int BUZZ  = D2;
  int BTN_A = -1;
  int BTN_B = -1;
  int I2C_SDA = D5;
  int I2C_SCL = D6;
  int SPI_MOSI = D7;
  int SPI_MISO = D1;
  int SPI_SCK  = D4;
  int SPI_CS   = D8;
};

struct PIDConfig {
  double Kp=20, Ki=0.8, Kd=50;
  double setpointC=200;
  double outMin=0, outMax=100;
  unsigned long windowMs=1000;
};

enum Mode : uint8_t { MODE_DYNAMIC=0, MODE_PROFILE=1 };

struct RuntimeConfig {
  PinConfig pins;
  PIDConfig pid;
  Mode mode = MODE_DYNAMIC;
  uint32_t sampleSec = 600;
  double maxTempC = 950.0;
  bool staEnabled = false;
  char staSSID[33] = "";
  char staPASS[65] = "";
  bool mqttEnabled = false;
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[64] = "";
  char mqttPass[64] = "";
  char mqttBaseTopic[64] = "kiln";
};

extern RuntimeConfig CFG;
void cfgLoadOrDefault();
void cfgSave();

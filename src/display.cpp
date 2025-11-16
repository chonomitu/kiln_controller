/***************************************************************************************
 * FILE: src/display.cpp
 * LAST MODIFIED: 2025-11-14 10:10 (Europe/Warsaw)
 * PURPOSE: OLED UI (SSD1306 128x64) – belka statusu + duża temp + czas kroku + wykres
 ***************************************************************************************/
#include "display.h"
#include "config.h"
#include "control.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// dodatkowe extern z control.cpp
extern uint32_t PROFILE_ELAPSED_SEC;
extern uint32_t PROFILE_REMAIN_SEC;
extern uint8_t  PROFILE_ACTIVE;
extern uint8_t  PROFILE_LEN;

// ────────────────────────────────────────────────────────────────────────────────
// Konfiguracja OLED
// ────────────────────────────────────────────────────────────────────────────────
#ifndef OLED_W
  #define OLED_W 128
#endif
#ifndef OLED_H
  #define OLED_H 64
#endif

static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static bool oledOK = false;

// ViewMode z display.h – na razie nieużywany, ale trzymamy pod API
static ViewMode g_mode;

// odświeżanie ekranu
static uint32_t nextDrawMs      = 0;
static const uint32_t DRAW_PERIOD_MS = 500;
static uint32_t lastRUN_REV     = 0;
static uint32_t runStartMs      = 0;

// przełączanie stron co 2 s
static uint8_t  g_page           = 0;     // 0 = temp, 1 = wykres
static uint32_t g_pageSwitchMs   = 0;
static const uint32_t PAGE_PERIOD_MS = 2000;

// ────────────────────────────────────────────────────────────────────────────────
// Historia do wykresu na OLED (temp + OUT [%])
// ────────────────────────────────────────────────────────────────────────────────

static const uint16_t HIST_CAP   = 64;
static float          histTemp[HIST_CAP];
static float          histOut [HIST_CAP];
static uint16_t       histLen     = 0;
static unsigned long  histLastMs  = 0;

// co ~1 s dorzucamy próbkę do historii
static void pushHistory(float vTemp, float vOut) {
  unsigned long now = millis();
  if (now - histLastMs < 1000UL) return;   // 1 Hz
  histLastMs = now;

  if (histLen < HIST_CAP) {
    histTemp[histLen] = vTemp;
    histOut [histLen] = vOut;
    histLen++;
  } else {
    // przesunięcie okna – najstarsza wyleci, nowe na końcu
    memmove(&histTemp[0], &histTemp[1], sizeof(float) * (HIST_CAP - 1));
    memmove(&histOut [0], &histOut [1], sizeof(float) * (HIST_CAP - 1));
    histTemp[HIST_CAP - 1] = vTemp;
    histOut [HIST_CAP - 1] = vOut;
  }
}

// wykres temperatury (°C)
static void drawSparklineTemp(int x, int y, int w, int h) {
  if (histLen < 2) {
    oled.drawRect(x, y, w, h, SSD1306_WHITE);
    return;
  }

  // znajdź min/max w buforze
  float tMin = histTemp[0];
  float tMax = histTemp[0];
  for (uint16_t i = 1; i < histLen; i++) {
    if (histTemp[i] < tMin) tMin = histTemp[i];
    if (histTemp[i] > tMax) tMax = histTemp[i];
  }
  if (!isfinite(tMin) || !isfinite(tMax)) {
    tMin = 0;
    tMax = 1;
  }
  if (tMin == tMax) {
    tMin -= 1;
    tMax += 1;
  }

  auto Y = [&](float v)->int16_t {
    float u = (v - tMin) / (tMax - tMin);
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    return y + h - 1 - (int16_t)roundf(u * (h - 1));
  };

  oled.drawRect(x, y, w, h, SSD1306_WHITE);

  uint16_t L    = histLen;
  float    step = (float)w / (float)(HIST_CAP - 1);

  int16_t px = x;
  int16_t py = Y(histTemp[0]);

  for (uint16_t i = 1; i < L; i++) {
    int16_t nx = x + (int16_t)roundf(i * step);
    int16_t ny = Y(histTemp[i]);
    if (nx >= x + w) nx = x + w - 1;
    oled.drawLine(px, py, nx, ny, SSD1306_WHITE);
    px = nx;
    py = ny;
  }
}

// wykres OUT [%]
static void drawSparklineOut(int x, int y, int w, int h) {
  if (histLen < 2) {
    oled.drawRect(x, y, w, h, SSD1306_WHITE);
    return;
  }

  auto Y = [&](float v)->int16_t {
    float u = v / 100.0f;
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    return y + h - 1 - (int16_t)roundf(u * (h - 1));
  };

  oled.drawRect(x, y, w, h, SSD1306_WHITE);

  uint16_t L    = histLen;
  float    step = (float)w / (float)(HIST_CAP - 1);

  int16_t px = x;
  int16_t py = Y(histOut[0]);

  for (uint16_t i = 1; i < L; i++) {
    int16_t nx = x + (int16_t)roundf(i * step);
    int16_t ny = Y(histOut[i]);
    if (nx >= x + w) nx = x + w - 1;
    oled.drawLine(px, py, nx, ny, SSD1306_WHITE);
    px = nx;
    py = ny;
  }
}

// ────────────────────────────────────────────────────────────────────────────────
// Pomocnicze
// ────────────────────────────────────────────────────────────────────────────────
static void drawTopBar(bool invert, const char* left, const char* mid, const char* right){
  if (invert){
    oled.fillRect(0,0, OLED_W, 12, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.fillRect(0,0, OLED_W, 12, SSD1306_BLACK);
    oled.drawFastHLine(0,12, OLED_W, SSD1306_WHITE);
    oled.setTextColor(SSD1306_WHITE);
  }
  oled.setCursor(2,2);   oled.print(left);
  int16_t x1,y1; uint16_t w,h;
  oled.getTextBounds(mid, 0,0, &x1,&y1,&w,&h);
  int16_t cx = (OLED_W - (int)w)/2; if (cx < 2) cx = 2;
  oled.setCursor(cx,2);  oled.print(mid);
  oled.getTextBounds(right, 0,0, &x1,&y1,&w,&h);
  int16_t rx = OLED_W - w - 2; if (rx < 2) rx = 2;
  oled.setCursor(rx,2);  oled.print(right);
}

static void fmtTimeSince(char* buf, size_t n, uint32_t ms){
  uint32_t s = ms / 1000UL;
  uint32_t m = s / 60; s %= 60;
  uint32_t h = m / 60; m %= 60;
  if (h > 0) snprintf(buf, n, "%luh%02lum", (unsigned long)h, (unsigned long)m);
  else       snprintf(buf, n, "%lum%02lus", (unsigned long)m, (unsigned long)s);
}

// ────────────────────────────────────────────────────────────────────────────────
// API z display.h
// ────────────────────────────────────────────────────────────────────────────────

void displaySetMode(ViewMode m){
  g_mode = m;
}

void displayForceRedraw(){
  nextDrawMs = 0;
}

void displayInit(){
  // I2C ustawia pinsAndBusesInit() – tu tylko OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    oledOK = false;
    Serial.println(F("[OLED] not found (SSD1306 init failed)"));
    return;
  }
  oledOK = true;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0);
  oled.println(F("KilnCtrl OLED"));
  oled.display();
  delay(500);
}

void displayLoop(){
  if (!oledOK) return;
  const uint32_t now = millis();
  if (now < nextDrawMs) return;
  nextDrawMs = now + DRAW_PERIOD_MS;

  if (RUN_REV != lastRUN_REV){
    lastRUN_REV = RUN_REV;
    if (RUN_ACTIVE) runStartMs = now;
  }

  const bool run   = RUN_ACTIVE;
  const bool trip  = SAFETY_TRIP;
  const bool heat  = HEATER_ON;
  const float tC   = isfinite(KILN_TEMP) ? (float)KILN_TEMP : NAN;
  const float sp   = isfinite(PID_SET)   ? (float)PID_SET   : NAN;
  const float out  = isfinite(PID_OUT)   ? (float)PID_OUT   : 0.0f;

  // historia do wykresu (temp + OUT)
  pushHistory(tC, out);

  // przełączanie stron – tylko gdy RUN i brak TRIP
  if (run && !trip){
    if (now - g_pageSwitchMs >= PAGE_PERIOD_MS){
      g_page ^= 1;                  // 0 <-> 1
      g_pageSwitchMs = now;
    }
  } else {
    // w IDLE / TRIP zawsze strona 0 (temp)
    g_page = 0;
    g_pageSwitchMs = now;
  }

  oled.clearDisplay();

  // ── GÓRNA BELKA ─────────────────────────────────────────────────────────────
  oled.setTextSize(1);

  char left[10], mid[16], right[12];
  if (trip)       snprintf(left, sizeof(left),  "TRIP");
  else if (run)   snprintf(left, sizeof(left),  "RUN");
  else            snprintf(left, sizeof(left),  "IDLE");

  // środkowy napis – czas RUN albo „--:--”
  if (run) {
    fmtTimeSince(mid, sizeof(mid), now - runStartMs);
  } else {
    snprintf(mid, sizeof(mid), "--:--");
  }

  if (isfinite(sp)) snprintf(right, sizeof(right), "%.0fC", sp);
  else              snprintf(right, sizeof(right), "--C");

  // górna belka (żółta w kolorystyce, tutaj: inwersja gdy grzeje)
  drawTopBar(heat, left, mid, right);

    // ★★★ WAŻNE: reszta ekranu ma zawsze biały tekst ★★★
  oled.setTextColor(SSD1306_WHITE);

  // ── STRONA 0: tylko temperatura (pod belką) ────────────────────────────────
  if (g_page == 0){
    uint8_t topY = 14;
    oled.fillRect(0, topY, OLED_W, OLED_H - topY, SSD1306_BLACK);

    // duża temperatura w środku ekranu
    oled.setTextSize(3);
    char buf[16];
    if (isfinite(tC)) {
      snprintf(buf, sizeof(buf), "%.1f", tC);
    } else {
      snprintf(buf, sizeof(buf), "--.-");
    }

    int16_t x1,y1; uint16_t w,h;
    oled.getTextBounds(buf, 0,0, &x1,&y1,&w,&h);
    int16_t cx = (OLED_W - (int)w)/2;
    int16_t cy = topY + (OLED_H - topY - h)/2 - 4;
    if (cx < 0) cx = 0;
    if (cy < topY) cy = topY;

    oled.setCursor(cx, cy);
    oled.print(buf);
    oled.setTextSize(2);
    oled.print(F("C"));

    // pod spodem: info o profilu/dynamicznym i czasie kroku
    oled.setTextSize(1);
    cy += h + 4;
    if (cy < OLED_H-8){
      oled.setCursor(2, cy);

      // jeśli tryb profilu – krok + minuty (minęło / zostało)
      if (CFG.mode == MODE_PROFILE && PROFILE_LEN > 0){
        uint32_t em = PROFILE_ELAPSED_SEC / 60;
        uint32_t rm = PROFILE_REMAIN_SEC / 60;
        oled.printf("P %u/%u  %lum/%lum",
                    (unsigned)(PROFILE_ACTIVE+1),
                    (unsigned)PROFILE_LEN,
                    (unsigned long)em,
                    (unsigned long)rm);
      } else {
        // tryb dynamiczny – tylko czas RUN (minuty)
        if (run){
          uint32_t sec = (now - runStartMs)/1000UL;
          uint32_t m = sec/60;
          oled.printf("DYN  %lumin", (unsigned long)m);
        } else {
          oled.print(F("DYN  --"));
        }
      }
    }

    oled.display();
    return;
  }

  // ── STRONA 1: wykres (temp + OUT) ──────────────────────────────────────────
  uint8_t plotY = 14;
  uint8_t plotH = OLED_H - plotY - 1;

  if (plotH >= 14){
    uint8_t h1 = plotH - 6;
    drawSparklineTemp(0, plotY, OLED_W, h1);
    drawSparklineOut(0, plotY + h1 + 2, OLED_W, 4);
  } else {
    drawSparklineTemp(0, plotY, OLED_W, plotH);
  }

  oled.display();
}

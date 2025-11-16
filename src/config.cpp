/***************************************************************************************
 * FILE: src/config.cpp
 * LAST MODIFIED: 2025-11-10 23:58 (Europe/Warsaw)
 * PURPOSE: Definicja globalnego CFG + load/save do LittleFS
 ***************************************************************************************/
#include "config.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

static const char* CFG_FILE = "/config.json";

// jedyna definicja
RuntimeConfig CFG;

// helpers
static void _safeCopy(char* dst, size_t cap, const char* src){
  if (!dst || cap==0) return;
  if (!src) { dst[0]=0; return; }
  strlcpy(dst, src, cap);
}

void cfgLoadOrDefault(){
  CFG = RuntimeConfig();
  if (!LittleFS.begin()) { LittleFS.begin(); }
  if (!LittleFS.exists(CFG_FILE)) { cfgSave(); return; }

  File f = LittleFS.open(CFG_FILE, "r");
  if (!f) return;

  StaticJsonDocument<1024> d;
  DeserializationError err = deserializeJson(d, f);
  f.close();
  if (err) return;

  auto p = d["pins"];
  if (p.is<JsonObject>()){
    CFG.pins.SSR      = p["SSR"]      | CFG.pins.SSR;
    CFG.pins.LED      = p["LED"]      | CFG.pins.LED;
    CFG.pins.BUZZ     = p["BUZZ"]     | CFG.pins.BUZZ;
    CFG.pins.BTN_A    = p["BTN_A"]    | CFG.pins.BTN_A;
    CFG.pins.BTN_B    = p["BTN_B"]    | CFG.pins.BTN_B;
    CFG.pins.I2C_SDA  = p["I2C_SDA"]  | CFG.pins.I2C_SDA;
    CFG.pins.I2C_SCL  = p["I2C_SCL"]  | CFG.pins.I2C_SCL;
    CFG.pins.SPI_MOSI = p["SPI_MOSI"] | CFG.pins.SPI_MOSI;
    CFG.pins.SPI_MISO = p["SPI_MISO"] | CFG.pins.SPI_MISO;
    CFG.pins.SPI_SCK  = p["SPI_SCK"]  | CFG.pins.SPI_SCK;
    CFG.pins.SPI_CS   = p["SPI_CS"]   | CFG.pins.SPI_CS;
  }

  auto q = d["pid"];
  if (q.is<JsonObject>()){
    CFG.pid.Kp        = q["Kp"]        | CFG.pid.Kp;
    CFG.pid.Ki        = q["Ki"]        | CFG.pid.Ki;
    CFG.pid.Kd        = q["Kd"]        | CFG.pid.Kd;
    CFG.pid.setpointC = q["setpointC"] | CFG.pid.setpointC;
    CFG.pid.outMin    = q["outMin"]    | CFG.pid.outMin;
    CFG.pid.outMax    = q["outMax"]    | CFG.pid.outMax;
    CFG.pid.windowMs  = q["windowMs"]  | CFG.pid.windowMs;
  }

  CFG.sampleSec = d["sampleSec"] | CFG.sampleSec;
  CFG.maxTempC  = d["maxTempC"]  | CFG.maxTempC;
  if (d.containsKey("mode")) CFG.mode = (d["mode"].as<int>() == 1) ? MODE_PROFILE : MODE_DYNAMIC;

  auto s = d["sta"];
  if (s.is<JsonObject>()){
    CFG.staEnabled = s["en"] | CFG.staEnabled;
    _safeCopy(CFG.staSSID, sizeof(CFG.staSSID), s["ssid"] | "");
    _safeCopy(CFG.staPASS, sizeof(CFG.staPASS), s["pass"] | "");
  }

  auto m = d["mqtt"];
  if (m.is<JsonObject>()){
    CFG.mqttEnabled = m["en"] | CFG.mqttEnabled;
    _safeCopy(CFG.mqttHost, sizeof(CFG.mqttHost), m["host"] | "");
    CFG.mqttPort = m["port"] | CFG.mqttPort;
    _safeCopy(CFG.mqttUser, sizeof(CFG.mqttUser), m["user"] | "");
    _safeCopy(CFG.mqttPass, sizeof(CFG.mqttPass), m["pass"] | "");
    _safeCopy(CFG.mqttBaseTopic, sizeof(CFG.mqttBaseTopic), m["base"] | "kiln");
  }
}

void cfgSave(){
  if (!LittleFS.begin()) LittleFS.begin();

  StaticJsonDocument<1024> d;

  {
    JsonObject p = d.createNestedObject("pins");
    p["SSR"]      = CFG.pins.SSR;
    p["LED"]      = CFG.pins.LED;
    p["BUZZ"]     = CFG.pins.BUZZ;
    p["BTN_A"]    = CFG.pins.BTN_A;
    p["BTN_B"]    = CFG.pins.BTN_B;
    p["I2C_SDA"]  = CFG.pins.I2C_SDA;
    p["I2C_SCL"]  = CFG.pins.I2C_SCL;
    p["SPI_MOSI"] = CFG.pins.SPI_MOSI;
    p["SPI_MISO"] = CFG.pins.SPI_MISO;
    p["SPI_SCK"]  = CFG.pins.SPI_SCK;
    p["SPI_CS"]   = CFG.pins.SPI_CS;
  }

  {
    JsonObject p = d.createNestedObject("pid");
    p["Kp"]        = CFG.pid.Kp;
    p["Ki"]        = CFG.pid.Ki;
    p["Kd"]        = CFG.pid.Kd;
    p["setpointC"] = CFG.pid.setpointC;
    p["outMin"]    = CFG.pid.outMin;
    p["outMax"]    = CFG.pid.outMax;
    p["windowMs"]  = CFG.pid.windowMs;
  }

  d["sampleSec"] = CFG.sampleSec;
  d["maxTempC"]  = CFG.maxTempC;
  d["mode"]      = (CFG.mode == MODE_PROFILE) ? 1 : 0;

  {
    JsonObject s = d.createNestedObject("sta");
    s["en"]   = CFG.staEnabled;
    s["ssid"] = CFG.staSSID;
    s["pass"] = CFG.staPASS;
  }

  {
    JsonObject m = d.createNestedObject("mqtt");
    m["en"]   = CFG.mqttEnabled;
    m["host"] = CFG.mqttHost;
    m["port"] = CFG.mqttPort;
    m["user"] = CFG.mqttUser;
    m["pass"] = CFG.mqttPass;
    m["base"] = CFG.mqttBaseTopic;
  }

  File f = LittleFS.open(CFG_FILE, "w");
  if (!f) return;
  serializeJson(d, f);
  f.close();
}

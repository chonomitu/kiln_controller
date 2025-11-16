/***************************************************************************************
 * FILE: src/web_config.cpp
 * LAST MODIFIED: 2025-11-14 19:10 (Europe/Warsaw)
 * PURPOSE: Strony i API do konfiguracji pinów oraz profili (programów) wypału
 ***************************************************************************************/
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "config.h"
#include "control.h"   // korzystamy z applyProfileFromJson(...)
#include "web_config.h"
#include <vector>

// ────────────────────────────────────────────────────────────────────────────────
// Wstrzyknięcie referencji do serwera (bez globalnego "extern")
static ESP8266WebServer* g_srv = nullptr;

// Re-init pinów / magistral po zmianie mapy pinów
extern void pinsAndBusesInit();  // z control.cpp – re-init (piny/I2C/SPI/MAX)

// Pliki w LittleFS
static const char* FILE_PINS     = "/pins.json";
static const char* FILE_PROFILE  = "/profile.json";
static const char* DIR_PROFILES  = "/profiles";

// ────────────────────────────────────────────────────────────────────────────────
// Helpers
static void sendCORS(){
  g_srv->sendHeader("Access-Control-Allow-Origin", "*");
  g_srv->sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  g_srv->sendHeader("Access-Control-Allow-Headers", "Content-Type");
}
static void opt204(){ sendCORS(); g_srv->send(204); }

static bool ensureDir(const char* path){
  if (LittleFS.exists(path)) return true;
  return LittleFS.mkdir(path);
}

static std::vector<String> listProfiles(){
  std::vector<String> out;
  if (!LittleFS.begin()) LittleFS.begin();
  ensureDir(DIR_PROFILES);
  Dir d = LittleFS.openDir(DIR_PROFILES);
  while (d.next()) {
    String name = d.fileName(); // "/profiles/name.json"
    if (!name.endsWith(".json")) continue;
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash+1);
    name.replace(".json","");
    out.push_back(name);
  }
  return out;
}

// ────────────────────────────────────────────────────────────────────────────────
// GPIO mapa do D-etykiet + wartość specjalna: -1 = "Brak"
static const int GPIO_LIST[] = { -1,16,5,4,0,2,14,12,13,15,3,1 }; // -1, D0..D8, RX, TX
static const size_t GPIO_LIST_LEN = sizeof(GPIO_LIST)/sizeof(GPIO_LIST[0]);

static const char* gpioToD(int g){
  if (g < 0) return "Brak";
  switch (g){
    case 16: return "D0";
    case 5:  return "D1";
    case 4:  return "D2";
    case 0:  return "D3";
    case 2:  return "D4";
    case 14: return "D5";
    case 12: return "D6";
    case 13: return "D7";
    case 15: return "D8";
    case 3:  return "RX(D9)";
    case 1:  return "TX(D10)";
  }
  return "GPIO?";
}

static bool isInList(int g){
  // -1 = "Brak" – zawsze dozwolone
  if (g < 0) return true;
  for (size_t i = 0; i < GPIO_LIST_LEN; ++i){
    if (GPIO_LIST[i] == g) return true;
  }
  return false;
}

// ────────────────────────────────────────────────────────────────────────────────
// PINY: load/save FS + zastosowanie do CFG
static void applyPinsFromJson(const JsonObject& j){
  auto& P = CFG.pins; // skrót
  if (j.containsKey("I2C_SDA")) P.I2C_SDA = j["I2C_SDA"].as<int>();
  if (j.containsKey("I2C_SCL")) P.I2C_SCL = j["I2C_SCL"].as<int>();
  if (j.containsKey("SPI_MOSI"))P.SPI_MOSI= j["SPI_MOSI"].as<int>();
  if (j.containsKey("SPI_MISO"))P.SPI_MISO= j["SPI_MISO"].as<int>();
  if (j.containsKey("SPI_SCK")) P.SPI_SCK = j["SPI_SCK"].as<int>();
  if (j.containsKey("SPI_CS"))  P.SPI_CS  = j["SPI_CS"].as<int>();
  if (j.containsKey("SSR"))     P.SSR     = j["SSR"].as<int>();
  if (j.containsKey("LED"))     P.LED     = j["LED"].as<int>();
  if (j.containsKey("BUZZ"))    P.BUZZ    = j["BUZZ"].as<int>();
}

static bool loadPinsFS(){
  if (!LittleFS.exists(FILE_PINS)) return false;
  File f = LittleFS.open(FILE_PINS, "r"); if (!f) return false;
  StaticJsonDocument<512> d;
  auto err = deserializeJson(d, f); f.close();
  if (err) return false;
  applyPinsFromJson(d.as<JsonObject>());
  return true;
}

static bool savePinsFS(const JsonObject& j){
  File f = LittleFS.open(FILE_PINS, "w"); if (!f) return false;
  serializeJson(j, f); f.close(); return true;
}

// ────────────────────────────────────────────────────────────────────────────────
// PROFIL: load/save FS + zastosowanie do RAM (applyProfileFromJson z control.cpp)
bool loadProfileFS(){

  if (!LittleFS.exists(FILE_PROFILE)) return false;
  File f = LittleFS.open(FILE_PROFILE, "r"); if (!f) return false;
  StaticJsonDocument<1024> d;
  auto err = deserializeJson(d, f); f.close();
  if (err) return false;

  JsonArray steps = d["steps"].as<JsonArray>();
  if (steps.isNull()) return false;

  bool ok = applyProfileFromJson(steps);
  if (ok) {
    CFG.mode = MODE_PROFILE;
    cfgSave();
  }
  return ok;
}

static bool saveProfileFS(const JsonArray& steps){
  StaticJsonDocument<1024> d;
  JsonArray out = d.createNestedArray("steps");
  uint8_t n = 0;
  for (JsonObject s : steps){
    if (n >= 8) break;
    JsonObject o = out.createNestedObject();
    o["targetC"] = s["targetC"] | 0;
    o["holdSec"] = s["holdSec"] | 0;
    n++;
  }
  File f = LittleFS.open(FILE_PROFILE, "w"); if (!f) return false;
  serializeJson(d, f); f.close();
  // zastosuj do RAM
  bool ok = applyProfileFromJson(out);
  if (ok){
    CFG.mode = MODE_PROFILE;
    cfgSave();
  }
  return ok;
}

// ────────────────────────────────────────────────────────────────────────────────
// HTML – Piny
#include "webui/pins_html.h"


// ────────────────────────────────────────────────────────────────────────────────
// HTML – Profil
#include "webui/profile_html.h"


// ────────────────────────────────────────────────────────────────────────────────
// Handlery: piny
static void handlePinsHtml(){ sendCORS(); g_srv->send_P(200,"text/html; charset=utf-8",PINS_HTML); }

static void handlePinsJson(){
  StaticJsonDocument<384> d; auto& P = CFG.pins;
  d["I2C_SDA"]=P.I2C_SDA; d["I2C_SCL"]=P.I2C_SCL;
  d["SPI_MOSI"]=P.SPI_MOSI; d["SPI_MISO"]=P.SPI_MISO; d["SPI_SCK"]=P.SPI_SCK; d["SPI_CS"]=P.SPI_CS;
  d["SSR"]=P.SSR; d["LED"]=P.LED; d["BUZZ"]=P.BUZZ;
  String out; serializeJson(d,out); sendCORS(); g_srv->send(200,"application/json",out);
}

static void handlePinsSave(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  StaticJsonDocument<384> d;
  DeserializationError err = deserializeJson(d, g_srv->arg("plain"));
  if (err){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"bad json\"}"); return; }

  // walidacja + kolizje
  const char* keys[9]={"I2C_SDA","I2C_SCL","SPI_MOSI","SPI_MISO","SPI_SCK","SPI_CS","SSR","LED","BUZZ"};
  int vals[9];
  for (int i=0;i<9;i++){
    if (!d.containsKey(keys[i])){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing field\"}"); return; }
    int g = d[keys[i]].as<int>();
    if (!isInList(g)){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"gpio out of list\"}"); return; }
    vals[i]=g;
  }
  for (int i=0;i<9;i++){
    for (int j=i+1;j<9;j++){
      // -1 = "Brak" – może się powtarzać, nie traktujemy tego jako kolizję
      if (vals[i] >= 0 && vals[i] == vals[j]){
        sendCORS();
        g_srv->send(400,"application/json","{\"error\":\"collision\"}");
        return;
      }
    }
  }

  if (!LittleFS.exists("/")) LittleFS.begin();
  if (!savePinsFS(d.as<JsonObject>())){ sendCORS(); g_srv->send(500,"application/json","{\"error\":\"fs write\"}"); return; }

  // zastosuj + re-init
  applyPinsFromJson(d.as<JsonObject>());
  pinsAndBusesInit();

  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true}");
}

// ────────────────────────────────────────────────────────────────────────────────
// Handlery: profil
static void handleProfileHtml(){ sendCORS(); g_srv->send_P(200,"text/html; charset=utf-8",PROFILE_HTML); }

static void handleProfileJson(){
  StaticJsonDocument<1024> d;
  JsonArray a = d.createNestedArray("steps");
  for (uint8_t i=0;i<PROFILE_LEN;i++){
    JsonObject o = a.createNestedObject();
    o["targetC"] = PROFILE[i].targetC;
    o["holdSec"] = PROFILE[i].holdSec;
  }
  String out; serializeJson(d,out);
  sendCORS(); g_srv->send(200,"application/json",out);
}

static void handleProfileSave(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  StaticJsonDocument<1024> d;
  DeserializationError err = deserializeJson(d, g_srv->arg("plain"));
  if (err){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"bad json\"}"); return; }
  JsonArray steps = d["steps"].as<JsonArray>();
  if (steps.isNull()){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing steps\"}"); return; }
  if (steps.size() > 8){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"max 8\"}"); return; }

  // prosta walidacja
  for (JsonObject s : steps){
    double t = s["targetC"] | NAN;
    uint32_t hold = s["holdSec"] | 0;
    if (!isfinite(t) || t < 0 || t > 2000){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"bad temp\"}"); return; }

    if (hold > 72UL*3600UL){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"max 72h\"}"); return; }
  }

  if (!LittleFS.exists("/")) LittleFS.begin();
  if (!saveProfileFS(steps)){ sendCORS(); g_srv->send(500,"application/json","{\"error\":\"fs write\"}"); return; }

  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true}");
}

// Manager wielu profili (nazwane zestawy)
static bool loadNamedProfile(const String& name){
  if (!LittleFS.begin()) LittleFS.begin();
  ensureDir(DIR_PROFILES);
  String fn = String(DIR_PROFILES) + "/" + name + ".json";
  if (!LittleFS.exists(fn)) return false;

  File f = LittleFS.open(fn, "r"); if (!f) return false;
  StaticJsonDocument<1024> d;
  auto err = deserializeJson(d, f); f.close();
  if (err) return false;

  JsonArray steps = d["steps"].as<JsonArray>();
  if (steps.isNull()) return false;

  // ustaw jako aktywny + zastosuj
  File fa = LittleFS.open(FILE_PROFILE, "w");
  if (fa) { serializeJson(d, fa); fa.close(); }
  bool ok = applyProfileFromJson(steps);
  if (ok){
    CFG.mode = MODE_PROFILE;
    cfgSave();
  }
  return ok;
}

static bool saveNamedProfile(const String& name, const JsonArray& steps){
  if (!LittleFS.begin()) LittleFS.begin();
  ensureDir(DIR_PROFILES);
  String fn = String(DIR_PROFILES) + "/" + name + ".json";

  StaticJsonDocument<1024> d;
  JsonArray out = d.createNestedArray("steps");
  for (JsonObject s : steps){
    JsonObject o = out.createNestedObject();
    o["targetC"] = s["targetC"] | 0;
    o["holdSec"] = s["holdSec"] | 0;
  }
  File f = LittleFS.open(fn, "w"); if (!f) return false;
  serializeJson(d, f); f.close();

  // uczyń aktywnym
  File fa = LittleFS.open(FILE_PROFILE, "w");
  if (fa) { serializeJson(d, fa); fa.close(); }

  bool ok = applyProfileFromJson(out);
  if (ok){
    CFG.mode = MODE_PROFILE;
    cfgSave();
  }
  return ok;
}

static bool deleteNamedProfile(const String& name){
  if (!LittleFS.begin()) LittleFS.begin();
  ensureDir(DIR_PROFILES);
  String fn = String(DIR_PROFILES) + "/" + name + ".json";
  return LittleFS.exists(fn) ? LittleFS.remove(fn) : true;
}

// API: /profiles/*
static void handleProfilesList(){
  auto lst = listProfiles();
  StaticJsonDocument<512> d; JsonArray a = d.createNestedArray("profiles");
  for (auto &n : lst) a.add(n);
  String out; serializeJson(d,out); sendCORS(); g_srv->send(200,"application/json",out);
}

static void handleProfilesSave(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  if(!g_srv->hasArg("name")){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing name\"}"); return; }
  String name = g_srv->arg("name");
  StaticJsonDocument<1024> d;
  if (deserializeJson(d, g_srv->arg("plain"))) { sendCORS(); g_srv->send(400,"application/json","{\"error\":\"bad json\"}"); return; }
  JsonArray steps = d["steps"].as<JsonArray>();
  if (steps.isNull()){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing steps\"}"); return; }
  if (!saveNamedProfile(name, steps)){ sendCORS(); g_srv->send(500,"application/json","{\"error\":\"fs write\"}"); return; }
  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true}");
}

static void handleProfilesLoad(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  if(!g_srv->hasArg("name")){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing name\"}"); return; }
  String name = g_srv->arg("name");
  if (!loadNamedProfile(name)){ sendCORS(); g_srv->send(404,"application/json","{\"error\":\"not found\"}"); return; }
  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true}");
}

static void handleProfilesDelete(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  if(!g_srv->hasArg("name")){ sendCORS(); g_srv->send(400,"application/json","{\"error\":\"missing name\"}"); return; }
  String name = g_srv->arg("name");
  if (!deleteNamedProfile(name)){ sendCORS(); g_srv->send(500,"application/json","{\"error\":\"fs\"}"); return; }
  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true}");
}

// ────────────────────────────────────────────────────────────────────────────────
// Tryb pracy: dynamiczny vs profilowy
// ────────────────────────────────────────────────────────────────────────────────
static void handleProfileModeDynamic(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  CFG.mode = MODE_DYNAMIC;
  cfgSave(); // zapisz w EEPROM/FS
  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true,\"mode\":\"dynamic\"}");
}

static void handleProfileModeProfile(){
  if (g_srv->method()==HTTP_OPTIONS){ opt204(); return; }
  if (PROFILE_LEN == 0){
    sendCORS(); g_srv->send(400,"application/json","{\"error\":\"no_profile\"}");
    return;
  }
  CFG.mode = MODE_PROFILE;
  cfgSave();
  sendCORS(); g_srv->send(200,"application/json","{\"ok\":true,\"mode\":\"profile\"}");
}

// ────────────────────────────────────────────────────────────────────────────────
// Rejestracja tras
void registerConfigRoutes(ESP8266WebServer& srv){
  g_srv = &srv;

  // Piny
  srv.on("/pins.html", HTTP_GET,      handlePinsHtml);
  srv.on("/pins/json", HTTP_GET,      handlePinsJson);
  srv.on("/pins/save", HTTP_POST,     handlePinsSave);
  srv.on("/pins/save", HTTP_OPTIONS,  opt204);

  // Profil (aktywny)
  srv.on("/profile.html", HTTP_GET,     handleProfileHtml);
  srv.on("/profile.json", HTTP_GET,     handleProfileJson);
  srv.on("/profile/save", HTTP_POST,    handleProfileSave);
  srv.on("/profile/save", HTTP_OPTIONS, opt204);

  // Manager profili nazwanych
  srv.on("/profiles/list",   HTTP_GET,     handleProfilesList);
  srv.on("/profiles/save",   HTTP_POST,    handleProfilesSave);
  srv.on("/profiles/save",   HTTP_OPTIONS, opt204);
  srv.on("/profiles/load",   HTTP_POST,    handleProfilesLoad);
  srv.on("/profiles/load",   HTTP_OPTIONS, opt204);
  srv.on("/profiles/delete", HTTP_POST,    handleProfilesDelete);
  srv.on("/profiles/delete", HTTP_OPTIONS, opt204);

  // Tryb pracy (dynamic/profile)
  srv.on("/profile/mode/dynamic", HTTP_POST,    handleProfileModeDynamic);
  srv.on("/profile/mode/dynamic", HTTP_OPTIONS, opt204);
  srv.on("/profile/mode/profile", HTTP_POST,    handleProfileModeProfile);
  srv.on("/profile/mode/profile", HTTP_OPTIONS, opt204);
}

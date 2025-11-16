/***************************************************************************************
 * FILE: src/webserver.cpp
 * LAST MODIFIED: 2025-11-15 03:40 (Europe/Warsaw)
 * PURPOSE: ESP8266 WebServer – SoftAP (open), panel mobilny, /ping + wykres z osiami
 ***************************************************************************************/
#include "webserver.h"
#include "control.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <math.h>
#include <DNSServer.h>

#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <LittleFS.h>

static ESP8266HTTPUpdateServer httpUpdater;

// dodatkowe extern z control.cpp – czas profilu (sekundy)
extern uint32_t PROFILE_ELAPSED_SEC;
extern uint32_t PROFILE_REMAIN_SEC;
extern uint8_t  PROFILE_ACTIVE;
extern uint8_t  PROFILE_LEN;

// CSV z bufora (definiowane w control.cpp / control.h)
// extern void buildSamplesCSV(String& out);  // jeśli nie masz w nagłówku, odkomentuj

// === Serwer / DNS ==============================================================
static DNSServer DNS;
ESP8266WebServer server(80);              // ← udostępniamy globalnie (extern w web_config.cpp)
static IPAddress AP_IP(192,168,4,1), AP_GW(192,168,4,1), AP_MASK(255,255,255,0);

static void sendCORS();   // forward deklaracja – używane np. w handlePresetsSave

// Rejestracja tras z web_config.cpp (piny + profil)
#include "web_config.h"

// Mapowanie GPIO → „D# / Brak”
static const char* gpioToDLabel(int gpio) {
  if (gpio < 0) {
    return "Brak";           // specjalna wartość: brak pinu
  }
  switch (gpio) {
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
    default: return nullptr; // inne GPIO – opis zrobimy dynamicznie
  }
}

// ── Presety setpointów (lokalne – przykładowe) ────────────────────────────────
struct Preset { char name[24]; double sp; };

// domyślne wartości (startowe)
static Preset PRESETS[] = {
  {"Suszenie 80°C",   80},
  {"Test 200°C",     200},
  {"Biskwit 950°C",  950},
  {"Szkliwo 1050°C", 1050}
};
static const uint8_t PRESET_COUNT = sizeof(PRESETS)/sizeof(PRESETS[0]);

static const char* FILE_PRESETS = "/presets.json";

static void presetsLoadFS() {
  if (!LittleFS.begin()) LittleFS.begin();
  if (!LittleFS.exists(FILE_PRESETS)) return;

  File f = LittleFS.open(FILE_PRESETS, "r");
  if (!f) return;

  StaticJsonDocument<512> d;
  auto err = deserializeJson(d, f);
  f.close();
  if (err) return;

  JsonArray a = d["presets"].as<JsonArray>();
  if (a.isNull()) return;

  uint8_t i = 0;
  for (JsonObject p : a) {
    if (i >= PRESET_COUNT) break;
    const char* name = p["name"] | nullptr;
    double sp       = p["sp"]   | PRESETS[i].sp;

    if (name && name[0]) {
      strncpy(PRESETS[i].name, name, sizeof(PRESETS[i].name)-1);
      PRESETS[i].name[sizeof(PRESETS[i].name)-1] = '\0';
    }
    PRESETS[i].sp = sp;
    i++;
  }
}

static bool presetsSaveFS() {
  if (!LittleFS.begin()) LittleFS.begin();

  StaticJsonDocument<512> d;
  JsonArray a = d.createNestedArray("presets");
  for (uint8_t i=0; i<PRESET_COUNT; i++){
    JsonObject o = a.createNestedObject();
    o["name"] = PRESETS[i].name;
    o["sp"]   = PRESETS[i].sp;
  }

  File f = LittleFS.open(FILE_PRESETS, "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

static void handlePresetsSave() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  StaticJsonDocument<512> d;
  DeserializationError err = deserializeJson(d, server.arg("plain"));
  if (err) {
    sendCORS();
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  JsonArray a = d["presets"].as<JsonArray>();
  if (a.isNull()) {
    sendCORS();
    server.send(400,"application/json","{\"error\":\"missing presets\"}");
    return;
  }

  uint8_t i = 0;
  for (JsonObject p : a) {
    if (i >= PRESET_COUNT) break;
    const char* name = p["name"] | PRESETS[i].name;
    double sp        = p["sp"]   | PRESETS[i].sp;

    if (!isfinite(sp) || sp < 0 || sp > 2000) {
      sendCORS();
      server.send(400, "application/json", "{\"error\":\"bad sp\"}");
      return;
    }

    strncpy(PRESETS[i].name, name, sizeof(PRESETS[i].name)-1);
    PRESETS[i].name[sizeof(PRESETS[i].name)-1] = '\0';
    PRESETS[i].sp = sp;
    i++;
  }

  if (!presetsSaveFS()) {
    sendCORS();
    server.send(500,"application/json","{\"error\":\"fs write\"}");
    return;
  }

  sendCORS();
  server.send(200,"application/json","{\"ok\":true}");
}

// === HTML UI ===================================================================
#include "webui/index_html.h"


// === CORS ======================================================================
static void sendCORS(){
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// Lekki status JSON (z RUN_REV)
static String buildStatusJson(){
  String j; j.reserve(256);
  j += F("{");
  j += F("\"rev\":");      j += String((uint32_t)RUN_REV);                       j += F(",");
  j += F("\"run\":");      j += (RUN_ACTIVE?F("true"):F("false"));               j += F(",");
  j += F("\"trip\":");     j += (SAFETY_TRIP?F("true"):F("false"));              j += F(",");
  j += F("\"heating\":");  j += (HEATER_ON?F("true"):F("false"));                j += F(",");
  j += F("\"sensor_ok\":");j += (SENSOR_OK?F("true"):F("false"));                j += F(",");
  j += F("\"temp_c\":");   j += (isfinite(KILN_TEMP)?String(KILN_TEMP,1):F("null")); j += F(",");
  j += F("\"set_c\":");    j += String(PID_SET,1);                               j += F(",");
  j += F("\"duty_pct\":"); j += String(PID_OUT,1);
  j += F("}");
  return j;
}

// === Handlery główne ==========================================================
static void handleIndex(){ sendCORS(); server.send_P(200,"text/html; charset=utf-8",INDEX_HTML); }
static void handlePing(){  sendCORS(); server.send(200,"text/plain","pong"); }

static void handleState() {
  StaticJsonDocument<512> d;

  double temp = KILN_TEMP;
  bool sensorOK = isfinite(temp);
  if (!sensorOK) temp = -127.0;

  d["active"] = RUN_ACTIVE;
  d["safety"] = SAFETY_TRIP;
  d["heater"] = HEATER_ON;
  d["sensor"] = sensorOK;
  d["temp"]   = temp;  // piec
  d["ctrl_temp"] = isfinite(CTRL_TEMP) ? CTRL_TEMP : NAN;  // sterownik
  d["set"]    = isfinite(PID_SET) ? PID_SET : 0;
  d["out"]    = isfinite(PID_OUT) ? PID_OUT : 0;
  d["mode"]   = (CFG.mode == MODE_PROFILE) ? "profile" : "dynamic";

  // dane profilu – krok + czasy etapu (sekundy)
  if (CFG.mode == MODE_PROFILE) {
    d["step"]            = PROFILE_ACTIVE;
    d["steps"]           = PROFILE_LEN;
    d["profile_elapsed"] = PROFILE_ELAPSED_SEC;
    d["profile_remain"]  = PROFILE_REMAIN_SEC;
  }

  // ─────────────────────────────────────────────
  // INFO O SIECI – AP (zawsze) + STA (opcjonalnie)
  // ─────────────────────────────────────────────
  JsonObject ap = d.createNestedObject("ap");
  ap["ssid"] = WiFi.softAPSSID();
  ap["ip"]   = WiFi.softAPIP().toString();
  ap["mac"]  = WiFi.softAPmacAddress();

  JsonObject sta = d.createNestedObject("sta");
  wl_status_t st = WiFi.status();
  bool staConnected = (st == WL_CONNECTED);
  sta["connected"] = staConnected;
  if (staConnected) {
    sta["ssid"] = WiFi.SSID();
    sta["ip"]   = WiFi.localIP().toString();
    sta["mac"]  = WiFi.macAddress();
  }

  String out;
  serializeJson(d, out);
  sendCORS();
  server.send(200, "application/json", out);
}

static void handleStatusJson(){
  String body = buildStatusJson();
  sendCORS();
  server.sendHeader("Cache-Control","no-store, no-cache, must-revalidate, proxy-revalidate");
  server.send(200,"application/json",body);
}

static void handleProfileNext() {
  profileNextStep();
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleProfilePrev() {
  profilePrevStep();
  server.send(200, "application/json", "{\"ok\":true}");
}

// Aktualizacja handlePins() – pełne mapowanie GPIO + "Brak" dla -1
static void handlePins(){
  StaticJsonDocument<768> d; JsonArray a = d.to<JsonArray>();

  struct Item{ const char* role; int gpio; };
  Item items[] = {
    {"I2C_SDA",CFG.pins.I2C_SDA},
    {"I2C_SCL",CFG.pins.I2C_SCL},
    {"SPI_MOSI",CFG.pins.SPI_MOSI},
    {"SPI_MISO",CFG.pins.SPI_MISO},
    {"SPI_SCK",CFG.pins.SPI_SCK},
    {"SPI_CS",CFG.pins.SPI_CS},
    {"SSR",CFG.pins.SSR},
    {"LED",CFG.pins.LED},
    {"BUZZ",CFG.pins.BUZZ},
  };

  for (auto &it : items){
    JsonObject o = a.createNestedObject();
    o["role"]  = it.role;
    o["gpio"]  = it.gpio;

    const char* lab = gpioToDLabel(it.gpio);
    if (lab) {
      o["label"] = lab;                 // D0..D8, RX, TX albo "Brak"
    } else {
      o["label"] = String("GPIO") + it.gpio; // inne numery GPIO
    }
  }

  String out; serializeJson(d,out);
  sendCORS();
  server.send(200,"application/json",out);
}

static void handleStart(){
  Serial.println(F("[HTTP] /start"));
  SAFETY_TRIP=false;
  runStart();
  sendCORS();
  server.send(200,"application/json","{\"ok\":true}");
}

static void handleStop(){
  Serial.println(F("[HTTP] /stop"));
  runStop();
  sendCORS();
  server.send(200,"application/json","{\"ok\":true}");
}

static void handleSet(){
  if (!server.hasArg("sp")) {
    sendCORS();
    server.send(400,"application/json","{\"error\":\"missing sp\"}");
    return;
  }

  String raw = server.arg("sp");
  raw.replace(',', '.');   // akceptuj przecinek
  double sp = atof(raw.c_str());

  // Zakres spójny z presetami i profilami: 0–2000°C
  if (!(sp >= 0 && sp <= 2000)) {
    sendCORS();
    server.send(400,"application/json","{\"error\":\"range 0..2000\"}");
    return;
  }

  PID_SET = sp;
  CFG.pid.setpointC = sp;

  Serial.print(F("[HTTP] /set sp="));
  Serial.println(sp);

  sendCORS();
  server.send(200,"application/json","{\"ok\":true}");
}

static void handlePresets(){
  StaticJsonDocument<512> d; JsonArray a = d.to<JsonArray>();
  for (uint8_t i=0;i<PRESET_COUNT;i++){
    JsonObject o=a.createNestedObject();
    o["i"]=i; o["name"]=PRESETS[i].name; o["sp"]=PRESETS[i].sp;
  }
  String out; serializeJson(d,out);
  sendCORS(); server.send(200,"application/json",out);
}

static void handlePresetSet(){
  if(!server.hasArg("i")){ sendCORS(); server.send(400,"application/json","{\"error\":\"missing i\"}"); return; }
  int i = atoi(server.arg("i").c_str());
  if (i<0 || i>=PRESET_COUNT){ sendCORS(); server.send(400,"application/json","{\"error\":\"bad index\"}"); return; }
  double sp = PRESETS[i].sp;
  PID_SET = sp; CFG.pid.setpointC = sp;
  Serial.print(F("[HTTP] /preset i=")); Serial.print(i); Serial.print(F(" sp=")); Serial.println(sp);
  sendCORS(); server.send(200,"application/json","{\"ok\":true}");
}

// /mode?m=dynamic|profile
static void handleMode(){
  if(!server.hasArg("m")){ sendCORS(); server.send(400,"application/json","{\"error\":\"missing m\"}"); return; }
  String m = server.arg("m");
  if (m == "dynamic"){ CFG.mode = MODE_DYNAMIC; }
  else if (m == "profile"){ CFG.mode = MODE_PROFILE; }
  else { sendCORS(); server.send(400,"application/json","{\"error\":\"bad mode\"}"); return; }

  cfgSave(); // zapis do pamięci

  Serial.print(F("[HTTP] /mode m=")); Serial.println(m);
  sendCORS(); server.send(200,"application/json","{\"ok\":true}");
}

// CSV eksport
static void handleExport(){
  String csv;
  buildSamplesCSV(csv);
  sendCORS();
  server.sendHeader("Content-Type","text/csv; charset=utf-8");
  server.sendHeader("Content-Disposition","attachment; filename=\"kiln_samples.csv\"");
  server.send(200,"text/csv",csv);
}

static void handleModeDynamic() {
  CFG.mode = MODE_DYNAMIC;
  cfgSave();
  server.send(200, "application/json", "{\"ok\":true,\"mode\":\"dynamic\"}");
}

static void handleModeProfile() {
  CFG.mode = MODE_PROFILE;
  cfgSave();
  server.send(200, "application/json", "{\"ok\":true,\"mode\":\"profile\"}");
}

// ── Stuby stron WiFi / MQTT ───────────────────────────────────────────────────
static void handleWifiPage() {
  sendCORS();
  String html;
  html.reserve(3000);
  html += F("<!doctype html><html lang=\"pl\"><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>Konfiguracja WiFi</title>"
            "<body style='font-family:sans-serif;background:#0b0e12;color:#e8eef6'>"
            "<div style='max-width:720px;margin:0 auto;padding:16px'>"
            "<h1>Konfiguracja WiFi</h1>"
            "<p>Sterownik zawsze uruchamia własną sieć AP do konfiguracji (domyślnie <code>KilnCtrl-XXXX</code>). "
            "Poniżej możesz włączyć tryb <b>klienta (STA)</b>, aby urządzenie łączyło się z Twoim routerem.</p>");

  html += F("<form method='POST' action='/wifi/save' style='margin:16px 0'>");
  html += F("<label style='display:block;margin:8px 0'>"
            "<input type='checkbox' name='sta_en' value='1'");
  if (CFG.staEnabled) html += F(" checked");
  html += F("> Włącz tryb klienta WiFi (STA)</label>");

  html += F("<label style='display:block;margin:8px 0'>SSID (nazwa sieci): "
            "<input name='ssid' maxlength='32' style='width:100%' value='");
  html += CFG.staSSID;
  html += F("'></label>");

  html += F("<label style='display:block;margin:8px 0'>Hasło: "
            "<input name='pass' type='password' maxlength='64' style='width:100%' value='");
  html += CFG.staPASS;
  html += F("'></label>");

  html += F("<button type='submit' style='padding:8px 16px;border:0;border-radius:8px;"
            "background:#48a1ff;color:#0b0e12;cursor:pointer'>Zapisz WiFi</button>");
  html += F("</form>");

  html += F("<h2>Jak znaleźć adres IP w sieci domowej?</h2>"
            "<ol>"
            "<li>Po zapisaniu ustawień odłącz i podłącz zasilanie sterownika.</li>"
            "<li>Wejdź w panel routera i sprawdź listę urządzeń (szukaj nazwy <code>KilnCtrl</code>).</li>"
            "<li>Adres IP wykorzystasz później w Home Assistant (REST lub MQTT).</li>"
            "</ol>");

  html += F("<p style='margin-top:24px'><a href=\"/\" style='color:#8bf7ff'>← Powrót do panelu</a></p>"
            "</div></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

static void handleWifiSave() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  bool en = server.hasArg("sta_en");
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  ssid.trim();
  pass.trim();

  CFG.staEnabled = en && ssid.length() > 0;
  ssid.toCharArray(CFG.staSSID, sizeof(CFG.staSSID));
  pass.toCharArray(CFG.staPASS, sizeof(CFG.staPASS));

  cfgSave();

  sendCORS();
  String html;
  html.reserve(1024);
  html += F("<!doctype html><html lang=\"pl\"><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>WiFi zapisane</title>"
            "<body style='font-family:sans-serif;background:#0b0e12;color:#e8eef6'>"
            "<div style='max-width:720px;margin:0 auto;padding:16px'>"
            "<h1>Ustawienia WiFi zapisane</h1>"
            "<p>Nowe ustawienia zostały zapisane w pamięci. Jeśli włączyłeś tryb klienta (STA), "
            "uruchom ponownie urządzenie, aby połączyło się z routerem.</p>"
            "<p><a href=\"/wifi\" style='color:#8bf7ff'>← Wróć do konfiguracji WiFi</a></p>"
            "<p><a href=\"/\" style='color:#8bf7ff'>← Wróć do panelu głównego</a></p>"
            "</div></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

static void handleMqttPage() {
  sendCORS();
  String base = String(CFG.mqttBaseTopic);
  if (!base.length()) base = F("kiln");

  String html;
  html.reserve(5000);
  html += F("<!doctype html><html lang=\"pl\"><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>Konfiguracja MQTT</title>"
            "<body style='font-family:sans-serif;background:#0b0e12;color:#e8eef6'>"
            "<div style='max-width:900px;margin:0 auto;padding:16px'>"
            "<h1>Konfiguracja MQTT</h1>");

  html += F("<form method='POST' action='/mqtt/save' style='margin:16px 0'>");
  html += F("<label style='display:block;margin:8px 0'>"
            "<input type='checkbox' name='mqtt_en' value='1'");
  if (CFG.mqttEnabled) html += F(" checked");
  html += F("> Włącz wysyłanie danych przez MQTT</label>");

  html += F("<label style='display:block;margin:8px 0'>Adres / host brokera MQTT: "
            "<input name='host' maxlength='63' style='width:100%' value='");
  html += CFG.mqttHost;
  html += F("'></label>");

  html += F("<label style='display:block;margin:8px 0'>Port: "
            "<input name='port' type='number' min='1' max='65535' style='width:120px' value='");
  html += String(CFG.mqttPort);
  html += F("'></label>");

  html += F("<label style='display:block;margin:8px 0'>Użytkownik (opcjonalnie): "
            "<input name='user' maxlength='63' style='width:100%' value='");
  html += CFG.mqttUser;
  html += F("'></label>");

  html += F("<label style='display:block;margin:8px 0'>Hasło (opcjonalnie): "
            "<input name='pass' type='password' maxlength='63' style='width:100%' value='");
  html += CFG.mqttPass;
  html += F("'></label>");

  html += F("<label style='display:block;margin:8px 0'>Bazowy topic (prefix): "
            "<input name='base' maxlength='63' style='width:100%' value='");
  html += CFG.mqttBaseTopic;
  html += F("'></label>");

  html += F("<button type='submit' style='padding:8px 16px;border:0;border-radius:8px;"
            "background:#48a1ff;color:#0b0e12;cursor:pointer'>Zapisz MQTT</button>");
  html += F("</form>");

  html += F("<h2>Integracja z Home Assistant – REST (bez MQTT)</h2>"
            "<p>Aktualny firmware udostępnia dane pieca pod adresem <code>/status.json</code>. "
            "Możesz dodać w Home Assistant integrację <b>REST</b> z wykorzystaniem tego endpointu.</p>"
            "<pre style='background:#12161d;padding:12px;border-radius:8px;white-space:pre;overflow:auto'><code>"
            "rest:\\n"
            "  - resource: \"http://ADRES_IP_PIECA/status.json\"\\n"
            "    scan_interval: 5\\n"
            "    sensor:\\n"
            "      - name: \\\"Piec temperatura\\\"\\n"
            "        value_template: \\\"{{ value_json.temp_c }}\\\"\\n"
            "        unit_of_measurement: \\\"°C\\\"\\n"
            "      - name: \\\"Piec zadana\\\"\\n"
            "        value_template: \\\"{{ value_json.set_c }}\\\"\\n"
            "        unit_of_measurement: \\\"°C\\\"\\n"
            "      - name: \\\"Piec moc\\\"\\n"
            "        value_template: \\\"{{ value_json.duty_pct }}\\\"\\n"
            "        unit_of_measurement: \\\"%\\\"\\n"
            "</code></pre>");

  html += F("<h2>Integracja MQTT – planowane topici</h2>"
            "<p>Po włączeniu MQTT i dodaniu obsługi publikowania w kolejnej wersji firmware "
            "urządzenie będzie wysyłało dane pod topicami:</p><ul>");
  html += F("<li><code>");
  html += base;
  html += F("/temp_c</code> – temperatura pieca (°C)</li><li><code>");
  html += base;
  html += F("/set_c</code> – zadana temperatura (°C)</li><li><code>");
  html += base;
  html += F("/duty_pct</code> – wyjście sterownika (% mocy)</li>");
  html += F("</ul><p>W Home Assistant będziesz mógł wtedy użyć klasycznych sensorów MQTT "
            "opartych o powyższe topici.</p>");

  html += F("<p style='margin-top:24px'><a href=\"/\" style='color:#8bf7ff'>← Powrót do panelu głównego</a></p>"
            "</div></body></html>");

  server.send(200, "text/html; charset=utf-8", html);
}

static void handleMqttSave() {
  if (server.method() == HTTP_OPTIONS) {
    sendCORS();
    server.send(204);
    return;
  }

  bool en = server.hasArg("mqtt_en");
  String host = server.hasArg("host") ? server.arg("host") : "";
  String portStr = server.hasArg("port") ? server.arg("port") : "";
  String user = server.hasArg("user") ? server.arg("user") : "";
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  String base = server.hasArg("base") ? server.arg("base") : "";
  host.trim(); user.trim(); pass.trim(); base.trim();

  CFG.mqttEnabled = en && host.length() > 0;
  host.toCharArray(CFG.mqttHost, sizeof(CFG.mqttHost));
  user.toCharArray(CFG.mqttUser, sizeof(CFG.mqttUser));
  pass.toCharArray(CFG.mqttPass, sizeof(CFG.mqttPass));
  base.toCharArray(CFG.mqttBaseTopic, sizeof(CFG.mqttBaseTopic));
  uint16_t port = (uint16_t)portStr.toInt();
  if (port == 0) port = 1883;
  CFG.mqttPort = port;

  cfgSave();

  sendCORS();
  String html;
  html.reserve(1024);
  html += F("<!doctype html><html lang=\"pl\"><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>MQTT zapisane</title>"
            "<body style='font-family:sans-serif;background:#0b0e12;color:#e8eef6'>"
            "<div style='max-width:720px;margin:0 auto;padding:16px'>"
            "<h1>Ustawienia MQTT zapisane</h1>"
            "<p>Nowe ustawienia brokera zostały zapisane w pamięci. "
            "Po dodaniu obsługi publikowania w firmware dane pieca będą widoczne w MQTT.</p>"
            "<p><a href=\"/mqtt\" style='color:#8bf7ff'>← Wróć do konfiguracji MQTT</a></p>"
            "<p><a href=\"/\" style='color:#8bf7ff'>← Wróć do panelu głównego</a></p>"
            "</div></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

// === Setup =====================================================================
void webserverSetup() {
  // ─────────────────────────────────────────────
  // TRYB WiFi: AP + (opcjonalnie) STA
  // ─────────────────────────────────────────────
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  // SoftAP – panel konfiguracyjny
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "KilnCtrl-%04X", (uint16_t)(ESP.getChipId() & 0xFFFF));
  WiFi.softAP(ssid);

  IPAddress ip = WiFi.softAPIP();
  Serial.print(F("[WiFi] SoftAP: ")); Serial.print(ssid);
  Serial.print(F("  IP: ")); Serial.println(ip);
  Serial.print(F("[WiFi] AP MAC: ")); Serial.println(WiFi.softAPmacAddress());

  DNS.start(53, "*", AP_IP);

  // ─────────────────────────────────────────────
  // STA – klient WiFi (jeśli włączony w configu)
  // ─────────────────────────────────────────────
  if (CFG.staEnabled && strlen(CFG.staSSID) > 0) {
    Serial.println(F("[WiFi] STA enabled in config"));
    Serial.print  (F("[WiFi] Connecting to SSID: "));
    Serial.println(CFG.staSSID);

    WiFi.begin(CFG.staSSID, CFG.staPASS);

    unsigned long t0 = millis();
    const unsigned long timeoutMs = 15000; // 15 s na połączenie

    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();

    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      Serial.println(F("[WiFi] STA CONNECTED"));
      Serial.print  (F("[WiFi] STA IP:   "));
      Serial.println(WiFi.localIP());
      Serial.print  (F("[WiFi] STA MAC:  "));
      Serial.println(WiFi.macAddress());
      Serial.print  (F("[WiFi] STA RSSI: "));
      Serial.println(WiFi.RSSI());
    } else {
      Serial.print(F("[WiFi] STA FAILED, status="));
      Serial.println((int)st);
      Serial.println(F("[WiFi] Staying AP-only (STA will retry w tle przy reconnect)"));
    }
  } else {
    Serial.println(F("[WiFi] STA disabled or SSID empty in config"));
  }

  // ─────────────────────────────────────────────
  // FS + preset profili + statyczne pliki (CSS, logo)
  // ─────────────────────────────────────────────
  LittleFS.begin();
  presetsLoadFS();

  // Statyczne pliki z LittleFS – jeśli istnieją, będą serwowane pod /style.css i /logo.png
  if (LittleFS.exists("/style.css")) {
    server.serveStatic("/style.css", LittleFS, "/style.css");
  }
  if (LittleFS.exists("/logo.png")) {
    server.serveStatic("/logo.png", LittleFS, "/logo.png");
  }


  // ROUTES – główne
  server.on("/",            HTTP_GET,     handleIndex);
  server.on("/ping",        HTTP_GET,     handlePing);
  server.on("/state",       HTTP_GET,     handleState);
  server.on("/status.json", HTTP_GET,     handleStatusJson);
  server.on("/pins",        HTTP_GET,     handlePins);

  server.on("/start",       HTTP_GET,     handleStart);
  server.on("/stop",        HTTP_GET,     handleStop);
  server.on("/set",         HTTP_ANY,     handleSet);

  server.on("/presets",     HTTP_GET,     handlePresets);
  server.on("/preset",      HTTP_ANY,     handlePresetSet);
  server.on("/presets/save",HTTP_POST,    handlePresetsSave);

  server.on("/mode",        HTTP_ANY,     handleMode);
  server.on("/mode/dynamic",HTTP_POST,    handleModeDynamic);
  server.on("/mode/profile",HTTP_POST,    handleModeProfile);

  // ręczne przełączanie kroków profilu
  server.on("/profile/next", HTTP_POST,   handleProfileNext);
  server.on("/profile/prev", HTTP_POST,   handleProfilePrev);

  server.on("/export",      HTTP_GET,     handleExport);

  // Konfiguracja WiFi / MQTT (stuby + .html pod linki z UI)
  server.on("/wifi",        HTTP_GET,     handleWifiPage);
  server.on("/wifi.html",   HTTP_GET,     handleWifiPage);
  server.on("/wifi/save",   HTTP_POST,    handleWifiSave);

  server.on("/mqtt",        HTTP_GET,     handleMqttPage);
  server.on("/mqtt.html",   HTTP_GET,     handleMqttPage);
  server.on("/mqtt/save",   HTTP_POST,    handleMqttSave);

  // OTA update przez /update (ESP8266HTTPUpdateServer)
  // login: "admin", hasło: "kyrson"  → ZMIEŃ NA SWOJE
  httpUpdater.setup(&server, "/update", "admin", "kyrson");

  // OPTIONS / CORS
  auto opt204 = [](){ sendCORS(); server.send(204); };

  server.on("/state",         HTTP_OPTIONS, opt204);
  server.on("/status.json",   HTTP_OPTIONS, opt204);
  server.on("/pins",          HTTP_OPTIONS, opt204);
  server.on("/start",         HTTP_OPTIONS, opt204);
  server.on("/stop",          HTTP_OPTIONS, opt204);
  server.on("/set",           HTTP_OPTIONS, opt204);
  server.on("/presets",       HTTP_OPTIONS, opt204);
  server.on("/preset",        HTTP_OPTIONS, opt204);
  server.on("/presets/save",  HTTP_OPTIONS, opt204);

  server.on("/mode",          HTTP_OPTIONS, opt204);
  server.on("/mode/dynamic",  HTTP_OPTIONS, opt204);
  server.on("/mode/profile",  HTTP_OPTIONS, opt204);

  server.on("/profile/next",  HTTP_OPTIONS, opt204);
  server.on("/profile/prev",  HTTP_OPTIONS, opt204);

  server.on("/export",        HTTP_OPTIONS, opt204);

  server.on("/wifi",          HTTP_OPTIONS, opt204);
  server.on("/wifi/save",     HTTP_OPTIONS, opt204);
  server.on("/mqtt",          HTTP_OPTIONS, opt204);
  server.on("/mqtt/save",     HTTP_OPTIONS, opt204);

  // Captive portal
  auto redirectToRoot = [](){
    sendCORS();
    server.sendHeader("Location","/",true);
    server.send(302,"text/plain","");
  };
  server.on("/generate_204", HTTP_ANY, redirectToRoot);
  server.on("/connectivitycheck.gstatic.com/generate_204", HTTP_ANY, redirectToRoot);
  server.on("/hotspot-detect.html", HTTP_ANY, [](){
    sendCORS();
    server.send(200,"text/html","<html><head><meta http-equiv='refresh' content='0;url=/'/></head><body>OK</body></html>");
  });
  server.on("/ncsi.txt", HTTP_ANY, redirectToRoot);
  server.on("/fwlink",   HTTP_ANY, redirectToRoot);

  // Rejestracja dodatkowych widoków (piny + profil)
  registerConfigRoutes(server);

  server.onNotFound(redirectToRoot);
  server.begin();
  Serial.println(F("[HTTP] WebServer started"));
}

void webserverLoop() {
  DNS.processNextRequest();
  server.handleClient();
}

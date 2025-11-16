/***************************************************************************************
 * FILE: src/web_status.cpp
 * DESCRIPTION: Endpoints statusowe dla UI + sterowanie RUN (start/stop)
 * LAST MODIFIED: 2025-11-09 17:10 (Europe/Warsaw)
 ***************************************************************************************/
#include <Arduino.h>
#include <ESP8266WebServer.h>

// ── import ze sterowania
extern bool   RUN_ACTIVE;
extern bool   SAFETY_TRIP;
extern bool   HEATER_ON;
extern bool   SENSOR_OK;
extern double KILN_TEMP;
extern double PID_SET;
extern double PID_OUT;
extern volatile uint32_t RUN_REV;
extern double INT_TEMP;


extern void runStart();
extern void runStop();

// ── pomocnicze: proste budowanie JSON bez zależności
static String jsonEscape(const String& s){
  String o; o.reserve(s.length()+4);
  for (size_t i=0;i<s.length();++i){
    char c=s[i];
    if      (c=='\"') o += "\\\"";
    else if (c=='\\') o += "\\\\";
    else if (c=='\b') o += "\\b";
    else if (c=='\f') o += "\\f";
    else if (c=='\n') o += "\\n";
    else if (c=='\r') o += "\\r";
    else if (c=='\t') o += "\\t";
    else o += c;
  }
  return o;
}

static String buildStatusJson(){
  // duty ≈ PID_OUT (%), heating=HEATER_ON (SSR w oknie)
  String j;
  j.reserve(256);
  j += F("{");
  j += F("\"rev\":");         j += String((uint32_t)RUN_REV);      j += F(",");
  j += F("\"run\":");         j += (RUN_ACTIVE?F("true"):F("false")); j += F(",");
  j += F("\"trip\":");        j += (SAFETY_TRIP?F("true"):F("false")); j += F(",");
  j += F("\"heating\":");     j += (HEATER_ON?F("true"):F("false"));  j += F(",");
  j += F("\"sensor_ok\":");   j += (SENSOR_OK?F("true"):F("false"));  j += F(",");
  j += F("\"temp_c\":");      j += isnan(KILN_TEMP)?F("null"):String(KILN_TEMP,1); j += F(",");
  j += F("\"temp_int_c\":");  j += isnan(INT_TEMP)?F("null"):String(INT_TEMP,1);   j += F(",");
  j += F("\"set_c\":");       j += String(PID_SET,1);                               j += F(",");
  j += F("\"duty_pct\":");    j += String(PID_OUT,1);

  return j;
}

// ── Rejestracja tras. Wywołaj w setupie po .begin()
void registerStatusRoutes(ESP8266WebServer& server)
{
  // JSON statusu do odpytywania przez UI
  server.on(F("/status.json"), HTTP_GET, [&server](){
    String body = buildStatusJson();
    // opcjonalnie ETag/Cache-Control, ale prosto:
    server.sendHeader(F("Cache-Control"), F("no-store, no-cache, must-revalidate, proxy-revalidate"));
    server.send(200, F("application/json"), body);
  });

  // Start
  server.on(F("/run/start"), HTTP_POST, [&server](){
    runStart();
    server.send(200, F("text/plain"), F("OK"));
  });

  // Stop
  server.on(F("/run/stop"), HTTP_POST, [&server](){
    runStop();
    server.send(200, F("text/plain"), F("OK"));
  });
}


/***************************************************************************************
 * FILE: src/network.cpp
 * LAST MODIFIED: 2025-11-09 03:55 (Europe/Warsaw)
 ***************************************************************************************/
#include "network.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#ifdef ARDUINO_ARCH_ESP8266
  #include <ESP8266mDNS.h>
#endif
#include <PubSubClient.h>

static const byte DNS_PORT=53;
static DNSServer dns;
static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static const char* AP_SSID = "KilnCtrl-AP";
static const char* AP_PASS = "kiln-ap-123";

static void mqttEnsure(){
  if(!CFG.mqttEnabled) return;
  if(!mqtt.connected()){
    if(strlen(CFG.mqttHost)==0) return;
    mqtt.setServer(CFG.mqttHost, CFG.mqttPort);
    String cid = String("kiln-")+String(ESP.getChipId(),HEX);
    bool ok = strlen(CFG.mqttUser)? mqtt.connect(cid.c_str(), CFG.mqttUser, CFG.mqttPass)
                                  : mqtt.connect(cid.c_str());
    if(!ok) return;
  }
}

static void mqttLoop(){
  if(!CFG.mqttEnabled) return;
  mqttEnsure(); if(!mqtt.connected()) return; mqtt.loop();
}

void netInit(){
  // WiFi / AP / STA są konfigurowane w webserver.cpp (webserverSetup).
  // Tutaj zostawiamy tylko warstwę MQTT.
  // Na razie nic nie musimy robić – mqttEnsure() samo ustawi serwer przy pierwszym użyciu.
}


void netLoop(){
  // DNS/captive portal obsługuje webserverLoop() w webserver.cpp
  mqttLoop();
}

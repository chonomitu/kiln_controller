#pragma once
#include <ESP8266WebServer.h>
void registerConfigRoutes(ESP8266WebServer& srv);
bool loadProfileFS();

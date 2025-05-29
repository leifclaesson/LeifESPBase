#pragma once

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#endif

void WakeOnLan(WiFiUDP & udp, const char * mac);



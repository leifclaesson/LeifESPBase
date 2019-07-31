#pragma once
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#endif
#include <ArduinoOTA.h>
#include "LeifESPBaseMain.h"
#include "LeifESPBaseWOL.h"

extern const char * wifi_ssid;
extern const char * wifi_key;

#ifdef NO_SERIAL_DEBUG
#define csprintf(...) { if(telnetClients) telnetClients.printf(__VA_ARGS__ ); }
#else
#define csprintf(...) { Serial.printf(__VA_ARGS__ ); if(telnetClients) telnetClients.printf(__VA_ARGS__ ); }
#endif

#if defined(ARDUINO_ARCH_ESP8266)
extern ESP8266WebServer server;
#else
extern WebServer server;
#endif
extern WiFiClient telnetClients;

const char * GetHostName();
const char * GetHeadingText();


void LeifSetupBegin();
void LeifSetupEnd();

void LeifLoop();

void LeifHtmlMainPageCommonHeader(String & string);

bool Interval100();
bool Interval250();
bool Interval500();
bool Interval1000();
bool Interval10s();

void LeifSetStatusLedPin(int iPin);	//-1 to disable
void LeifSetInvertLedBlink(bool bInvertLed);
bool LeifGetInvertLedBlink();


void LeifSecondsToUptimeString(String & string,unsigned long ulSeconds);

void LeifUptimeString(String & string);

unsigned long seconds();

int HttpRequest(const char * url, int retries=3);
int HttpPost(String & url, String & payload, int retries=3);
byte chartohex(char asciichar);


#ifdef USE_HOMIE

#include <LeifHomieLib.h>

extern HomieDevice homie;

void LeifPublishMQTT(const char* topic, const char* payload, bool retain);

#endif

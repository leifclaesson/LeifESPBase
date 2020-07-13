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
#ifndef NO_OTA
#include <ArduinoOTA.h>
#endif
#include "LeifESPBaseMain.h"
#include "LeifESPBaseWOL.h"
#include "LeifESPBaseAP.h"

extern const char * wifi_ssid;
extern const char * wifi_key;
extern const char * backup_ssid;
extern const char * backup_key;

#define HAS_CSPRINTF

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

#define LeifUpdateCompileTime() { extern char szExtCompileDate[]; sprintf(szExtCompileDate,"%s %s",__DATE__,__TIME__); }

void LeifSetupBSSID(const char * pszBSSID, int ch, const char * pszAccessPointIP);
IPAddress LeifGetAccessPointIP();

void LeifSetupConsole();	//can be called before LeifSetupBegin

void LeifSetupBegin();
void LeifSetupEnd();

bool IsLeifSetupBeginDone();

void LeifLoop();

void LeifHtmlMainPageCommonHeader(String & string);

bool Interval50();
bool Interval100();
bool Interval250();
bool Interval500();
bool Interval1000();
bool Interval10s();

void LeifSetStatusLedPin(int iPin);	//-1 to disable
void LeifSetAllowFadeLed(bool bAllowFade, int analogWriteBits);
void LeifSetInvertLedBlink(bool bInvertLed);
bool LeifGetInvertLedBlink();

void LeifSetAllowWifiConnection(bool bAllow);
bool LeifGetAllowWifiConnection();

bool IsNewWifiConnection();	//returns true ONCE after a new wifi connection has been established

void LeifSecondsToShortUptimeString(String & string,unsigned long ulSeconds);


void LeifSecondsToUptimeString(String & string,unsigned long ulSeconds);

void LeifUptimeString(String & string);

String LeifGetCompileDate();

String LeifGetWifiStatus();

unsigned long seconds();
const unsigned long * pSeconds();

unsigned long secondsWiFI();
const unsigned long * pSecondsWiFi();

int HttpRequest(const char * url, int retries=3);
int HttpPost(String & url, String & payload, int retries=3);
byte chartohex(char asciichar);

uint32_t LeifGetTotalWifiConnectionAttempts();


String MacToString(const uint8_t * mac);

String GetArgument(const String & input, const char * argname);


bool LeifIsBSSIDConnection();	//returns true if we're connected an access point configured by BSSID+CH


#ifdef USE_HOMIE

#include <LeifHomieLib.h>

extern HomieDevice homie;

void LeifPublishMQTT(const char* topic, const char* payload, bool retain);

#endif

#pragma once
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#endif
#ifndef NO_OTA
#include <ArduinoOTA.h>
#endif

#ifdef NO_GLOBAL_SERIAL
#ifndef NO_SERIAL_DEBUG
#define NO_SERIAL_DEBUG
#endif
#endif


#include "LeifESPBaseMain.h"
#include "LeifESPBaseWOL.h"
#include "LeifESPBaseAP.h"




extern const char * wifi_ssid;
extern const char * wifi_key;
extern const char * backup_ssid;
extern const char * backup_key;

#define HAS_CSPRINTF

class ScrollbackBuffer : public Print
{
public:

	void alloc(uint16_t bytes);

    size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    const char * dataFirst();
    size_t sizeFirst();
    const char * dataSecond();
    size_t sizeSecond();

private:

	char * buf=NULL;

	uint16_t bufsize=0;
	uint16_t kept=0;
	uint16_t idx=0;


};






#ifdef NO_SERIAL_DEBUG
#ifdef USE_SERIAL1_DEBUG
#define csprintf(...) { Serial1.printf(__VA_ARGS__ ); Serial1.flush(); if(telnetClients) telnetprint.printf(__VA_ARGS__); scrollbackBuffer.printf(__VA_ARGS__); }
#else
#define csprintf(...) { if(telnetClients) telnetprint.printf(__VA_ARGS__); scrollbackBuffer.printf(__VA_ARGS__); }
#endif
#else
#define csprintf(...) { Serial.printf(__VA_ARGS__ ); if(telnetClients) telnetprint.printf(__VA_ARGS__); scrollbackBuffer.printf(__VA_ARGS__); }
#endif

#if defined(ARDUINO_ARCH_ESP8266)
extern ESP8266WebServer server;
#else
extern WebServer server;
#endif
extern WiFiClient telnetClients;

extern ScrollbackBuffer scrollbackBuffer;

const char * GetHostName();
const char * GetHeadingText();

#define LeifUpdateCompileTime() { extern char szExtCompileDate[]; sprintf(szExtCompileDate,"%s %s",__DATE__,__TIME__); extern String strProjectName; strProjectName=__FILE__; }

void LeifSetProjectName(const char * szProjectName);

void LeifSetupBSSID(const char * pszBSSID, int ch, const char * pszAccessPointIP);
IPAddress LeifGetAccessPointIP();

void LeifSetupConsole(uint16_t _scrollback_bytes=0);	//can be called before LeifSetupBegin

void LeifSetupBegin();
void LeifSetupEnd();

bool IsLeifSetupBeginDone();

void LeifLoop();

void LeifHtmlMainPageCommonHeader(String & string);

void LeifScheduleRestart(uint32_t ms);
void LeifScheduleReconnect(uint32_t ms);

bool Interval50();
bool Interval100();
bool Interval250();
bool Interval500();
bool Interval1000();
bool Interval10s();

void LeifSetStatusLedPin(int iPin);	//-1 to disable
void LeifSetAllowFadeLed(bool bAllowFade, int analogWriteBits);
void LeifSetLedBrightness(int scale);	//0-3, 1 is default

void LeifSetInvertLedBlink(bool bInvertLed);
bool LeifGetInvertLedBlink();

void LeifSetSuppressLedWrite(bool bSuppress);

void LeifSetAllowWifiConnection(bool bAllow);
bool LeifGetAllowWifiConnection();

void LeifSetAllowSerialCommands(bool bAllow);
bool LeifGetAllowSerialCommands();

bool IsNewWifiConnection();	//returns true ONCE after a new wifi connection has been established

void LeifSecondsToShortUptimeString(String & string,unsigned long ulSeconds);


void LeifSecondsToUptimeString(String & string,unsigned long ulSeconds);

void LeifUptimeString(String & string);

String LeifGetCompileDate();
String LeifGetVersionText();

void LeifSetVersionText(const char * szVersion);

String LeifGetWifiStatus();

uint32_t seconds();
const uint32_t * pSeconds();

uint32_t secondsWiFI();
const uint32_t * pSecondsWiFi();

int HttpRequest(const char * url, int retries=3);
int HttpPost(String & url, String & payload, int retries=3);
byte chartohex(char asciichar);

uint32_t LeifGetTotalWifiConnectionAttempts();


String MacToString(const uint8_t * mac);
bool ParseMacAddress(const char * pszMAC, uint8_t * cMacOut);

String GetArgument(const String & input, const char * argname);


bool LeifIsBSSIDConnection();	//returns true if we're connected an access point configured by BSSID+CH



#pragma once
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#include "LeifWebServer.h"
#endif
#ifndef NO_OTA
#include <ArduinoOTA.h>
#endif

#if defined(NO_GLOBAL_SERIAL) | defined(NO_GLOBAL_INSTANCES)
#ifndef NO_SERIAL_DEBUG
#define NO_SERIAL_DEBUG
#endif
#endif


#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
#include "..\ethernet_setup.h"
#include <ETH.h>
#endif


#include "LeifESPBaseMain.h"
#include "LeifESPBaseWOL.h"
#include "LeifESPBaseAP.h"




extern const char * wifi_ssid;
extern const char * wifi_key;
extern const char * backup_ssid;
extern const char * backup_key;

#define HAS_CSPRINTF

//The console's one buffer, and since 2026-09-03 it is not just scrollback: it is the
//DELIVERY QUEUE every telnet seat reads from. Each seat keeps its own position in this
//stream (LeifESPBaseMain.cpp, uTelnetSeatSent) and is fed from here at whatever rate its
//socket will take, so a print costs no socket time at all and a slow viewer holds nobody
//up. See the block above TelnetDrainSeats() for the whole arrangement.
//
//Positions are ABSOLUTE stream offsets (Head() counts every byte ever written), not ring
//indices, so a seat's position stays meaningful across any number of wraps.
//
//⛔ Bytes go in with CRLF line endings, because what comes out goes straight down a
//telnet socket with no pass over it. write() does the LF -> CRLF conversion on the way in.
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

    //--- the stream view, used by the telnet seats ---
    uint32_t Head() const { return total; }				//absolute offset one past the newest byte
    uint32_t Oldest() const { return total-kept; }		//absolute offset of the oldest byte still held
    uint16_t Capacity() const { return bufsize; }

    //Hands back a pointer to the bytes at absolute offset pos and how many of them are
    //contiguous from there (the ring wraps, so a caller asking for a long run gets it in
    //at most two goes). Returns 0 when pos has already fallen off the tail, or is at the
    //head with nothing new behind it.
    size_t Peek(uint32_t pos, const char * & data) const;

private:

	void RawAppend(const uint8_t * buffer, size_t size);

	char * buf=NULL;

	uint16_t bufsize=0;
	uint16_t kept=0;
	uint16_t idx=0;
	uint32_t total=0;	//every byte ever written, so a seat's position never has to chase a wrap
	char lastch=0;		//so an already-CRLF source does not come out as CR CR LF


};






//Console seats. The scrollback is one shared buffer and does NOT scale with this; what
//costs per seat is the TCP connection. //Leif, 2026-08-23: three on the bulbs, five on ESP32.
//Amended the same day, once the ceiling below was measured -- //Leif, 2026-08-23: "the ESP32
//1.0.6 has a three client limit, doesn't it? So let's do five clients if it's the version 3
//SDK. Otherwise, let's do two as well. So we leave one for something else." and "we can do
//two on the ESP8266."
//
//What that leaves one for: lwIP hands out a FIXED table of descriptors for the whole device,
//and every UDP endpoint, every listening server and every ACCEPTED connection takes one. A
//Lightbulb already spends 7 of them -- http listen, telnet listen, the MQTT connection,
//ArduinoOTA's UDP, ESP1588's two PTP UDPs, and the RTA multicast UDP that the first rta() in
//a deck creates and never frees. Against CONFIG_LWIP_MAX_SOCKETS that is:
//    core 1.0.6 (IDF v3.3.5)  10 - 7 = 3 spare  -> 2 seats, one left for http / OTA
//    core 3.x   (IDF v5.4)    16 - 7 = 9 spare  -> 5 seats, four to spare
//so the ESP32 is TIGHTER than it looks on 1.0.6, not roomier than the bulbs. Anything older
//than core 3 falls to 2; the gate is the version macro, and 1.0.6 does not define it at all.
//Measured 2026-08-23 on the bench ESP32 172.22.28.123, core 1.0.6, with
//misc/claude/rigs/zemismart-bench/socketbudget.py: the board grants exactly 3 seats, and
//holding the third makes its own front page return zero bytes until the seat is released.
//The ESP8266 splits its pools instead (MEMP_NUM_TCP_PCB=5 active connections, listeners and
//UDP counted separately) and is NOT measured on hardware yet.
//Leif, 2026-08-30: the multi-client console has been in use and works. That retires the
//"has not been run on hardware" caveat the two commits that built it were committed under.
//The ESP8266 pool split just above is a separate claim (a number, not "does it work") and
//is still unmeasured.
//
//Setting this past the socket budget does not fail loudly: lwip_accept() runs out of
//descriptors and WiFiServer::hasClient() just returns false, so the accept path below never
//sees the newcomer and cannot tell it the seats are full. lwIP has already completed the
//handshake by then, so the client believes it connected and is then closed without a word.
#ifndef LEIF_TELNET_MAX_CLIENTS
#if defined(ARDUINO_ARCH_ESP8266)
#define LEIF_TELNET_MAX_CLIENTS 2
#elif defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
#define LEIF_TELNET_MAX_CLIENTS 5
#else
#define LEIF_TELNET_MAX_CLIENTS 2
#endif
#endif

//The console. This was a macro until 2026-09-03, and a macro pasted its format string once per
//printf in its body -- so PSTR() declared a fresh copy each time and every console line sat in
//flash two or three times over. //Leif, 2026-09-03: "could you rework that macro so that it
//calls a function call? That will be safer and more stable for the future."
//One function, one copy, both sinks fed from a single formatting pass. WHICH sinks is decided in
//LeifESPBaseMain.cpp, under the same NO_SERIAL_DEBUG / USE_SERIAL1_DEBUG switches that used to
//pick between the three macro bodies.
//⛔ PGM_P and vsnprintf_P, not const char*/vsnprintf: the format string lives in flash, and only
//the _P family reads it word-at-a-time. Plain printf works, but takes an unaligned-access
//exception per byte on the hottest logging path there is.
//⛔ Keep the format attribute. Print::printf carried one, so argument type-checking is free
//today and vanishes without a sound the moment the macro does.
void csprintf(PGM_P fmt, ...) __attribute__((format(printf, 1, 2)));

#if defined(ARDUINO_ARCH_ESP8266)
extern ESP8266WebServer server;
#else
extern LeifWebServer server;
#endif
extern WiFiClient telnetClients[LEIF_TELNET_MAX_CLIENTS];
extern uint8_t telnetClientCount;	//seats currently occupied; kept current by LeifLoop

extern ScrollbackBuffer scrollbackBuffer;

const char * GetHostName();
const char * GetHeadingText();

#define LeifUpdateCompileTime() { extern char szExtCompileDate[]; sprintf(szExtCompileDate,"%s %s",__DATE__,__TIME__); extern String strProjectName; strProjectName=__FILE__; }
#define LeifUpdateCompileTime_VersionString(a) { extern char szExtCompileDate[]; sprintf(szExtCompileDate,"%s %s %s",__DATE__,__TIME__,a); extern String strProjectName; strProjectName=__FILE__; }

void LeifSetProjectName(const char * szProjectName);
const String & LeifGetProjectName();

void LeifSetupBSSID(const char * pszBSSID, int ch, const char * pszAccessPointIP);
IPAddress LeifGetAccessPointIP();

void LeifForceWifiReconnect();	//runtime "reconnect without reboot": re-scan all channels and associate to the strongest AP

void LeifServiceBackground();	//pump background services (MQTT keepalive) around a long blocking op such as a web page render; no-op unless the linked MQTT lib registered a handler
void LeifSetServiceBackgroundCallback(void (*fn)());	//called by the linked MQTT lib (lsm.Loop / homie.Loop) to register its pump

void LeifSetupConsole(uint16_t _scrollback_bytes=0);	//can be called before LeifSetupBegin

void LeifSetupBegin();
void LeifSetupEnd();

bool IsLeifSetupBeginDone();

void LeifLoop();

void LeifHtmlMainPageCommonHeader(String & string);

void LeifScheduleRestart(uint32_t ms);
void LeifScheduleReconnect(uint32_t ms);
void LeifScheduleForceReconnect(uint32_t ms);	//deferred full reconnect (re-scan for strongest AP); safe to call from a web handler -- fires after the response flushes

bool Interval50();
bool Interval100();
bool Interval250();
bool Interval500();
bool Interval1000();
bool Interval10s();

void LeifSetStatusLedPin(int iPin);	//-1 to disable
void LeifSetAllowFadeLed(bool bAllowFade, int analogWriteBits);
void LeifSetLedBrightness(int scale);	//0-3, 1 is default

void LeifSetStatusLED_Override(bool bOverride, int value);

void LeifSetInvertLedBlink(bool bInvertLed);
bool LeifGetInvertLedBlink();

void LeifSetSuppressLedWrite(bool bSuppress);

void LeifSetAllowWifiConnection(bool bAllow);
bool LeifGetAllowWifiConnection();

void LeifSetAllowSerialCommands(bool bAllow);
bool LeifGetAllowSerialCommands();

bool IsNewWifiConnection();	//returns true ONCE after a new wifi connection has been established

bool IsWiFiConnected();

void LeifSecondsToShortUptimeString(String & string,unsigned long ulSeconds);


void LeifSecondsToUptimeString(String & string,unsigned long ulSeconds);

void LeifUptimeString(String & string);
String LeifGetResetReasonString();	//cross-arch last-reset cause: "POWERON"/"BROWNOUT"/"PANIC"/"SW"/"EXT"(external reset pin, e.g. STM8 watchdog)/"TASK_WDT"/"INT_WDT"/"DEEPSLEEP" (ESP32), or ESP.getResetReason() (ESP8266)

String LeifGetCompileDate();
String LeifGetVersionText();

void LeifSetVersionText(const char * szVersion);

typedef std::function<void(void)> fn_LeifESPBaseInterimCallback;
void LeifSetInterimCallback(fn_LeifESPBaseInterimCallback cb);	//this function will get called periodically during lengthy operations such as sending the telnet scrollback buffer

typedef std::function<void(void)> fn_LeifESPBaseOtaTooLargeCallback;
void LeifSetOtaTooLargeCallback(fn_LeifESPBaseOtaTooLargeCallback cb);	//called when an OTA is refused because the image is larger than the inactive app slot. Refused by Update::begin() BEFORE any byte is written, so there is no partial image -- and it is the one moment the unit knows for certain its partition layout is too small for the firmware being pushed. Default none: the OTA fails exactly as it always did. A handler may repartition and restart, in which case it never returns.

String LeifGetWifiStatus();

uint32_t seconds();
const uint32_t * pSeconds();

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)

uint32_t secondsEthernet();
const uint32_t * pSecondsEthernet();

bool LeifIsEthernetInitialized();

#endif

uint32_t secondsWiFi();
const uint32_t * pSecondsWiFi();

int HttpRequest(const char * url, int retries=3);
int HttpPost(String & url, String & payload, int retries=3);
byte chartohex(char asciichar);

uint32_t LeifGetTotalWifiConnectionAttempts();


String MacToString(const uint8_t * mac);
bool ParseMacAddress(const char * pszMAC, uint8_t * cMacOut);

String GetArgument(const String & input, const char * argname);


bool LeifIsBSSIDConnection();	//returns true if we're connected an access point configured by BSSID+CH
void LeifSetBSSIDSessionOnly(bool bSessionOnly);	//mark the active BSSID pin as temporary (runtime pick) vs saved (from config)
bool LeifIsBSSIDSessionOnly();	//true if the active pin is a temporary runtime pick not written to config


#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
bool HasEthernetIP();
bool HasEthernetLink();
#endif


#pragma once

#include <Arduino.h>
#include <functional>

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#endif

enum eCommandLineSource
{
	eCommandLineSource_Telnet,
	eCommandLineSource_Serial,
};

typedef std::function<void(const String &, eCommandLineSource)> LeifCommandCallback;

typedef std::function<void(const char *)> LeifOnShutdownCallback;
void LeifRegisterOnShutdownCallback(LeifOnShutdownCallback cb);
void LeifRegisterCommandCallback(LeifCommandCallback cb);
void LeifSetMaxCommandLength(uint16_t max_chars);

typedef std::function<const char * (const String &)> LeifGetWiFiAPName;
void LeifRegisterGetWiFiAPName(LeifGetWiFiAPName fn);

//The other half of the WiFi link: what the ACCESS POINT hears US at, which we can never measure
//ourselves (WiFi.RSSI() is only our own side). Something outside this lib has to be told it and
//register a way to read it back -- the main-page status render shows it next to our own RSSI when
//a callback is registered and returns a non-empty string. nullptr on projects that don't do this.
//The string is emitted into the page as-is, so the provider owns its markup and its escaping.
typedef std::function<const char * ()> LeifGetApRxText;
void LeifRegisterGetApRxText(LeifGetApRxText fn);

//True if the WiFi-scan picker has a persist callback registered (i.e. a runtime "Save" can write config.txt).
//Registered by WiFiScan's InitWifiScan(); nullptr on projects without a scan page, so the main-page Save link
//is only offered where it can actually work.
typedef std::function<bool ()> LeifWifiScanPersistAvailable;
void LeifRegisterWifiScanPersistAvailable(LeifWifiScanPersistAvailable fn);

enum eHttpMainTable
{
	eHttpMainTable_BeforeFirstRow,
	eHttpMainTable_AfterLastRow,
};

class String;
typedef std::function<void(String &, eHttpMainTable)> LeifHttpMainTableCallback;
void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb);

class TelnetClientPrint : public Print
{
public:
	TelnetClientPrint(WiFiClient * pDestination)
	{
		this->pDest=pDestination;
	}

	WiFiClient * pDest;		//the seat array, LEIF_TELNET_MAX_CLIENTS long

	//⛔ This is now the ONLY way to use this class, and it must always be >=0 while writing.
	//Ordinary console output does not come through here at all any more -- it goes into
	//scrollbackBuffer and the seats read themselves out of that. What is left is the welcome
	//banner, which is the one thing that is addressed to a single newcomer and must therefore
	//NOT go into the shared stream. A write with iOnlySeat still -1 is a bug, and is dropped.
	int8_t iOnlySeat=-1;

	//The whole banner shares ONE budget, not one per write. See the note on fanout().
	uint32_t ulBannerDeadline=0;

	uint32_t cbcounter=0;

    size_t dbg(const uint8_t *buffer, size_t size);

	size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size);

private:
	void fanout(const uint8_t * buffer, size_t size);
};

extern TelnetClientPrint telnetprint;



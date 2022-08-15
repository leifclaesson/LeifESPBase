#pragma once

#include <Arduino.h>
#include <functional>

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

enum eHttpMainTable
{
	eHttpMainTable_BeforeFirstRow,
	eHttpMainTable_AfterLastRow,
};

class String;
typedef std::function<void(String &, eHttpMainTable)> LeifHttpMainTableCallback;
void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb);

class WiFiClient;
class TelnetClientPrint : public Print
{
public:
	TelnetClientPrint(WiFiClient * pDestination)
	{
		this->pDest=pDestination;
	}

	WiFiClient * pDest;

    size_t dbg(const uint8_t *buffer, size_t size);

	size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size);
};

extern TelnetClientPrint telnetprint;

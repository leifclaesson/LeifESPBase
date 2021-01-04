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


enum eHttpMainTable
{
	eHttpMainTable_BeforeFirstRow,
	eHttpMainTable_AfterLastRow
};

class String;
typedef std::function<void(String &, eHttpMainTable)> LeifHttpMainTableCallback;
void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb);

#pragma once

#include <functional>

typedef std::function<void(const char *)> LeifOnShutdownCallback;
void LeifRegisterOnShutdownCallback(LeifOnShutdownCallback cb);


enum eHttpMainTable
{
	eHttpMainTable_BeforeFirstRow,
	eHttpMainTable_InsideFirstRow,
	eHttpMainTable_InsideFirstRowEnd,
	eHttpMainTable_InsideLastRow,
	eHttpMainTable_InsideLastRowEnd,
	eHttpMainTable_AfterLastRow
};

class String;
typedef std::function<void(String &, eHttpMainTable)> LeifHttpMainTableCallback;
void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb);

String MacToString(const uint8_t * mac);

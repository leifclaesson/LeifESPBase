#pragma once

#include <Arduino.h>

//Firmware update over the network is CLOSED until somebody who holds the web password opens it,
//and it closes itself again a few minutes later.
//
//⛔ What this is NOT fixing. //Leif, 2026-10-02: "this password has never changed. It's not a
//secret. The password isn't for security. The password is to avoid sending the wrong firmware
//by accident." Taken at his word, the OTA password is a wrong-firmware guard and nothing else --
//so firmware update had no authentication at all, and anything that could reach the board could
//push arbitrary firmware to it. Authentication moves here, to the web password, which IS a
//per-device secret. The OTA password keeps doing the only job it was ever meant to do and may
//stay non-secret and unchanged forever.
//
//⛔ And this is still only ACCESS to the updater. What gets loaded is verified by nothing; that
//needs signed images and is a separate decision. See misc\docs\plans\babelfish-cra-compliance-plan.md.
//
//A window rather than a toggle, because a toggle somebody enables once and forgets just
//relocates the problem. Same idiom as the commissioning access point, which is opened by hand
//and closes itself after 900 s (Babelfish_Model42.cpp).
//
//While it is closed the OTA service is not started at all, so UDP port 8266 is unbound -- there
//is nothing on the network to answer, not merely something that refuses.
//
//⛔ ESP32 only, and that is the same boundary the web password has: the authentication gate
//lives in LeifWebServer, an ESP32 class, while ESP8266 keeps ESP8266WebServer. An enable
//endpoint with no gate in front of it is a free pass for anyone on the network, which is worse
//than what those products have now -- so they are unchanged and still start OTA at boot.
//
//Serial flashing is unaffected, correctly: that one already costs physical access.

//How long one enable lasts. A fleet script authenticates, enables and pushes in one run, so
//this only has to cover a human who opens it and then goes to find the file.
#ifndef LEIF_OTA_WINDOW_SECONDS
#define LEIF_OTA_WINDOW_SECONDS 900
#endif

#if defined(ARDUINO_ARCH_ESP32) && !defined(NO_OTA) && !defined(NO_OTA_WINDOW)
#define LEIF_HAS_OTA_WINDOW 1
#endif

#ifdef LEIF_HAS_OTA_WINDOW

//Registers /otaenable, /otadisable and the "ota" console command. ⛔ Must be called after
//LeifWebAuthBegin(), or the endpoints go up ungated.
void LeifOtaWindowBegin();

//Called from LeifLoop(). This is what makes the window self-closing.
void LeifOtaWindowLoop();

bool LeifOtaWindowIsOpen();
uint32_t LeifOtaWindowSecondsLeft();

//Opening is what the authenticated endpoint and the SERIAL console command do. ⛔ Never reachable
//from telnet: the console is open on the LAN, so an enable there would hand the network exactly
//the bypass this file exists to close.
void LeifOtaWindowOpen();
void LeifOtaWindowClose(const char * pszWhy);

void LeifOtaWindowAppendStatus(String & str);			// one /sysinfo line
void LeifOtaWindowAppendToolsSection(String & str);		// the /tools button

#else

//Compiled out on ESP8266, with NO_OTA, or with NO_OTA_WINDOW. The window reports itself
//permanently open so the one call site that asks -- LeifSetupEnd() deciding whether to start the
//OTA service -- keeps the old always-on behaviour without an #ifdef of its own.
inline void LeifOtaWindowBegin() {}
inline void LeifOtaWindowLoop() {}
inline bool LeifOtaWindowIsOpen() { return true; }
inline uint32_t LeifOtaWindowSecondsLeft() { return 0; }
inline void LeifOtaWindowAppendStatus(String &) {}
inline void LeifOtaWindowAppendToolsSection(String &) {}

#endif

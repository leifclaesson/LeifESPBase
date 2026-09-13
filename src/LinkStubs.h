#pragma once

//---------------------------------------------------------------------------------------------
//The cuts in LinkStubs.cpp are INDEPENDENT -- a project may take any subset, on either
//platform. Only the ESP32 AP/WPA3 pair is inseparable, and that pair enforces itself in
//the .cpp. ⛔ LEIF_LINKSTUBS_ANY is ESP32-only by design: the ESP8266 stubs carry no hit
//counter, so LeifGetLinkStubHits() is the inline zero below on that platform.
//
//  ESP32    NO_HOST_AP + NO_WPA3 (+ NO_SOFT_AP)   84,576 B   SAE/SAE-PK/OWE + AP authenticator
//  ESP32    NO_CAMELLIA_ARIA                       6,543 B   mbedtls ciphers nothing selects
//  ESP32    NO_PPP                                19,351 B   lwIP's PPP / PPPoS stack
//  ESP8266  NO_SOFT_AP                             3,088 B   LwipDhcpServer
//  ESP8266  NO_HOST_AP                             7,397 B   ieee80211_hostap.o (+73 B RAM)
//
//Measured on Lightbulb_ESP32_PCF8574 and Lightbulb (ESP8266), both proven on a bench board.
//Working: misc\docs\plans\lightbulb-esp32-pcf8574-size-audit-plan.md and
//misc\docs\plans\lightbulb-esp8266-strip-plan.md.
//---------------------------------------------------------------------------------------------
#if defined(ARDUINO_ARCH_ESP32) && \
	(defined(NO_HOST_AP) || defined(NO_WPA3) || defined(NO_CAMELLIA_ARIA) || defined(NO_PPP))
#define LEIF_LINKSTUBS_ANY 1
#endif

//Non-zero if a link stub in LinkStubs.cpp was ever entered. Every stub that can be
//reached only when a premise of the cut is wrong bumps this; the ones a station legitimately
//calls do not, so a non-zero value is an alarm and not a tally. Always 0 in a build that did
//not take the cut.
//⛔ A build with no cut gets this INLINE rather than as a symbol in LinkStubs.cpp, because
//such a project has no reason to carry that file -- and none of them does. Sloeber bakes the
//source list into Release\**\subdir.mk and rewrites it only when the IDE opens the project, so
//the /sysinfo call added to LeifESPBaseMain.cpp left every project that had not been opened
//since failing to link on `undefined reference to LeifGetLinkStubHits()` -- none of which
//had asked for anything, and NOT only ESP32 ones: the old declaration carried no arch
//guard, so 102 of the 105 projects that build LeifESPBaseMain.cpp were affected, ESP8266
//included. Found by building DevConfigSkeleton_ESP32 headlessly, 2026-09-13.
//A project that DOES take a cut must have the .cpp in its link; the IDE puts it there by
//itself, and headless builds use bench\add-linkstub-source.py.
#ifdef LEIF_LINKSTUBS_ANY
uint32_t LeifGetLinkStubHits();
#else
inline uint32_t LeifGetLinkStubHits()
{
	return 0;
}
#endif

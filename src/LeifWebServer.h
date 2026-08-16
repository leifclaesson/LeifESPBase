#pragma once

#if defined(ARDUINO_ARCH_ESP32)
#include "WebServer.h"

//A drop-in replacement for the sync WebServer whose response writes can't wedge loopTask
//into a task-watchdog reboot on a marginal WiFi link.
//
//The stock WebServer funnels EVERY send (headers, sendContent / sendContent_P, and the
//SendLargeOutput streaming chunks) through the single protected virtual _currentClientWrite /
//_currentClientWrite_P choke, which calls NetworkClient::write(). On a stalled (send-buffer-full,
//half-open) socket that write loops WIFI_CLIENT_MAX_WRITE_RETRY(10) x select(1 s), so ONE blocked
//page flush starves loopTask >10 s and trips the task WDT -- the field "reboots when reloading the
//HTTP page" crash. The core's _currentClient.setTimeout(HTTP_MAX_SEND_WAIT=5000) is illusory here
//(it sets SO_SNDTIMEO; the stall is that hardcoded retry loop, which ignores it).
//
//We override the one write choke with a bounded writability probe: on a healthy link select
//reports the socket writable immediately, so the write is byte-identical passthrough; when a write
//can't make progress within the budget we stop() the client, which makes the remaining writes in
//that render return 0 instantly (write() checks _connected) so the whole page unwinds well under
//the 10 s WDT. Net: the REQUEST errors/closes, the box stays up.
//
//The lwip select() implementation lives in LeifESPBaseMain.cpp so <lwip/sockets.h> stays out of
//this widely-included header. esp8266 keeps ESP8266WebServer (different stack) -- this class is
//esp32-only.
class LeifWebServer : public WebServer
{
public:
	using WebServer::WebServer;	//inherit the (port) and (addr,port) ctors -- no call-site changes

protected:
	size_t _currentClientWrite(const char *b, size_t l) override;
	size_t _currentClientWrite_P(PGM_P b, size_t l) override;

private:
	size_t BoundedClientWrite(const char *b, size_t l, bool progmem);
};
#endif

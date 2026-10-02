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

	//---- the authentication gate ------------------------------------------------------------
	//These SHADOW the base class's on() rather than override it -- WebServer::on() is not
	//virtual. That is deliberate, and it is what makes the gate impossible to forget: `server`
	//is declared as this concrete type in LeifESPBase.h, so every server.on() in this library
	//and in every project binds to these and comes out wrapped. There is no second way to
	//register a handler, so no call site can publish an unauthenticated endpoint by omission.
	//
	//⛔ Not a style preference. /firmware.bin served the whole running image -- OTA password
	//included, recoverable with `strings` -- to anyone who could reach port 80, for exactly as
	//long as protecting an endpoint was something a call site had to remember to do.
	//
	//A page that genuinely must answer with no credential registers through the base class
	//explicitly (server.WebServer::on(...)) and declares itself with LeifWebAuthDeclarePublic.
	//Only the first-boot setup page does that.
	RequestHandler & on(const Uri &uri, THandlerFunction fn)
	{
		return WebServer::on(uri,Gated(fn));
	}

	RequestHandler & on(const Uri &uri, HTTPMethod method, THandlerFunction fn)
	{
		return WebServer::on(uri,method,Gated(fn));
	}

	//Both handlers are gated. The upload handler runs while the body is still arriving, i.e.
	//BEFORE the main handler, so leaving it open would let an unauthenticated client stream a
	//file into whatever the project does with one.
	RequestHandler & on(const Uri &uri, HTTPMethod method, THandlerFunction fn, THandlerFunction ufn)
	{
		return WebServer::on(uri,method,Gated(fn),Gated(ufn));
	}

	void onNotFound(THandlerFunction fn)
	{
		WebServer::onNotFound(Gated(fn));
	}

	void onFileUpload(THandlerFunction fn)
	{
		WebServer::onFileUpload(Gated(fn));
	}

	//Verifies an RFC 2617 digest response against a stored H1 (MD5 of user:realm:password).
	//A member because it needs the base class's protected request state and parameter parser.
	//Implemented in LeifWebAuth.cpp.
	//
	//bStale comes back true only when the response hashed correctly and the challenge it
	//answered is the part that was wrong -- expired, evicted, or already used with that counter.
	//That is the one case a browser may retry silently, so it decides what the challenge below
	//says.
	bool LeifCheckDigestAuth(const String & strUser, const String & strRealm, const String & strH1, bool & bStale);

	//Issues a fresh digest challenge and remembers it alongside the last few, rather than
	//replacing them the way WebServer::requestAuthentication() does.
	void LeifSendDigestChallenge(const String & strRealm, bool bStale);

protected:
	size_t _currentClientWrite(const char *b, size_t l) override;
	size_t _currentClientWrite_P(PGM_P b, size_t l) override;

private:
	size_t BoundedClientWrite(const char *b, size_t l, bool progmem);

	//Defined in LeifWebAuth.cpp, so this widely-included header pulls in no auth module.
	static THandlerFunction Gated(THandlerFunction fn);
};
#endif

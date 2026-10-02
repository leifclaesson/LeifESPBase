#pragma once

#include <Arduino.h>

//HTTP authentication for the shared web server, with a password that MUST be set before the
//device will do anything else.
//
//Why digest and not basic: basic sends the password, base64-wrapped, on every single request.
//Digest sends a hash over a server nonce, so the password never crosses the wire. That matters
//here because the config interface is plain HTTP -- there is no TLS on this server and a LAN
//appliance at a bare IP cannot hold a certificate a browser trusts.
//
//Why a password at all is not optional: EN 18031-1 (the harmonised standard behind the RED
//cybersecurity articles, mandatory since 2025-08-01) is cited in the OJEU WITH a restriction --
//it does not cover a device that lets the user decline to set a password. A device that can be
//used without one loses self-assessment and needs a paid notified body. Hence LeifWebAuthGate()
//refusing everything until a password exists, rather than nagging.
//
//⛔ What this does NOT do: it does not encrypt the session. Page contents stay readable to
//anyone on the wire. Only the credential is protected.

//---------------------------------------------------------------------------------------------
//What is stored, and what that is worth
//---------------------------------------------------------------------------------------------
//Digest authentication requires the server to hold a password-equivalent: the protocol's H1,
//MD5(username:realm:password). That is dictated by the protocol, not chosen here -- the core's
//own WebServer::authenticate() comment says "digest request us to know the password in the
//clear", and it stores nothing because it asks the caller for the plaintext every time.
//
//We store H1 instead of the plaintext, which is the best available: the user's actual password
//never lands in flash, so a dumped device cannot hand back a password they reused elsewhere.
//⛔ H1 is still enough to authenticate TO THIS DEVICE. A salted slow hash is impossible with
//digest -- there is no protocol variant that permits it. If at-rest strength ever has to go
//further than this, the answer is flash encryption, not a different hash.

#ifndef LEIF_WEBAUTH_MIN_PASSWORD_LEN
#define LEIF_WEBAUTH_MIN_PASSWORD_LEN 8
#endif

//How many outstanding challenges the device remembers at once. A browser opening a page fires
//the document and every asset together, none of them carrying a credential yet, so it collects
//one challenge per connection before it can answer any of them -- and a device that remembers
//only the newest refuses every answer but one. 8 covers the 6 connections per host that both
//Chrome and Firefox open, with two spare.
#ifndef LEIF_WEBAUTH_CHALLENGE_SLOTS
#define LEIF_WEBAUTH_CHALLENGE_SLOTS 8
#endif

//How long a challenge stays answerable. A browser meeting the stale=true refusal that follows
//retries by itself, so expiry costs a round trip rather than a password prompt.
#ifndef LEIF_WEBAUTH_CHALLENGE_LIFETIME_MS
#define LEIF_WEBAUTH_CHALLENGE_LIFETIME_MS 300000
#endif

//Call once from LeifSetupBegin(), before any handler can run.
void LeifWebAuthBegin();

//Registers the first-boot setup page. ⛔ Must be called before server.begin(), and the two
//endpoints it adds are the only ungated ones on the device.
void LeifWebAuthRegisterSetupPage();

bool LeifWebAuthIsConfigured();		//false until a password has been set on this device

//Sets the credential. Returns false and fills strError with something a human can act on when
//the password is refused -- a weak one is REFUSED, never accepted with a warning, because
//"accept it anyway" is the case the standard's restriction is about.
bool LeifWebAuthSet(const String & strUsername, const String & strPassword, String & strError);

//Forgets the credential, putting the device back into first-boot setup. ⛔ Only ever call this
//from something that needed PHYSICAL access -- a held button, a serial console. Reachable over
//the network it would be the bypass this whole file exists to prevent.
void LeifWebAuthClear();

String LeifWebAuthGetUsername();	//empty when unconfigured
String LeifWebAuthGetRealm();		//the realm H1 was computed against; see the note in the .cpp

//The gate. Returns true when the request may proceed. When it returns false it has ALREADY
//answered the client -- a 401 challenge, or the first-boot setup page -- so the caller must
//return immediately and write nothing more.
//
//Nothing calls this by hand: LeifWebServer::on() wraps every handler in it. See LeifWebServer.h.
bool LeifWebAuthGate();

//Marks a path as reachable without a credential. Only the setup page and its POST need this,
//and they are registered by this module. ⛔ Adding anything else here is how the next
///firmware.bin happens -- that endpoint served the whole running image, OTA password included,
//to anyone who asked.
void LeifWebAuthDeclarePublic(const char * pszPath);
bool LeifWebAuthIsPublic(const String & strPath);

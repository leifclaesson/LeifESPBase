#include "LeifWebAuth.h"
#include "LeifESPBase.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <nvs.h>
#include <MD5Builder.h>
#include <vector>

//NVS, not SPIFFS: the credential has to survive an OTA and has to exist on a product that
//never mounts a filesystem. The nvs partition is present on every ESP32 layout this library
//runs on, and a repartition that preserves config preserves this with it.
//
//⛔ The raw IDF API rather than the Arduino Preferences wrapper, deliberately. Preferences is a
//LIBRARY, so Sloeber only puts it on the include path for a project that links it -- a shared
//library cannot depend on one without making every product add it (this cost one build: "fatal
//error: Preferences.h: No such file or directory"). nvs.h is a platform header and is always
//reachable, and the core has already called nvs_flash_init() before setup() runs.
static const char * const szAuthNamespace="leifauth";

static bool NvsGetString(nvs_handle_t handle, const char * pszKey, String & strOut)
{
	size_t len=0;

	if(nvs_get_str(handle,pszKey,NULL,&len)!=ESP_OK || !len)
	{
		strOut="";
		return false;
	}

	char * psz=(char *) malloc(len);

	if(!psz)
	{
		strOut="";
		return false;
	}

	bool bOk = nvs_get_str(handle,pszKey,psz,&len)==ESP_OK;
	strOut = bOk ? psz : "";
	free(psz);

	return bOk;
}

static bool bConfigured=false;
static String strUsername;
static String strRealm;
static String strH1;

static std::vector<const char *> vecPublicPaths;

//The realm is baked into H1, so it can never be recomputed from a value that drifts. It is
//captured ONCE when the password is set and read back from storage thereafter -- renaming the
//host afterwards must not silently invalidate the password. It is also what the browser shows
//in its login prompt, which is why it is the hostname rather than a constant.
static String MakeRealm()
{
	String str=GetHostName();

	if(!str.length())
	{
		str="device";
	}

	return str;
}

static String Md5Hex(const String & str)
{
	MD5Builder md5;
	md5.begin();
	md5.add(str);
	md5.calculate();
	return md5.toString();
}

void LeifWebAuthBegin()
{
	nvs_handle_t handle=0;

	if(nvs_open(szAuthNamespace,NVS_READONLY,&handle)!=ESP_OK)
	{
		//Namespace absent is the normal first-boot case, not a fault.
		bConfigured=false;
		return;
	}

	NvsGetString(handle,"user",strUsername);
	NvsGetString(handle,"realm",strRealm);
	NvsGetString(handle,"h1",strH1);
	nvs_close(handle);

	//All three or none. A partial record is treated as unconfigured so the device falls back to
	//demanding setup rather than to an open interface.
	bConfigured = strUsername.length() && strRealm.length() && strH1.length()==32;

	if(!bConfigured)
	{
		strUsername="";
		strRealm="";
		strH1="";
	}
}

bool LeifWebAuthIsConfigured()
{
	return bConfigured;
}

String LeifWebAuthGetUsername()
{
	return strUsername;
}

String LeifWebAuthGetRealm()
{
	return strRealm;
}

//Refusals, not warnings. Each returns the reason in the user's terms -- a message that only
//says "too weak" makes the next attempt a guess.
static bool CheckPasswordStrength(const String & strUser, const String & strPass, String & strError)
{
	if((int) strPass.length() < LEIF_WEBAUTH_MIN_PASSWORD_LEN)
	{
		strError="Password must be at least ";
		strError+=LEIF_WEBAUTH_MIN_PASSWORD_LEN;
		strError+=" characters.";
		return false;
	}

	bool bLower=false;
	bool bUpper=false;
	bool bDigit=false;
	bool bOther=false;

	for(size_t i=0;i<strPass.length();i++)
	{
		char c=strPass[i];

		if(c>='a' && c<='z')
		{
			bLower=true;
		}
		else if(c>='A' && c<='Z')
		{
			bUpper=true;
		}
		else if(c>='0' && c<='9')
		{
			bDigit=true;
		}
		else
		{
			bOther=true;
		}
	}

	int iClasses=(bLower?1:0)+(bUpper?1:0)+(bDigit?1:0)+(bOther?1:0);

	if(iClasses<2)
	{
		strError="Password must mix at least two of: lower case, upper case, digits, symbols.";
		return false;
	}

	if(strPass.equalsIgnoreCase(strUser))
	{
		strError="Password must not be the same as the user name.";
		return false;
	}

	//A token list, not a dictionary -- the flash cost of a real one is not worth it on this
	//class of device. It catches the handful a hurried installer actually types.
	static const char * const szObvious[]={ "password","12345678","babelfish","admin123","letmein","changeme","qwertyui" };

	for(size_t i=0;i<sizeof(szObvious)/sizeof(szObvious[0]);i++)
	{
		if(strPass.equalsIgnoreCase(szObvious[i]))
		{
			strError="That password is one of the first things an attacker tries. Pick another.";
			return false;
		}
	}

	return true;
}

bool LeifWebAuthSet(const String & strUser, const String & strPassword, String & strError)
{
	String strWantUser=strUser;
	strWantUser.trim();

	if(!strWantUser.length())
	{
		strWantUser="admin";
	}

	if(strWantUser.indexOf(':')>=0)
	{
		//H1 is a colon-delimited MD5. A colon in the user name would let one user name
		//impersonate another.
		strError="User name must not contain a colon.";
		return false;
	}

	if(!CheckPasswordStrength(strWantUser,strPassword,strError))
	{
		return false;
	}

	String strWantRealm=MakeRealm();
	String strWantH1=Md5Hex(strWantUser+":"+strWantRealm+":"+strPassword);

	nvs_handle_t handle=0;

	if(nvs_open(szAuthNamespace,NVS_READWRITE,&handle)!=ESP_OK)
	{
		strError="Could not open storage to save the password.";
		return false;
	}

	bool bOk=true;
	bOk = nvs_set_str(handle,"user",strWantUser.c_str())==ESP_OK && bOk;
	bOk = nvs_set_str(handle,"realm",strWantRealm.c_str())==ESP_OK && bOk;
	bOk = nvs_set_str(handle,"h1",strWantH1.c_str())==ESP_OK && bOk;
	bOk = nvs_commit(handle)==ESP_OK && bOk;
	nvs_close(handle);

	if(!bOk)
	{
		strError="Writing the password to storage failed.";
		return false;
	}

	strUsername=strWantUser;
	strRealm=strWantRealm;
	strH1=strWantH1;
	bConfigured=true;

	csprintf("Web authentication configured for user \"%s\"\n",strUsername.c_str());

	return true;
}

void LeifWebAuthClear()
{
	nvs_handle_t handle=0;

	if(nvs_open(szAuthNamespace,NVS_READWRITE,&handle)==ESP_OK)
	{
		nvs_erase_all(handle);
		nvs_commit(handle);
		nvs_close(handle);
	}

	strUsername="";
	strRealm="";
	strH1="";
	bConfigured=false;

	csprintf("Web authentication cleared -- the device will demand a new password\n");
}

void LeifWebAuthDeclarePublic(const char * pszPath)
{
	//Nothing is copied: the vector holds the pointer, so the argument must outlive the call.
	//Same contract as LeifDeclareEndpoint.
	vecPublicPaths.push_back(pszPath);
}

bool LeifWebAuthIsPublic(const String & strPath)
{
	for(size_t i=0;i<vecPublicPaths.size();i++)
	{
		if(strPath==vecPublicPaths[i])
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------------------------
//The first-boot setup page
//---------------------------------------------------------------------------------------------
//Deliberately hand-written HTML with no dependency on any project's web UI toolkit: this page
//has to work on a product that has no UI of its own, and it is the one page that must render
//before anything else on the device will answer.

static const char szSetupPath[]="/setpassword";
static const char szSetupPost[]="/setpassword-save";

static void SendSetupPage(const String & strMessage)
{
	String s;
	s.reserve(2200);

	s+=F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
		"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
		"<title>Set a password</title><style>"
		"body{font-family:system-ui,sans-serif;background:#1b1d1e;color:#e8e8e8;margin:0;"
		"display:flex;min-height:100vh;align-items:center;justify-content:center}"
		"div.card{background:#26292b;padding:1.6rem;border-radius:12px;max-width:26rem;width:100%;"
		"box-sizing:border-box;box-shadow:0 2px 16px #0008}"
		"h1{font-size:1.25rem;margin:0 0 .4rem}p{line-height:1.45;color:#bdbdbd}"
		"label{display:block;margin:.9rem 0 .25rem}"
		"input{width:100%;padding:.55rem;border-radius:6px;border:1px solid #3a3e40;"
		"background:#1b1d1e;color:#e8e8e8;box-sizing:border-box}"
		"button{margin-top:1.2rem;width:100%;padding:.6rem;border:0;border-radius:6px;"
		"background:#6ebe4a;color:#10220a;font-weight:600;font-size:1rem;cursor:pointer}"
		"p.err{color:#ff9c8a}"
		"</style></head><body><div class=\"card\"><h1>Set a password</h1>"
		"<p>This device has no password yet, and will not do anything else until it has one.</p>");

	if(strMessage.length())
	{
		s+=F("<p class=\"err\">");
		s+=strMessage;
		s+=F("</p>");
	}

	s+=F("<form method=\"POST\" action=\"");
	s+=szSetupPost;
	s+=F("\"><label for=\"u\">User name</label>"
		"<input id=\"u\" name=\"user\" value=\"admin\" autocomplete=\"username\">"
		"<label for=\"p\">Password</label>"
		"<input id=\"p\" name=\"pass\" type=\"password\" autocomplete=\"new-password\">"
		"<label for=\"p2\">Password again</label>"
		"<input id=\"p2\" name=\"pass2\" type=\"password\" autocomplete=\"new-password\">"
		"<button type=\"submit\">Set password</button></form>");

	//Said here rather than only in the docs: the password cannot be read back off the device, so
	//losing it means physical access to clear it.
	s+=F("<p>Keep it somewhere safe. It is stored as a one-way digest, so it cannot be read back "
		"-- recovering from a lost password needs physical access to the board.</p>"
		"</div></body></html>");

	server.send(200,F("text/html"),s);
}

static void HandleSetupPage()
{
	if(LeifWebAuthIsConfigured())
	{
		//Once set, this page stops being a way to set one. Changing a known password is a
		//different, authenticated operation.
		server.send(409,F("text/plain"),F("A password is already set on this device.\n"));
		return;
	}

	SendSetupPage("");
}

static void HandleSetupSave()
{
	if(LeifWebAuthIsConfigured())
	{
		server.send(409,F("text/plain"),F("A password is already set on this device.\n"));
		return;
	}

	String strUser=server.hasArg("user")?server.arg("user"):String("admin");
	String strPass=server.hasArg("pass")?server.arg("pass"):String("");
	String strPass2=server.hasArg("pass2")?server.arg("pass2"):String("");

	if(strPass!=strPass2)
	{
		SendSetupPage("The two passwords do not match.");
		return;
	}

	String strError;

	if(!LeifWebAuthSet(strUser,strPass,strError))
	{
		SendSetupPage(strError);
		return;
	}

	String s;
	s+=F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
		"<title>Password set</title></head><body style=\"font-family:system-ui,sans-serif;"
		"background:#1b1d1e;color:#e8e8e8;padding:2rem\">"
		"<h1>Password set</h1><p>The device will now ask for it. "
		"<a style=\"color:#6ebe4a\" href=\"/\">Continue</a></p></body></html>");

	server.send(200,F("text/html"),s);
}

//---------------------------------------------------------------------------------------------
//The recovery path
//---------------------------------------------------------------------------------------------
//A forgotten password must be recoverable, or the first customer to lose one has a brick. It
//must ALSO need physical access, or it is a bypass of everything above.
//
//⛔ SERIAL ONLY, and that is the whole design. //Leif, 2026-10-01, ruling that the telnet
//console may stay open: "the console doesn't allow you to control anything, does it? ... you're
//not supposed to open that to the internet to begin with." That ruling holds only while the
//console controls nothing -- so putting `webpass clear` on telnet would retroactively falsify
//the premise he decided on, and hand the network a way to drop the gate. The dispatcher tells
//the two sources apart, so this refuses over telnet and says why.
static void WebPassCommand(const String & strCommand, eCommandLineSource source)
{
	String str=strCommand;
	str.trim();

	if(str!="webpass" && !str.startsWith("webpass "))
	{
		return;
	}

	String strArg=str.substring(7);
	strArg.trim();

	if(!strArg.length())
	{
		if(LeifWebAuthIsConfigured())
		{
			csprintf(PSTR("web password is SET, user \"%s\"\n"),LeifWebAuthGetUsername().c_str());
		}
		else
		{
			csprintf(PSTR("web password is NOT set -- every page shows the setup form\n"));
		}

		return;
	}

	if(strArg!="clear")
	{
		csprintf(PSTR("usage: webpass          show whether a password is set\n"));
		csprintf(PSTR("       webpass clear    forget it (serial console only)\n"));
		return;
	}

	if(source!=eCommandLineSource_Serial)
	{
		csprintf(PSTR("webpass clear is refused over telnet -- it needs the serial console, so\n"));
		csprintf(PSTR("that clearing the web password always costs physical access to the board\n"));
		return;
	}

	LeifWebAuthClear();
}

void LeifWebAuthRegisterSetupPage()
{
	LeifRegisterCommandCallback(WebPassCommand);
	LeifDeclareCommand(PSTR("webpass"),PSTR("show whether a web password is set; 'webpass clear' forgets it, serial only"));

	//⛔ Registered through the BASE class on purpose: these two are the only endpoints that must
	//answer while the device has no password, so they cannot go through the gate that would send
	//them back to themselves forever.
	server.WebServer::on(szSetupPath,HTTP_GET,HandleSetupPage);
	server.WebServer::on(szSetupPost,HTTP_POST,HandleSetupSave);

	LeifWebAuthDeclarePublic(szSetupPath);
	LeifWebAuthDeclarePublic(szSetupPost);
}

//---------------------------------------------------------------------------------------------
//The gate
//---------------------------------------------------------------------------------------------

bool LeifWebAuthGate()
{
	if(LeifWebAuthIsPublic(server.uri()))
	{
		return true;
	}

	if(!bConfigured)
	{
		//First boot. Everything the device can do is withheld until a password exists -- this is
		//the EN 18031-1 clause, so it is a refusal rather than a redirect that could be ignored.
		SendSetupPage("");
		return false;
	}

	if(server.LeifCheckDigestAuth(strUsername,strRealm,strH1))
	{
		return true;
	}

	server.requestAuthentication(DIGEST_AUTH,strRealm.c_str(),F("Authentication required.\n"));
	return false;
}

//---------------------------------------------------------------------------------------------
//LeifWebServer members -- the wrapper and the digest check
//---------------------------------------------------------------------------------------------

LeifWebServer::THandlerFunction LeifWebServer::Gated(LeifWebServer::THandlerFunction fn)
{
	return [fn]()
	{
		if(!LeifWebAuthGate())
		{
			//The gate has already answered the client. Writing anything further here would
			//append a page body to a 401.
			return;
		}

		if(fn)
		{
			fn();
		}
	};
}

static const char * MethodName(HTTPMethod method)
{
	//The client computed its hash over the method string it actually sent, so this has to agree
	//with it exactly -- a wrong name here is an authentication failure with no other symptom.
	switch(method)
	{
	case HTTP_GET:		return "GET";
	case HTTP_POST:		return "POST";
	case HTTP_PUT:		return "PUT";
	case HTTP_PATCH:	return "PATCH";
	case HTTP_DELETE:	return "DELETE";
	case HTTP_HEAD:		return "HEAD";
	case HTTP_OPTIONS:	return "OPTIONS";
	default:			return "GET";
	}
}

bool LeifWebServer::LeifCheckDigestAuth(const String & strUser, const String & strWantRealm, const String & strWantH1)
{
	static const char szAuthHeader[]="Authorization";

	if(!hasHeader(szAuthHeader))
	{
		return false;
	}

	String strAuth=header(szAuthHeader);

	if(!strAuth.startsWith("Digest "))
	{
		//A client that answered a digest challenge with basic credentials has put the password
		//on the wire. Refuse it rather than accept it -- accepting would make the whole point
		//of choosing digest negotiable by the client.
		return false;
	}

	strAuth=strAuth.substring(7);

	String strReqUser=_extractParam(strAuth,F("username=\""),'\"');
	String strReqRealm=_extractParam(strAuth,F("realm=\""),'\"');
	String strReqUri=_extractParam(strAuth,F("uri=\""),'\"');
	String strNonce=_extractParam(strAuth,F("nonce=\""),'\"');
	String strResponse=_extractParam(strAuth,F("response=\""),'\"');
	String strOpaque=_extractParam(strAuth,F("opaque=\""),'\"');

	if(!strReqUser.length() || !strReqRealm.length() || !strReqUri.length()
		|| !strNonce.length() || !strResponse.length() || !strOpaque.length())
	{
		return false;
	}

	if(strReqUser!=strUser || strReqRealm!=strWantRealm)
	{
		return false;
	}

	//The nonce and opaque must be the pair THIS server last issued. Without this check a
	//captured Authorization header would authenticate forever.
	if(strNonce!=_snonce || strOpaque!=_sopaque)
	{
		return false;
	}

	//The digest covers the URI, so a response captured for one path cannot be replayed against
	//another. The header's uri may carry a query string where _currentUri does not.
	String strUriPath=strReqUri;
	int iQuery=strUriPath.indexOf('?');

	if(iQuery>=0)
	{
		strUriPath=strUriPath.substring(0,iQuery);
	}

	if(strUriPath!=_currentUri)
	{
		return false;
	}

	String strH2=Md5Hex(String(MethodName(_currentMethod))+":"+strReqUri);
	String strExpected;

	if(strAuth.indexOf("qop=auth")!=-1 || strAuth.indexOf("qop=\"auth\"")!=-1)
	{
		String strNc=_extractParam(strAuth,F("nc="),',');
		String strCnonce=_extractParam(strAuth,F("cnonce=\""),'\"');

		if(!strNc.length() || !strCnonce.length())
		{
			return false;
		}

		strExpected=Md5Hex(strWantH1+":"+strNonce+":"+strNc+":"+strCnonce+":auth:"+strH2);
	}
	else
	{
		strExpected=Md5Hex(strWantH1+":"+strNonce+":"+strH2);
	}

	return strResponse.equalsConstantTime(strExpected);
}

#endif

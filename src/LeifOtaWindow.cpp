#include "LeifOtaWindow.h"
#include "LeifESPBase.h"

#ifdef LEIF_HAS_OTA_WINDOW

#include <ArduinoOTA.h>

//Set by the ArduinoOTA onStart callback in LeifESPBaseMain.cpp, cleared only by the reboot that
//ends an update. The window must never shut on a transfer that is already running.
extern bool bUpdatingOTA;

static bool bOpen=false;
static uint32_t uCloseAtSeconds=0;

static const char szEnablePath[]="/otaenable";
static const char szDisablePath[]="/otadisable";

bool LeifOtaWindowIsOpen()
{
	return bOpen;
}

uint32_t LeifOtaWindowSecondsLeft()
{
	if(!bOpen)
	{
		return 0;
	}

	//Signed difference, so this keeps working after the uptime counter wraps.
	int32_t iLeft=(int32_t) (uCloseAtSeconds-seconds());

	return iLeft>0 ? (uint32_t) iLeft : 0;
}

void LeifOtaWindowOpen()
{
	uCloseAtSeconds=seconds()+LEIF_OTA_WINDOW_SECONDS;

	if(bOpen)
	{
		csprintf(PSTR("Firmware update window extended, %u seconds left\n"),(unsigned) LEIF_OTA_WINDOW_SECONDS);
		return;
	}

	bOpen=true;
	ArduinoOTA.begin();

	csprintf(PSTR("Firmware update ENABLED for %u seconds -- port 8266 is listening\n"),(unsigned) LEIF_OTA_WINDOW_SECONDS);
}

void LeifOtaWindowClose(const char * pszWhy)
{
	if(!bOpen)
	{
		return;
	}

	if(bUpdatingOTA)
	{
		//ArduinoOTA.end() would stop the UDP socket mid-transfer and leave the idle slot half
		//written. The board reboots when the update finishes, which closes the window anyway.
		csprintf(PSTR("Firmware update window kept open -- an update is in progress\n"));
		return;
	}

	bOpen=false;
	ArduinoOTA.end();

	csprintf(PSTR("Firmware update disabled (%s) -- port 8266 is closed\n"),pszWhy);
}

void LeifOtaWindowLoop()
{
	if(!bOpen)
	{
		return;
	}

	if((int32_t) (seconds()-uCloseAtSeconds)>=0)
	{
		LeifOtaWindowClose(PSTR("the window ran out"));
	}
}

//---------------------------------------------------------------------------------------------
//The endpoints
//---------------------------------------------------------------------------------------------
//POST rather than GET, so that a page on another site cannot open the window with an <img> tag
//pointing at the board while a browser that has already authenticated to it is open. It costs a
//script nothing -- //Leif, 2026-10-02: "wget can of course issue that command if it has the
//password, and then do the OTA flashing in the same session":
//
//	curl --digest -u admin:PASSWORD -d "" http://board/otaenable
//	wget --http-user=admin --http-password=PASSWORD --post-data="" -O - http://board/otaenable
//
//Plain text out, so the same script can read the answer.

static void SendWindowState()
{
	String s;

	if(bOpen)
	{
		s=F("firmware update ENABLED, ");
		s+=LeifOtaWindowSecondsLeft();
		s+=F(" seconds left\n");
	}
	else
	{
		s=F("firmware update disabled\n");
	}

	server.send(200,PSTR("text/plain"),s);
}

void LeifOtaWindowAppendStatus(String & str)
{
	char temp[128];

	if(bOpen)
	{
		sprintf(temp,PSTR("OTA window.......: OPEN, %u seconds left\n"),(unsigned) LeifOtaWindowSecondsLeft());
	}
	else
	{
		sprintf(temp,PSTR("OTA window.......: closed -- nothing is listening on port 8266\n"));
	}

	str+=temp;
}

#ifdef NO_TOOLS_PAGE
//An empty inline would still leave the markup below in the image -- string literals pool into
//one .rodata section --gc-sections cannot split. A board whose owner removed the Tools page
//must not go on paying for the Tools page's text. Same reasoning as LeifDeclareEndpoint.
void LeifOtaWindowAppendToolsSection(String &) {}
#else
void LeifOtaWindowAppendToolsSection(String & str)
{
	char temp[320];

	str.concat(PSTR("<h3>Firmware update</h3>"));

	if(bOpen)
	{
		sprintf(temp,PSTR("<p>Push one now -- <b>open for %u more seconds</b>, then it closes itself.</p>"
				"<form method=\"POST\" action=\"%s\"><button type=\"submit\">Close it now</button></form>"),
				(unsigned) LeifOtaWindowSecondsLeft(),szDisablePath);
	}
	else
	{
		sprintf(temp,PSTR("<p>Over-the-air update is <b>off</b>. Opening it starts the updater for %u seconds.</p>"
				"<form method=\"POST\" action=\"%s\"><button type=\"submit\">Enable firmware update</button></form>"),
				(unsigned) LEIF_OTA_WINDOW_SECONDS,szEnablePath);
	}

	str.concat(temp);
}
#endif

//---------------------------------------------------------------------------------------------
//The console command
//---------------------------------------------------------------------------------------------
//Reporting is fine from anywhere. ⛔ ENABLING is serial only, for the same reason webpass clear
//is: //Leif, 2026-10-01 ruled telnet may stay open on the grounds that "the console doesn't
//allow you to control anything". An enable on telnet would both falsify that premise and hand
//the LAN a way round the web password. Closing is allowed from either -- it only takes access
//away, and it is the lever you want when something looks wrong.
static void OtaCommand(const String & strCommand, eCommandLineSource source)
{
	String str=strCommand;
	str.trim();

	if(str!="ota" && !str.startsWith("ota "))
	{
		return;
	}

	String strArg=str.substring(3);
	strArg.trim();

	if(!strArg.length())
	{
		if(bOpen)
		{
			csprintf(PSTR("firmware update is ENABLED, %u seconds left\n"),(unsigned) LeifOtaWindowSecondsLeft());
		}
		else
		{
			csprintf(PSTR("firmware update is disabled -- POST %s with the web password to open it\n"),szEnablePath);
		}

		return;
	}

	if(strArg=="disable")
	{
		LeifOtaWindowClose(PSTR("console"));
		return;
	}

	if(strArg!="enable")
	{
		csprintf(PSTR("usage: ota              show whether firmware update is open\n"));
		csprintf(PSTR("       ota enable       open it for %u seconds (serial console only)\n"),(unsigned) LEIF_OTA_WINDOW_SECONDS);
		csprintf(PSTR("       ota disable      close it now\n"));
		return;
	}

	if(source!=eCommandLineSource_Serial)
	{
		csprintf(PSTR("ota enable is refused over telnet -- it needs the serial console, so that\n"));
		csprintf(PSTR("opening firmware update always costs either the web password or physical access\n"));
		return;
	}

	LeifOtaWindowOpen();
}

void LeifOtaWindowBegin()
{
	LeifRegisterCommandCallback(OtaCommand);
	LeifDeclareCommand(PSTR("ota"),PSTR("show whether firmware update is open; 'ota enable' (serial only) / 'ota disable'"));

	server.on(szEnablePath,HTTP_POST,[]()
	{
		LeifOtaWindowOpen();
		SendWindowState();
	});

	server.on(szDisablePath,HTTP_POST,[]()
	{
		LeifOtaWindowClose(PSTR("asked to over the web"));
		SendWindowState();
	});

	//Named, never linked: both act the moment they are opened, and both are POST so a link
	//could not reach them anyway. The buttons that do are on the Tools page above this table.
	LeifDeclareEndpoint(szEnablePath,PSTR("POST: starts the firmware updater, which then closes itself again"),eLeifEndpoint_Acts);
	LeifDeclareEndpoint(szDisablePath,PSTR("POST: closes the firmware updater now"),eLeifEndpoint_Acts);
}

#endif

#include "LeifESPBaseMain.h"
#include "LeifESPBase.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#else
#include "WiFi.h"
#include "WebServer.h"
#include "ESPmDNS.h"
#endif
#ifndef NO_OTA
#include <ArduinoOTA.h>
#endif


#include "..\environment_setup.h"


extern HardwareSerial Serial;


static int iStatusLedPin=LED_BUILTIN;
void LeifSetStatusLedPin(int iPin)
{
	iStatusLedPin=iPin;
}

int LeifGetStatusLedPin() { return iStatusLedPin; }


#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server(80);
#else
WebServer server(80);
#endif


WiFiServer telnet(23);
WiFiClient telnetClients;
int disconnectedClient = 1;

#define WIFI_RECONNECT


#if defined(WIFI_RECONNECT)
unsigned long ulWifiReconnect=millis()-10000;
#endif


static std::vector<LeifOnShutdownCallback> vecOnShutdown;

void LeifRegisterOnShutdownCallback(LeifOnShutdownCallback cb)
{
	vecOnShutdown.push_back(cb);

}

void DoOnShutdownCallback(const char * pszReason)
{
	for(size_t i=0;i<vecOnShutdown.size();i++)
	{
		vecOnShutdown[i](pszReason);
	}

}



void LeifSetupBegin()
{
#ifndef NO_SERIAL_DEBUG
	Serial.begin(115200);
#endif

	csprintf("\nBooting...\n");

	if(iStatusLedPin>=0)
	{
		pinMode(iStatusLedPin, OUTPUT);
		digitalWrite(iStatusLedPin, LOW);
	}

	WiFi.mode(WIFI_STA);

	csprintf("Using WiFi SSID: %s\n",wifi_ssid);

#if defined(WIFI_RECONNECT)
	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(false);
	WiFi.begin(wifi_ssid, wifi_key);
#else
	WiFi.begin(wifi_ssid, wifi_key);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
#endif

	csprintf("MAC address: %s\n",WiFi.macAddress().c_str());
	csprintf("Host name: %s\n",GetHostName());

#if defined(ARDUINO_ARCH_ESP32)
#ifndef NO_OTA
	ArduinoOTA.setPort(8266);
#endif
#endif

#ifndef NO_OTA
	ArduinoOTA.setHostname(GetHostName());
	ArduinoOTA.onStart([]()   // switch off all the PWMs during upgrade
	{
		if(iStatusLedPin>=0)
		{
		  pinMode(iStatusLedPin, OUTPUT);
#if defined(ARDUINO_ARCH_ESP32)
		  digitalWrite(iStatusLedPin, LOW);
#else
		  digitalWrite(iStatusLedPin, HIGH);
#endif
		}
		DoOnShutdownCallback("OTA");
	});

	ArduinoOTA.onEnd([]()   // do a fancy thing with our board led at end
	{

#if defined(ARDUINO_ARCH_ESP32)
		const int ledchannel=0;
    	if(iStatusLedPin>=0)
    	{
    		ledcSetup(0,5000,10);
    		ledcAttachPin(iStatusLedPin,0);
    	}
#endif

	    for (int i = 0; i < 30; i++) {
	    	if(iStatusLedPin>=0)
	    	{

//#if defined(ARDUINO_ARCH_ESP8266)
	    		int value=(i * 100) % 1001;
#if defined(ARDUINO_ARCH_ESP32)
	    		float temp=1.0f-(value*0.001f);
	    		temp*=temp;
	    		value=temp*1000.0f;
	    		ledcWrite(ledchannel,value);
#else
	    		analogWrite(iStatusLedPin, value);
#endif

//#endif
	    	}
	      delay(50);
	    }
		if(iStatusLedPin>=0)
		{
#if defined(ARDUINO_ARCH_ESP32)
			ledcDetachPin(iStatusLedPin);
			digitalWrite(iStatusLedPin,LOW);
#else
			digitalWrite(iStatusLedPin,HIGH);
#endif
		}

	    delay(50);
	});

	ArduinoOTA.onError([](ota_error_t error)
	{
		(void)error;
		ESP.restart();
	});
#endif

	telnet.begin();
	telnet.setNoDelay(true);
	csprintf("Telnet server started\n");

	server.on("/ping", []()
	{
		char ping_response[128];
		sprintf(ping_response,"pong from %s",GetHostName());
	server.send(200, "text/plain", ping_response);
	});

	server.on("/sysinfo", []()
	{
		String s;

		char temp[128];

		uint32_t ideSize = ESP.getFlashChipSize();
		FlashMode_t ideMode = ESP.getFlashChipMode();

#if defined(ARDUINO_ARCH_ESP8266)
		sprintf(temp,"SOC..............: ESP8266\n");
		s+=temp;
#endif

#if defined(ARDUINO_ARCH_ESP32)
		{
			esp_chip_info_t chip_info;

			esp_chip_info(&chip_info);
			sprintf(temp,"SOC..............: ESP32 (%d cores), WiFi%s%s, ",
				  chip_info.cores,
				  (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
				  (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
			s+=temp;

			sprintf(temp,"rev %d\n", chip_info.revision);
			s+=temp;
		}
#endif

#if defined(ARDUINO_ARCH_ESP32)
		sprintf(temp,"Efuse MAC........: %02X:%02X:%02X:%02X:%02X:%02X\n",
				(uint8_t) (ESP.getEfuseMac()>>0),
				(uint8_t) (ESP.getEfuseMac()>>8),
				(uint8_t) (ESP.getEfuseMac()>>16),
				(uint8_t) (ESP.getEfuseMac()>>24),
				(uint8_t) (ESP.getEfuseMac()>>32),
				(uint8_t) (ESP.getEfuseMac()>>40)
				); s+=temp;
#endif

#if defined(ARDUINO_ARCH_ESP8266)
		sprintf(temp,"Flash real id....: %08X\n", ESP.getFlashChipId()); s+=temp;
		sprintf(temp,"Flash real size..: %u bytes\n\n", ESP.getFlashChipRealSize()); s+=temp;
#endif

		sprintf(temp,"Flash ide size...: %u bytes\n", ideSize); s+=temp;
		sprintf(temp,"Flash ide speed..: %u Hz\n", ESP.getFlashChipSpeed()); s+=temp;
		sprintf(temp,"Flash ide mode...: %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN")); s+=temp;

		server.send(200, "text/plain", s);
	});


}

void LeifSetupEnd()
{
	server.begin();
#ifndef NO_OTA
	ArduinoOTA.begin();
#endif


}

static int iDayCount=0;
static unsigned long ulLastDayMillis=0;
unsigned long ulLastLoopMillis=0;

static unsigned long ulLastLoopSecond=0;
static unsigned long ulLastLoopHalfSecond=0;
static unsigned long ulLastLoopQuarterSecond=0;
static unsigned long ulLastLoopDeciSecond=10;

static bool bInterval100=false;
static bool bInterval250=false;
static bool bInterval500=false;
static bool bInterval1000=false;
static bool bInterval10s=false;

static unsigned long ulSecondCounter=0;

unsigned long seconds()
{
	return ulSecondCounter;
}

bool Interval100()
{
	return bInterval100;
}

bool Interval250()
{
	return bInterval250;
}

bool Interval500()
{
	return bInterval500;
}

bool Interval1000()
{
	return bInterval1000;
}

bool Interval10s()
{
	return bInterval10s;
}

void LeifUptimeString(String & string)
{
	char temp[128];
	if(iDayCount>30)
	{
		sprintf(temp,"%i days",iDayCount);

	}
	else
	{

		sprintf(temp,"%lud %02lu:%02lu:%02lu",
			(ulLastLoopMillis / 86400000),
			(ulLastLoopMillis / 3600000) % 24,
			(ulLastLoopMillis / 60000) % 60,
			(ulLastLoopMillis / 1000) % 60
			);
	}

	string=temp;

}


static bool bAllowLedWrite=true;
static bool bInvertLedBlink=false;

void LeifSetSuppressLedWrite(bool bSuppress)
{
	bAllowLedWrite=!bSuppress;
}

void LeifSetInvertLedBlink(bool bInvertLed)
{
	bInvertLedBlink=bInvertLed;
}

bool LeifGetInvertLedBlink()
{
	return bInvertLedBlink;
}

void LeifLoop()
{
#ifndef NO_OTA
	  ArduinoOTA.handle();
#endif
	  server.handleClient();
#if defined(ARDUINO_ARCH_ESP8266)
	  MDNS.update();
#endif

#ifdef USE_HOMIE
	homie.Loop();
#endif


	  ulLastLoopMillis=millis();
	  if((int) (ulLastLoopMillis-ulLastDayMillis)>86400000)
	  {
		  ulLastDayMillis=ulLastLoopMillis;
		  iDayCount++;
	  }



	  if(bInterval1000 && ulSecondCounter % 10 == 0)
	  {
		  bInterval10s=true;
	  }
	  else
	  {
		  bInterval10s=false;
	  }

	  if((int) (ulLastLoopMillis-ulLastLoopSecond)>=1000)
	  {
		  ulLastLoopSecond+=1000;
		  bInterval1000=true;
		  ulSecondCounter++;
	  }
	  else
	  {
		  bInterval1000=false;
	  }

 	  if((int) (ulLastLoopMillis-ulLastLoopHalfSecond)>=500)
	  {
		  ulLastLoopHalfSecond+=500;
		  bInterval500=true;
	  }
	  else
	  {
		  bInterval500=false;

	  }


	  if((int) (ulLastLoopMillis-ulLastLoopQuarterSecond)>=250)
	  {
		  ulLastLoopQuarterSecond+=250;
		  bInterval250=true;
	  }
	  else
	  {
		  bInterval250=false;

	  }

	  if((int) (ulLastLoopMillis-ulLastLoopDeciSecond)>=100)
	  {
		  ulLastLoopDeciSecond+=100;
		  bInterval100=true;
	  }
	  else
	  {
		  bInterval100=false;
	  }


	  static bool bIpPrinted=false;
	  if(WiFi.status() == WL_CONNECTED)
	  {
		  if(!bIpPrinted)
		  {
			  bIpPrinted=true;
			  csprintf("IP: %s\n",WiFi.localIP().toString().c_str());
		  }

	  }
	  else
	  {
		  bIpPrinted=false;
#if defined(WIFI_RECONNECT)
		  if((millis()-ulWifiReconnect)>=15000)
		  {
			  ulWifiReconnect=millis();
			  csprintf("WiFi attempting to reconnect to %s...\n",wifi_ssid);
			  WiFi.reconnect();

		  }
#endif
	  }


		if(telnet.hasClient())
		{
			if(!telnetClients || !telnetClients.connected())
			{
				if(telnetClients)
				{
					telnetClients.stop();
					csprintf("Telnet Client Stop\n");
				}
				telnetClients = telnet.available();

				String strUptime;
				LeifUptimeString(strUptime);

				csprintf("Welcome to %s! Uptime: %s\n",GetHostName(),strUptime.c_str());
				telnetClients.flush();  // clear input buffer, else you get strange characters
				disconnectedClient = 0;
			}
		}
		else
		{
			if(!telnetClients.connected())
			{
				if(disconnectedClient == 0)
				{
					csprintf("Telnet client disconnected.\n");
					telnetClients.stop();
					disconnectedClient = 1;
				}
			}
		}

		if(iStatusLedPin>=0 && bAllowLedWrite)
		{
			int interval=2000;
			if(!WiFi.isConnected()) interval=500;

			unsigned long thresh=100;
			if(bInvertLedBlink) thresh=50;

			bool bBlink=(millis()%interval)<thresh?false:true;

#if defined(ARDUINO_ARCH_ESP8266)
			bBlink ^= true;
#endif

			bBlink ^= bInvertLedBlink;

			digitalWrite(iStatusLedPin, bBlink);

		}

}


void LeifHtmlMainPageCommonHeader(String & string)
{

	string.concat("<table>");

	string.concat("<tr>");
	string.concat("<td>");

	string.concat("Uptime: ");

	String strUptime;
	LeifUptimeString(strUptime);
	string.concat(strUptime);
	const char compile_date[] = __DATE__ " " __TIME__;

	string.concat("</td>");


	string.concat("<td>");
	string.concat("Compile time: ");
	string.concat(compile_date);
	string.concat("</td>");
	string.concat("</tr>");

	string.concat("<tr>");
	string.concat("<td>");
	string.concat("Host name: ");
	string.concat(GetHostName());
	string.concat("</td>");
	string.concat("<td>");
	string.concat("MAC: ");
	string.concat(WiFi.macAddress());
	string.concat("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
	string.concat("</td>");
	string.concat("<td>");
	string.concat("IP: ");
	string.concat(WiFi.localIP().toString());
	string.concat("</td>");
	string.concat("</tr>");
	string.concat("</table>");

	string.concat("<br>");


}

byte chartohex(char asciichar)
{
	if(asciichar>='0' && asciichar<='9')
	{
		return asciichar-'0';
	}

	if(asciichar>='a' && asciichar<='f')
	{
		return (asciichar-'a') + 0xA;
	}

	if(asciichar>='A' && asciichar<='F')
	{
		return (asciichar-'A') + 0xA;
	}
	return 0;
}


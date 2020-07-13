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


#if defined(ARDUINO_ARCH_ESP32)
static unsigned short usLogTable256[256]=
{0, 0, 1, 1, 1, 2, 3, 3, 4, 5, 7, 8, 9, 11, 13, 15, 17, 19, 21, 23, 26, 28, 31, 34, 37, 40, 43, 46, 50, 53, 57, 61, 65, 69, 73, 78, 82, 87, 91, 96, 101, 106, 111, 117, 122, 128, 134, 139, 145, 152, 158, 164, 171, 177, 184, 191, 198, 205, 212, 220, 227, 235, 242, 250, 258, 266, 275, 283, 292, 300, 309, 318, 327, 336, 345, 355, 364, 374, 383, 393, 403, 413, 424, 434, 445, 455, 466, 477, 488, 499, 510, 522, 533, 545, 557, 569, 581, 593, 605, 617, 630, 643, 655, 668, 681, 695, 708, 721, 735, 748, 762, 776, 790, 804, 819, 833, 848, 862, 877, 892, 907, 922, 938, 953, 969, 984, 1000, 1016, 1032, 1048, 1064, 1081, 1097, 1114, 1131, 1148, 1165, 1182, 1199, 1217, 1234, 1252, 1270, 1288, 1306, 1324, 1342, 1361, 1380, 1398, 1417, 1436, 1455, 1474, 1494, 1513, 1533, 1552, 1572, 1592, 1612, 1632, 1653, 1673, 1694, 1715, 1735, 1756, 1777, 1799, 1820, 1841, 1863, 1885, 1907, 1929, 1951, 1973, 1995, 2018, 2040, 2063, 2086, 2109, 2132, 2155, 2179, 2202, 2226, 2249, 2273, 2297, 2321, 2346, 2370, 2395, 2419, 2444, 2469, 2494, 2519, 2544, 2569, 2595, 2621, 2646, 2672, 2698, 2724, 2751, 2777, 2804, 2830, 2857, 2884, 2911, 2938, 2965, 2993, 3020, 3048, 3076, 3103, 3131, 3160, 3188, 3216, 3245, 3273, 3302, 3331, 3360, 3389, 3419, 3448, 3477, 3507, 3537, 3567, 3597, 3627, 3657, 3688, 3718, 3749, 3780, 3811, 3842, 3873, 3904, 3936, 3967, 3999, 4031, 4062, 4095};
#endif


extern HardwareSerial Serial;


static uint32_t cpu_freq_khz=0;

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
unsigned long ulWifiReconnect=millis()-15000;
int iWifiConnAttempts=0;
uint32_t ulWifiTotalConnAttempts=0;
#endif



static std::vector<LeifOnShutdownCallback> vecOnShutdown;

void LeifRegisterOnShutdownCallback(LeifOnShutdownCallback cb)
{
	vecOnShutdown.push_back(cb);

}


LeifHttpMainTableCallback fnHttpMainTableCallback;

void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb)
{
	fnHttpMainTableCallback=cb;
}




void DoOnShutdownCallback(const char * pszReason)
{
	for(size_t i=0;i<vecOnShutdown.size();i++)
	{
		vecOnShutdown[i](pszReason);
	}

}

uint8_t cBSSID[6]={0,0,0,0,0,0};
int iWifiChannel=-1;
bool bAllowBSSID=false;

String g_lastWifiSSID=wifi_ssid;
String g_lastWifiPSK=wifi_key;
int g_lastWifiChannel=iWifiChannel;
uint8_t g_lastBSSID[6]={0,0,0,0,0,0};


String strWifiStatus="Disconnected";

bool bAllowConnect=true;



bool ParseMacAddress(const char * pszMAC, uint8_t * cMacOut)
{
	if(strlen(pszMAC)==17)
	{
		const char * temp=pszMAC;
		for(int i=0;i<6;i++)
		{
			cMacOut[i]=0;
			for(int j=0;j<2;j++)
			{
				unsigned char hex=0;
				if(*temp>='0' && *temp<='9') hex=*temp-'0';
				else if(*temp>='A' && *temp<='F') hex=*temp-'A'+0xa;
				else if(*temp>='a' && *temp<='f') hex=*temp-'a'+0xa;
				cMacOut[i] |= (hex << (4*(1-j)));
				temp++;
			}
			temp++;
		}
		return true;
	}
	else
	{
		memset(cMacOut,0,6);
		return false;
	}
}

static IPAddress ipAccessPoint;

IPAddress LeifGetAccessPointIP()
{
	if(iWifiChannel>=0 && !memcmp(WiFi.BSSID(),cBSSID,6))
	{
		return ipAccessPoint;
	}

	return IPAddress(0,0,0,0);
}

void LeifSetupBSSID(const char * pszBSSID, int ch, const char * pszAccessPointIP)
{



	if(pszBSSID && ParseMacAddress(pszBSSID,cBSSID))
	{
		bAllowBSSID=true;
		iWifiChannel=ch;
		ipAccessPoint.fromString(pszAccessPointIP);
	}
	else
	{
		iWifiChannel=-1;
		ipAccessPoint=IPAddress(0,0,0,0);
	}
}


bool bNewWifiConnection=false;

bool IsNewWifiConnection()
{
	//returns true ONCE after a new wifi connection has been established
	bool bRet=bNewWifiConnection;
	bNewWifiConnection=false;

	return bRet;
}




class MyWiFiSTAClass:
#if defined(ARDUINO_ARCH_ESP8266)
public ESP8266WiFiSTAClass
#else
public WiFiSTAClass
#endif
{
public:
	static bool GetIsStatic()	//ugly workaround to access protected flag in the WiFiSTAClass. We need to see whether we're on static IP or not.
	{
		void * mypointer=&WiFi;
		return ((MyWiFiSTAClass *) mypointer)->_useStaticIp;
	}
};

/*
void onStationModeDisconnectedEvent(const WiFiEventStationModeDisconnected& evt)
{
  if (WiFi.status() == WL_CONNECTED)
  {
	WiFi.disconnect();
  } else {
	Serial.println("         WiFi disconnected...");
  }
}
*/

String MacToString(const uint8_t * mac)
{
	char temp[20];
	if(mac)
	{
		sprintf(temp,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	}
	else
	{
		sprintf(temp,"*MAC IS NULL PTR*");
	}
	return temp;
}

void SetupWifiInternal()
{
#if defined(ARDUINO_ARCH_ESP8266)
	WiFi.hostname(GetHostName());
#else
	WiFi.setHostname(GetHostName());
#endif
#if defined(WIFI_RECONNECT)
	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(false);

	//WiFi.onStationModeDisconnected(onStationModeDisconnectedEvent);


	if(iWifiChannel>=0 && bAllowBSSID)
	{
	  csprintf("WiFi attempting to connect to %s at BSSID %s, Ch %i (attempt %i)...\n",wifi_ssid,MacToString(cBSSID).c_str(),iWifiChannel,iWifiConnAttempts);
		WiFi.begin(wifi_ssid, wifi_key,iWifiChannel,cBSSID,true);

		g_lastWifiSSID=wifi_ssid;
		g_lastWifiPSK=wifi_key;
		g_lastWifiChannel=iWifiChannel;
		memcpy(g_lastBSSID,cBSSID,sizeof(g_lastBSSID));

		strWifiStatus=wifi_ssid;
		strWifiStatus+=" ";
		strWifiStatus+=MacToString(cBSSID);
	}
	else
	{
//		csprintf("No BSSID configured\n");

		const char * use_ssid=wifi_ssid;
		const char * use_key=wifi_key;

		if(backup_ssid)
		{
			int x=iWifiConnAttempts/2;
			if(x && (x & 1))
			{
				use_ssid=backup_ssid;
				use_key=backup_key;
			}
		}

		csprintf("WiFi attempting to connect to %s (attempt %i)...\n",use_ssid,iWifiConnAttempts);
		WiFi.begin(use_ssid, use_key);

		g_lastWifiSSID=use_ssid;
		g_lastWifiPSK=wifi_key;
		g_lastWifiChannel=-1;
		memset(g_lastBSSID,0,sizeof(g_lastBSSID));

		strWifiStatus="SSID ";
		strWifiStatus+=use_ssid;

	}
#else
//	csprintf("Using Auto Reconnect\n");
	WiFi.begin(wifi_ssid, wifi_key);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	strWifiStatus="SSID ";
	strWifiStatus+=wifi_ssid;

	g_lastWifiSSID=wifi_ssid;
	g_lastWifiPSK=wifi_key;
	g_lastWifiChannel=-1;
	memset(g_lastBSSID,0,sizeof(g_lastBSSID));

#endif
}

bool bConsoleInitDone=false;
void LeifSetupConsole()
{
	if(bConsoleInitDone) return;

#ifndef NO_SERIAL_DEBUG
	Serial.begin(115200);
#endif

	bConsoleInitDone=true;

	csprintf("\nBooting...\n");

};

uint8_t ucLedFadeChannel=15;	//ESP32 ledc
static bool bAllowLedFade=true;
#if defined(ARDUINO_ARCH_ESP32)
int iAnalogWriteBits=12;
#else
int iAnalogWriteBits=10;
#endif

void LeifSetAllowFadeLed(bool bAllowFade, int analogWriteBits)
{
	bAllowLedFade=bAllowFade;
	iAnalogWriteBits=analogWriteBits;
}

bool bLeifSetupBeginDone=false;
void LeifSetupBegin()
{
	while(IsLeifSetupBeginDone())
	{
		delay(500);
		csprintf("LeifSetupBegin() called twice\n");
	}

	bLeifSetupBeginDone=true;

	LeifSetupConsole();

	if(iStatusLedPin>=0)
	{
		pinMode(iStatusLedPin, OUTPUT);
		digitalWrite(iStatusLedPin, LOW);
	}

	WiFi.mode(WIFI_STA);

	csprintf("Using WiFi SSID: %s\n",wifi_ssid);
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
		if(iStatusLedPin>=0)
		{
			ledcSetup(ucLedFadeChannel,500,12);
			ledcAttachPin(iStatusLedPin,ucLedFadeChannel);
		}
#else
		analogWriteRange(1023);
#endif

		for (int i = 0; i < 25; i++) {
			if(iStatusLedPin>=0)
			{
#if defined(ARDUINO_ARCH_ESP32)
				int value=(i * 250) % 1001;
				float temp=1.0f-(value*0.001f);
				temp*=temp;
				value=temp*4095.0f;
				ledcWrite(ucLedFadeChannel,value);
#else
				int value=(i * 200) % 1001;
				analogWrite(iStatusLedPin, value);
#endif
				delay(40);
			}
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

		delay(500);
		ESP.restart();
		delay(500);
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

		sprintf(temp,"Clock Freq.......: %.01f MHz\n", cpu_freq_khz / 1000.0f);
		s+=temp;

#if defined(ARDUINO_ARCH_ESP32)

		uint64_t efusemac=ESP.getEfuseMac();
		sprintf(temp,"Efuse MAC........: %s\n",
				MacToString((uint8_t *) &efusemac).c_str()
				); s+=temp;
#endif

#if defined(ARDUINO_ARCH_ESP8266)
		sprintf(temp,"Flash real id....: %08X\n", ESP.getFlashChipId()); s+=temp;
		sprintf(temp,"Flash real size..: %u bytes\n\n", ESP.getFlashChipRealSize()); s+=temp;
#endif

		sprintf(temp,"Flash ide size...: %u bytes\n", ideSize); s+=temp;
		sprintf(temp,"Flash ide speed..: %u Hz\n", ESP.getFlashChipSpeed()); s+=temp;
		sprintf(temp,"Flash ide mode...: %s\n\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN")); s+=temp;

		sprintf(temp,"Heap free........: %i\n",ESP.getFreeHeap()); s+=temp;
#if defined(ARDUINO_ARCH_ESP8266)
#ifndef NO_MAX_FREE_BLOCKSIZE
		sprintf(temp,"Heap max alloc...: %i\n",ESP.getMaxFreeBlockSize()); s+=temp;
#endif
#else
		sprintf(temp,"Heap max alloc...: %i\n",ESP.getMaxAllocHeap()); s+=temp;
#endif
		//sprintf(temp,"Heap frag........: %i\n",ESP.getHeapFragmentation()); s+=temp;


		server.send(200, "text/plain", s);
	});


#if defined(ARDUINO_ARCH_ESP32)
	if(bAllowLedFade)
	{
		ledcSetup(ucLedFadeChannel,500,12);
		ledcAttachPin(iStatusLedPin,ucLedFadeChannel);

	}
#endif


}

void LeifSetupEnd()
{
	server.begin();
#ifndef NO_OTA
	ArduinoOTA.begin();
#endif


}

unsigned long ulLastLoopMillis=0;

static unsigned long ulLastLoopSecond=0;
static unsigned long ulLastLoopHalfSecond=0;
static unsigned long ulLastLoopQuarterSecond=0;
static unsigned long ulLastLoopDeciSecond=10;

static bool bInterval50=false;
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

const unsigned long * pSeconds()
{
	return &ulSecondCounter;
}


static unsigned long ulSecondCounterWiFi=0;

unsigned long secondsWiFi()
{
	return ulSecondCounterWiFi;
}

const unsigned long * pSecondsWiFi()
{
	return &ulSecondCounterWiFi;
}


bool Interval50()
{
	return bInterval50;
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

void LeifSecondsToShortUptimeString(String & string,unsigned long ulSeconds)
{
	char temp[8];
	if(ulSeconds>=(86400*365))
	{
		sprintf(temp,"%luy",ulSeconds/(86400*365));	//years
	}
	if(ulSeconds>=(86400*30))
	{
		sprintf(temp,"%lum",ulSeconds/(86400*30));	//months
	}
	else if(ulSeconds>=86400)
	{
		sprintf(temp,"%lud",ulSeconds/86400);	//days
	}
	else if(ulSeconds>=3600)
	{
		sprintf(temp,"%luh",ulSeconds/3600);	//hours
	}
	else if(ulSeconds>=60)
	{
		sprintf(temp,"%lum",ulSeconds/60);	//minutes
	}
	else
	{
		sprintf(temp,"%lus",ulSeconds);	//seconds
	}

	string=temp;

}

void LeifSecondsToUptimeString(String & string,unsigned long ulSeconds)
{

	unsigned long ulDays=ulSeconds/86400;

	if(ulDays>30)
	{
		if(ulDays>365)
		{
			char temp[32];
			sprintf(temp,"%luy %lud",ulDays/366,ulDays % 366);
			string=temp;
		}
		else
		{
			unsigned long ulHours=(ulSeconds - (ulDays*86400))/3600;
			char temp[32];
			sprintf(temp,"%lud %luh",ulDays,ulHours);
			string=temp;
		}
	}
	else
	{
		char temp[128];
		if(ulDays==0)
		{
			sprintf(temp,"%02lu:%02lu:%02lu",
				(ulSeconds / 3600) % 24,
				(ulSeconds / 60) % 60,
				(ulSeconds) % 60
				);
		}
		else
		{
			sprintf(temp,"%lud %02lu:%02lu:%02lu",
				(ulSeconds / 86400),
				(ulSeconds / 3600) % 24,
				(ulSeconds / 60) % 60,
				(ulSeconds) % 60
				);
		}
		string=temp;
	}
}

void LeifUptimeString(String & string)
{
	LeifSecondsToUptimeString(string,seconds());
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

	if(((int32_t) (millis()-ulLastLoopMillis))<50)
	{
		if(bInterval50)
		{
			bInterval10s=false;
			bInterval1000=false;
			bInterval500=false;
			bInterval250=false;
			bInterval100=false;
			bInterval50=false;
		}
		return;
	}
	bInterval50=true;

	ulLastLoopMillis=millis();

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


		  static uint32_t last_cyclecount=0;
		  static uint32_t last_cc_millis=0;

		  uint32_t mdiff=millis()-last_cc_millis;

		  if(mdiff)
		  {
			  cpu_freq_khz=(ESP.getCycleCount()-last_cyclecount) / mdiff;
		  }

		  last_cyclecount=ESP.getCycleCount();
		  last_cc_millis=millis();

		  if(WiFi.status()==WL_CONNECTED)
		  {
			  ulSecondCounterWiFi++;
		  }
		  else
		  {
			  ulSecondCounterWiFi=0;
		  }

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


	  if(WiFi.getMode() & WIFI_STA)
	  {

		  static bool bIpPrinted=false;
		  if(WiFi.status() == WL_CONNECTED)
		  {
			  if(!bIpPrinted)
			  {
				  bIpPrinted=true;
				  strWifiStatus=MyWiFiSTAClass::GetIsStatic()?"*":"";
				  strWifiStatus+=WiFi.localIP().toString();

				  csprintf("IP: %s%s, SSID %s, BSSID %s, Ch %i\n",WiFi.localIP().toString().c_str(),MyWiFiSTAClass::GetIsStatic()?" (static)":" (DHCP)",WiFi.SSID().c_str(), WiFi.BSSIDstr().c_str(),WiFi.channel());
				  csprintf("Gateway: %s\n",WiFi.gatewayIP().toString().c_str());
				  iWifiConnAttempts=0;
				  bAllowBSSID=true;
				  bNewWifiConnection=true;
			  }

		  }
		  else if(LeifGetAllowWifiConnection())
		  {
			  bIpPrinted=false;
	#if defined(WIFI_RECONNECT)
			  if((millis()-ulWifiReconnect)>=15000)
			  {
				  ulWifiReconnect=millis();

	#ifdef ESP32
				  //for some reason the ESP32 _always_ fails the first attempt, so let's just skip ahead to the second attempt and save some time.
				  if(!iWifiConnAttempts)
				  {
					  ulWifiReconnect=millis()-12000;
				  }
	#endif

				  if(iWifiConnAttempts>=1 && bAllowBSSID)
				  {
					  bAllowBSSID=false;
				  }

				  SetupWifiInternal();

				  WiFi.reconnect();
				  iWifiConnAttempts++;
				  ulWifiTotalConnAttempts++;
				  ulSecondCounterWiFi=0;
			  }
	#endif
		  }
		  else
		  {
			  g_lastWifiSSID="Disabled";
			  memset(g_lastBSSID,0,6);
			  g_lastWifiChannel=0;
			  ulSecondCounterWiFi=0;


		  }
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
			if(bAllowLedFade)
			{

				uint16_t use;
				if(WiFi.isConnected() || !bAllowConnect)
				{
					static int counter=0;
					static int add=250;
					if(counter>65536) add=50;
					counter+=add;
					int value=counter & 8191;
					//int value=(millis() & 8191);
					if(value>4095) value=8191-value;

#if defined(ARDUINO_ARCH_ESP32)
					use=usLogTable256[(value>>7)+(value>>8)+64];
#else
					use=((value>>3)+256);
#endif

					if(!bAllowConnect)
					{
						use>>=1;
					}

				}
				else
				{
					int value=(millis() & 511);
					if(value>255) value=511-value;
#if defined(ARDUINO_ARCH_ESP32)
					use=usLogTable256[value>>1];
#else
					use=(value<<1);
#endif
				}

				//if(Interval100()) csprintf("use %i\n",use);

				if(iAnalogWriteBits<12) use>>=(12-iAnalogWriteBits);
					else if(iAnalogWriteBits>12) use<<=(iAnalogWriteBits-12);


#if defined(ARDUINO_ARCH_ESP32)
				ledcWrite(ucLedFadeChannel,bInvertLedBlink?((1<<iAnalogWriteBits)-1)-use:use);
				//if(Interval100()) csprintf("use after=%i %i\n",use,bInvertLedBlink?((1<<iAnalogWriteBits)-1)-use:use);
#else
				analogWrite(iStatusLedPin,bInvertLedBlink?use:((1<<iAnalogWriteBits)-1)-use);

				//if(Interval100()) csprintf("use after=%i %i\n",use,bInvertLedBlink?use:((1<<iAnalogWriteBits)-1)-use);
#endif

				/*
				uint16_t use;
				if(WiFi.isConnected())
				{
					static int counter=0;
					static int add=250;
					if(counter>65536) add=50;
					counter+=add;
					int value=counter & 8191;
					//int value=(millis() & 8191);
					if(value>4095) value=8191-value;
					use=(((value>>3)+(value>>4))+256);
				}
				else
				{
					int value=(millis() & 511);
					if(value>255) value=511-value;
					use=(value<<1);
				}



#if defined(ARDUINO_ARCH_ESP32)
				uint16_t use;
				if(WiFi.isConnected())
				{
					int value=(millis() & 8191);
					if(value>4095) value=8191-value;
					use=usLogTable256[(value>>5)+(value>>6)+64];
				}
				else
				{
					int value=(millis() & 511);
					if(value>255) value=511-value;
					use=usLogTable256[value];
				}
				ledcWrite(ucLedFadeChannel,bInvertLedBlink?4095-use:use);
#else
				uint16_t use;
				if(WiFi.isConnected())
				{
					static int counter=0;
					static int add=250;
					if(counter>65536) add=50;
					counter+=add;
					int value=counter & 8191;
					//int value=(millis() & 8191);
					if(value>4095) value=8191-value;
					use=(((value>>3)+(value>>4))+256);
				}
				else
				{
					int value=(millis() & 511);
					if(value>255) value=511-value;
					use=(value<<1);
				}

				analogWrite(iStatusLedPin,bInvertLedBlink?use:1023-use);
#endif
*/

			}
			else
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

}


char szExtCompileDate[32]={0};

void LeifHtmlMainPageCommonHeader(String & string)
{

	string.concat("<table>");

	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_BeforeFirstRow);


	string.concat("<tr>");

	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_InsideFirstRow);

	string.concat("<td>");

	string.concat("Uptime: ");

	String strUptime;
	LeifUptimeString(strUptime);
	string.concat(strUptime);


	string.concat("</td>");


	string.concat("<td>");
	string.concat("Compile time: ");

	string.concat(LeifGetCompileDate());

	string.concat("</td>");

	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_InsideFirstRowEnd);

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
	if(MyWiFiSTAClass::GetIsStatic()) string.concat("<font color=\"green\">");
	string.concat("IP: ");
	string.concat(WiFi.localIP().toString());
	if(MyWiFiSTAClass::GetIsStatic()) string.concat("</font>");
	string.concat("</td>");
	string.concat("</tr>");


	string.concat("<tr>");

	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_InsideLastRow);

	string.concat("<td>");

	int bssid_color=0;

	if(iWifiChannel>=0)
	{
		bssid_color=LeifIsBSSIDConnection()?1:-1;
	}

	switch(bssid_color)
	{
	case -1:
		string.concat("<font color=\"red\">");
		break;
	case 1:
		string.concat("<font color=\"green\">");
		break;
	}

	string.concat("BSSID: ");
	string.concat(WiFi.BSSIDstr());

	string.concat("&nbsp;&nbsp;&nbsp;CH: ");
	string.concat(WiFi.channel());

	if(bssid_color)
	{
		string.concat("</font>");
	}

	string.concat("</td>");
	string.concat("<td>");

	bool bWrongWifi=strcmp(wifi_ssid,WiFi.SSID().c_str());

	if(bWrongWifi) string.concat("<font color=\"red\">");
	string.concat("SSID: ");
	string.concat(WiFi.SSID());
	if(bWrongWifi) string.concat("</font>");

	string.concat("&nbsp;&nbsp;&nbsp;RSSI: ");
	string.concat(WiFi.RSSI());
	string.concat("</td>");
	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_InsideLastRowEnd);
	string.concat("</tr>");
	if(fnHttpMainTableCallback) fnHttpMainTableCallback(string,eHttpMainTable_AfterLastRow);

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



String LeifGetCompileDate()
{
	const char compile_date[] = __DATE__ " " __TIME__;
	if(strlen(szExtCompileDate))
	{
		return szExtCompileDate;
	}
	else
	{
		return compile_date;
	}

}


uint32_t LeifGetTotalWifiConnectionAttempts()
{
	return ulWifiTotalConnAttempts;
}

String LeifGetWifiStatus()
{
	return strWifiStatus;
}

void LeifSetAllowWifiConnection(bool bAllow)
{
	bAllowConnect=bAllow;

	if(bAllow)
	{
		ulWifiReconnect=millis()-15000;
	}

}

bool LeifGetAllowWifiConnection()
{
	return bAllowConnect;
}

bool IsLeifSetupBeginDone()
{
	return bLeifSetupBeginDone;
}

String GetArgument(const String & input, const char * argname)
{
	int mac=input.indexOf(argname);
	if(mac>=0)
	{
		mac+=strlen(argname);
		int space=input.indexOf(' ', mac);
		if(space>=0)
		{
			return input.substring(mac,space);
		}
		else
		{
			return input.substring(mac);
		}
	}
	return "";
}

bool LeifIsBSSIDConnection()	//returns true if we're connected an access point configured by BSSID+CH
{
	if(WiFi.status()!=WL_CONNECTED) return false;
	if(iWifiChannel>=0)
	{
		if(memcmp(WiFi.BSSID(),cBSSID,6))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	return false;
}

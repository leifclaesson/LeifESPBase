#include "LeifESPBaseMain.h"
#include "LeifESPBase.h"


#ifndef NO_OTA
#include <ArduinoOTA.h>
bool bUpdatingOTA = false;
#endif

#include "..\environment_setup.h"

#ifndef WDT_TIMEOUT
#define WDT_TIMEOUT 10
#endif

uint32_t serial_debug_rate=115200;

static unsigned long ulSecondCounterWiFiWatchdog = 0;


#if defined(ARDUINO_ARCH_ESP32)

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include "esp_chip_info.h"
#endif

#include <esp_task_wdt.h>

#include "core_version.h"

unsigned short usLEDLogTable256[256] =
{0, 0, 1, 1, 1, 2, 3, 3, 4, 5, 7, 8, 9, 11, 13, 15, 17, 19, 21, 23, 26, 28, 31, 34, 37, 40, 43, 46, 50, 53, 57, 61, 65, 69, 73, 78, 82, 87, 91, 96, 101, 106, 111, 117, 122, 128, 134, 139, 145, 152, 158, 164, 171, 177, 184, 191, 198, 205, 212, 220, 227, 235, 242, 250, 258, 266, 275, 283, 292, 300, 309, 318, 327, 336, 345, 355, 364, 374, 383, 393, 403, 413, 424, 434, 445, 455, 466, 477, 488, 499, 510, 522, 533, 545, 557, 569, 581, 593, 605, 617, 630, 643, 655, 668, 681, 695, 708, 721, 735, 748, 762, 776, 790, 804, 819, 833, 848, 862, 877, 892, 907, 922, 938, 953, 969, 984, 1000, 1016, 1032, 1048, 1064, 1081, 1097, 1114, 1131, 1148, 1165, 1182, 1199, 1217, 1234, 1252, 1270, 1288, 1306, 1324, 1342, 1361, 1380, 1398, 1417, 1436, 1455, 1474, 1494, 1513, 1533, 1552, 1572, 1592, 1612, 1632, 1653, 1673, 1694, 1715, 1735, 1756, 1777, 1799, 1820, 1841, 1863, 1885, 1907, 1929, 1951, 1973, 1995, 2018, 2040, 2063, 2086, 2109, 2132, 2155, 2179, 2202, 2226, 2249, 2273, 2297, 2321, 2346, 2370, 2395, 2419, 2444, 2469, 2494, 2519, 2544, 2569, 2595, 2621, 2646, 2672, 2698, 2724, 2751, 2777, 2804, 2830, 2857, 2884, 2911, 2938, 2965, 2993, 3020, 3048, 3076, 3103, 3131, 3160, 3188, 3216, 3245, 3273, 3302, 3331, 3360, 3389, 3419, 3448, 3477, 3507, 3537, 3567, 3597, 3627, 3657, 3688, 3718, 3749, 3780, 3811, 3842, 3873, 3904, 3936, 3967, 3999, 4031, 4062, 4095};
#endif


#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE //Serial used for USB CDC
extern USBCDC Serial;
#else
extern HardwareSerial Serial;
#endif


#ifdef USE_SERIAL1_DEBUG
extern HardwareSerial Serial1;
#endif

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
bool bEthernetInitialized=false;
#endif

/*
#if defined(ARDUINO_ARCH_ESP8266)

void OnWiFiDisconnectedEvent(const WiFiEventStationModeDisconnected & event)
{
	(void)event;
	WiFi.disconnect();
}
*/

/*
static void onWiFiEvent(WiFiEvent_t event)
{*/

	/*
	const char * pszReason="Unknown";

	switch(event)
	{
	default: break;
	case WIFI_EVENT_STAMODE_CONNECTED           : pszReason="STAMODE_CONNECTED"; break;
	case WIFI_EVENT_STAMODE_DISCONNECTED        :
		pszReason="STAMODE_DISCONNECTED";
		break;
	case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE     : pszReason="STAMODE_AUTHMODE_CHANGE"; break;
	case WIFI_EVENT_STAMODE_GOT_IP              : pszReason="STAMODE_GOT_IP"; break;
	case WIFI_EVENT_STAMODE_DHCP_TIMEOUT        : pszReason="STAMODE_DHCP_TIMEOUT"; break;
	case WIFI_EVENT_SOFTAPMODE_STACONNECTED     : pszReason="SOFTAPMODE_STACONNECTED"; break;
	case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED  : pszReason="SOFTAPMODE_STADISCONNECTED"; break;
	case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED   : pszReason="SOFTAPMODE_PROBEREQRECVED"; break;
	case WIFI_EVENT_MODE_CHANGE                 : pszReason="MODE_CHANGE"; break;
#if defined(ARDUINO_ARCH_ESP8266)
	case WIFI_EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP: pszReason="SOFTAPMODE_DISTRIBUTE_STA_IP"; break;
#endif
	case WIFI_EVENT_MAX                         : pszReason="ANY"; break;
	}

	csprintf("WiFi event %i (%s)\n",event,pszReason);
*/
/*	if(event==WIFI_EVENT_STAMODE_DISCONNECTED)
	{
		//Bug fixed as of 2021-08-08
		csprintf("WIFI_EVENT_STAMODE_DISCONNECTED\n");
		WiFi.disconnect(false);	//WHY is this not done internally? isConnected() still returns true if we don't manually disconnect, while RSSI() returns 31
	}

    //sEventsReceived[event]++;
}
*/
/*
#endif
*/



bool IsWiFiConnected()
{
#if defined(ARDUINO_ARCH_ESP8266)
	if(WiFi.RSSI()>0) return false;
	if(!WiFi.localIP().isSet()) return false;
#endif

	return WiFi.status() == WL_CONNECTED && WiFi.isConnected();
}

void WiFiWatchdog();

static uint32_t cpu_freq_khz = 0;

static int iStatusLedPin =
#ifdef LED_BUILTIN
LED_BUILTIN ;
#else
-1 ;
#endif
void LeifSetStatusLedPin(int iPin)
{
	iStatusLedPin = iPin;
}

int LeifGetStatusLedPin()
{
	return iStatusLedPin;
}


#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server(80);
#else
WebServer server(80);
#endif

void ScrollbackBuffer::alloc(uint16_t bytes)
{
	if(!bytes)
	{
		return;
	}

	bufsize = bytes;
	buf = (char *) malloc(bytes);
	memset(buf,0,bytes);

}

size_t ScrollbackBuffer::write(uint8_t value)
{
	if(!buf)
	{
		return 1;
	}

	buf[idx++] = value;
	idx %= bufsize;
	kept++;
	if(kept > bufsize)
	{
		kept = bufsize;
	}

	return 1;
}

size_t ScrollbackBuffer::write(const uint8_t * buffer, size_t writesize)
{
	if(!buf)
	{
		return writesize;
	}


	size_t remaining = writesize;

	//Serial.printf("WRITE %u\n",remaining);

	while(remaining)
	{
		uint16_t bytes_now = remaining;
		if(bytes_now > (size_t)(bufsize - idx))
		{
			bytes_now = bufsize - idx;
		}
		//csprintf("copy %i bytes to buffer at idx %i",bytes_now,idx);
		memcpy(&buf[idx], buffer, bytes_now);
		remaining -= bytes_now;
		buffer += bytes_now;
		idx += bytes_now;
		//Serial.printf("idx %u\n",idx);
		idx %= bufsize;
		kept += bufsize;
		if(kept > bufsize)
		{
			kept = bufsize;
		}
	}

	return writesize;
}

const char * ScrollbackBuffer::dataFirst()
{
	return &buf[idx];

}

size_t ScrollbackBuffer::sizeFirst()
{
	if(kept < bufsize)
	{
		return 0;
	}
	return bufsize - idx;
}

const char * ScrollbackBuffer::dataSecond()
{
	return &buf[0];
}
size_t ScrollbackBuffer::sizeSecond()
{
	return idx;
}



ScrollbackBuffer scrollbackBuffer;


bool g_bAllowSerialCommands=true;
void LeifSetAllowSerialCommands(bool bAllow)
{
	g_bAllowSerialCommands=bAllow;
}

bool LeifGetAllowSerialCommands()
{
	return g_bAllowSerialCommands;
}

void HandleCommandLine();

WiFiServer telnet(23);
WiFiClient telnetClients;

TelnetClientPrint telnetprint(&telnetClients);

size_t TelnetClientPrint::write(uint8_t value)
{
	if(value=='\n')
	{
		pDest->write('\r');
	}

	pDest->write(value);
	return 1;
}

size_t TelnetClientPrint::dbg(const uint8_t *buffer, size_t size)
{
	for(size_t i=0;i<size;i++)
	{
		uint8_t temp=buffer[i];
/*		switch(temp)
		{
		case '\r': temp='R'; break;
		case '\n': temp='N'; break;
		default: break;
		}*/
		pDest->write(temp);
	}
	//pDest->write(buffer,size);
	return size;
}

size_t TelnetClientPrint::write(const uint8_t *buffer, size_t size)
{

	size_t begin=0;

	//Serial.printf("write called with %i chars: '%s'\n",size,buffer);

	for(size_t i=0;i<size;i++)
	{
		//Serial.printf("%02x ",buffer[i]);

		if(buffer[i]=='\n' || i==size-1)
		{
			bool bLastCharNewLine=buffer[i]=='\n';
			int add=0;
			if(!bLastCharNewLine) add=1;	//if the last character is not a new line, we need to pass it through!
			//Serial.printf("!(%i-%i=%i)",i,begin,i-begin);
			pDest->write((const uint8_t *) &buffer[begin],i-begin+add);
			if(bLastCharNewLine) pDest->write((const uint8_t *) "\r\n",2);
			begin=i+1;
		}

	}

	//Serial.printf("*\n");
	return size;
//	return pDest->write(buffer,size);
}




int disconnectedClient = 1;

String strTelnetCmdBuffer;
String strSerialCmdBuffer;

#define WIFI_RECONNECT


#if defined(WIFI_RECONNECT)
unsigned long ulWifiReconnect = millis() - 15000;
int iWifiConnAttempts = 0;
uint32_t ulWifiTotalConnAttempts = 0;
#endif


LeifGetWiFiAPName fnGetWiFiAPName;
void LeifRegisterGetWiFiAPName(LeifGetWiFiAPName fn)
{
	fnGetWiFiAPName=fn;
}

static std::vector<LeifCommandCallback> vecOnCommand;

void LeifRegisterCommandCallback(LeifCommandCallback cb)
{
	vecOnCommand.push_back(cb);
}

void DoCommandCallback(const String & strCommand, eCommandLineSource source)
{
	for(size_t i=0;i<vecOnCommand.size();i++)
	{
		vecOnCommand[i](strCommand,source);
	}
}



static std::vector<LeifOnShutdownCallback> vecOnShutdown;

void LeifRegisterOnShutdownCallback(LeifOnShutdownCallback cb)
{
	vecOnShutdown.push_back(cb);
}


LeifHttpMainTableCallback fnHttpMainTableCallback;

void LeifSetHttpMainTableCallback(LeifHttpMainTableCallback cb)
{
	fnHttpMainTableCallback = cb;
}

LeifHttpMainTableCallback fnHttpMainTableExtraCallback;	//used by LeifESPBaseHomie and LeifESPBaseMQTT





void DoOnShutdownCallback(const char * pszReason)
{
	for(size_t i = 0; i < vecOnShutdown.size(); i++)
	{
		vecOnShutdown[i](pszReason);
	}

}

uint8_t cBSSID[6] = {0, 0, 0, 0, 0, 0};
int iWifiChannel = -1;
bool bAllowBSSID = false;

int8_t rssi_history[8];
int16_t rssi_sum;
uint8_t rssi_history_idx;
int8_t max_rssi=-128;

int8_t get_avg_rssi()
{
	return rssi_sum/(int) sizeof(rssi_history);
}

int iHealthDisconnects=0;

String g_lastWifiSSID = wifi_ssid;
String g_lastWifiPSK = wifi_key;
int g_lastWifiChannel = iWifiChannel;
uint8_t g_lastBSSID[6] = {0, 0, 0, 0, 0, 0};


String strWifiStatus = "Disconnected";

bool bAllowConnect = true;

const uint32_t * pMqttUptime=NULL;
const char * pMqttLibrary=NULL;

String strProjectName;

bool ParseMacAddress(const char * pszMAC, uint8_t * cMacOut)
{
	if(strlen(pszMAC) == 17)
	{
		const char * temp = pszMAC;
		for(int i = 0; i < 6; i++)
		{
			cMacOut[i] = 0;
			for(int j = 0; j < 2; j++)
			{
				unsigned char hex = 0;
				if(*temp >= '0' && *temp <= '9')
				{
					hex = *temp - '0';
				}
				else if(*temp >= 'A' && *temp <= 'F')
				{
					hex = *temp - 'A' + 0xa;
				}
				else if(*temp >= 'a' && *temp <= 'f')
				{
					hex = *temp - 'a' + 0xa;
				}
				cMacOut[i] |= (hex << (4 * (1 - j)));
				temp++;
			}
			temp++;
		}
		return true;
	}
	else
	{
		memset(cMacOut, 0, 6);
		return false;
	}
}

static IPAddress ipAccessPoint;

IPAddress LeifGetAccessPointIP()
{
	if(iWifiChannel >= 0 && !memcmp(WiFi.BSSID(), cBSSID, 6))
	{
		return ipAccessPoint;
	}

	return IPAddress(0, 0, 0, 0);
}

void LeifSetupBSSID(const char * pszBSSID, int ch, const char * pszAccessPointIP)
{



	if(pszBSSID && ParseMacAddress(pszBSSID, cBSSID))
	{
		bAllowBSSID = true;
		iWifiChannel = ch;
		ipAccessPoint.fromString(pszAccessPointIP);
	}
	else
	{
		iWifiChannel = -1;
		ipAccessPoint = IPAddress(0, 0, 0, 0);
	}
}


bool bNewWifiConnection = false;

bool IsNewWifiConnection()
{
	//returns true ONCE after a new wifi connection has been established
	bool bRet = bNewWifiConnection;
	bNewWifiConnection = false;

	return bRet;
}


void LeifSetProjectName(const char * szProjectName)
{
	strProjectName=szProjectName;
}

const String & LeifGetProjectName()
{
	return strProjectName;
}

/*
class MyWiFiSTAClass:
#if defined(ARDUINO_ARCH_ESP8266)
	public ESP8266WiFiSTAClass
#else
	public WiFiSTAClass
#endif
{
public:
	static bool GetIsStatic()  	//ugly workaround to access protected flag in the WiFiSTAClass. We need to see whether we're on static IP or not.
	{
		void * mypointer = &WiFi;
		return ((MyWiFiSTAClass *) mypointer)->_useStaticIp;
	}
};
*/

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
		sprintf(temp, PSTR("%02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
	else
	{
		sprintf(temp, PSTR("*MAC IS NULL PTR*"));
	}
	return temp;
}

void ResetRSSIHistory()
{
	rssi_sum=-128*(int16_t) sizeof(rssi_history);
	for(int i=0;i<(int) sizeof(rssi_history);i++)
	{
		rssi_history[i]=-128;
	}
	rssi_history_idx=0;
}

void SetupWifiInternal()
{
#if defined(ARDUINO_ARCH_ESP8266)
	WiFi.hostname(GetHostName());
#else
	WiFi.setHostname(GetHostName());
#endif
	ulSecondCounterWiFiWatchdog=0;
#if defined(WIFI_RECONNECT)
#if !defined(ARDUINO_ARCH_ESP32) && ESP_ARDUINO_VERSION_MAJOR < 3
	WiFi.setAutoConnect(false);
#endif
	WiFi.setAutoReconnect(false);

	ResetRSSIHistory();


	if(iWifiChannel >= 0 && bAllowBSSID)
	{
		csprintf(PSTR("WiFi attempting to connect to %s at BSSID %s, Ch %i (attempt %i)...\n"), wifi_ssid, MacToString(cBSSID).c_str(), iWifiChannel, iWifiConnAttempts);
		WiFi.begin(wifi_ssid, wifi_key, iWifiChannel, cBSSID, true);

		g_lastWifiSSID = wifi_ssid;
		g_lastWifiPSK = wifi_key;
		g_lastWifiChannel = iWifiChannel;
		memcpy(g_lastBSSID, cBSSID, sizeof(g_lastBSSID));

		strWifiStatus = wifi_ssid;
		strWifiStatus += " ";
		strWifiStatus += MacToString(cBSSID);
	}
	else
	{
		//		csprintf("No BSSID configured\n");

		const char * use_ssid = wifi_ssid;
		const char * use_key = wifi_key;

		if(backup_ssid && strlen(backup_ssid))
		{
			int x = iWifiConnAttempts / 2;
			if(x && (x & 1))
			{
				use_ssid = backup_ssid;
				use_key = backup_key;
			}
		}

		csprintf(PSTR("WiFi attempting to connect to %s (attempt %i)...\n"), use_ssid, iWifiConnAttempts);
		WiFi.begin(use_ssid, use_key);

		g_lastWifiSSID = use_ssid;
		g_lastWifiPSK = wifi_key;
		g_lastWifiChannel = -1;
		memset(g_lastBSSID, 0, sizeof(g_lastBSSID));

		strWifiStatus = "SSID ";
		strWifiStatus += use_ssid;

	}
#else
	//	csprintf("Using Auto Reconnect\n");
	WiFi.begin(wifi_ssid, wifi_key);
	WiFi.setAutoConnect(true);
	WiFi.setAutoReconnect(true);
	strWifiStatus = "SSID ";
	strWifiStatus += wifi_ssid;

	g_lastWifiSSID = wifi_ssid;
	g_lastWifiPSK = wifi_key;
	g_lastWifiChannel = -1;
	memset(g_lastBSSID, 0, sizeof(g_lastBSSID));

#endif
}





bool bConsoleInitDone = false;
void LeifSetupConsole(uint16_t _scrollback_bytes)
{
	if(bConsoleInitDone)
	{
		return;
	}

#ifndef NO_SERIAL_DEBUG
#ifdef USE_SERIAL1_DEBUG
	Serial1.begin(serial_debug_rate);
#else
	Serial.begin(serial_debug_rate);
#endif
#endif

	scrollbackBuffer.alloc(_scrollback_bytes);

	bConsoleInitDone = true;

	csprintf("\nBooting...\n");

};

#ifndef NO_FADE_LED
uint8_t ucLedFadeChannel = 15;	//ESP32 ledc
static bool bAllowLedFade = true;
#if defined(ARDUINO_ARCH_ESP32)
int iAnalogWriteBits = 12;
#else
int iAnalogWriteBits = 10;
#endif

int iLedBrightnessScale=12;

void LeifSetLedBrightness(int scale)
{
	if(scale<0) scale=0; else if(scale>3) scale=3;
	iLedBrightnessScale=13-scale;
}

void LeifSetAllowFadeLed(bool bAllowFade, int analogWriteBits)
{
	bAllowLedFade = bAllowFade;
	iAnalogWriteBits = analogWriteBits;
}
#endif

bool bLeifSetupBeginDone = false;
void LeifSetupBegin()
{
	while(IsLeifSetupBeginDone())
	{
		delay(500);
		csprintf(PSTR("LeifSetupBegin() called twice\n"));
	}

	bLeifSetupBeginDone = true;

	LeifSetupConsole();

#if defined(ARDUINO_ARCH_ESP8266)
	analogWriteRange(1023);
#endif

	if(iStatusLedPin >= 0)
	{
		pinMode(iStatusLedPin, OUTPUT);
		digitalWrite(iStatusLedPin, LOW);
	}


/*
#if defined(ARDUINO_ARCH_ESP8266)
	//WiFi.onEvent(onWiFiEvent, WIFI_EVENT_ANY);
	WiFi.onStationModeDisconnected(OnWiFiDisconnectedEvent);
#endif
*/

	ResetRSSIHistory();

	WiFi.mode(WIFI_STA);

	csprintf(PSTR("WiFi: %s\n"), LeifGetAllowWifiConnection()?PSTR("ENABLED"):PSTR("DISABLED"));
	csprintf(PSTR("Using WiFi SSID: %s\n"), wifi_ssid);
	csprintf(PSTR("MAC address: %s\n"), WiFi.macAddress().c_str());
	csprintf(PSTR("Host name: %s\n"), GetHostName());

#if defined(ARDUINO_ARCH_ESP32)
#ifndef NO_OTA
	ArduinoOTA.setPort(8266);
#endif
#endif


#ifndef NO_OTA
	ArduinoOTA.setHostname(GetHostName());
	ArduinoOTA.onStart([]()   // switch off all the PWMs during upgrade
	{
		csprintf(PSTR("OTA update starting\n"));
		if(iStatusLedPin >= 0)
		{
			pinMode(iStatusLedPin, OUTPUT);
#if defined(ARDUINO_ARCH_ESP32)
#if ESP_ARDUINO_VERSION_MAJOR < 3
			ledcDetachPin(iStatusLedPin);
#else
			ledcDetach(iStatusLedPin);
#endif
			digitalWrite(iStatusLedPin, HIGH);
#else
			digitalWrite(iStatusLedPin, HIGH);
#endif
		}
		DoOnShutdownCallback("OTA");
		bUpdatingOTA = true;

		telnet.close();
		delay(250);

	});


	ArduinoOTA.onProgress([](unsigned int param1, unsigned int param2)
	{
		(void)param1;
		(void)param2;
#if defined(ARDUINO_ARCH_ESP32)
		//csprintf("ONPROGRESS: wdt reset from core %i\n",xPortGetCoreID());
		esp_task_wdt_reset();
#endif
	}
	);



	ArduinoOTA.onEnd([]()   // do a fancy thing with our board led at end
	{

		csprintf(PSTR("OTA update done\n"));
		DoOnShutdownCallback("OTA_DONE");

#ifdef NO_FADE_LED
		if(iStatusLedPin >= 0)
		{
			for(int i=0;i<10;i++)
			{
				digitalWrite(iStatusLedPin, (i & 1));
			}
		}

#else
#if defined(ARDUINO_ARCH_ESP32)
		if(iStatusLedPin >= 0)
		{
#if ESP_ARDUINO_VERSION_MAJOR < 3
			ledcSetup(ucLedFadeChannel, 500, 12);
			ledcAttachPin(iStatusLedPin, ucLedFadeChannel);
#else
			ledcAttachChannel(iStatusLedPin,500, 12, ucLedFadeChannel);
#endif
		}
#else
		analogWriteRange(1023);
#endif

		for(int i = 0; i < 25; i++)
		{
			if(iStatusLedPin >= 0)
			{
#if defined(ARDUINO_ARCH_ESP32)
				int value = (i * 250) % 1001;
				float temp = 1.0f - (value * 0.001f);
				temp *= temp;
				value = temp * 4095.0f;
				ledcWrite(ucLedFadeChannel, value);
#else
				int value = (i * 200) % 1001;
				analogWrite(iStatusLedPin, value);
#endif
				delay(40);
			}
		}
		if(iStatusLedPin >= 0)
		{
#if defined(ARDUINO_ARCH_ESP32)
#if ESP_ARDUINO_VERSION_MAJOR < 3
			ledcDetachPin(iStatusLedPin);
#else
			ledcDetach(iStatusLedPin);
#endif
			digitalWrite(iStatusLedPin, LOW);
#else
			digitalWrite(iStatusLedPin, HIGH);
#endif
		}
#endif

		csprintf(PSTR("OTA update triggering restart\n"));
		delay(500);
		ESP.restart();
		delay(2000);
	});

	ArduinoOTA.onError([](ota_error_t error)
	{
		if(error==OTA_AUTH_ERROR)
		{
			csprintf("OTA AUTH error!\n");
		}
		else
		{
			DoOnShutdownCallback("OTA_FAILED");
			csprintf(PSTR("OTA update FAILED (%i)\n"),error);
			ESP.restart();
		}
	});
#endif

	telnet.begin();
	telnet.setNoDelay(true);
	csprintf(PSTR("Telnet server started\n"));

	server.on("/ping", []()
	{
		char ping_response[128];
		sprintf(ping_response, PSTR("pong from %s"), GetHostName());
		server.send(200, PSTR("text/plain"), ping_response);
	});

	server.on("/sysinfo", []()
	{
#ifdef MMU_EXTERNAL_HEAP
		ESP.setExternalHeap();
		uint32_t heapFreeExt=ESP.getFreeHeap();
		ESP.resetHeap();
#endif
#ifdef MMU_IRAM_HEAP
		ESP.setIramHeap();
		uint32_t heapFreeIram=ESP.getFreeHeap();
		ESP.resetHeap();
#endif
//		ESP.setDramHeap();
		uint32_t heapFreeDram=ESP.getFreeHeap();
//		ESP.resetHeap();


		String s;

		char temp[128];

		uint32_t ideSize = ESP.getFlashChipSize();
		FlashMode_t ideMode = ESP.getFlashChipMode();

#if defined(ARDUINO_ARCH_ESP8266)
		sprintf(temp, PSTR("SOC..............: ESP8266\n"));
		s += temp;
		sprintf(temp, PSTR("SDK..............: %s, %s\n"),ARDUINO_ESP8266_RELEASE,ESP.getSdkVersion());
		s += temp;
#endif

#if defined(ARDUINO_ARCH_ESP32)
		{
			esp_chip_info_t chip_info;

			esp_chip_info(&chip_info);
			sprintf(temp, "SOC..............: ESP32 (%d cores), WiFi%s%s, ",
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
			s += temp;

			sprintf(temp, "rev %d\n", chip_info.revision);
			s += temp;
			sprintf(temp, "SDK..............: %s (%s)\n",ARDUINO_ESP32_RELEASE,ESP.getSdkVersion());
			s += temp;
		}
#endif


		sprintf(temp, PSTR("Clock Freq.......: %.01f MHz\n"), cpu_freq_khz / 1000.0f);
		s += temp;

#if defined(ARDUINO_ARCH_ESP32)

		uint64_t efusemac = ESP.getEfuseMac();
		sprintf(temp, "Efuse MAC........: %s\n",
		MacToString((uint8_t *) &efusemac).c_str()
		       );
		s += temp;
#endif

#if defined(ARDUINO_ARCH_ESP8266)
		sprintf(temp, PSTR("Flash real id....: %08X\n"), ESP.getFlashChipId());
		s += temp;
		sprintf(temp, PSTR("Flash real size..: %u bytes\n\n"), ESP.getFlashChipRealSize());
		s += temp;

		sprintf(temp, PSTR("Flash ide size...: %u bytes\n"), ideSize);
		s += temp;
#endif
#if defined(ARDUINO_ARCH_ESP32)
		sprintf(temp, "Flash size.......: %u bytes\n", ideSize);
		s += temp;
#endif
		sprintf(temp, PSTR("Flash ide speed..: %u Hz\n"), ESP.getFlashChipSpeed());
		s += temp;
		sprintf(temp, PSTR("Flash ide mode...: %s\n\n"), (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : PSTR("UNKNOWN")));
		s += temp;

#ifdef MMU_EXTERNAL_HEAP
		sprintf(temp, "Heap free (Ext).: %i\n", heapFreeExt);
		s += temp;
#endif
#ifdef MMU_IRAM_HEAP
		sprintf(temp, "Heap free (IRAM).: %i\n", heapFreeIram);
		s += temp;
#endif
		sprintf(temp, PSTR("Heap free (DRAM).: %i\n"), heapFreeDram);
		s += temp;
#if defined(ARDUINO_ARCH_ESP8266)
#ifndef NO_MAX_FREE_BLOCKSIZE
		sprintf(temp, PSTR("Heap max alloc...: %i\n"), ESP.getMaxFreeBlockSize());
		s += temp;
#endif
		sprintf(temp, PSTR("Heap frag........: %i\n"), ESP.getHeapFragmentation());
		s += temp;
#else
		sprintf(temp, "Heap max alloc...: %i\n", ESP.getMaxAllocHeap());
		s += temp;
#endif

		s += "\n";

		if(strProjectName.length())
		{
			sprintf(temp, PSTR("Project..........: %s\n"), strProjectName.c_str());
			s += temp;
		}

		if(pMqttLibrary)
		{
			sprintf(temp, PSTR("MQTT.............: %s\n"), pMqttLibrary);
			s += temp;
		}




		server.send(200, PSTR("text/plain"), s);
	});


#if defined(ARDUINO_ARCH_ESP32)
#ifndef NO_FADE_LED
	if(bAllowLedFade)
	{
#if ESP_ARDUINO_VERSION_MAJOR < 3
		ledcSetup(ucLedFadeChannel, 500, 12);
		ledcAttachPin(iStatusLedPin, ucLedFadeChannel);
#else
		ledcAttachChannel(iStatusLedPin,500,12,ucLedFadeChannel);
#endif

	}
#endif
#endif


}

void LeifSetupEnd()
{


	server.begin();
#ifndef NO_OTA
	ArduinoOTA.begin();
#endif

#if defined(ARDUINO_ARCH_ESP32)
#if ESP_ARDUINO_VERSION_MAJOR < 3
	esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
#else
	esp_task_wdt_deinit();
	esp_task_wdt_config_t cfg;
	cfg.timeout_ms=WDT_TIMEOUT * 1000;
	cfg.idle_core_mask=3;
	cfg.trigger_panic=true;
	esp_task_wdt_init(&cfg);
#endif
	esp_task_wdt_add(NULL); //add current thread to WDT watch
#endif

	if(strProjectName.length())
	{
		csprintf(PSTR("Firmware: %s\n"),strProjectName.c_str());
	}

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
	//WiFi.onEvent(WiFiEvent);

	#ifdef ETH_EXT_CLK
	pinMode(16,OUTPUT);
	digitalWrite(16,HIGH);
	#endif

	bEthernetInitialized=ETH.begin();
	ETH.setHostname(GetHostName());
#endif

}

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
bool LeifIsEthernetInitialized()
{
	return bEthernetInitialized;
}
#endif


unsigned long ulLastLoopMillis = 0;

static unsigned long ulLastLoopSecond = 0;
static unsigned long ulLastLoopHalfSecond = 0;
static unsigned long ulLastLoopQuarterSecond = 0;
static unsigned long ulLastLoopDeciSecond = 10;

static bool bInterval50 = false;
static bool bInterval100 = false;
static bool bInterval250 = false;
static bool bInterval500 = false;
static bool bInterval1000 = false;
static bool bInterval10s = false;

static uint32_t ulSecondCounter = 0;

uint32_t seconds()
{
	return ulSecondCounter;
}

const uint32_t * pSeconds()
{
	return &ulSecondCounter;
}


static uint32_t ulSecondCounterWiFi = 0;


uint32_t secondsWiFi()
{
	return ulSecondCounterWiFi;
}

const uint32_t * pSecondsWiFi()
{
	return &ulSecondCounterWiFi;
}


#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
static uint32_t ulSecondCounterEthernet = 0;

uint32_t secondsEthernet()
{
	return ulSecondCounterEthernet;
}

const uint32_t * pSecondsEthernet()
{
	return &ulSecondCounterEthernet;
}
#endif


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

void LeifSecondsToShortUptimeString(String & string, unsigned long ulSeconds)
{
	char temp[8];
	if(ulSeconds >= (86400 * 365))
	{
		sprintf(temp, "%luy", ulSeconds / (86400 * 365));	//years
	}
	if(ulSeconds >= (86400 * 30))
	{
		sprintf(temp, "%lum", ulSeconds / (86400 * 30));	//months
	}
	else if(ulSeconds >= 86400)
	{
		sprintf(temp, "%lud", ulSeconds / 86400);	//days
	}
	else if(ulSeconds >= 3600)
	{
		sprintf(temp, "%luh", ulSeconds / 3600);	//hours
	}
	else if(ulSeconds >= 60)
	{
		sprintf(temp, "%lum", ulSeconds / 60);	//minutes
	}
	else
	{
		sprintf(temp, "%lus", ulSeconds);	//seconds
	}

	string = temp;

}

void LeifSecondsToUptimeString(String & string, unsigned long ulSeconds)
{

	unsigned long ulDays = ulSeconds / 86400;

	if(ulDays > 30)
	{
		if(ulDays > 365)
		{
			char temp[32];
			sprintf(temp, PSTR("%luy %lud"), ulDays / 366, ulDays % 366);
			string = temp;
		}
		else
		{
			unsigned long ulHours = (ulSeconds - (ulDays * 86400)) / 3600;
			char temp[32];
			sprintf(temp, PSTR("%lud %luh"), ulDays, ulHours);
			string = temp;
		}
	}
	else
	{
		char temp[128];
		if(ulDays == 0)
		{
			sprintf(temp, PSTR("%02lu:%02lu:%02lu"),
			(ulSeconds / 3600) % 24,
			(ulSeconds / 60) % 60,
			(ulSeconds) % 60
			       );
		}
		else
		{
			sprintf(temp, PSTR("%lud %02lu:%02lu:%02lu"),
			(ulSeconds / 86400),
			(ulSeconds / 3600) % 24,
			(ulSeconds / 60) % 60,
			(ulSeconds) % 60
			       );
		}
		string = temp;
	}
}

void LeifUptimeString(String & string)
{
	LeifSecondsToUptimeString(string, seconds());
}


static bool bAllowLedWrite = true;
static bool bInvertLedBlink = false;

void LeifSetSuppressLedWrite(bool bSuppress)
{
	bAllowLedWrite = !bSuppress;
}

void LeifSetInvertLedBlink(bool bInvertLed)
{
	bInvertLedBlink = bInvertLed;
}

bool LeifGetInvertLedBlink()
{
	return bInvertLedBlink;
}

static uint32_t ulRestartTimestamp=0;

void LeifScheduleRestart(uint32_t ms)
{
	ulRestartTimestamp=millis()+ms;
}

static uint32_t ulReconnectTimestamp=0;

void LeifScheduleReconnect(uint32_t ms)
{
	ulReconnectTimestamp=millis()+ms;
}

void LeifLoop()
{

	static bool bFirst = true;
	if(bFirst)
	{
		bFirst = false;

		ulLastLoopMillis = millis();
		ulLastLoopSecond = millis();
		ulLastLoopHalfSecond = millis();
		ulLastLoopQuarterSecond = millis();
		ulLastLoopDeciSecond = millis();
	}

	if(((int32_t)(millis() - ulLastLoopMillis)) < 50)
	{
		if(bInterval50)
		{
			bInterval10s = false;
			bInterval1000 = false;
			bInterval500 = false;
			bInterval250 = false;
			bInterval100 = false;
			bInterval50 = false;
		}
		return;
	}
	bInterval50 = true;

	ulLastLoopMillis = millis();

#ifndef NO_OTA
	ArduinoOTA.handle();
	if(bUpdatingOTA) return;
#endif
	server.handleClient();
#if defined(ARDUINO_ARCH_ESP8266)
#endif


#ifdef USE_HOMIE
	homie.Loop();
#endif


	if(ulRestartTimestamp && (int32_t) (millis()-ulRestartTimestamp)>0)
	{
		ulRestartTimestamp=0;
		ESP.restart();
	}


	if(ulReconnectTimestamp && (int32_t) (millis()-ulReconnectTimestamp)>0)
	{
		ulReconnectTimestamp=0;
		WiFi.disconnect(0);
	}


	HandleCommandLine();


	if((int)(ulLastLoopMillis - ulLastLoopSecond) >= 1000)
	{
		ulLastLoopSecond += 1000;
		bInterval1000 = true;
		ulSecondCounter++;

		static uint32_t last_cyclecount = 0;
		static uint32_t last_cc_millis = 0;

		uint32_t mdiff = millis() - last_cc_millis;

		if(mdiff)
		{
			cpu_freq_khz = (ESP.getCycleCount() - last_cyclecount) / mdiff;
		}

		last_cyclecount = ESP.getCycleCount();
		last_cc_millis = millis();

		if(IsWiFiConnected())
		{
			ulSecondCounterWiFi++;
			ulSecondCounterWiFiWatchdog=0;
		}
		else
		{
			ulSecondCounterWiFi = 0;
			ulSecondCounterWiFiWatchdog++;
		}

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
		uint32_t _eth_ip=ETH.localIP();
		if(_eth_ip!=0)
		{
			if(!ulSecondCounterEthernet)
			{
				csprintf(PSTR("Ethernet IP: %s\n"), ETH.localIP().toString().c_str());
			}
			ulSecondCounterEthernet++;
		}
		else
		{
			ulSecondCounterEthernet = 0;
		}
#endif

		WiFiWatchdog();


#if defined(ARDUINO_ARCH_ESP32)
		//csprintf("LEIFLOOP: wdt reset from core %i\n",xPortGetCoreID());
		esp_task_wdt_reset();
#endif


	}
	else
	{
		bInterval1000 = false;
	}

	if(bInterval1000 && ulSecondCounter % 10 == 0)
	{
		bInterval10s = true;
	}
	else
	{
		bInterval10s = false;
	}

	if((int)(ulLastLoopMillis - ulLastLoopHalfSecond) >= 500)
	{
		ulLastLoopHalfSecond += 500;
		bInterval500 = true;
	}
	else
	{
		bInterval500 = false;

	}


	if((int)(ulLastLoopMillis - ulLastLoopQuarterSecond) >= 250)
	{
		ulLastLoopQuarterSecond += 250;
		bInterval250 = true;
	}
	else
	{
		bInterval250 = false;

	}

	if((int)(ulLastLoopMillis - ulLastLoopDeciSecond) >= 100)
	{
		ulLastLoopDeciSecond += 100;
		bInterval100 = true;
	}
	else
	{
		bInterval100 = false;
	}

	if(WiFi.getMode() & WIFI_STA)
	{

		static bool bIpPrinted = false;
		if(IsWiFiConnected())
		{
			if(!bIpPrinted)
			{
				bIpPrinted = true;
				strWifiStatus = /*MyWiFiSTAClass::GetIsStatic() ? "*" : */"";
				strWifiStatus += WiFi.localIP().toString();

				csprintf(PSTR("IP: %s, SSID %s, BSSID %s, Ch %i, RSSI %i\n"), WiFi.localIP().toString().c_str(), /*MyWiFiSTAClass::GetIsStatic() ? " (static)" : " (DHCP)", */WiFi.SSID().c_str(), WiFi.BSSIDstr().c_str(), WiFi.channel(), WiFi.RSSI());
				csprintf(PSTR("Gateway: %s\n"), WiFi.gatewayIP().toString().c_str());
				iWifiConnAttempts = 0;
				bAllowBSSID = true;
				bNewWifiConnection = true;

			}

		}
		else if(LeifGetAllowWifiConnection())
		{
			bIpPrinted = false;
#if defined(WIFI_RECONNECT)
			uint32_t ulReconnectMs=15000;
			if(iWifiConnAttempts>5) ulReconnectMs=30000;	//reconnect more slowly
			if(iWifiConnAttempts>10) ulReconnectMs=60000;	//reconnect more slowly
			if(iWifiConnAttempts>15) ulReconnectMs=120000;	//reconnect more slowly

			if((millis() - ulWifiReconnect) >= ulReconnectMs)
			{

#if defined(ARDUINO_ARCH_ESP8266)
				static bool bDoDisconnect=false;
				if(bDoDisconnect)
				{
					uint32_t period=(ulReconnectMs>>3);		//disconnect 1/8 of the delay period before reconnecting

					csprintf(PSTR("Force WiFi Disconnect %u ms ahead of reconnect!\n"),period);
					WiFi.disconnect(false);

					ulWifiReconnect = millis() - (ulReconnectMs - period);

					bDoDisconnect=false;

				}
				else
				{
					bDoDisconnect=true;
#else
				{
#endif

					ulWifiReconnect = millis();

#if defined(ARDUINO_ARCH_ESP32)
					//for some reason the ESP32 _always_ fails the first attempt, so let's just skip ahead to the second attempt and save some time.
					if(!iWifiConnAttempts)
					{
						ulWifiReconnect = millis() - 12000;
					}

					if(iWifiConnAttempts >= 2 && bAllowBSSID)
					{
						bAllowBSSID = false;
					}
#else
					if(iWifiConnAttempts >= 1 && bAllowBSSID)
					{
						bAllowBSSID = false;
					}

#endif


					SetupWifiInternal();

					WiFi.reconnect();
					iWifiConnAttempts++;
					ulWifiTotalConnAttempts++;
				}
				ulSecondCounterWiFi = 0;
			}
#endif
		}
		else
		{
			g_lastWifiSSID = PSTR("Disabled");
			memset(g_lastBSSID, 0, 6);
			g_lastWifiChannel = 0;
			ulSecondCounterWiFi = 0;


		}
	}

	if(telnet.hasClient())
	{
		if(!telnetClients || !telnetClients.connected())
		{
			if(telnetClients)
			{
				telnetClients.stop();
				//csprintf("Telnet Client Stop\n");
			}
			telnetClients = telnet.available();

			String strUptime;
			LeifUptimeString(strUptime);


			telnetprint.printf("\n");
			for(int k=0;k<2;k++)
			{
				for(int i=0;i<7;i++)
				{
					telnetprint.write((uint8_t *) "==========",10);
				}
				telnetprint.write((uint8_t *) "=========\n",10);
				if(k>0) break;
				telnetprint.printf(PSTR("Welcome to %s, ip %s! Uptime: %s\n"), GetHostName(), WiFi.localIP().toString().c_str(), strUptime.c_str());
			}

#ifndef NO_SERIAL_DEBUG
			Serial.printf(PSTR("New telnet connection from %s, uptime %s\n"), telnetClients.remoteIP().toString().c_str(), strUptime.c_str());
#endif

#ifdef USE_SERIAL1_DEBUG
			Serial1.printf(PSTR("New telnet connection from %s, uptime %s\n"), telnetClients.remoteIP().toString().c_str(), strUptime.c_str());
#endif

			telnetprint.write((const uint8_t *) scrollbackBuffer.dataFirst(), scrollbackBuffer.sizeFirst());
			telnetprint.write((const uint8_t *) scrollbackBuffer.dataSecond(), scrollbackBuffer.sizeSecond());

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
				csprintf(PSTR("Telnet client disconnected.\n"));
				telnetClients.stop();
				disconnectedClient = 1;
			}
		}
	}

	if(iStatusLedPin >= 0 && bAllowLedWrite)
	{
#ifndef NO_FADE_LED
		if(bAllowLedFade)
		{

			uint16_t use;

			if(WiFi.getMode() & WIFI_AP)
			{

				int time = ((millis() % 1100) * 768) / 1100;

				int fadeval=time & 255;
				if(fadeval > 127)
				{
					fadeval = 255 - fadeval;
				}

				if(time>=512) fadeval=0;

				fadeval+=128;

#if defined(ARDUINO_ARCH_ESP32)
				use = usLEDLogTable256[fadeval>>1];
#else
				use = (fadeval << 1);
#endif

			}
			else
			{

				if(IsWiFiConnected() || !LeifGetAllowWifiConnection())
				{
					static int counter = 0;
					static int add = 250;
					if(counter > 65536)
					{
						add = 50;
					}
					counter += add;
					int value = counter & 8191;
					//int value=(millis() & 8191);
					if(value > 4095)
					{
						value = 8191 - value;
					}

	#if defined(ARDUINO_ARCH_ESP32)
					use = usLEDLogTable256[(value>>7)+(value>>8)+48];
	#else
					use = ((value >> 3) + 256);
	#endif

					if(!bAllowConnect)
					{
						use >>= 1;
					}

				}
				else
				{
					int value = (millis() & 511);
					if(value > 255)
					{
						value = 511 - value;
					}
	#if defined(ARDUINO_ARCH_ESP32)
					use = usLEDLogTable256[value>>1];
	#else
					use = (value << 1);
	#endif
				}
			}

			//if(Interval100()) csprintf("use %i\n",use);

			if(iAnalogWriteBits < iLedBrightnessScale)
			{
				use >>= (iLedBrightnessScale - iAnalogWriteBits);
			}
			else if(iAnalogWriteBits > iLedBrightnessScale)
			{
				use <<= (iAnalogWriteBits - iLedBrightnessScale);
			}


#if defined(ARDUINO_ARCH_ESP32)
			ledcWrite(ucLedFadeChannel, bInvertLedBlink ? ((1 << iAnalogWriteBits) - 1) - use : use);
			//if(Interval100()) csprintf("use after=%i %i\n",use,bInvertLedBlink?((1<<iAnalogWriteBits)-1)-use:use);
#else
			analogWrite(iStatusLedPin, bInvertLedBlink ? use : ((1 << iAnalogWriteBits) - 1) - use);

			//if(Interval100()) csprintf("use after=%i %i\n",use,bInvertLedBlink?use:((1<<iAnalogWriteBits)-1)-use);
#endif

		}
		else
#endif
		{
			int interval = 15000;
			if(!IsWiFiConnected())
			{
				interval = 500;
			}

			unsigned long thresh = 100;
			if(bInvertLedBlink)
			{
				thresh = 50;
			}

			bool bBlink = (millis() % interval) < thresh ? false : true;

#if defined(ARDUINO_ARCH_ESP8266)
			bBlink ^= true;
#endif

			bBlink ^= bInvertLedBlink;

			digitalWrite(iStatusLedPin, bBlink);
		}

	}

}


char szVersionText[12] = {0};

char szExtCompileDate[32] = {0};

void LeifHtmlMainPageCommonHeader(String & string)
{

	string.concat(PSTR("<table>"));

	if(fnHttpMainTableCallback)
	{
		fnHttpMainTableCallback(string, eHttpMainTable_BeforeFirstRow);
	}

	if(fnHttpMainTableExtraCallback)
	{
		fnHttpMainTableExtraCallback(string,eHttpMainTable_BeforeFirstRow);
	}

	string.concat(PSTR("<tr><td colspan=\"1\">"));

	string.concat(PSTR("Uptime: "));

	String strUptime;
	LeifUptimeString(strUptime);
	string.concat(strUptime);

	string.concat(PSTR("</td><td colspan=\"2\">"));
	string.concat(PSTR("Host: "));
	string.concat(GetHostName());

	string.concat(PSTR("</td><td colspan=\"2\">"));


#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
	string.concat(PSTR("ETH: "));
	if(ETH.linkUp())
	{
		uint32_t _eth_ip=ETH.localIP();
		if(_eth_ip!=0)
		{
			string.concat(ETH.localIP().toString());
		}
		else
		{
			string.concat(PSTR("Awaiting IP"));
		}
	}
	else
	{
		string.concat(PSTR("Down"));
	}
#endif


	if(LeifGetAllowWifiConnection())
	{
	#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
		string.concat(PSTR(", WiFi: "));
	#else
		string.concat(PSTR("IP: "));
	#endif

/*		if(MyWiFiSTAClass::GetIsStatic())
		{
			string.concat(PSTR("<font color=\"green\">"));
		}*/
		string.concat(WiFi.localIP().toString());
/*		if(MyWiFiSTAClass::GetIsStatic())
		{
			string.concat(PSTR("</font>"));
		}*/
		string.concat(PSTR("</td>"));
	}
	string.concat(PSTR("</tr>"));

	uint32_t heapFree = ESP.getFreeHeap();
	String temp;

	string.concat(PSTR("<tr><td colspan=\"3\">"));

	{
		bool bFirst=true;

	#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
		if(bFirst) bFirst=false; else string.concat(PSTR(", "));
		string.concat(PSTR("ETH: "));
		LeifSecondsToUptimeString(temp,ulSecondCounterEthernet);
		string.concat(temp);
	#endif

		if(LeifGetAllowWifiConnection())
		{
			if(bFirst) bFirst=false; else string.concat(PSTR(", "));
			string.concat(PSTR("WiFi: "));
			LeifSecondsToUptimeString(temp,ulSecondCounterWiFi);
			string.concat(temp);
		}

		if(pMqttUptime)
		{
			if(bFirst) bFirst=false; else string.concat(PSTR(", "));
			string.concat(PSTR("MQTT: "));
			LeifSecondsToUptimeString(temp,*pMqttUptime);
			string.concat(temp);
		}
	}

	string.concat(PSTR("</td><td colspan=\"2\">Heap: "));

	string.concat(heapFree);

#if defined(ARDUINO_ARCH_ESP8266)
#ifndef NO_MAX_FREE_BLOCKSIZE
	string.concat(" (");
	string.concat(ESP.getMaxFreeBlockSize());
	string.concat(")");
#endif
#else
	string.concat(" (");
	string.concat(ESP.getMaxAllocHeap());
	string.concat(")");
#endif



	string.concat(PSTR("</td></tr>"));

	/*
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
	string.concat(PSTR("<tr>"));
	string.concat(PSTR("<td colspan=\"2\">ETH MAC: "));
	string.concat(ETH.macAddress());
	string.concat(PSTR("</td>"));
	string.concat(PSTR("</tr>"));
#endif
*/

	string.concat(PSTR("<tr><td colspan=\"2\">Compile time: "));

	string.concat(LeifGetCompileDate());

	string.concat(PSTR("</td><td colspan=\"3\">"));

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
	string.concat(PSTR("WiFi "));
#endif

	if(LeifGetAllowWifiConnection())
	{
		string.concat(PSTR("MAC: "));

		string.concat(WiFi.macAddress());
		//string.concat(PSTR("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));
		string.concat(PSTR("</td>"));
	}
	else
	{
		string.concat(PSTR("disabled"));
	}
	string.concat(PSTR("</tr>"));


	if(LeifGetAllowWifiConnection())
	{
		string.concat(PSTR("<tr></tr><tr><td colspan=\"2\">"));

		int bssid_color = 0;

		if(iWifiChannel >= 0)
		{
			bssid_color = LeifIsBSSIDConnection() ? 1 : -1;
		}

		switch(bssid_color)
		{
		case -1:
			string.concat(PSTR("<font color=\"red\">"));
			break;
		case 1:
			string.concat(PSTR("<font color=\"green\">"));
			break;
		}

		string.concat(PSTR("BSSID: "));
		string.concat(WiFi.BSSIDstr());

		if(fnGetWiFiAPName)
		{
			string.concat(" (");
			string.concat(fnGetWiFiAPName(WiFi.BSSIDstr()));
			string.concat(")");
		}


		string.concat(PSTR("&nbsp;&nbsp;&nbsp;CH: "));
		string.concat(WiFi.channel());

		if(bssid_color)
		{
			string.concat(PSTR("</font>"));
		}

		string.concat(PSTR("</td><td colspan=\"3\">"));

		bool bWrongWifi = strcmp(wifi_ssid, WiFi.SSID().c_str());

		if(bWrongWifi)
		{
			string.concat(PSTR("<font color=\"red\">"));
		}
		string.concat(PSTR("SSID: "));
		string.concat(WiFi.SSID());
		if(bWrongWifi)
		{
			string.concat(PSTR("</font>"));
		}

		string.concat(PSTR("&nbsp;&nbsp;&nbsp;RSSI: "));
		string.concat(WiFi.RSSI());
		string.concat(PSTR(" (max "));
		string.concat(max_rssi);
		string.concat(PSTR(")</td></tr>"));
	}


	//string.concat(PSTR("<tr><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td></tr>"));



	if(fnHttpMainTableExtraCallback)
	{
		fnHttpMainTableExtraCallback(string,eHttpMainTable_AfterLastRow);
	}

	if(fnHttpMainTableCallback)
	{
		fnHttpMainTableCallback(string, eHttpMainTable_AfterLastRow);
	}

	string.concat(PSTR("</table><br>"));


}

byte chartohex(char asciichar)
{
	if(asciichar >= '0' && asciichar <= '9')
	{
		return asciichar - '0';
	}

	if(asciichar >= 'a' && asciichar <= 'f')
	{
		return (asciichar - 'a') + 0xA;
	}

	if(asciichar >= 'A' && asciichar <= 'F')
	{
		return (asciichar - 'A') + 0xA;
	}
	return 0;
}


void LeifSetVersionText(const char * szVersion)
{
	strncpy(szVersionText,szVersion,sizeof(szVersionText));
	szVersionText[sizeof(szVersionText)-1]=0;
}

String LeifGetVersionText()
{
	return szVersionText;
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
	bAllowConnect = bAllow;

	if(bAllow)
	{
		ulWifiReconnect = millis() - 15000;
	}

}

bool LeifGetAllowWifiConnection()
{
	return bAllowConnect && strlen(wifi_ssid);
}

bool IsLeifSetupBeginDone()
{
	return bLeifSetupBeginDone;
}

String GetArgument(const String & input, const char * argname)
{
	int mac = input.indexOf(argname);
	if(mac >= 0)
	{
		mac += strlen(argname);
		int space = input.indexOf(' ', mac);
		if(space >= 0)
		{
			return input.substring(mac, space);
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
	if(!IsWiFiConnected())
	{
		return false;
	}
	if(iWifiChannel >= 0)
	{
		if(memcmp(WiFi.BSSID(), cBSSID, 6))
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


uint16_t uCmdMax=16;


void LeifSetMaxCommandLength(uint16_t max_chars)
{
	uCmdMax=max_chars;
}

void HandleCommandLine()
{

	while(telnetClients.available())
	{
		char inputChar=telnetClients.read();

		static int iNegotiate=0;
		if(iNegotiate>0)
		{
			iNegotiate--;
			continue;
		}

		if(vecOnCommand.size())
		{
			switch(inputChar)
			{
			case '\r':
				//if(strTelnetCmdBuffer.length())
				{
					DoCommandCallback(strTelnetCmdBuffer,eCommandLineSource_Telnet);
				}
				//fall through
			case '\n':
				strTelnetCmdBuffer="";
				break;
			case '\b':
			case 0x7f:
				{
					int len=strTelnetCmdBuffer.length();
					if(len)
					{
						strTelnetCmdBuffer.remove(len-1, 1);
					}
				}
				break;
			case 0xff:	//ignore telnet negotiation
				iNegotiate=2;
				break;
			default:
				strTelnetCmdBuffer+=inputChar;
				break;
			}
		}
	}

	if(strTelnetCmdBuffer.length()>uCmdMax)
	{
		strTelnetCmdBuffer.remove(0, strTelnetCmdBuffer.length()-uCmdMax);
	}



#ifndef NO_SERIAL_DEBUG
	if(Serial.available())
	{
		char inputChar=Serial.read();

		if(g_bAllowSerialCommands)
		{

			switch(inputChar)
			{
			case '\r':
				//if(strSerialCmdBuffer.length())
				{
					DoCommandCallback(strSerialCmdBuffer,eCommandLineSource_Serial);
				}
				//fall through
			case '\n':

				strSerialCmdBuffer="";
				break;
			default:
				if(vecOnCommand.size())
				{
					strSerialCmdBuffer+=inputChar;
				}
				break;
			}
		}
	}

	if(strSerialCmdBuffer.length()>uCmdMax)
	{
		strSerialCmdBuffer.remove(0, strSerialCmdBuffer.length()-uCmdMax);
	}
#endif

}


void WiFiHealthMaintenance();

void WiFiWatchdog()
{

	if(!LeifGetAllowWifiConnection())
	{
		ulSecondCounterWiFiWatchdog=0;
		return;
	}

	if(ulSecondCounterWiFiWatchdog>150)
	{
		//csprintf("WiFi watchdog: WiFi has been stuck for a while, force disconnect and retry\n");
		WiFi.disconnect(false);
		ulSecondCounterWiFi=0;
		ulSecondCounterWiFiWatchdog=0;
	}

	int8_t cur_rssi=WiFi.RSSI();
	if(cur_rssi>=0) cur_rssi=-128;
	rssi_sum -= rssi_history[rssi_history_idx];
	rssi_history[rssi_history_idx]=cur_rssi;
	rssi_sum += cur_rssi;
	rssi_history_idx++;
	rssi_history_idx&=7;

	int8_t avg_rssi=get_avg_rssi();

	if(max_rssi<avg_rssi) max_rssi=avg_rssi;

	//csprintf("cur rssi: %i, avg: %i  record: %i\n",cur_rssi, avg_rssi, max_rssi);


	if((ulSecondCounterWiFi & 2047) == 2047)
	{
		WiFiHealthMaintenance();
	}


//	csprintf("WD %u ",ulSecondCounterWiFiWatchdog);

}


void WiFiHealthMaintenance()
{
	//csprintf("WiFi health maintenance\n");

	bool bDoDisconnect=false;

	if(iWifiChannel>=0)	//we're supposed to use a BSSID connection
	{
		if(!LeifIsBSSIDConnection())
		{	//but we're not, so disconnect.
			csprintf(PSTR("WiFi Health Maintenance: We're not on the correct BSSID connection. Disconnecting. (count: %i)\n"),iHealthDisconnects);
			bAllowBSSID=true;
			bDoDisconnect=true;
		}
	}
	else
	{
		int8_t avg_rssi=get_avg_rssi();

		if((int16_t) avg_rssi < (int16_t) max_rssi-10)
		{
			csprintf(PSTR("WiFi Health Maintenance: Current RSSI %i too low compared to max %i. Disconnecting. (count: %i)\n"), avg_rssi, max_rssi,iHealthDisconnects);

			if(max_rssi>-90)
			{
				max_rssi-=2;
				if(max_rssi<-90) max_rssi=-90;
			}

			bDoDisconnect=true;
		}
	}

	if(bDoDisconnect)
	{

	#if defined(ARDUINO_ARCH_ESP32)
		iWifiConnAttempts=1;	//to account for the code above to work around that esp32 always fails the first attempt
	#else
		iWifiConnAttempts=0;
	#endif
		ulSecondCounterWiFi=0;
		WiFi.disconnect(false);
		iHealthDisconnects++;
	}


}

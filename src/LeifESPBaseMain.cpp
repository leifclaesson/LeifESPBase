#include "LeifESPBaseMain.h"
#include "LeifESPBase.h"

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>		//esp_reset_reason()
#include <esp_wifi.h>		//esp_wifi_set_country() -- regulatory domain so channel 13 is scannable/joinable
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2)
#include <esp_mac.h>		//esp_read_mac() -- the eFuse MAC, readable before the STA netif exists.
						//⛔ 1.0.6 does not ship this header at all, and the absence of
						//ESP_ARDUINO_VERSION_MAJOR (esp_arduino_version.h, core 2.0.0+) is the test.
#endif
#include <lwip/sockets.h>	//select()/fd_set for LeifWebServer's bounded non-blocking response write (below)
#endif

#ifndef NO_OTA
#include <ArduinoOTA.h>
bool bUpdatingOTA = false;
#endif



#include "..\environment_setup.h"

#ifndef WDT_TIMEOUT
#define WDT_TIMEOUT 10
#endif

uint32_t serial_debug_rate=115200;

String LeifGetResetReasonString()
{
#if defined(ARDUINO_ARCH_ESP32)
	switch(esp_reset_reason())
	{
		case ESP_RST_POWERON:	return F("POWERON");	//normal power-on / cold boot
		case ESP_RST_EXT:		return F("EXT");		//reset via external pin (e.g. the STM8 watchdog pulling the ESP32 RESET line)
		case ESP_RST_SW:		return F("SW");			//esp_restart() / ESP.restart()
		case ESP_RST_PANIC:		return F("PANIC");		//exception / panic handler (crash)
		case ESP_RST_INT_WDT:	return F("INT_WDT");	//interrupt watchdog
		case ESP_RST_TASK_WDT:	return F("TASK_WDT");	//task watchdog
		case ESP_RST_WDT:		return F("WDT");		//other watchdog
		case ESP_RST_DEEPSLEEP:	return F("DEEPSLEEP");	//wake from deep sleep
		case ESP_RST_BROWNOUT:	return F("BROWNOUT");	//brownout (supply sag) -> suspect PSU / bulk cap
		case ESP_RST_SDIO:		return F("SDIO");
		default:				return F("UNKNOWN");
	}
#elif defined(ARDUINO_ARCH_ESP8266)
	return ESP.getResetReason();
#else
	return F("n/a");
#endif
}

static unsigned long ulSecondCounterWiFiWatchdog = 0;

static fn_LeifESPBaseOtaTooLargeCallback g_cbOtaTooLarge=NULL;
void LeifSetOtaTooLargeCallback(fn_LeifESPBaseOtaTooLargeCallback cb) { g_cbOtaTooLarge=cb; }

static std::vector<fn_LeifESPBaseSysinfoSection> g_vecSysinfoSections;
void LeifAddSysinfoSection(fn_LeifESPBaseSysinfoSection cb)
{
	if(cb)
	{
		g_vecSysinfoSections.push_back(cb);
	}
}


#if defined(ARDUINO_ARCH_ESP32)

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include "esp_chip_info.h"
#endif

#include <esp_task_wdt.h>

#include "esp_partition.h"	//the live partition table, reported on /sysinfo
#include "esp_ota_ops.h"	//which slot we are running from, and which one an OTA would land in

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

void DisableSerialLogging()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
	esp_log_level_set("*", ESP_LOG_NONE);
#endif
}


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


typedef std::function<void(void)> fn_LeifESPBaseInterimCallback;

static fn_LeifESPBaseInterimCallback fnInterimCallback;

void LeifSetInterimCallback(fn_LeifESPBaseInterimCallback cb)
{
	fnInterimCallback=cb;
}

static void DoInterimCallback()
{
	if(fnInterimCallback)
	{
		fnInterimCallback();
	}
}


#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer server(80);
#else
LeifWebServer server(80);

//LeifWebServer -- see the class note in LeifWebServer.h. The stock WebServer's every response
//write funnels through _currentClientWrite / _currentClientWrite_P; we bound them so a stalled
//socket write can't starve loopTask into a task-WDT reboot on a marginal link.
#ifndef LEIFWEBSERVER_WRITE_BUDGET_MS
#define LEIFWEBSERVER_WRITE_BUDGET_MS 1000	//per-chunk stall budget; a write that can't progress within this stop()s the client
#endif

size_t LeifWebServer::_currentClientWrite(const char *b, size_t l)
{
	return BoundedClientWrite(b, l, false);
}

size_t LeifWebServer::_currentClientWrite_P(PGM_P b, size_t l)
{
	return BoundedClientWrite(b, l, true);
}

size_t LeifWebServer::BoundedClientWrite(const char *b, size_t l, bool progmem)
{
	if(!_currentClient.connected())
	{
		return 0;
	}

	int sockfd=_currentClient.fd();
	if(sockfd<0)
	{
		return 0;
	}

	size_t written=0;
	const size_t chunk_max=1436;	//1 esp32 TCP_MSS: select reports writable at >=1 MSS free, so a <=1 MSS write is taken in one shot -> the write() call itself can't loop-block

	while(written<l)
	{
		size_t n=l-written;
		if(n>chunk_max)
		{
			n=chunk_max;
		}

		//Wait (bounded) for the socket to accept bytes. Healthy link -> select returns writable
		//immediately -> byte-identical to the stock write. Stalled link -> we give up after the
		//budget and stop() the client, so this request errors instead of starving loopTask; the
		//remaining writes in this render then return 0 instantly (write() checks _connected).
		fd_set setWrite;
		FD_ZERO(&setWrite);
		FD_SET(sockfd,&setWrite);
		struct timeval tv={ LEIFWEBSERVER_WRITE_BUDGET_MS/1000, (LEIFWEBSERVER_WRITE_BUDGET_MS%1000)*1000 };
		int r=select(sockfd+1,nullptr,&setWrite,nullptr,&tv);
		if(!(r>0 && FD_ISSET(sockfd,&setWrite)))
		{
			_currentClient.stop();	//stall past budget -> error the request, keep the box up
			return written;
		}

		size_t w = progmem ? _currentClient.write_P(b+written,n) : _currentClient.write((const uint8_t *)(b+written),n);
		if(!w)
		{
			_currentClient.stop();
			return written;
		}
		written+=w;
	}

	return written;
}
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

//Raw ring append -- no line-ending handling, no bookkeeping beyond the ring's own.
void ScrollbackBuffer::RawAppend(const uint8_t * buffer, size_t size)
{
	if(!buf || !size)
	{
		return;
	}

	lastch = (char) buffer[size-1];
	total += size;

	//A write longer than the whole ring can only ever leave its LAST bufsize bytes, so skip
	//straight to those rather than memcpy()ing the doomed prefix through the buffer first.
	if(size >= bufsize)
	{
		buffer += size-bufsize;
		size = bufsize;
		idx = 0;
	}

	while(size)
	{
		uint16_t bytes_now = size;
		if(bytes_now > (uint16_t)(bufsize - idx))
		{
			bytes_now = bufsize - idx;
		}
		memcpy(&buf[idx], buffer, bytes_now);
		size -= bytes_now;
		buffer += bytes_now;
		idx += bytes_now;
		idx %= bufsize;

		//⛔ This used to read "kept += bufsize", which made the buffer claim to be FULL after
		//the very first multi-byte print -- so the first viewer to connect was replayed up to a
		//whole buffer of the zeros alloc() memset in, ahead of the real text.
		kept += bytes_now;
		if(kept > bufsize)
		{
			kept = bufsize;
		}
	}
}

size_t ScrollbackBuffer::write(uint8_t value)
{
	if(!buf)
	{
		return 1;
	}

	if(value=='\n' && lastch!='\r')
	{
		RawAppend((const uint8_t *) "\r\n",2);
		return 1;
	}

	RawAppend(&value,1);

	return 1;
}

//LF -> CRLF on the way IN, because the bytes in here are handed to telnet sockets verbatim
//and nothing downstream looks at them again. A source that already writes CRLF is left alone
//rather than turned into CR CR LF.
size_t ScrollbackBuffer::write(const uint8_t * buffer, size_t writesize)
{
	if(!buf)
	{
		return writesize;
	}

	size_t begin=0;

	for(size_t i=0;i<writesize;i++)
	{
		if(buffer[i]!='\n')
		{
			continue;
		}

		RawAppend(&buffer[begin],i-begin);
		if(i>begin ? buffer[i-1]!='\r' : lastch!='\r')
		{
			RawAppend((const uint8_t *) "\r",1);
		}
		RawAppend((const uint8_t *) "\n",1);
		begin=i+1;
	}

	RawAppend(&buffer[begin],writesize-begin);

	return writesize;
}

//Bytes at absolute offset pos, and how many run on contiguously from there.
size_t ScrollbackBuffer::Peek(uint32_t pos, const char * & data) const
{
	data=NULL;

	if(!buf || !bufsize)
	{
		return 0;
	}

	//Signed differences throughout, so this keeps working after total wraps past 2^32.
	if((int32_t)(pos-Oldest())<0 || (int32_t)(total-pos)<=0)
	{
		return 0;
	}

	uint32_t behind=total-pos;					//how far back from the write head pos sits
	uint16_t off=(uint16_t)(((uint32_t)idx+bufsize-(behind%bufsize))%bufsize);
	size_t contiguous=bufsize-off;
	if(contiguous>behind)
	{
		contiguous=behind;
	}

	data=&buf[off];

	return contiguous;
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

//The console's one formatting pass -- see the block above its declaration in LeifESPBase.h for
//why it stopped being a macro. Buffer strategy is Print::printf_P's own: 64 bytes of stack,
//heap only for a line that overruns it. Both sinks are then handed the SAME bytes, so a line
//costs one vsnprintf where the macro spent one per sink.
void csprintf(PGM_P fmt, ...)
{
	va_list arg;
	char temp[64];
	char * buffer=temp;

	va_start(arg,fmt);
	int len=vsnprintf_P(temp,sizeof(temp),fmt,arg);
	va_end(arg);

	if(len<=0)
	{
		return;
	}

	if((size_t) len > sizeof(temp)-1)
	{
		buffer=(char *) malloc(len+1);
		if(!buffer)
		{
			return;
		}
		va_start(arg,fmt);
		vsnprintf_P(buffer,len+1,fmt,arg);
		va_end(arg);
	}

#ifdef NO_SERIAL_DEBUG
#ifdef USE_SERIAL1_DEBUG
	Serial1.write((const uint8_t *) buffer,len);
	Serial1.flush();
#endif
#else
	Serial.write((const uint8_t *) buffer,len);
#endif

	//⛔ The telnet half is NOT a second write at the sockets. scrollbackBuffer IS the telnet
	//path -- every seat reads out of it at its own pace -- so a console line costs one format
	//and one memcpy, and no socket is touched here.
	scrollbackBuffer.write((const uint8_t *) buffer,len);

	if(buffer!=temp)
	{
		free(buffer);
	}
}

bool bLedOverride=false;
int iLedOverride=0;


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
WiFiClient telnetClients[LEIF_TELNET_MAX_CLIENTS];
uint8_t telnetClientCount=0;

TelnetClientPrint telnetprint(telnetClients);

//A console viewer that stops reading must never become the PRODUCT's problem. The stock
//THE CONSOLE'S DELIVERY MODEL, and why it is not the obvious one.
//
//It used to be: every console line was written straight at every telnet seat, blocking.
//WiFiClient::write parks in select() for a full second whenever the peer's receive window is
//shut, and that is per line -- so one wedged viewer stalled loopTask for as long as there was
//backlog, starved the idle task, and the task watchdog rebooted the board. Measured on a
//Lightbulb 2026-09-03: a Bluetooth inquiry's console burst plus one telnet client that never
//reads = SW_CPU_RESET three seconds later, on demand. It stalled the MQTT publish in the same
//pass, so this was never console-only damage.
//
//⛔ The answer is NOT to drop lines. //Leif, 2026-09-03: "a console that loses lines is
//bad... it would almost be better to disconnect because, well, warn in the console that hey,
//disconnect it because you're not keeping up."
//
//⛔ Nor is it to disconnect a seat the moment it misses a write, which is what the first
//attempt at this did. //Leif, 2026-09-03: "what if there's a temporary hiccup? Then we're just
//gonna keep dropping consoles, that'll be annoying. And the fact is we already have a ring
//buffer, the one that we use for the console, for the scroll back... so all we have to do is
//to have separate pointers for each client. To how far they have been sent in that buffer.
//That way we don't have to kick a client until the part that is still unsent is about to get
//bumped out of the buffer due to a new print."
//
//⭐ So that is the model, and it costs no new memory:
//
//  - csprintf() writes ONLY to scrollbackBuffer. No socket is touched while printing, so a
//    print is a memcpy and can never block loopTask no matter how many seats are wedged.
//  - each seat holds an absolute position in that stream (uTelnetSeatSent).
//  - TelnetDrainSeats(), once per LeifLoop pass, hands each seat whatever its socket will take
//    RIGHT NOW and advances only by what was actually accepted. Nothing ever waits.
//  - a hiccup is therefore free: a viewer that goes quiet for a moment simply falls behind and
//    catches up, and the buffer is exactly the slack it is allowed.
//  - a seat is bumped ONLY when its unsent bytes have been overwritten by newer output, i.e.
//    it fell a whole buffer behind. Then it is told why, because a viewer that is silently
//    missing lines is worse than one that knows it was disconnected.
//
//A seat therefore either receives every byte in order or is disconnected with a stated reason.
//There is never a silent gap for a reader to mistrust.

uint32_t uTelnetSeatSent[LEIF_TELNET_MAX_CLIENTS]={0};	//absolute stream position each seat has been fed to
uint32_t uTelnetSeatFloor[LEIF_TELNET_MAX_CLIENTS]={0};	//where LIVE output began for that seat -- everything before it is replay
bool bTelnetSeatBumped[LEIF_TELNET_MAX_CLIENTS]={false};	//read by LeifLoop's departure sweep, so the disconnect line can say WHY

//The whole welcome banner's budget, not one write's. It is spent only by a newcomer whose
//socket will not take its own greeting, and the seat is dropped the moment it runs out.
#ifndef LEIFTELNET_BANNER_BUDGET_MS
#define LEIFTELNET_BANNER_BUDGET_MS 100
#endif


//As much of buffer as this seat's socket will take within uBudgetMs; returns how many went.
//⛔ The drain passes a budget of ZERO, so the worst case there is a syscall and never a
//peer's TCP window -- that unbounded wait is the whole bug this exists to remove. The write is
//capped at 1 MSS for the same reason LeifWebServer::BoundedClientWrite caps it: select reports
//writable only with at least an MSS free, so a <=1 MSS write is taken in one shot and write()
//cannot drop into its own 1-second-per-retry loop underneath us.
//
//⚠ A zero budget really does mean "right now", and lwIP is stricter about that than it
//looks: its writable flag is raised by the TCP sent callback, so back-to-back small writes go
//not-writable after the first one or two until the peer ACKs. That is harmless for the drain,
//which simply continues on the next loop pass -- but it silently truncated the welcome banner,
//which has no next pass. Hence the budget, and hence the banner having one at all.
//⛔ The ESP8266 has no socket to select() on: its WiFiClient wraps a ClientContext, not an
//lwIP fd, so the arm below is the same contract reached a different way. availableForWrite() is
//tcp_sndbuf() read straight off the pcb -- the writable test itself, with no syscall and no
//wait -- and ClientContext::write only parks in its delay(1) retry loop when it is handed MORE
//than that room. Hand it exactly what already fits and it returns on the first pass.
//⚠ The two budgets mean different things and that is deliberate, not an oversight: the drain
//(budget 0) takes a SHORT write happily, because the seat resumes at whatever position it
//reached and catches up next pass -- refusing partial writes there would starve a seat whose
//peer keeps a small window open and then bump it for "loss" it never suffered. The banner
//(budget > 0) has no next pass, so it waits for the whole line to fit and otherwise writes
//nothing, which is what costs that seat its greeting and its seat.
static size_t TelnetSeatSendNow(WiFiClient & client, const char * buffer, size_t size, uint32_t uBudgetMs=0)
{
#if defined(ARDUINO_ARCH_ESP8266)

	if(!size || !client.connected())
	{
		return 0;
	}

	if(size>1436)
	{
		size=1436;
	}

	size_t room=client.availableForWrite();

	if(uBudgetMs)
	{
		uint32_t ulStart=millis();
		while(room<size && (millis()-ulStart)<uBudgetMs)
		{
			delay(1);
			room=client.availableForWrite();
		}

		if(room<size)
		{
			return 0;
		}
	}

	if(!room)
	{
		return 0;
	}

	if(size>room)
	{
		size=room;
	}

	return client.write((const uint8_t *) buffer,size);

#else

	int sockfd=client.fd();
	if(sockfd<0 || !size)
	{
		return 0;
	}

	if(size>1436)
	{
		size=1436;
	}

	fd_set setWrite;
	FD_ZERO(&setWrite);
	FD_SET(sockfd,&setWrite);
	struct timeval tv={ (time_t)(uBudgetMs/1000), (suseconds_t)((uBudgetMs%1000)*1000) };
	int r=select(sockfd+1,nullptr,&setWrite,nullptr,&tv);
	if(!(r>0 && FD_ISSET(sockfd,&setWrite)))
	{
		return 0;
	}

	return client.write((const uint8_t *) buffer,size);

#endif
}

//One pass over the seats: feed each one from the console stream, then bump any seat whose
//unsent bytes have actually been overwritten. Called once per LeifLoop pass.
void TelnetDrainSeats(void)
{
	for(int i=0;i<LEIF_TELNET_MAX_CLIENTS;i++)
	{
		if(!(telnetClients[i] && telnetClients[i].connected()))
		{
			continue;
		}

		//Feed it. The ring wraps, so a seat that is a long way behind needs more than one go;
		//two is always enough for one wrap, and the cap keeps a pathological case bounded.
		for(int pass=0;pass<3;pass++)
		{
			const char * data=NULL;
			size_t avail=scrollbackBuffer.Peek(uTelnetSeatSent[i],data);
			if(!avail)
			{
				break;
			}

			size_t sent=TelnetSeatSendNow(telnetClients[i],data,avail);
			if(!sent)
			{
				break;		//window shut -- it is behind, not broken. It catches up next pass.
			}
			uTelnetSeatSent[i]+=sent;
		}

		//⛔ The bump test is LOSS, not slowness. A seat only gets here once newer output has
		//overwritten bytes it had not been given yet, which takes a whole buffer of backlog --
		//so a viewer that merely stalls for a moment is never touched.
		uint32_t ulOldest=scrollbackBuffer.Oldest();
		if((int32_t)(uTelnetSeatSent[i]-ulOldest)>=0)
		{
			continue;
		}

		//⭐ Something was overwritten, but WHICH bytes decides whether that is a fault. A seat
		//starts at the oldest byte in the buffer, so it begins life a full buffer behind and the
		//very next print would otherwise evict something it had not been handed yet -- which
		//bumped every newcomer on the spot the first time this was written. Everything before
		//uTelnetSeatFloor is scrollback the seat was never promised: losing that is just the
		//backfill scrolling away under it, so it skips ahead and reads on. Only once the buffer
		//has cycled past the moment it CONNECTED has it missed something live.
		if((int32_t)(ulOldest-uTelnetSeatFloor[i])<=0)
		{
			uTelnetSeatSent[i]=ulOldest;
			continue;
		}

		//Tell the viewer to its face why it is going -- best effort, since by definition this
		//socket is barely accepting -- then stop() it. LeifLoop's departure sweep reports it to
		//everyone else on the next pass.
		//⛔ Do NOT csprintf() the notice from here: it would land in the very stream this
		//loop is walking, which is a fine way to write an infinite console.
		//⚠ This one deliberately does NOT go through TelnetSeatSendNow, because that asks
		//select() whether the socket is writable and by definition this is the one socket where
		//the answer is no -- measured 2026-09-03, first with no budget and then with 50 ms: the
		//seat was closed correctly both times and the viewer was never told a thing, which is
		//the half of the bargain that makes bumping better than dropping lines. A bare
		//MSG_DONTWAIT send takes whatever room is left in lwIP's own send buffer even while the
		//peer's window is shut, so a merely SLOW viewer gets the notice as soon as it reads
		//again. It cannot block: MSG_DONTWAIT returns rather than waiting.
		//⛔ Genuinely best effort even so. A viewer that has stopped reading altogether cannot
		//be told anything, which is why the board's own console says it too -- the
		//DISCONNECTED BY US line in the departure sweep below is the guaranteed surface.
		static const char szBump[]="\r\n*** Disconnecting this console: it fell so far behind that output was lost. ***\r\n";
#if defined(ARDUINO_ARCH_ESP8266)
		//Same bargain without a socket to reach past the client with: whatever lwIP's send
		//buffer will take this instant it takes without waiting, so a merely SLOW viewer still
		//gets told. One that has stopped reading has no room left and hears nothing, which is
		//the case the DISCONNECTED BY US line below exists to cover.
		if(telnetClients[i].availableForWrite()>=(int)(sizeof(szBump)-1))
		{
			telnetClients[i].write((const uint8_t *) szBump,sizeof(szBump)-1);
		}
#else
		int sockfd=telnetClients[i].fd();
		if(sockfd>=0)
		{
			send(sockfd,szBump,sizeof(szBump)-1,MSG_DONTWAIT);
		}
#endif
		telnetClients[i].stop();
		bTelnetSeatBumped[i]=true;
	}
}

//The welcome banner, and nothing else -- see the note on iOnlySeat in the header. This is the
//one piece of console text addressed to a single newcomer, so it cannot go through the shared
//stream; it is written straight at that seat instead.
//
//⛔ ulBannerDeadline is the budget for the WHOLE banner, not for each of the ~20 little
//writes it arrives in. Per-write budgets multiply, and a per-write budget big enough to be
//useful would let one wedged newcomer hold loopTask for the sum of them.
//
//⭐ And a banner that does not fit costs the seat, immediately. Everywhere else a seat that
//falls behind is simply fed later out of the buffer -- but the banner is not in the buffer and
//never will be, so a short write here is a hole that can never be filled. Rather than leave a
//viewer reading a truncated greeting, the seat goes and the drain's departure sweep says why.
void TelnetClientPrint::fanout(const uint8_t * buffer, size_t size)
{
	if(iOnlySeat<0)
	{
		return;
	}

	if(!pDest[iOnlySeat].connected())
	{
		return;
	}

	int32_t iLeftMs=(int32_t)(ulBannerDeadline-millis());	//signed, so this survives the millis() wrap
	if(iLeftMs<0)
	{
		iLeftMs=0;
	}

	if(TelnetSeatSendNow(pDest[iOnlySeat],(const char *) buffer,size,(uint32_t) iLeftMs)>=size)
	{
		return;
	}

	pDest[iOnlySeat].stop();
	bTelnetSeatBumped[iOnlySeat]=true;
}

size_t TelnetClientPrint::write(uint8_t value)
{
	if(value=='\n')
	{
		uint8_t cr='\r';
		fanout(&cr,1);
	}

	fanout(&value,1);
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
		fanout(&temp,1);
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
			fanout((const uint8_t *) &buffer[begin],i-begin+add);
			if(bLastCharNewLine) fanout((const uint8_t *) "\r\n",2);
			begin=i+1;
		}
		cbcounter++;
		if(cbcounter>127)
		{
			cbcounter=0;
			DoInterimCallback();
		}

	}

	//Serial.printf("*\n");
	return size;
//	return pDest->write(buffer,size);
}




bool bSeatOccupied[LEIF_TELNET_MAX_CLIENTS]={false};

//Per seat, not shared: two people typing at once would otherwise interleave into one
//line, and one client's telnet negotiation bytes would eat the other's characters.
String strTelnetCmdBuffer[LEIF_TELNET_MAX_CLIENTS];
int iTelnetNegotiate[LEIF_TELNET_MAX_CLIENTS]={0};
String strSerialCmdBuffer;

#define WIFI_RECONNECT


#if defined(WIFI_RECONNECT)
unsigned long ulWifiReconnect = millis() - 15000;
int iWifiConnAttempts = 0;

// --- non-blocking strongest-AP scan state (software-scan platforms: ESP8266 + ESP32 core 1.0.x) ---
// The old code blocked loop() ~2s on WiFi.scanNetworks(); unacceptable in a base class that drives real
// hardware. Instead the scan runs asynchronously across loop() passes -- kicked in SetupWifiInternal(),
// completed by its Phase-2 block on a later pass. See SetupWifiInternal() and the WiFi reconnect caller.
static bool g_bWifiScanPending = false;			// an async WiFi.scanNetworks() is in flight for a reconnect
static const char * g_pendWifiSsid = nullptr;	// SSID captured at scan-kick, consumed when the scan completes
static const char * g_pendWifiKey  = nullptr;	// key   captured at scan-kick
static uint32_t g_ulWifiScanKickMs = 0;			// millis() at kick, for the stuck-scan safety timeout
uint32_t ulWifiTotalConnAttempts = 0;
#endif


LeifGetWiFiAPName fnGetWiFiAPName;
void LeifRegisterGetWiFiAPName(LeifGetWiFiAPName fn)
{
	fnGetWiFiAPName=fn;
}

LeifWifiScanPersistAvailable fnWifiScanPersistAvailable;
void LeifRegisterWifiScanPersistAvailable(LeifWifiScanPersistAvailable fn)
{
	fnWifiScanPersistAvailable=fn;
}

LeifGetApRxText fnGetApRxText;
void LeifRegisterGetApRxText(LeifGetApRxText fn)
{
	fnGetApRxText=fn;
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


//Background-service hook. A long blocking operation that owns loop() -- typically a web
//page render, where server.sendContent() does a BLOCKING WiFiClient::write() that can stall
//for the client write timeout on a marginal link -- starves the MQTT keepalive, so the broker
//drops us and on reconnect we reload (possibly wrong) retained state. Servicing right before
//AND after each flush bounds the unserviced gap to a single write. Whichever MQTT lib is
//linked registers its pump here (LeifESPBaseMQTT -> lsm.Loop, LeifESPBaseHomie -> homie.Loop);
//those Loop()s self-throttle (~100ms) so calling this around every flush is cheap. No-op (safe)
//for a project with no MQTT lib. Same shape as Lightbulb's FlushServiceMqtt / GateController's
//FastPeriodic.
static void (*fnServiceBackground)()=nullptr;

void LeifSetServiceBackgroundCallback(void (*fn)())
{
	fnServiceBackground=fn;
}

void LeifServiceBackground()
{
	if(fnServiceBackground) fnServiceBackground();
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
bool bBSSIDSessionOnly = false;	//true = the current BSSID pin was chosen at runtime and is NOT written to config (temporary); false = came from config (saved)

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
		bBSSIDSessionOnly = false;	//default: a pin set through here is treated as saved (config load). The runtime picker overrides via LeifSetBSSIDSessionOnly() for a temporary pick.
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

//Our own station MAC. ⛔ Never call WiFi.macAddress() for this -- on ESP32 core 3.x it needs the
//STA netif, which does not exist until WiFi.mode(WIFI_STA) has run AND finished registering it
//asynchronously. Anything asking earlier (a DefaultConfig() building a hostname or an MQTT topic,
//an identity check, the boot banner) gets nothing, and gets it SILENTLY:
//  - the String overload zero-fills its own buffer and ignores the failure -> a clean 00:00:...:00
//  - the pointer overload returns NULL and NEVER WRITES the caller's buffer -> uninitialized stack
//The library's only complaint is a log_e, compiled out at CORE_DEBUG_LEVEL=0 -- so a wrong answer
//is indistinguishable from a right one, and a DefaultConfig() persists it to flash.
//⭐ This is a core 3.x REGRESSION, not how it always was: 1.0.6 and 2.x both fell back to
//esp_read_mac() themselves when the mode was still WIFI_MODE_NULL (WiFiSTA.cpp), which is exactly
//the early case. 3.x moved the call to NetworkInterface::macAddress() and dropped that fallback.
//The eFuse holds the factory station MAC and needs no netif and no radio, so it is right from the
//first instruction of boot.
uint8_t * LeifGetMacAddress(uint8_t * mac)
{
	if(!mac) return NULL;
#if defined(ARDUINO_ARCH_ESP32) && defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2)
	esp_read_mac(mac, ESP_MAC_WIFI_STA);	//esp_mac.h, guarded at the top of this file -- 1.0.6 has no such header
	return mac;
#else
	//ESP32 core 1.0.6 has the WIFI_MODE_NULL -> esp_read_mac() fallback built in, and the ESP8266
	//reads its SDK directly (wifi_get_macaddr, no netif involved). Both are correct this early.
	return WiFi.macAddress(mac);
#endif
}

String LeifGetMacAddressString()
{
	uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
	LeifGetMacAddress(mac);
	return MacToString(mac);
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

// Returns true if a connection attempt (WiFi.begin) was issued this call -> caller counts it as an attempt.
// Returns false only on the software-scan platforms when it kicked an async scan and is waiting on results;
// the caller then polls SetupWifiInternal() again next loop until it returns true.
bool SetupWifiInternal()
{
	// --- Phase 2: an async strongest-AP scan kicked on an earlier pass is completing. -----------------
	// (Software-scan platforms only -- g_bWifiScanPending is never set elsewhere. Runs BEFORE the one-time
	//  per-attempt setup below so that setup happens once at kick, not on every poll.)
	if(g_bWifiScanPending)
	{
		int n = WiFi.scanComplete();		// >=0 = AP count ready; -1 = still running; -2 = failed/none
		if(n == -1 && (millis() - g_ulWifiScanKickMs) < 8000)
		{
			return false;					// still scanning within the timeout budget -- poll again next loop
		}

		int best = -1;
		for(int i = 0; i < n; i++)			// n<0 (failed / timed out) -> body skipped, best stays -1
		{
			if(WiFi.SSID(i) == g_pendWifiSsid && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)))
			{
				best = i;
			}
		}

		if(best >= 0)
		{
			csprintf(PSTR("  strongest '%s': BSSID %s, Ch %i, RSSI %i\n"), g_pendWifiSsid, WiFi.BSSIDstr(best).c_str(), WiFi.channel(best), WiFi.RSSI(best));
			WiFi.begin(g_pendWifiSsid, g_pendWifiKey, WiFi.channel(best), WiFi.BSSID(best));	//begin() copies the BSSID, so scanDelete() below is safe
			g_lastWifiChannel = WiFi.channel(best);
		}
		else
		{
			WiFi.begin(g_pendWifiSsid, g_pendWifiKey);	//scan failed / timed out / SSID hidden -> plain connect
			g_lastWifiChannel = -1;
		}
		WiFi.scanDelete();					//free the scan-result heap
		g_bWifiScanPending = false;
		return true;						//begin() issued -- caller counts the attempt
	}

#if defined(ARDUINO_ARCH_ESP8266)
	WiFi.hostname(GetHostName());
#else
	WiFi.setHostname(GetHostName());
#endif
	//Leif, 2026-09-08: "do turn power save off. For both 8266 and ESP32 in LeifESPBase. That
	//way it will catch all of them when I update."
	//WHY, measured on the basement bench 2026-09-07/08 (misc/docs/plans/unifi-mcast-bench-rig-plan.md):
	//ONE station in 802.11 power save on a BSS makes the access point hold every group frame to
	//the DTIM beacon and release ~8 of them back-to-back, and on a Wi-Fi 6 AP (U6 Pro) every
	//receiver inside that lump -- Linux, ESP8266, ESP32 -- loses ~32 points of the light show.
	//Sleeper-free, the same AP delivers like an AC Pro. 105 of the 197 stations on the IoT SSID
	//were sleepers and every one was an Espressif board with the core default, so the fix has
	//to be fleet-wide and firmware-side: no LeifESPBase device may be the sleeper.
	//This REVERSES the 2026-08-30 removal (//Leif then: "Drop it everywhere. Let's get rid of
	//it. Keep the default."). That removal was right about the MQTT-keepalive claim it undid
	//(never instrumented; the real fault then was ERP power) and wrong about the cost of the
	//default: WIFI_PS_MIN_MODEM on ESP32 / modem sleep on ESP8266 is exactly what lumps the BSS
	//for everybody.
	//⚠ IDF 4.4 hard-aborts if power save is NONE while Bluetooth is up: a product that starts
	//the BT controller must call WiFi.setSleep(true) first (none in the tree does at 2026-09-08
	//per grep; re-check when one appears).
#if defined(ARDUINO_ARCH_ESP8266)
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
	WiFi.setSleep(false);
#endif
	ulSecondCounterWiFiWatchdog=0;
#if defined(WIFI_RECONNECT)
#if !defined(ARDUINO_ARCH_ESP32) || ESP_ARDUINO_VERSION_MAJOR < 3
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

		return true;
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

		//Common bookkeeping for every SSID-mode path, set up-front so it stays correct even when the
		//software-scan path below defers the actual begin() to a later async-completion pass.
		g_lastWifiSSID = use_ssid;
		g_lastWifiPSK = wifi_key;
		g_lastWifiChannel = -1;
		memset(g_lastBSSID, 0, sizeof(g_lastBSSID));
		strWifiStatus = "SSID ";
		strWifiStatus += use_ssid;

#if defined(ARDUINO_ARCH_ESP32) && ESP_ARDUINO_VERSION_MAJOR >= 2
		//ESP32 default is WIFI_FAST_SCAN: it stops at the FIRST matching-SSID AP found in channel order and
		//associates regardless of RSSI, so on a multi-AP roaming SSID it never picks the strongest. Force a
		//full scan of every channel so the (default) by-signal sorter has all candidates and connects to the
		//nearest AP. Only for the SSID-only path -- the BSSID-pinned branch above deliberately targets one AP.
		//This scan runs inside begin() in the WiFi task -- non-blocking, so no state machine is needed here.
		//setScanMethod/setSortMethod arrived in ESP32 core 2.0.0; older cores fall into the async scan below.
		WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
		WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
		WiFi.begin(use_ssid, use_key);
		return true;
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
		//ESP8266 has no scan/sort-method control, and ESP32 core 1.0.x predates setScanMethod; both grab the
		//first matching AP, not the strongest. Do the strongest-AP pick in software, but ASYNCHRONOUSLY: the
		//old blocking WiFi.scanNetworks() stalled loop() ~2s, unacceptable in a base class that drives real
		//hardware. Kick a non-blocking scan here and return false; the caller polls SetupWifiInternal() every
		//loop and Phase 2 (top of this function) picks the strongest BSSID and begins once results land.
		//Strictly scan-THEN-connect, exactly like the old blocking path -- never a scan running during association.
		g_pendWifiSsid = use_ssid;
		g_pendWifiKey  = use_key;
		WiFi.scanNetworks(true /*async*/, false);
		g_ulWifiScanKickMs = millis();
		g_bWifiScanPending = true;
		return false;		//no begin() yet -- caller must NOT count this as an attempt until Phase 2 completes
#else
		WiFi.begin(use_ssid, use_key);
		return true;
#endif

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

	return true;
}





bool bConsoleInitDone = false;
void LeifSetupConsole(uint16_t _scrollback_bytes)
{
	if(bConsoleInitDone)
	{
		return;
	}

	DisableSerialLogging();

#ifndef NO_SERIAL_DEBUG
#ifdef USE_SERIAL1_DEBUG
	Serial1.begin(serial_debug_rate);
#else
	Serial.begin(serial_debug_rate);
#endif
#endif

	//⛔ Not optional any more, and a zero here used to be legal. Since 2026-09-03 this buffer
	//is what every telnet seat is fed FROM, so a console with no buffer would be a console that
	//can deliver nothing. Every project in the tree already asks for 1024-16384; the floor only
	//catches a caller that took the default.
	if(_scrollback_bytes<512)
	{
		_scrollback_bytes=512;
	}
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
//Human-readable PHY protocol / rate ceiling of the STA interface, read LIVE at call time rather
//than cached at boot. It exists because a radio pinned to 802.11b looks completely normal from
//every other angle -- it associates, it holds a strong RSSI, its retry count is clean -- it just
//moves every frame at 11 Mbps. Reporting it here makes the pin greppable across the whole fleet
//over HTTP, instead of visible only by reading tx_rate from the access point's side.
static String PhyProtocolString()
{
#if defined(ARDUINO_ARCH_ESP32)
	uint8_t bitmap = 0;
	if(esp_wifi_get_protocol(WIFI_IF_STA, &bitmap) != ESP_OK)
	{
		return F("(unavailable)");
	}

	String s = F("802.11");
	if(bitmap & WIFI_PROTOCOL_11B) s += F("b");
	if(bitmap & WIFI_PROTOCOL_11G) s += F("g");
	if(bitmap & WIFI_PROTOCOL_11N) s += F("n");
	if(bitmap & WIFI_PROTOCOL_LR)  s += F("+LR");

	uint8_t unknown = bitmap & ~(WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
	if(unknown)		//11AX and friends -- naming them by macro would break the older cores this base still builds for
	{
		char temp[16];
		sprintf(temp, PSTR("+0x%02X"), unknown);
		s += temp;
	}

	return bitmap ? s : F("802.11 none?!");
#elif defined(ARDUINO_ARCH_ESP8266)
	switch(WiFi.getPhyMode())	//a single MAXIMUM, not a bitmap: 11N implies b/g fallback
	{
	case WIFI_PHY_MODE_11B:	return F("802.11b");
	case WIFI_PHY_MODE_11G:	return F("802.11bg");
	case WIFI_PHY_MODE_11N:	return F("802.11bgn");
	default:				return F("(unknown)");
	}
#else
	return F("(n/a)");
#endif
}

void LeifSetupBegin()
{
	while(IsLeifSetupBeginDone())
	{
		delay(500);
		csprintf(PSTR("LeifSetupBegin() called twice\n"));
	}

	bLeifSetupBeginDone = true;

	DisableSerialLogging();

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

#if defined(ARDUINO_ARCH_ESP32) && ESP_ARDUINO_VERSION_MAJOR >= 3
	//esp32 core 3.x (IDF5): the STA hostname is latched into the netif at the WIFI_MODE_NULL->STA
	//transition inside WiFi.mode() (WiFiGeneric.cpp: esp_netif_set_hostname(..., NetworkManager::getHostname())),
	//reading the stored default_hostname at that instant. The later WiFi.setHostname() in SetupWifiInternal()
	//only rewrites that buffer and never re-latches the live netif, so on 3.x the name must be set BEFORE
	//mode(WIFI_STA) or the interface keeps the default esp32-XXXXXX. (1.0.6 applied it via the later call, so
	//this is 3.x-only and the SetupWifiInternal() call stays for the 1.x path.)
	WiFi.setHostname(GetHostName());
#endif

	WiFi.mode(WIFI_STA);

	//---- Regulatory domain: force the full 13-channel plan (1..13) ---------------------
	//Leif's WiFi runs a 4-channel plan (1/5/9/13); channel 13 only exists outside the US
	//domain, and neither core enables it robustly by default. The ESP32 boots country
	//"01" with WIFI_COUNTRY_POLICY_AUTO, which passive-scans 12/13 and can miss a
	//ch13-only AP until it hears the AP's country IE; the ESP8266 default is
	//{"CN",1,13,AUTO}. Pin an explicit MANUAL 1..13 domain on both so all 13 channels are
	//actively scanned and joinable regardless of the AP beacon. Set-once here, right after
	//WiFi.mode() has started the driver and before the first scan/connect.
	{
		wifi_country_t country;
		memset(&country, 0, sizeof(country));		//zeros ESP32 max_tx_power (use-default) + any optional fields
		country.cc[0] = 'T'; country.cc[1] = 'H'; country.cc[2] = 0;	//Thailand: a real 1..13 domain
		country.schan = 1;
		country.nchan = 13;
		country.policy = WIFI_COUNTRY_POLICY_MANUAL;	//always use this domain, never defer to the AP
#if defined(ARDUINO_ARCH_ESP32)
		esp_wifi_set_country(&country);				//must follow the esp_wifi_start() inside WiFi.mode()
#elif defined(ARDUINO_ARCH_ESP8266)
		wifi_set_country(&country);
#endif
	}
	//------------------------------------------------------------------------------------

	//---- PHY protocol: never let a STORED setting silently pin the radio to 802.11b -----
	//The protocol bitmap is a PERSISTENT setting, not an application one: esp_wifi_restore()'s
	//own documentation lists esp_wifi_set_protocol alongside set_config and set_mode as "WiFi
	//stack persistent settings", i.e. it lives in NVS (nvs.net80211). The radio reads it at
	//init, before any sketch code runs, and an OTA never touches that partition -- so a board
	//that once got pinned to 802.11b STAYS pinned across a reflash, forever, in silence.
	//2026-08-16, measured on Leif's fleet: three boards were sitting at 11 Mbps on clean links
	//(retries 0.08-0.10), and aircon-gateway proved the mechanism -- reflashed, software reset
	//confirmed, came back still at 11 Mbps. At 11b a frame costs several times the airtime of
	//the same frame at 72 Mbps on a shared channel, and nothing on the device would ever say so.
	//Setting it explicitly every boot fixes the live radio AND rewrites the stored value, so
	//flashing this firmware IS the un-pin -- no separate NVS erase step is needed.
	//IDF's own default is already B|G|N, so on a healthy board this is a no-op.
#if defined(ARDUINO_ARCH_ESP32)
	{
		//⚠ On a dual-band part left in WIFI_BAND_MODE_AUTO this API returns ESP_ERR_NOT_SUPPORTED
		//and esp_wifi_set_protocols() is the replacement; the ESP32 is 2.4GHz-only so it takes the
		//simple path. Report the outcome either way -- a SILENT pin is what hid this for months,
		//so a silent failure to UNpin would be the same bug wearing a different hat.
		esp_err_t err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
		if(err != ESP_OK)
		{
			csprintf(PSTR("PHY protocol.....: SET FAILED (%s) -- radio may still be pinned\n"), esp_err_to_name(err));
		}
	}
#elif defined(ARDUINO_ARCH_ESP8266)
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);	//same trap, same fix -- the 8266's phy mode is flash-persisted by the SDK too
#endif
	csprintf(PSTR("PHY protocol.....: %s\n"), PhyProtocolString().c_str());
	//------------------------------------------------------------------------------------

	csprintf(PSTR("WiFi: %s\n"), LeifGetAllowWifiConnection()?PSTR("ENABLED"):PSTR("DISABLED"));
	csprintf(PSTR("Using WiFi SSID: %s\n"), wifi_ssid);
	//⛔ NOT WiFi.macAddress() this early -- it silently returns nothing before the STA netif is up.
	//LeifGetMacAddressString() is right at any point in boot; the why is on its definition above.
	csprintf(PSTR("MAC address: %s\n"), LeifGetMacAddressString().c_str());
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
		//loop() doesn't run for the whole upload, so anything the app must keep alive
		//(e.g. an external hardware watchdog) gets its chance here
		DoInterimCallback();
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
#if ESP_ARDUINO_VERSION_MAJOR < 3
				ledcWrite(ucLedFadeChannel, value);
#else
				ledcWrite(iStatusLedPin, value);
#endif
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
#if defined(ARDUINO_ARCH_ESP32)
			//An image bigger than the inactive app slot is refused up front: Updater.cpp compares the
			//size declared in the OTA handshake against the partition and returns UPDATE_ERROR_SIZE
			//("too large %lu > %u", Updater.cpp:285) before writing a byte, so nothing is half-flashed
			//here. It is also the one moment a unit knows for certain that its partition layout cannot
			//hold the firmware it is being handed. Default is no handler, so every project that does
			//not opt in fails exactly as before. A handler that repartitions ends in esp_restart() and
			//never returns.
			//⛔ It is UPDATE_ERROR_SIZE, NOT UPDATE_ERROR_SPACE. SPACE is only ever set while WRITING
			//(Updater.cpp:838), which an over-size image never reaches -- so testing for SPACE here is
			//a condition that can never be true. Bench-caught on COM14 2026-08-22: the board refused a
			//1,400,000-byte push into a 1,310,720-byte slot and printed only "OTA update FAILED (1)".
			//The other two UPDATE_ERROR_SIZE sites are size==0 and a signature-too-small check that is
			//compiled out without UPDATE_SIGN, so this stays effectively unambiguous.
			if(error==OTA_BEGIN_ERROR && Update.getError()==UPDATE_ERROR_SIZE && g_cbOtaTooLarge)
			{
				csprintf(PSTR("OTA REFUSED: image is larger than the app slot.\n"));
				g_cbOtaTooLarge();
			}
#endif
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

	server.on("/wifireconnect", []()
	{
		server.send(200, PSTR("text/plain"), PSTR("Reconnecting WiFi -- re-scanning for the strongest AP. Reload /wifiscan in ~20s to see the new BSSID."));
		LeifScheduleForceReconnect(1000);	//defer the disconnect so this response reaches the client first
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


		//⛔ NOT %f, deliberately. A product may cancel the float printf hooks at link time to claw back
		//flash (the Lightbulb bulbs do -- ~16.6 KB), and this field then renders BLANK on that product's
		///sysinfo page with nothing on it to say why. Found on a bulb 2026-09-07. Integer maths gives the
		//identical text on every product and cannot be silently broken by a linker flag.
		{
			uint32_t tenths=(cpu_freq_khz+50)/100;		//kHz -> tenths of a MHz, rounded
			sprintf(temp, PSTR("Clock Freq.......: %u.%u MHz\n"), tenths/10, tenths%10);
		}
		s += temp;

		sprintf(temp, PSTR("Reset reason.....: %s\n"), LeifGetResetReasonString().c_str());
		s += temp;

		{
			String strUptime;
			LeifUptimeString(strUptime);
			sprintf(temp, PSTR("Uptime...........: %s\n"), strUptime.c_str());
			s += temp;
		}

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

#if defined(ARDUINO_ARCH_ESP32)
		//How full the app slot is, and the live table. Bumping the arduino-esp32 core costs
		//upwards of 230 KB, and the stock "default" layout gives only 0x140000 per slot -- so
		//this is what says whether a board can take its next build in place or has to be
		//repartitioned to min_spiffs first.
		{
			const esp_partition_t * run = esp_ota_get_running_partition();
			if(run)
			{
				uint32_t used = ESP.getSketchSize();
				sprintf(temp, "App slot.........: %s 0x%06X + 0x%06X (%u KB)\n",
					run->label, (unsigned) run->address, (unsigned) run->size, (unsigned) (run->size / 1024));
				s += temp;

				sprintf(temp, "Sketch...........: %u bytes, %u.%u%% of the slot, %u free\n",
					(unsigned) used,
					(unsigned) (100ULL * used / run->size),
					(unsigned) ((1000ULL * used / run->size) % 10),
					(unsigned) (run->size > used ? run->size - used : 0));
				s += temp;
			}

			//An OTA image must land in the OTHER slot before it can run, so that slot -- not this
			//one -- is the ceiling on the next build pushed to this board.
			const esp_partition_t * nxt = esp_ota_get_next_update_partition(NULL);
			if(nxt)
			{
				sprintf(temp, "OTA lands in.....: %s 0x%06X + 0x%06X, a build over %u bytes will not flash\n",
					nxt->label, (unsigned) nxt->address, (unsigned) nxt->size, (unsigned) nxt->size);
			}
			else
			{
				sprintf(temp, "OTA lands in.....: nowhere -- single-app layout, no over-the-air update possible\n");
			}
			s += temp;

			//App slots first, then data. ⛔ Not one pass over ESP_PARTITION_TYPE_ANY -- that value
			//arrived with IDF 4, so it does not compile for the core 1.0.6 fleet, which is exactly
			//the fleet this page has to be readable on.
			s += "Partitions.......:\n";
			const esp_partition_type_t types[2] = { ESP_PARTITION_TYPE_APP, ESP_PARTITION_TYPE_DATA };
			int t;
			for(t = 0; t < 2; t++)
			{
				esp_partition_iterator_t it = esp_partition_find(types[t], ESP_PARTITION_SUBTYPE_ANY, NULL);
				for(; it != NULL; it = esp_partition_next(it))
				{
					const esp_partition_t * p = esp_partition_get(it);
					if(!p)
					{
						continue;
					}
					sprintf(temp, "  %-9s type %u sub 0x%02X  0x%06X + 0x%06X (%u KB)\n",
						p->label, p->type, p->subtype, (unsigned) p->address, (unsigned) p->size,
						(unsigned) (p->size / 1024));
					s += temp;
				}
				if(it)
				{
					esp_partition_iterator_release(it);
				}
			}
			s += "\n";
		}
#endif

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

		//Anything but 802.11bgn here means the radio is pinned -- see the PHY protocol block in
		//LeifSetupBegin() for why that survives a reflash and what it costs.
		sprintf(temp, PSTR("PHY protocol.....: %s\n"), PhyProtocolString().c_str());
		s += temp;

		//Sections other modules asked to add. They append here rather than registering their own
		///sysinfo, which would never be reached.
		size_t sec;
		for(sec = 0; sec < g_vecSysinfoSections.size(); sec++)
		{
			g_vecSysinfoSections[sec](s);
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

		//bool ret=ledcAttachChannel(iStatusLedPin,500,12,ucLedFadeChannel);
		bool ret=ledcAttachChannel(iStatusLedPin,500,12,ucLedFadeChannel);
		//csprintf("ATTACH CHANNEL %i, led pin %i.   ret=%i\n",ucLedFadeChannel,iStatusLedPin,ret);
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

static uint32_t ulForceReconnectTimestamp=0;

void LeifScheduleForceReconnect(uint32_t ms)	//deferred LeifForceWifiReconnect(): lets a web handler flush its response BEFORE the association drops (same pattern as LeifScheduleRestart)
{
	ulForceReconnectTimestamp=millis()+ms;
	if(!ulForceReconnectTimestamp) ulForceReconnectTimestamp=1;	//never let the "armed" sentinel land on 0
}


void LeifUpdateStatusLED()
{
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

			if(bLedOverride)
			{
				use=iLedOverride;
			}


#if defined(ARDUINO_ARCH_ESP32)

#if ESP_ARDUINO_VERSION_MAJOR < 3
			ledcWrite(ucLedFadeChannel, bInvertLedBlink ? ((1 << iAnalogWriteBits) - 1) - use : use);
#else
			ledcWrite(iStatusLedPin, bInvertLedBlink ? ((1 << iAnalogWriteBits) - 1) - use : use);
#endif
			//if(Interval100()) csprintf("use %i\n",use);
			//if(Interval100()) csprintf("use after=%i %i\n",use,bInvertLedBlink?((1<<iAnalogWriteBits)-1)-use:use);
			//if(Interval250()) csprintf("channel=%i  value=%i\n",ucLedFadeChannel,bInvertLedBlink ? ((1 << iAnalogWriteBits) - 1) - use : use);
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

void LeifSetStatusLED_Override(bool override_enable, int value)
{
	bLedOverride=override_enable;
	iLedOverride=value;
	LeifUpdateStatusLED();
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

	if(ulForceReconnectTimestamp && (int32_t) (millis()-ulForceReconnectTimestamp)>0)
	{
		ulForceReconnectTimestamp=0;
		LeifForceWifiReconnect();
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
				g_bWifiScanPending = false;	//connected -- drop any in-flight async scan bookkeeping (a later reconnect rescans fresh)

			}

		}
		else if(LeifGetAllowWifiConnection())
		{
			bIpPrinted = false;
#if defined(WIFI_RECONNECT)
			if(g_bWifiScanPending)
			{
				//A software strongest-AP scan kicked on an earlier pass is completing asynchronously.
				//Poll it every loop (never blocks loop()). Only when it finishes and issues begin() do
				//we count the attempt and (re)start the reconnect clock.
				if(SetupWifiInternal())
				{
					WiFi.reconnect();
					iWifiConnAttempts++;
					ulWifiTotalConnAttempts++;
					ulWifiReconnect = millis();
					ulSecondCounterWiFi = 0;
				}
			}

			uint32_t ulReconnectMs=15000;
			if(iWifiConnAttempts>5) ulReconnectMs=30000;	//reconnect more slowly
			if(iWifiConnAttempts>10) ulReconnectMs=60000;	//reconnect more slowly
			if(iWifiConnAttempts>15) ulReconnectMs=120000;	//reconnect more slowly

			if(!g_bWifiScanPending && (millis() - ulWifiReconnect) >= ulReconnectMs)
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


					//SetupWifiInternal() issues begin() now and returns true (BSSID-pinned, ESP32>=2
					//native, or plain), OR -- on the software-scan platforms (ESP8266 / ESP32 core 1.0.x)
					//-- kicks an async scan and returns false; the poll branch above then finishes the
					//attempt on a later loop pass. Only a real begin() counts as an attempt.
					if(SetupWifiInternal())
					{
						WiFi.reconnect();
						iWifiConnAttempts++;
						ulWifiTotalConnAttempts++;
					}
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

	//Feed the seats before anything else looks at them, so a seat bumped for losing output is
	//reaped and announced by the sweep immediately below rather than a pass later.
	TelnetDrainSeats();

	//Sweep for departures FIRST, and every pass -- the old single-seat code only checked
	//this when no new client was pending, so a seat freed in the same pass looked taken.
	for(int i=0;i<LEIF_TELNET_MAX_CLIENTS;i++)
	{
		if(bSeatOccupied[i] && !telnetClients[i].connected())
		{
			telnetClients[i].stop();
			bSeatOccupied[i]=false;
			strTelnetCmdBuffer[i]="";
			iTelnetNegotiate[i]=0;
			if(telnetClientCount) telnetClientCount--;
			//⛔ Say WHY when we were the ones who ended it. A viewer bumped for not keeping
			//up looks identical to a network drop from the remaining seats, and the whole
			//point of bumping rather than dropping lines is that the reason is visible.
			if(bTelnetSeatBumped[i])
			{
				bTelnetSeatBumped[i]=false;
				csprintf(PSTR("Telnet client DISCONNECTED BY US -- it fell a whole console buffer behind and output was lost. %u of %u seats in use.\n"),
						(unsigned) telnetClientCount, (unsigned) LEIF_TELNET_MAX_CLIENTS);
			}
			else
			{
				csprintf(PSTR("Telnet client disconnected. %u of %u seats in use.\n"),
						(unsigned) telnetClientCount, (unsigned) LEIF_TELNET_MAX_CLIENTS);
			}
		}
	}

	if(telnet.hasClient())
	{
		int seat=-1;
		for(int i=0;i<LEIF_TELNET_MAX_CLIENTS;i++)
		{
			if(!bSeatOccupied[i]) { seat=i; break; }
		}

		if(seat<0)
		{
			//Full. TELL the newcomer instead of evicting a seated client -- the old code
			//dropped whoever was on, silently, which reads as a random disconnection.
			WiFiClient reject=telnet.available();
			reject.print(F("\r\nAll console seats on this device are in use. Try again shortly.\r\n"));
			reject.stop();
		}
		else
		{
			telnetClients[seat] = telnet.available();
			bSeatOccupied[seat]=true;
			telnetClientCount++;
			strTelnetCmdBuffer[seat]="";
			iTelnetNegotiate[seat]=0;

			String strUptime;
			LeifUptimeString(strUptime);

			//Welcome banner and scrollback go to the NEWCOMER only; everyone already
			//seated has seen the scrollback and does not want it replayed at them.
			telnetprint.ulBannerDeadline=millis()+LEIFTELNET_BANNER_BUDGET_MS;
			telnetprint.iOnlySeat=seat;

			telnetprint.printf("\n");
			for(int k=0;k<2;k++)
			{
				DoInterimCallback();
				for(int i=0;i<7;i++)
				{
					telnetprint.write((uint8_t *) "==========",10);
				}
				telnetprint.write((uint8_t *) "=========\n",10);
				if(k>0) break;
				telnetprint.printf(PSTR("Welcome to %s, ip %s! Uptime: %s (seat %u of %u)\n"),
						GetHostName(), WiFi.localIP().toString().c_str(), strUptime.c_str(),
						(unsigned) telnetClientCount, (unsigned) LEIF_TELNET_MAX_CLIENTS);
			}

#ifndef NO_SERIAL_DEBUG
			Serial.printf(PSTR("New telnet connection from %s, uptime %s\n"), telnetClients[seat].remoteIP().toString().c_str(), strUptime.c_str());
#endif

#ifdef USE_SERIAL1_DEBUG
			Serial1.printf(PSTR("New telnet connection from %s, uptime %s\n"), telnetClients[seat].remoteIP().toString().c_str(), strUptime.c_str());
#endif

			//⭐ The scrollback replay is not written here any more. Pointing the seat at the
			//oldest byte still held IS the replay: TelnetDrainSeats() feeds it from there at
			//whatever rate the socket takes, and everything printed from now on simply follows
			//it in the same stream. So the replay costs one assignment, cannot block, and a
			//newcomer that never reads is bumped by the same rule as everybody else.
			uTelnetSeatSent[seat]=scrollbackBuffer.Oldest();
			uTelnetSeatFloor[seat]=scrollbackBuffer.Head();		//live output for this seat starts here

			telnetprint.iOnlySeat=-1;

			telnetClients[seat].flush();  // clear input buffer, else you get strange characters

			//Announced to the others AFTER the banner, so a shared debug session shows
			//who else turned up rather than just going quiet.
			csprintf(PSTR("Telnet client connected. %u of %u seats in use.\n"),
					(unsigned) telnetClientCount, (unsigned) LEIF_TELNET_MAX_CLIENTS);
		}
	}

	LeifUpdateStatusLED();

}


char szVersionText[12] = {0};

char szExtCompileDate[32] = {0};

//Link-time build stamp. The build patches the linked .elf, overwriting the placeholder that
//follows the sentinel's terminating null with the local time, so the reported date is the
//time the image was actually produced rather than the time this file happened to compile.
//Sentinel and layout are CompileTimeInserter's (code_2/CompileTimeInserter), deliberately.
//volatile: without it the compiler folds the placeholder straight into LeifGetLinkDate().
#define LEIF_LINK_STAMP_OFFSET	23	//sentinel is 22 bytes, then a null, then the field
#define LEIF_LINK_STAMP_LENGTH	24

volatile char szLeifLinkStamp[] =
		"**COMPILE_TIME_DUMMY**"
		"\0"
		"N/A                     ";

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

		string.concat(LeifGetMacAddressString());	//WiFi.macAddress() happens to be right here (the page is served long
													//after the netif is up), but there is one way to ask for our MAC, not two
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

		int bssid_color = 0;	//0=auto (no pin), 1=on saved pin (green), 2=on temporary pin (orange), -1=pinned but not on it (red)

		if(iWifiChannel >= 0)
		{
			if(LeifIsBSSIDConnection())
			{
				bssid_color = LeifIsBSSIDSessionOnly() ? 2 : 1;
			}
			else
			{
				bssid_color = -1;
			}
		}

		switch(bssid_color)
		{
		case -1:
			string.concat(PSTR("<font color=\"red\">"));
			break;
		case 1:
			string.concat(PSTR("<font color=\"green\">"));
			break;
		case 2:
			string.concat(PSTR("<font color=\"orange\">"));
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

		if(bssid_color == 1)
		{
			string.concat(PSTR("&nbsp;(saved)"));
		}
		else if(bssid_color == 2)
		{
			//Offer a one-click Save that persists the currently-pinned AP to config WITHOUT reconnecting,
			//as the actionable word right inside the "temporary" note. Only where WiFiScan registered a
			//persist callback (else the /wifipick?savecur=1 handler no-ops) -- otherwise a plain note.
			if(fnWifiScanPersistAvailable && fnWifiScanPersistAvailable())
			{
				string.concat(PSTR("&nbsp;(temporary - <a href=\"/wifipick?b="));
				string.concat(WiFi.BSSIDstr());
				string.concat(PSTR("&c="));
				string.concat(WiFi.channel());
				string.concat(PSTR("&savecur=1\">Save</a>)"));
			}
			else
			{
				string.concat(PSTR("&nbsp;(temporary - not saved)"));
			}
		}

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

		//Leif, 2026-08-16: the far end of the link gets its OWN row rather than being crammed in beside
		//the RSSI, which stretched the whole table. The RSSI above is only OUR side; this is what the
		//access point hears US at. The pair is what makes a deaf AP visible -- ours stays healthy while
		//the AP's collapses. No row at all unless something is actually reporting it.
		if(fnGetApRxText)
		{
			const char * szApRx=fnGetApRxText();
			if(szApRx && *szApRx)
			{
				string.concat(PSTR("<tr><td colspan=\"5\">"));
				string.concat(szApRx);
				string.concat(PSTR("</td></tr>"));
			}
		}
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


String LeifGetLinkDate()
{
	char temp[LEIF_LINK_STAMP_LENGTH+1];
	int i;

	for(i=0;i<LEIF_LINK_STAMP_LENGTH;i++)
	{
		temp[i]=szLeifLinkStamp[LEIF_LINK_STAMP_OFFSET+i];	//volatile read, one byte at a time
	}
	temp[LEIF_LINK_STAMP_LENGTH]=0;

	for(i=0;i<LEIF_LINK_STAMP_LENGTH;i++)
	{
		if(!temp[i])
		{
			break;
		}
	}

	while(i && temp[i-1]==' ')
	{
		i--;
	}
	temp[i]=0;

	if(!strcmp(temp,"N/A"))
	{
		return String();	//never patched: this build did not go through the stamping step
	}

	return temp;
}

String LeifGetCompileDate()
{
	const char compile_date[] = __DATE__ " " __TIME__;

	String strLink=LeifGetLinkDate();
	if(strLink.length())
	{
		return strLink;
	}
	else if(strlen(szExtCompileDate))
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

void LeifSetBSSIDSessionOnly(bool bSessionOnly)	//mark the current pin as temporary (runtime pick, not in config) or saved
{
	bBSSIDSessionOnly = bSessionOnly;
}

bool LeifIsBSSIDSessionOnly()	//true if the active BSSID pin is a temporary runtime pick (not written to config)
{
	return bBSSIDSessionOnly;
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

	for(int seat=0;seat<LEIF_TELNET_MAX_CLIENTS;seat++)
	{
		if(!bSeatOccupied[seat]) continue;

		while(telnetClients[seat].available())
		{
			char inputChar=telnetClients[seat].read();

			if(iTelnetNegotiate[seat]>0)
			{
				iTelnetNegotiate[seat]--;
				continue;
			}

			if(vecOnCommand.size())
			{
				switch(inputChar)
				{
				case '\r':
					//if(strTelnetCmdBuffer[seat].length())
					{
						DoCommandCallback(strTelnetCmdBuffer[seat],eCommandLineSource_Telnet);
					}
					//fall through
				case '\n':
					strTelnetCmdBuffer[seat]="";
					break;
				case '\b':
				case 0x7f:
					{
						int len=strTelnetCmdBuffer[seat].length();
						if(len)
						{
							strTelnetCmdBuffer[seat].remove(len-1, 1);
						}
					}
					break;
				case 0xff:	//ignore telnet negotiation
					iTelnetNegotiate[seat]=2;
					break;
				default:
					strTelnetCmdBuffer[seat]+=inputChar;
					break;
				}
			}
		}

		if(strTelnetCmdBuffer[seat].length()>uCmdMax)
		{
			strTelnetCmdBuffer[seat].remove(0, strTelnetCmdBuffer[seat].length()-uCmdMax);
		}
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
		//max_rssi health-maintenance DISABLED (2026-07-07). It disconnected an unpinned link whenever the 8-sample
		//avg RSSI fell >10 dB below the best-ever seen this boot -- but that best-ever is a noisy high-water mark that
		//never really resets, so a lone RSSI spike made it flap a perfectly healthy link every ~34 min, and it judged
		//RSSI-vs-personal-best rather than actual link health. Its payoff also depended on the reconnect landing
		//somewhere better, which only became true once the strongest-AP scan went in. Re-enable, or replace with an
		//absolute floor / a real "is a stronger AP available right now?" roam check, if wanted.
#if 0
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
#endif
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


void LeifForceWifiReconnect()	//runtime "reconnect without reboot": just drop the association; the normal auto-reconnect
{								//path re-associates via SetupWifiInternal -- for an unpinned device that's the strongest-AP scan
								//(all-channel/by-signal on ESP32, software scan on ESP8266), for a pinned one the BSSID branch
								//(LeifSetupBSSID already re-armed it). After a healthy connection the attempt/pin state is already
								//clean and the ESP32 skip-ahead handles a fast retry, so a bare disconnect is all that's needed.
								//(The max_rssi ceiling reset that used to live here went away with the max_rssi health-maint.)
	csprintf(PSTR("Manual WiFi reconnect requested.\n"));
	WiFi.disconnect(false);
}

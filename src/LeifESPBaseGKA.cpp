#if defined(ARDUINO_ARCH_ESP8266)
#ifndef NO_PINGER

#include "Arduino.h"
#include "LeifESPBase.h"

#include "Pinger.h"

extern "C"
{
#include <lwip/icmp.h> // needed for icmp packet definitions
}

static unsigned long ulLastPingReceived=0;


static bool bGatewayKeepaliveRestart=false;
static unsigned long ulGatewayKeepaliveRestartMillis=0;

static String strLastGatewayIp;

void LeifSetSuppressLedWrite(bool bSuppress);
int LeifGetStatusLedPin();

void LeifGatewayKeepalive(const char * szPingIP)
{
	if(bGatewayKeepaliveRestart)
	{
		if(!ulGatewayKeepaliveRestartMillis)
		{
			ulGatewayKeepaliveRestartMillis=millis();
		}

		LeifSetSuppressLedWrite(true);

		digitalWrite(LeifGetStatusLedPin(), (millis()%100)<50?true:false);

		if((int) (millis()-ulGatewayKeepaliveRestartMillis)>5000)
		{
			ESP.restart();
		}

		return;
	}


	if(!Interval1000()) return;

	static Pinger pingGatewayKeepalive;
	Pinger & pinger=pingGatewayKeepalive;

	static bool bPingerSetup=false;

	if(!bPingerSetup)
	{
		bPingerSetup=true;
		pinger.OnReceive([](const PingerResponse & response)
		{
			if(response.ReceivedResponse)
			{
				//csprintf("received\n");
				return false;
			}
			else
			{
				//csprintf("not received\n");
			}
			return true;
		});

		pinger.OnEnd([](const PingerResponse & response)
		{
			if(response.TotalReceivedResponses)
			{
				ulLastPingReceived=seconds();
			}
			else
			{
				csprintf("No response pinging gateway %s. Last successful: %i seconds ago\n",strLastGatewayIp.c_str(),(int) (seconds()-ulLastPingReceived));
			}
			//csprintf("Ping done. Responses: %lu\n",response.TotalReceivedResponses);

			return true;
		});

	}

	static unsigned long ulLastAsyncPing=seconds();






	if(WiFi.isConnected() && (int) (seconds()-ulLastAsyncPing)>60)
	{

		ulLastAsyncPing=seconds();
//		csprintf("ping at seconds=%lu\n",seconds());

		if(szPingIP)
		{
			strLastGatewayIp=szPingIP;
		}
		else
		{
			strLastGatewayIp=WiFi.gatewayIP().toString();
		}

		//csprintf("Pinging gateway IP %s\n",strLastGatewayIp.c_str());

		pinger.Ping(strLastGatewayIp);
//		pinger.Ping("172.22.22.243");

/*		if(pinger.Ping(WiFi.gatewayIP()) == false)
		{
			csprintf("Error during last ping command.\n");
		}
		else
		{
			csprintf("worked");
		}*/


	}



	int diff=(int) (seconds()-ulLastPingReceived);

	if(diff>300)	//5 minutes with no wifi? restart!
	{
		csprintf("NO WIFI for %i seconds -- restarting!\n",diff);
		bGatewayKeepaliveRestart=true;
	}



}


#endif
#endif

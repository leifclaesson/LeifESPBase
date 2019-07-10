#include "LeifESPBase.h"
#include "LeifESPBaseWOL.h"

#include <WiFiUdp.h>
void WakeOnLan(WiFiUDP & udp, const char * mac)
{
	byte mac_binary[6];
	int length=strlen(mac);
	memset(&mac_binary,0,sizeof(mac_binary));
	int b=0;
	for(int a=0;a<length;a++)
	{
		switch(a%3)
		{
		case 0:
			mac_binary[b]=chartohex(mac[a])<<4; break;
		case 1:
			mac_binary[b]|=chartohex(mac[a]); b++; break;
		}
	}


	IPAddress addr(255,255,255,255);

	csprintf("Waking %02X:%02X:%02X:%02X:%02X:%02X\n",mac_binary[0],mac_binary[1],mac_binary[2],mac_binary[3],mac_binary[4],mac_binary[5]);

    byte preamble[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    byte i;

    udp.beginPacket(addr, 9); //sending packet at 9,

    udp.write(preamble, sizeof preamble);

    for (i = 0; i < 16; i++)
	{
        udp.write(mac_binary,sizeof(mac_binary));
	}
    udp.endPacket();



}

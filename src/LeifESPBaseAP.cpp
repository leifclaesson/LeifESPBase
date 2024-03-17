#include "LeifESPBase.h"
#ifndef NO_SOFT_AP

static uint32_t timestampSoftAP=0;

uint32_t LeifGetSoftAP_Duration()	//returns how many seconds the AP has been enabled
{
	if(WiFi.getMode() & WIFI_AP)
	{
		return seconds()-timestampSoftAP;
	}
	return 0;
}


void LeifSetSoftAP(bool bEnable, const char * szPSK)
{

	uint8_t desired_mode=bEnable?WIFI_AP_STA:WIFI_STA;

	if(desired_mode!=WiFi.getMode())
	{
		csprintf("Setting Soft AP mode - enable=%i, psk=%s\n",bEnable,szPSK);

		if(bEnable)
		{
			timestampSoftAP=seconds();
			delay(250);
		}


		WiFi.mode((WiFiMode_t) desired_mode);



		if(bEnable)
		{

#if defined(ARDUINO_ARCH_ESP32)
			WiFi.softAP(GetHostName(), szPSK);
#endif
#if defined(ARDUINO_ARCH_ESP8266)
			softap_config config;
			if(wifi_softap_get_config_default(&config))
			{
				strncpy((char *) config.ssid,GetHostName(),sizeof(config.ssid));
				config.ssid[sizeof(config.ssid)-1]=0;
				config.ssid_len=strlen((const char *) config.ssid);

				strncpy((char *) config.password,szPSK?szPSK:"",sizeof(config.password));
				config.authmode=strlen((const char *) config.password)?AUTH_WPA2_PSK:AUTH_OPEN;

				if(wifi_softap_set_config(&config))
				{
					csprintf("softap config OK: %s %i\n",config.ssid,config.ssid_len);
				}
				else
				{
					csprintf("softap config FAILED\n");
				}

			}
#endif
		}
	}

}

#endif

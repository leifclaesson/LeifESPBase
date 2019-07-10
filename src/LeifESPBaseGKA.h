#pragma once

#if defined(ARDUINO_ARCH_ESP8266)
#ifndef NO_PINGER
void LeifGatewayKeepalive(const char * szPingIP=NULL);
#endif
#endif

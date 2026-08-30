# LeifESPBase
Base library for ESP8266/ESP32

Provides HTTP server, mDNS, simultaneous serial and telnet console output (for debugging), OTA update.

It is designed for use _by programmers_.

For example, SSID, Password, host name are hardcoded and not settable from any user interface.

These and other things were *conscious design decisions* that reduce code complexity and resource requirements.

It's designed to help in the construction of _purpose-built specialty devices_. It is _not_ designed to build user-configurable devices.

As a programmer, I will always have a development environment set up and ready to upload new code to my devices through OTA.

This library depends on several great libraries listed below, which do most of the actual work. I am not claiming any ownership of those - LeifESPBase just ties them together.

Enter your WiFi SSID/key to `environment_setup.h`

## Put the device's own IP address on its periodic status line

If your sketch prints a recurring status line (uptime, free heap, RSSI), put the IP address on it too. An address printed once, at boot, is one you can only catch by being there at the time. A recurring line means anything that starts listening later finds the board without a reboot.

    Uptime=1d 4h  WiFi=1d 4h  MQTT=1d 4h  HeapFree=118332  WiFi: -61  IP 192.168.1.50

Spell it `IP <address>` or `IP: <address>`, case doesn't matter. Keep the word IP next to it. A bare dotted quad is ambiguous, because the same console carries the gateway and the MQTT broker, and anything scanning for the first number it sees will hand back the wrong host. Print 0.0.0.0 rather than hiding the field before DHCP answers.

### Ethernet

Build with `-DUSE_ETHERNET` (ESP32 only) and the library does the rest: it includes `<ETH.h>`, calls `ETH.begin()` and sets the hostname. It also prints the address, but only once, when the link first gets one:

    Ethernet IP: 192.168.1.51

Same limitation as above, so a wired device wants its own recurring line:

    #if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
        csprintf("ETH link=%s  IP %s
",
                 ETH.linkUp() ? "up" : "down",
                 ETH.localIP().toString().c_str());
    #endif

On a device with both interfaces, print the wired line AFTER the WiFi one. Take the last labeled address on the console and the two resolve to whichever interface is up, since the other one reports 0.0.0.0.

Two things that catch people out:

- The PHY pin defines live in `ethernet_setup.h` here, not in your sketch. Older sketches carry a commented-out `ETH_PHY_ADDR` / `ETH_CLK_MODE` block from before that move. It's dead text, and it does NOT mean the board has no Ethernet. The build flags decide.
- You don't need to include `<ETH.h>` yourself.

Set `-DETH_EXT_CLK` if the PHY is fed an external clock on GPIO0 instead of the ESP32 driving GPIO17. `ethernet_setup.h` has the pin-out.

## The telnet console

Five seats on ESP32 with Arduino core 3 or later, two on ESP8266 and on older ESP32 cores. Override with `-DLEIF_TELNET_MAX_CLIENTS=n`.

The low number on the old cores isn't arbitrary. lwIP hands out one fixed table of socket descriptors for the whole device, and a busy sketch has usually spent most of it already (http listen, telnet listen, MQTT, OTA's UDP). Setting the seat count past what is left does not fail loudly: the accept never happens, so the client believes it connected and is then closed without a word.

A full device refuses the newcomer ("All console seats on this device are in use. Try again shortly.") instead of dropping whoever is already sitting there. The scrollback is one shared buffer, replayed to whoever just joined and not at everyone else.

## dependencies (ESP32)

ArduinoOTA
ESP32
ESPmDNS
FS
Update
WebServer
WiFi

## dependencies (ESP8266)

ArduinoOTA
ESP8266mDNS
ESP8266-ping
ESP8266WebServer
ESP8266WiFi
ESPAsyncTCP


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

If your sketch prints a recurring status line — uptime, free heap, RSSI — **put the device's
own IP address on it too.** It costs a few bytes and it is the difference between a device
you can find and one you have to reboot to find.

The reason is that an address announced *once* is an address you can only catch by being
there at the time. A board says where it is when it joins the network, and then never again;
anything that starts listening afterwards — a serial monitor you open an hour later, a tool
that watches the console passively — has no way to ask. Rebooting the device to make it
introduce itself is a poor answer when the whole point was to reach it without touching it.

A recurring line has none of that problem. Whatever is listening learns the address within
one interval of starting to listen, no matter how long the device has been up.

Print it in a shape something else can read:

    Uptime=1d 4h  WiFi=1d 4h  MQTT=1d 4h  HeapFree=118332  WiFi: -61  IP 172.22.24.23

`IP <address>` or `IP: <address>` — either spelling, case-insensitive. Keep the word `IP`
next to it: a bare dotted quad on a console is ambiguous, because status lines are full of
other machines' addresses (a gateway, an MQTT broker) and a reader that takes the first
number it sees will confidently hand back the wrong host. Print `0.0.0.0` rather than
suppressing the field when DHCP has not answered yet — it is unambiguous, and a reader can
discard it.

### Ethernet

Build with `-DUSE_ETHERNET` (ESP32 only) and this library handles the rest: it pulls in
`<ETH.h>`, calls `ETH.begin()`, sets the hostname, and prints

    Ethernet IP: 172.22.24.38

**once, when the link first acquires an address.** That is a one-shot announcement with
exactly the limitation described above, so a wired device wants a recurring line of its own:

    #if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
        csprintf("ETH link=%s  IP %s\n",
                 ETH.linkUp() ? "up" : "down",
                 ETH.localIP().toString().c_str());
    #endif

On a device that has both interfaces, print the wired line **after** the WiFi one. A reader
scanning a rolling console should take the last labelled address it saw, and since the
interface that is down reports `0.0.0.0` and gets discarded, the two lines resolve to
whichever interface is actually carrying traffic without any extra logic.

Two things that catch people out:

- **The PHY pin defines live in `ethernet_setup.h` in this library, not in your sketch.**
  Older sketches carry a commented-out `ETH_PHY_ADDR` / `ETH_CLK_MODE` block from before
  that move. It is dead text. Do not read it as "this board has no Ethernet" — check the
  build flags for `-DUSE_ETHERNET`, which is the only thing that decides.
- **You do not need to include `<ETH.h>` yourself.** `LeifESPBase.h` includes it under the
  same guard, so `ETH` is already declared anywhere the library is.

Set `-DETH_EXT_CLK` if the PHY is fed an external clock on GPIO0 rather than the ESP32
driving GPIO17; `ethernet_setup.h` also documents the full pin-out.

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


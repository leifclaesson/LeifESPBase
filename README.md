# LeifESPBase
Base library for ESP8266/ESP32

Provides HTTP server, simultaneous serial and telnet console output (for debugging), OTA update.

It is designed for use _by programmers_.

For example, SSID, Password, host name are hardcoded and not settable from any user interface.

These and other things were *conscious design decisions* that reduce code complexity and resource requirements.

It's designed to help in the construction of _purpose-built specialty devices_. It is _not_ designed to build user-configurable devices.

As a programmer, I will always have a development environment set up and ready to upload new code to my devices through OTA.

This library depends on several great libraries listed below, which do most of the actual work. I am not claiming any ownership of those - LeifESPBase just ties them together.

Enter your WiFi SSID/key to `environment_setup.h`

[CHANGELOG.md](CHANGELOG.md) has the history.

## Put the device's own IP address on its periodic status line

If your sketch prints a recurring status line (uptime, free heap, RSSI), put the IP address on it too. An address printed once, at boot, is one you can only catch by being there at the time. A recurring line means anything that starts listening later finds the board without a reboot.

    Uptime=1d 4h  WiFi=1d 4h  MQTT=1d 4h  HeapFree=118332  WiFi: -61  IP 192.168.1.50

Spell it `IP <address>` or `IP: <address>`, case doesn't matter. Keep the word IP next to it. A bare dotted quad is ambiguous, because the same console carries the gateway and the MQTT broker, and anything scanning for the first number it sees will hand back the wrong host. Print 0.0.0.0 rather than hiding the field before DHCP answers.

### Ethernet

Build with `-DUSE_ETHERNET` (ESP32 only) and the library does the rest: it includes `<ETH.h>`, calls `ETH.begin()` and sets the hostname. It also prints the address, but only once, when the link first gets one:

    Ethernet IP: 192.168.1.51

Same limitation as above, so a wired device wants its own recurring line:

    #if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
        csprintf("ETH link=%s  IP %s\n",
                 ETH.linkUp() ? "up" : "down",
                 ETH.localIP().toString().c_str());
    #endif

On a device with both interfaces, print the wired line AFTER the WiFi one. Take the last labeled address on the console and the two resolve to whichever interface is up, since the other one reports 0.0.0.0.

Two things that catch people out:

- The PHY pin defines live in `ethernet_setup.h` here, not in your sketch. Older sketches carry a commented-out `ETH_PHY_ADDR` / `ETH_CLK_MODE` block from before that move. It's dead text, and it does NOT mean the board has no Ethernet. The build flags decide.
- You don't need to include `<ETH.h>` yourself.

Set `-DETH_EXT_CLK` if the PHY is fed an external clock on GPIO0 instead of the ESP32 driving GPIO17. `ethernet_setup.h` has the pin-out.

## The telnet console

Everything the sketch prints with `csprintf()` goes to the serial port and to a telnet server on port 23 at the same time, so a deployed board is as readable over the network as it is on the bench. Connect with any telnet client and you get the scrollback replayed first, then the live output. Typed lines go back to the sketch through `LeifRegisterCommandCallback`.

`LeifSetupConsole(bytes)` sizes the scrollback, before `LeifSetupBegin()`. It is one shared buffer, replayed to whoever just joined and not at everyone else.

The console has a fixed number of seats: five on ESP32 with Arduino core 3 or later, two on ESP8266 and on older ESP32 cores. Override with `-DLEIF_TELNET_MAX_CLIENTS=n`. A device with every seat taken refuses the newcomer, with a message:

    All console seats on this device are in use. Try again shortly.

Check what the sketch has left before raising the seat count. lwIP hands out one fixed table of socket descriptors for the whole device, and a busy sketch has usually spent most of it already (http listen, telnet listen, MQTT, OTA's UDP). Past that budget the accept never happens, so the client believes it connected and is then closed without a word.

## The status LED

The onboard LED breathes instead of blinking, and the shape says what the board is doing: a fast deep breath while it is looking for WiFi, a slow shallow one once it is on the network, two pulses and a pause while it is serving its own access point. `LeifSetStatusLedPin(pin)` picks the pin and `-1` turns it off. `LeifSetStatusLED_Override(true, duty)` hands the lamp to the sketch.

On ESP32 the fade runs on LEDC channel 15. A sketch that has spent all sixteen LEDC channels on its own outputs has none left for it, and the only way out used to be `-DNO_FADE_LED` and a plain blink. Build with this instead and the fade moves to the motor-control PWM unit, which most boards never touch, so LEDC channel 15 goes back to the sketch:

    -DUSE_MCPWM_FADE_LED

It takes MCPWM unit 0, timer 0, operator A, and builds the same on Arduino core 1.x and 3.x. Drop `-DNO_FADE_LED` when you add it. The two contradict each other and the build says so.

ESP8266 needs none of this. Its `analogWrite` is a software waveform generator, so there is no channel budget to run out of.

## dependencies (ESP32)

ArduinoOTA
ESP32
FS
Update
WebServer
WiFi

## dependencies (ESP8266)

ArduinoOTA
ESP8266-ping
ESP8266WebServer
ESP8266WiFi
ESPAsyncTCP


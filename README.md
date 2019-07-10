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


# Changelog

Reconstructed from the commit history. The library has never carried release numbers, so entries are dated instead.

## 2026

- **2026-09-10** ESP32: a pad the previous run left attached to a peripheral in the GPIO matrix (a PWM channel, a UART, hardware I2C) is handed back to the GPIO register before `setup()`, from a static constructor, so a pin that a config change moved from a peripheral to plain GPIO works after a software restart or an OTA and no longer needs a power cycle. Arduino core 1.x never touched the matrix in `pinMode`, and a software restart does not reset the GPIO block. Freed pads are listed once on the console after `LeifSetupConsole`: `GPIO matrix: freed pad 2 (signal 86), 18 (signal 30), 19 (signal 29) left attached by the previous run`. A board with a status LED prints its LED pad on every software restart; that is the LED's PWM channel being released, not a fault. Pads driven as plain GPIO keep their level through the restart as before; the SPI flash pads and UART0 TX are never touched.
- **2026-09-05** The compile time on the status page is the time the image was actually built. It used to be `__DATE__`/`__TIME__` from whenever the sketch's own main file last compiled, so changing anything else left the page reporting an old date. The build now writes the time into a sentinel in the linked `.elf`; a build that has nothing to do still writes nothing, and an image that was never stamped falls back to the old value. `LeifGetLinkDate` returns the stamp alone, empty when there is none.
- **2026-08-30** WiFi modem sleep is no longer disabled on ESP32, so the Arduino core default applies again.
- **2026-08-30** The README now documents the telnet console, and the convention for putting the device's IP address on a recurring status line.
- **2026-08-23** The telnet console takes several clients at once: five seats on ESP32 with Arduino core 3 or later, two on ESP8266 and on older ESP32 cores. Override with `-DLEIF_TELNET_MAX_CLIENTS=n`.
- **2026-08-23** A device with every console seat taken refuses the newcomer with a message, instead of silently dropping whoever was already connected.
- **2026-08-23** A console seat freed in the same pass as a new arrival is reusable immediately.
- **2026-08-22** New `LeifSetOtaTooLargeCallback` fires when an OTA image is refused for being larger than the app slot, so a sketch can repartition and restart. Without a handler the OTA fails as before.
- **2026-08-16** The PHY mode is set explicitly at every boot, so a board cannot stay pinned to 802.11b, and `/sysinfo` prints the live mode.
- **2026-08-16** The status page shows the signal level the access point hears the board at.
- **2026-08-16** The WiFi scan runs asynchronously instead of blocking `loop()` for a couple of seconds.
- **2026-08-16** The web server writes long responses without blocking.
- **2026-08-16** A temporarily pinned BSSID can be saved from the status page in one click.
- **2026-07-07** WiFi connects to the strongest access point rather than the first one answering. `LeifScheduleForceReconnect` and the `/wifireconnect` handler force a fresh pick, and `LeifSetBSSIDSessionOnly` keeps a pin from being written back.
- **2026-07-07** The old max_rssi health-maintenance disconnect is retired.
- **2026-07-07** New `LeifSetServiceBackgroundCallback` registers a pump (typically the linked MQTT library) that keeps running while a long web page renders.
- **2026-07-06** ESP32 core 3.x: the hostname is set before station mode starts, so the interface no longer keeps the default `esp32-XXXXXX`.
- **2026-07-04** `GetResetReasonString` added.
- **2026-07-04** WiFi modem sleep disabled on ESP32.
- **2026-04-18** New `LeifSetStatusLED_Override` lets a sketch drive the status LED itself.
- **2026-01-22** ESP-IDF log output is silenced before the console opens rather than after, so core 3 boot spam stays off the console.
- **2026-01-22** `WiFi.setAutoConnect(false)` now also applies on ESP32 with Arduino core 1.x.

## 2025

- **2025-09-16** New `LeifSetInterimCallback` registers a callback that runs during lengthy operations, such as sending the telnet scrollback.
- **2025-09-16** The status LED fade uses the Arduino core 3 `ledcAttachChannel` and `ledcWrite` API.
- **2025-09-16** ESP-IDF log output is silenced on Arduino core 3.
- **2025-05-29** ESP32 Arduino SDK v3 support.
- **2025-05-03** Ethernet support improved.

## 2024

- **2024-08-23** Ethernet support for ESP32.
- **2024-03-18** `NO_DEFAULT_WIFI` in `environment_setup.h` for sketches that supply their own credentials.
- **2024-03-17** Soft AP support, with its own status LED heartbeat pattern while in AP mode.

## 2023

- **2023-08-31** Deeper status LED fade, easier to see.
- **2023-06-18** Constant strings moved to PSTR, saving RAM.
- **2023-06-14** The project name is printed at startup.
- **2023-04-18** Compile warnings fixed.
- **2023-02-11** New `NO_FADE_LED` and `NO_SOFT_AP` build options drop the LED fade code and the soft AP code.
- **2023-01-20** `ScheduleReconnect` added.

## 2022

- **2022-08-30** max_rssi drops 2 dB for every reconnection attempt, so a board cannot get stuck retrying forever.
- **2022-08-16** WiFi health maintenance added.
- **2022-08-15** New `GetWiFiAPName` hook.
- **2022-08-01** With fade disabled, the status LED blinks every 15 seconds once WiFi is up, rather than every 2.
- **2022-05-17** Empty command lines are passed through to the sketch.
- **2022-05-16** New `allow_serial_commands` device config option.
- **2022-05-02** An empty backup SSID is ignored instead of being tried.
- **2022-04-30** ESP32 heap information on the system info page works again.
- **2022-04-27** ESP8266 MMU heap statistics reported correctly.
- **2022-04-27** Status LED brightness is adjustable.
- **2022-03-31** The telnet console handles backspace and filters out telnet negotiation characters.
- **2022-03-24** `SetMaxCommandLength` added.
- **2022-02-25** The ESP32 hardware watchdog is used.

## 2021

- **2021-12-05** mDNS support removed.
- **2021-12-05** `GatewayKeepAlive` removed.
- **2021-12-05** `ScheduleRestart` added.
- **2021-12-05** `delay()` removed from network callbacks, where the `yield()` inside it could throw an ESP8266 exception.
- **2021-12-05** IRAM heap status shown on ESP8266 Arduino core 3.x.
- **2021-12-05** TelnetPrint carriage return bug fixed.
- **2021-10-15** `SetProjectName` added.
- **2021-08-20** The telnet console sends a carriage return with each line feed.
- **2021-08-08** ESP8266: `WiFi.disconnect()` is called on a station disconnect event, working around a core connection-loss bug.
- **2021-03-28** ESP32 no longer reopens mDNS periodically.
- **2021-02-14** Uptime on the main HTTP page.
- **2021-02-13** A WiFi watchdog restarts WiFi if it gets stuck connecting.
- **2021-01-04** Command line support: the console takes typed commands and hands them to the sketch through `LeifRegisterCommandCallback`.

## 2020

- **2020-10-19** New `OTA_DONE` and `OTA_FAILED` shutdown callbacks.
- **2020-10-19** The flood of timed events right after an OTA update is fixed.
- **2020-10-12** mDNS is restarted on each WiFi reconnection (ESP32).
- **2020-08-17** Telnet console scrollback buffer: a client that connects gets the recent output replayed. `LeifSetupConsole(bytes)` sizes it.
- **2020-08-01** Heap fragmentation shown on the system info page.
- **2020-07-13** Time-limited soft AP functions added.
- **2020-05-25** Status LED intensity reduced.
- **2020-05-24** `GetWifiStatus` string added.
- **2020-05-17** The LED fade range is selectable.
- **2020-05-17** OTA resets `analogWriteRange`.
- **2020-05-16** The status LED breathes fast for the first few seconds after boot, then settles down to slow, so it is easy to spot early without being a constant annoyance afterwards.
- **2020-05-12** Clock speed measurement on the system info page.
- **2020-05-11** The status LED fades instead of blinking (optional), and the main loop polls at most every 50 ms.
- **2020-04-30** BSSID support.
- **2020-04-05** SSID, BSSID and RSSI on the HTML main page.

## 2019

- **2019-08-16** `GatewayKeepAlive` checks that the status LED pin is valid before using it.
- **2019-08-02** New `LeifUpdateCompileTime` macro captures the compile time of the sketch rather than of the library.
- **2019-07-31** `LeifSecondsToUptimeString` added, and the day counter removed (the second counter will not overflow for 137 years).
- **2019-07-29** `environment_setup.h` is shipped as `environment_setup.h.example` and gitignored, so your credentials stay out of git.
- **2019-07-29** The SSID is printed to the console on each reconnection.
- **2019-07-11** Initial check-in.

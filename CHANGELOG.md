# Changelog

Reconstructed from the commit history. The library has never carried release numbers, so entries are dated instead.

## 2026

- **2026-10-02** ESP32: signing in to a board over HTTP no longer has to win a race. `WebServer::requestAuthentication()` keeps exactly one nonce/opaque pair and discards the previous one, so a client answering an earlier challenge was refused -- and because that slot is global to the server, a second tab, another browser or a script polling the same board silently invalidated the challenge the first one was still using. `LeifWebAuthGate` now keeps the last `LEIF_WEBAUTH_CHALLENGE_SLOTS` (8, the per-host connection limit both Chrome and Firefox use) in a fixed-size ring that costs no heap, each answerable for `LEIF_WEBAUTH_CHALLENGE_LIFETIME_MS` (5 minutes), and the verifier takes a match against any live entry. The client's `nc` request counter is checked for the first time -- the stock server parses it and then ignores it, so a captured `Authorization` header could be replayed until the next challenge was issued. Each value is accepted once, inside a 32-wide window of the highest seen, rather than having to climb strictly: one challenge is spent over several connections and a single-threaded server sees those answers out of order, so a strict rule would refuse legitimate requests. A refusal that hashed correctly and only met a dead challenge answers `stale=true`, which a browser retries by itself instead of prompting the user -- decided **after** the hash is verified and never before, because telling a client with the *wrong* password that its challenge was stale makes it retry silently forever. **Proved on hardware 2026-10-02** on a Babelfish Model 42 at `172.22.28.48`: six simultaneous authenticated requests got **1 of 6** in on the old code and **6 of 6** on the new, three rounds each. A real `/ui` page load in headless Edge, with one other client talking to the board at the same time, demanded the password **65 times** and abandoned the stylesheet and the script on `ERR_TOO_MANY_RETRIES`; in Firefox **702 times** in 40 s, with the page half-built. Both now ask exactly once and render all 944 elements. Eight protocol arms pass against the board: a replayed header refused, the next counter on that challenge still accepted, one arriving late inside the window accepted, a dead challenge marked stale, and a wrong password *not* marked stale.
- **2026-09-15** ESP32: the slot table on `/tools` says how many of a slot's bytes are actually firmware, not just how big the partition is. Every bulb's partition is 1,310,720 bytes whatever is in it, so the old column answered a question nobody asked; an image carries its own length in its segment table, and `/sysinfo`'s `Sketch` line has been reading it out all along -- but only for the slot the board is running from, which is the one slot whose contents were never in doubt. `LeifImageLengthInSlot` aims the same walk at either slot, so the idle one -- the previous firmware, and the whole reason the readback exists -- can say what it holds too. The number also rides on `/firmware.bin` as an `X-Image-Length` header, so a script can trim the padding without guessing, and the download itself is **unchanged**: still the whole partition, `Content-Length` and all. That asymmetry is the design. A bug in the walk makes a number on a page wrong, where trimming on the board would make the *file* wrong -- a short image that looks complete, of a firmware that may not exist anywhere else. A slot the walk cannot make sense of is reported as empty or unrecognised rather than as a number. Not a backwards scan for the first non-`0xFF` byte, which is the obvious shortcut: measured on a bench bulb 2026-09-15, past the end of the 1,178,416-byte image in its idle slot sat 118,263 non-`0xFF` bytes, the remains of the larger 4.43 that had occupied that slot before, and the scan would have answered about 1.3 MB. Not `esp_image_verify()` either, which would take an arbitrary partition happily but reads and SHA256s the whole image, half a second per slot on a board whose outputs freeze while a page renders; the walk reads about a hundred bytes. The image header is read as raw bytes at fixed offsets rather than through `esp_image_header_t`, whose middle has been re-carved repeatedly across IDF versions while the three fields wanted here never moved -- one code path for core 1.0.6 and core 3.3.10 instead of two. Costs 832 bytes on core 3.3.10, the page's own explanation of why the download is bigger than the number beside it included. **Proved on hardware 2026-09-15** on the bench bulb at `172.22.28.181`, holding a known build in each slot: the page reported 1,178,416 and 1,179,248, exactly the two build sizes, and the running slot's figure matches `ESP.getSketchSize()` -- two separate implementations, same slot, same answer. The idle slot was then downloaded whole and its first 1,178,416 bytes were byte-identical to the archived image of what had been running.
- **2026-09-14** New `/tools` page: the things this library can do that no menu has ever pointed at. It lists the two flash slots with a download link each (the `/firmware.bin` readback below, whose whole value depends on somebody remembering it exists at the one moment it matters), whatever console commands the project has declared, and the endpoints that have always been there and were never written down -- `/ping`, `/sysinfo`, `/wifireconnect`. The library places its own link to it under the status table on every project's main page, so no sketch has to be edited for the page to be reachable: a menu row is built by a differently named function in each of the 86 projects and the library has never been able to add a cell to one. A project that would rather carry the link in its own menu row says so with `LeifToolsLinkHandledByProject()` and the library's link stands down instead of appearing twice on one page. Console commands are declared, not discovered: every project registers one callback that receives whatever was typed, so names and meanings exist only inside that project's own `if` chain and nothing can enumerate them. `LeifDeclareCommand("locate", "flash the status LED...")` adds one, `LeifDeclaredCommandsText` renders the list -- which is how a project's own `help` and this page stop being two hand-maintained copies that drift. Declaring is optional throughout; a project that never calls it shows nothing and breaks nothing. Costs 3,328 bytes measured on `Lightbulb_ESP32`. `-DNO_TOOLS_PAGE` removes it, and removes the declarations with it: `LeifDeclareCommand` becomes a macro rather than an empty function precisely so the declared text leaves the image too, which an unused inline would not achieve -- string literals pool into one section that `--gc-sections` cannot split.
- **2026-09-14** ESP32: `/firmware.bin` downloads the firmware back out of a flash slot, so a build that exists only on a board can be recovered instead of being lost the next time that board is flashed. An over-the-air update never writes into the slot it is running from -- it fills the other one and switches over only after that verifies -- so the firmware a board is running survives the push that replaces it, and then sits in the idle slot until the push *after* that lands on top of it. One push buys one recovery; a push that fails costs nothing, because while the wanted firmware is the running one it cannot be written to at all. With no argument the endpoint serves the idle slot, which holds the previous firmware and is the reason it exists; `?slot=running` serves the live one, and `?slot=<label>` any app partition listed on `/sysinfo`. The whole partition is sent raw, trailing padding included: finding where an image ends would mean walking its segment headers on the board, and a bug there would silently truncate what may be the only surviving copy, whereas `esptool image-info` reads a padded image without complaint. The outputs freeze for the length of the transfer -- roughly 2 MB down one socket -- because this renders on `loopTask` like every other page, so it suits a deliberate recovery and nothing automatic; the task watchdog is fed per chunk. Costs 1,296 bytes. `-DNO_FIRMWARE_READBACK` removes it. *(A line naming the endpoint was added to `/sysinfo` and taken back out the same day: that page answers what a board is doing, not what can be done to it. The `/tools` page below names it instead.)* **Proved on hardware 2026-09-14** on a bench bulb at `172.22.28.181`: a fitting image was pushed over plain 4.43, and the idle slot read back byte-identical over its whole 1,300,528 bytes to the archived copy of what had been running, the remaining 10,192 bytes all `0xFF`. The running slot read back byte-identical to the image that had just been pushed -- and its tail was *not* padding but the remains of the larger image that had occupied that slot before, which is the clearest evidence that this reads real flash rather than reconstructing anything.
- **2026-09-13** ESP32: a project that takes no link-stub cut no longer has to carry `LinkStubsESP32.cpp` at all. `LeifGetLinkStubHits` is an inline zero in the header unless one of the cut flags is set, and only then is it a symbol in that file. Without this, the `/sysinfo` line added the day before was an unconditional call into a source file that Sloeber lists per project in its generated makefile and only refreshes when the IDE opens the project -- so every ESP32 project built from an existing `Release\` snapshot stopped linking on `undefined reference to LeifGetLinkStubHits()`, having asked for nothing. Opening the project in Sloeber always fixed it; nothing else did.
- **2026-09-13** ESP32: the arduino-esp32 3.3.x version check now covers every cut in `LinkStubsESP32.cpp` instead of only the AP/WPA3 pair. All three stub sets are derived against the exact archives shipped with core 3.3.10 / ESP-IDF 5.5.4, and `NO_PPP` -- the one with no interlock of its own and the easiest to add anywhere -- was the one that could reach an older core unchecked.
- **2026-09-13** ESP32: `-DNO_CAMELLIA_ARIA` takes **6,543 bytes**, the Camellia and ARIA block ciphers out of `libmbedcrypto.a`. They are reached only through mbedtls' cipher lookup table, never named by anything in the image, and the stubs return an error from `setkey` rather than a success -- so a build that somehow did negotiate one fails to set up the cipher instead of handing back plaintext dressed as ciphertext. Safe for a project that uses TLS.
- **2026-09-13** ESP32: `-DNO_PPP` takes **19,351 bytes**, lwIP's PPP and PPPoS stack, for a board that reaches the network over WiFi or Ethernet. Nine stubs; `pppos_create` returns NULL so `esp_netif_new_ppp` fails the netif cleanly rather than holding a half-built pcb. One of them, `ppp_init`, genuinely runs at every boot from `lwip_init` -- its real body only builds the free lists of PPP's own memory pools, which nothing then allocates from -- so it is the one stub that deliberately does **not** count a hit, and `LINK STUB HITS` on `/sysinfo` stays an alarm rather than becoming a tally.
- **2026-09-12** ESP32: `-DNO_HOST_AP -DNO_WPA3 -DNO_SOFT_AP` together take **84,576 bytes** out of the image, by defining the SDK-internal symbols locally so the linker never opens the WPA3/SAE, OWE and AP-authenticator members of `libwpa_supplicant.a` -- and with them the mbedtls elliptic-curve, bignum, ASN.1, PEM and RSA cascade, 39,277 bytes of `libmbedcrypto.a` that nothing else in a station-only image referenced. On Lightbulb_ESP32_PCF8574 that is 1,300,528 -> 1,215,952, free space in the app slot 10,192 -> 94,768. The three flags are inseparable on ESP32 and the build says so: cutting WPA3 alone needs 29 stubs instead of 22, one of them a FreeRTOS mutex handle the AP code would lock. `NO_HOST_AP` also requires `NO_SOFT_AP`, which removes `LeifSetSoftAP` and so makes the cut a link error in any project that still brings an access point up. No feature is lost on a station: what leaves is WPA3/SAE, OWE and the ability to authenticate clients of your own AP. New `LeifGetLinkStubHits` is non-zero if one of those stubs was ever entered, and `/sysinfo` prints a line only when it is.
- **2026-09-12** ESP32: `-DUSE_MCPWM_FADE_LED` runs the status LED fade on the motor-control PWM unit instead of LEDC channel 15, so a board that has spent all sixteen LEDC channels on its own outputs can still breathe rather than blink, and gets channel 15 back for itself. It takes MCPWM unit 0, timer 0, operator A, and builds the same on Arduino core 1.x and 3.x. Drop `-DNO_FADE_LED` when you add it; the two contradict each other and the build says so.
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

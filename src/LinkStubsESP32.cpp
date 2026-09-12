//Stubs that exist only to keep code OUT of the image -- the ESP32 counterpart of
//arduino\Lightbulb\LinkStubs.cpp, which is the ESP8266 original.
//Each one satisfies a reference from a precompiled ESP-IDF archive so the linker never opens
//the member that would otherwise be dragged in. Measurements and the full reachability
//working live in misc\docs\plans\lightbulb-esp32-pcf8574-size-audit-plan.md.
//
//Worth 84,816 B on Lightbulb_ESP32_PCF8574 (1,300,528 -> 1,215,712; margin 10,192 -> 95,008),
//measured 2026-09-12 and proven on a bench board the same night: associates to WPA2-PSK,
//MQTT + PTP up, OTA push accepted, all identical to stock.

#include "LeifESPBase.h"

#if defined(ARDUINO_ARCH_ESP32) && (defined(NO_HOST_AP) || defined(NO_WPA3))

//---------------------------------------------------------------------------------------------
//Three interlocks, because getting this wrong produces a board that cannot join a network --
//and the whole reason the cut exists is boards nobody goes back to.
//---------------------------------------------------------------------------------------------

#if !defined(NO_HOST_AP) || !defined(NO_WPA3)
//⛔ On ESP32 the two CANNOT be separated, and this is measured, not assumed. Cutting WPA3
//alone (keeping the AP authenticator) needs 29 stubs instead of 22, because esp_hostap.c,
//ieee802_11.c, sta_info.c and ap_config.c all still reference SAE for AP-side WPA3 -- and one
//of the 29 is g_wpa3_hostap_auth_api_lock, a FreeRTOS mutex HANDLE whose value is load-bearing
//(esp_hostap.c would lock a NULL). Cutting the AP side alone leaves station SAE, which is the
//half that drags in 39 KB of mbedcrypto. Taken together it is 22 stubs, every one a function,
//with no data symbol whose initial value matters. Set both or set neither.
#error "On ESP32, NO_HOST_AP and NO_WPA3 are inseparable -- define both (see lightbulb-esp32-pcf8574-size-audit-plan.md)"
#endif

#ifndef NO_SOFT_AP
//Same rule as the ESP8266 file: cutting the AP-side authenticator means the soft AP cannot
//authenticate anyone, so a project that still brings one up must not take this cut.
//⭐ This is also the automatic guard. LeifESPBaseAP.cpp is wholly inside #ifndef NO_SOFT_AP,
//so once NO_SOFT_AP is set, LeifSetSoftAP() no longer exists and any project that calls it
//fails to LINK. As of 2026-09-12 that is Babelfish_Model42, CDPlayerInterface and
//LeifESPBaseFlexMenu -- none of which may take this cut.
#error "NO_HOST_AP cuts the AP-side authenticator, so a soft AP cannot work -- set NO_SOFT_AP too"
#endif

//A canary against an SDK bump moving the ground under us. These symbols are INTERNAL to the
//wpa_supplicant component -- they appear in no public header, so nothing else would notice a
//renamed or re-signatured one until a board failed to associate in the field. The prototypes
//below were recovered from the shipped .debug_info of esp32-libs 3.3.10's own archives
//(misc\claude\lightbulb-size-audit\bench\dwarfproto.py), so they are exact for THAT build.
//⛔ Nested, not one || expression: on core 1.0.6 ESP_ARDUINO_VERSION_VAL does not exist, and a
//function-like macro that does not exist is a preprocessor SYNTAX error inside #if, not a
//quietly-zero identifier -- so the guard would fail as a parse error instead of as this message.
#ifndef ESP_ARDUINO_VERSION
#error "LinkStubsESP32 needs arduino-esp32 3.3.x (ESP-IDF 5.5.4); this core is too old -- re-derive the stub set first"
#else
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 0)
#error "LinkStubsESP32 was derived against arduino-esp32 3.3.x / ESP-IDF 5.5.4 -- re-derive the stub set before using it on an older core"
#endif
#endif

#include <stdint.h>
#include <stddef.h>

//---------------------------------------------------------------------------------------------
//How reachable each stub is. Three classes -- and the honest answer is NOT "none of them run".
//---------------------------------------------------------------------------------------------
//
//  Class A -- UNREACHABLE, structurally. Called only through the wpa_funcs table the WiFi
//     driver populates for AP opmode, or from the AP's own join / EAPOL paths. The board is
//     WIFI_STA (LeifESPBaseMain.cpp: WiFi.mode(WIFI_STA)) and the NO_SOFT_AP interlock above
//     makes it a link error for any project that would change that.
//
//  Class B -- REACHED, and a no-op is what the real one does anyway. Proven by disassembling
//     the real function, not by reading source:
//       esp_wpa3_free_sae_data()  every branch is guarded by beqz on a static pointer
//                                 (g_sae_data, g_sae_token, g_sae_pt). On a station that never
//                                 ran SAE all three are NULL, so the real body frees nothing.
//       owe_deinit()              first act is `bne sm->key_mgmt, WPA_KEY_MGMT_OWE -> return`.
//                                 On WPA2-PSK it returns having done nothing.
//     Both are called from wpa_sta_disconnected_cb() / wpa_deattach(), which DO run on a
//     station -- every disconnect. Silence here is correct; a log line would be noise on a
//     path that fires normally.
//
//  Class C -- REACHED, and false is the RIGHT answer, not merely a safe one.
//       sae_pk_valid_password()   called from wpa_sm_set_ap_rsnxe(), which returns early
//                                 unless the AP sent an RSNXE element AND the station's
//                                 sae_pk mode is not DISABLED. It asks "is this passphrase in
//                                 SAE-PK form?" -- for a WPA2 PSK the true answer is no, and
//                                 the caller's `beqz -> return` path on false is exactly the
//                                 path a stock build takes.
//
//⛔ No stub below returns a pointer that its caller then dereferences on a path that can run.
//The Class A returns are NULL/false/0 because on the one path that could observe them the
//meaning is "there is no AP here", which is true.

//Opaque forward declarations -- every one of these is only ever passed or returned as a
//pointer, so an incomplete type is exactly right and keeps the SDK's private headers out.
struct wpa_funcs;
struct crypto_ecdh;
struct wpabuf;
struct hostapd_data;
struct sta_info;
struct wpa_authenticator;
struct wpa_state_machine;

//wpa_supplicant's own scalar spellings, and its `wpa_event` -- an anonymous unsigned enum of
//byte_size 4 in the shipped DWARF, so unsigned int is ABI-exact.
typedef uint8_t u8;
typedef uint16_t u16;
typedef unsigned int wpa_event;

//A stub that is ever entered means a premise above is wrong. Counting costs 4 bytes of .bss and
//a handful of instructions, and turns "impossible" into something a board can actually be asked
//about -- LeifGetLinkStubHits() is non-zero if and only if one of these ran.
//⛔ Deliberately NOT a csprintf: two of these sit on the disconnect path and one on the RSNXE
//parse, and a print from the WiFi driver task is a worse bug than the one it would report.
static volatile uint32_t g_LinkStubHits = 0;
#define STUB_HIT() do { g_LinkStubHits++; } while (0)

extern "C"
{

//--- Class A: the AP-side authenticator (hostapd) ---------------------------------------------
//Reached only from esp_supplicant_init() (address-taken into the wpa_funcs table),
//hostap_sta_join() and wpa_ap_rx_eapol() -- all AP opmode. Verified by relocation, not by
//reading source: misc\claude\lightbulb-size-audit\bench\callsites.py.

void * hostap_init(void)
{
	STUB_HIT();
	return 0;					//"no AP context" -- true, there is no AP
}

bool hostap_deinit(void *data)
{
	(void) data;
	STUB_HIT();
	return true;					//nothing to tear down, so teardown succeeded
}

struct hostapd_data * hostapd_get_hapd_data(void)
{
	STUB_HIT();
	return 0;
}

bool hostap_new_assoc_sta(struct sta_info *sta, uint8_t *bssid, uint8_t *wpa_ie,
		uint8_t wpa_ie_len, uint8_t *rsnxe, uint16_t rsnxe_len, bool *pmf_enable,
		int subtype, uint8_t *pairwise_cipher, uint8_t *reason)
{
	(void) sta; (void) bssid; (void) wpa_ie; (void) wpa_ie_len; (void) rsnxe;
	(void) rsnxe_len; (void) pmf_enable; (void) subtype; (void) pairwise_cipher; (void) reason;
	STUB_HIT();
	return false;					//refuse the association rather than half-admit it
}

u16 esp_send_assoc_resp(struct hostapd_data *hapd, const u8 *addr, u16 status_code,
		bool omit_rsnxe, int subtype)
{
	(void) hapd; (void) addr; (void) status_code; (void) omit_rsnxe; (void) subtype;
	STUB_HIT();
	return 1;					//802.11 status 1 = UNSPECIFIED_FAILURE
}

bool wpa_ap_remove(u8 *bssid)
{
	(void) bssid;
	STUB_HIT();
	return false;
}

void wpa_receive(struct wpa_authenticator *wpa_auth, struct wpa_state_machine *sm,
		u8 *data, size_t data_len)
{
	(void) wpa_auth; (void) sm; (void) data; (void) data_len;
	STUB_HIT();
}

//--- Class A: the AP-side station table --------------------------------------------------------
//Only hostap_sta_join() (AP opmode) and ieee802_1x.c call these. ieee802_1x.c.obj is in the
//archive but contributes ZERO image bytes -- 802.1X / WPA-Enterprise is absent from this build
//(esp_eap_client is 29 bytes of stub), so --gc-sections already discards it.

struct sta_info * ap_get_sta(struct hostapd_data *hapd, const u8 *sta)
{
	(void) hapd; (void) sta;
	STUB_HIT();
	return 0;
}

struct sta_info * ap_sta_add(struct hostapd_data *hapd, const u8 *addr)
{
	(void) hapd; (void) addr;
	STUB_HIT();
	return 0;
}

void ap_free_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
	(void) hapd; (void) sta;
	STUB_HIT();
}

void ap_sta_delayed_1x_auth_fail_disconnect(struct hostapd_data *hapd, struct sta_info *sta)
{
	(void) hapd; (void) sta;
	STUB_HIT();
}

int wpa_auth_sm_event(struct wpa_state_machine *sm, wpa_event event)
{
	(void) sm; (void) event;
	STUB_HIT();
	return 0;
}

//--- Class A: WPA3 / OWE callback registration --------------------------------------------------
//esp_supplicant_init() calls all three to plant function pointers in the wpa_funcs table. Not
//registering them is the cut: the driver then has no SAE/OWE entry points to call, which is the
//intended state on a network where no SSID an ESP32 can reach offers either.
//⭐ Safety gate, checked against the controller and not against a snapshot of who is connected:
//of 6 WLANs only `stranger` offers WPA3, and it is 5 GHz only while the ESP32 is 2.4 GHz-only
//hardware -- so a board can never associate to it. No SSID anywhere requires PMF.

void esp_wifi_register_wpa3_cb(struct wpa_funcs *wpa_cb)
{
	(void) wpa_cb;
	//Not counted as a hit: this one is CALLED every boot by design -- registering nothing is
	//the whole point. Counting it would make the counter useless as an alarm.
}

void esp_wifi_register_wpa3_ap_cb(struct wpa_funcs *wpa_cb)
{
	(void) wpa_cb;
}

void esp_wifi_register_owe_cb(struct wpa_funcs *wpa_cb)
{
	(void) wpa_cb;
}

//--- Class B: cleanup that a station really does call, where a no-op IS the real behaviour ------

void esp_wpa3_free_sae_data(void)
{
	//Real body: three beqz-guarded wpabuf_free()s on g_sae_data / g_sae_token / g_sae_pt, plus
	//sae_clear_data() over an all-zero g_sae. A station that never ran SAE allocated none of
	//them, so this frees nothing and leaks nothing.
}

void owe_deinit(void)
{
	//Real body returns immediately unless sm->key_mgmt == WPA_KEY_MGMT_OWE (0x400000).
	//On WPA2-PSK it is not, so the real one is already a no-op here.
}

//--- Class B/C: OWE key agreement, and the SAE-PK password test ---------------------------------
//The crypto_ecdh_* four are the root of the entire 39,277 B mbedcrypto cascade -- ecp,
//ecp_curves, bignum, oid, pkparse, pem, asn1*, ecdsa, rsa and hmac_drbg all hang off them.
//They are called only from owe_build_assoc_req() / owe_process_assoc_resp(), and
//owe_build_assoc_req() returns at its second instruction unless the group is 19.

struct crypto_ecdh * crypto_ecdh_init(int group)
{
	(void) group;
	STUB_HIT();
	return 0;					//caller's next instruction is beqz -> bail out
}

void crypto_ecdh_deinit(struct crypto_ecdh *ecdh)
{
	(void) ecdh;
	//Reached from owe_deinit() in a stock build, which itself never runs on WPA2-PSK.
	//Not counted: freeing a NULL handle is a legitimate no-op.
}

struct wpabuf * crypto_ecdh_get_pubkey(struct crypto_ecdh *ecdh, int y)
{
	(void) ecdh; (void) y;
	STUB_HIT();
	return 0;
}

struct wpabuf * crypto_ecdh_set_peerkey(struct crypto_ecdh *ecdh, int inc_y,
		const u8 *key, size_t len)
{
	(void) ecdh; (void) inc_y; (void) key; (void) len;
	STUB_HIT();
	return 0;
}

bool sae_pk_valid_password(const char *pw)
{
	(void) pw;
	//Class C: false is the CORRECT answer for a WPA2 PSK, not a safe fallback. See the header
	//comment -- wpa_sm_set_ap_rsnxe() only gets here when the AP sent an RSNXE, and its false
	//branch is the same one a stock build takes for a non-SAE-PK passphrase.
	return false;
}

}	//extern "C"

#endif	//ARDUINO_ARCH_ESP32 && (NO_HOST_AP || NO_WPA3)

//Always defined, so a project can read it without caring whether it took the cut -- a build
//that did not stub anything has nothing to report and says zero.
uint32_t LeifGetLinkStubHits()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(NO_HOST_AP) && defined(NO_WPA3)
	return g_LinkStubHits;
#else
	return 0;
#endif
}

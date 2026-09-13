//Stubs that exist only to keep code OUT of the image -- they are never called.
//Each one satisfies a reference from a precompiled SDK archive (ESP-IDF on ESP32, the Espressif
//SDK on ESP8266) so the linker never opens the member that would otherwise be dragged in.
//
//⭐ ONE file for BOTH platforms, split by #ifdef -- the ESP8266 half moved in from
//arduino\Lightbulb\LinkStubs.cpp on 2026-09-13, which is where this trick started.
//Every block below is guarded by its architecture AND its flag, so a project that sets no cut
//flag compiles this file to nothing at all.
//
//  ESP32    NO_HOST_AP + NO_WPA3 (+ NO_SOFT_AP)   84,576 B   SAE/SAE-PK/OWE + AP authenticator
//  ESP32    NO_CAMELLIA_ARIA                       6,543 B   mbedtls ciphers nothing selects
//  ESP32    NO_PPP                                19,351 B   lwIP's PPP / PPPoS stack
//  ESP8266  NO_SOFT_AP                             3,088 B   LwipDhcpServer
//  ESP8266  NO_HOST_AP                             7,397 B   ieee80211_hostap.o (+73 B RAM)
//
//Working: misc\docs\plans\lightbulb-esp32-pcf8574-size-audit-plan.md (ESP32) and
//misc\docs\plans\lightbulb-esp8266-strip-plan.md (ESP8266).
//
//The ESP32 AP/WPA3 pair is worth 84,576 B on Lightbulb_ESP32_PCF8574 (1,300,528 -> 1,215,952;
//margin 10,192 -> 94,768), proven on a bench board 2026-09-12: associates to WPA2-PSK, MQTT +
//PTP up, OTA push accepted, all identical to stock, and a 6 h paired soak against a stock
//control. (84,816 / 1,215,712 appear in older notes -- that was the loose measurement stub,
//before the promotion into this file. The tracked build measures 1,215,952 exactly.)

#include "LeifESPBase.h"

//LEIF_LINKSTUBS_ANY and the list of what each flag cuts live in LinkStubs.h, which
//LeifESPBase.h pulls in above -- the header has to make the same decision anyway, to know
//whether LeifGetLinkStubHits() is a symbol here or an inline zero.

#ifdef LEIF_LINKSTUBS_ANY

//A canary against an SDK bump moving the ground under us. Every symbol this file defines is
//INTERNAL to its component -- none appears in a public header, so nothing else would notice a
//renamed or re-signatured one until a board failed in the field. The prototypes were recovered
//from the shipped .debug_info of esp32-libs 3.3.10's own archives
//(misc\claude\lightbulb-size-audit\bench\dwarfproto.py), so they are exact for THAT build.
//⛔ This guard covers EVERY cut in this file, not just the AP/WPA3 pair. It used to sit inside
//the NO_HOST_AP/NO_WPA3 block, which left NO_CAMELLIA_ARIA and NO_PPP unguarded -- and those two
//are derived against exactly the same SDK. That matters here and not in theory: of the 68 ESP32
//Sloeber projects, 50 are on core 1.0.6 / 2.0.x / 3.1.x (surveyed 2026-09-13), so the cut with
//the fewest interlocks was also the one most likely to be added to a core it was never derived
//for. Moved up before the first promotion outside Lightbulb_ESP32_PCF8574.
//⛔ Nested, not one || expression: on core 1.0.6 ESP_ARDUINO_VERSION_VAL does not exist, and a
//function-like macro that does not exist is a preprocessor SYNTAX error inside #if, not a
//quietly-zero identifier -- so the guard would fail as a parse error instead of as this message.
#ifndef ESP_ARDUINO_VERSION
#error "The ESP32 link stubs need arduino-esp32 3.3.x (ESP-IDF 5.5.4); this core is too old -- re-derive the stub set first"
#else
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 0)
#error "The ESP32 link stubs were derived against arduino-esp32 3.3.x / ESP-IDF 5.5.4 -- re-derive the stub set before using it on an older core"
#endif
#endif

#include <stdint.h>
#include <stddef.h>

//A stub that is ever entered means a premise of its own cut is wrong. Counting costs 4 bytes of
//.bss and a handful of instructions, and turns "impossible" into something a board can actually
//be asked about -- LeifGetLinkStubHits() is non-zero if and only if one of these ran.
//⛔ Deliberately NOT a csprintf: some of these sit on the disconnect path and one on the RSNXE
//parse, and a print from the WiFi driver task is a worse bug than the one it would report.
//⛔ A stub a healthy board enters on a NORMAL path must NOT call this. One that does turns the
//counter from an alarm into a tally, and an alarm that is always on is not an alarm. ppp_init()
//below is the worked example: it genuinely runs, at every boot, from lwip_init().
static volatile uint32_t g_LinkStubHits = 0;
#define STUB_HIT() do { g_LinkStubHits++; } while (0)

#endif	//LEIF_LINKSTUBS_ANY

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

//The core-version canary that used to sit here now covers every cut in this file -- see the top
//of the file, just inside LEIF_LINKSTUBS_ANY.

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

//g_LinkStubHits / STUB_HIT() are defined once at the top of the file, shared by every cut.

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


//=============================================================================================
//NO_CAMELLIA_ARIA -- 6,543 B. camellia.c.obj (4,114) + aria.c.obj (2,429) in libmbedcrypto.a.
//=============================================================================================
//
//These are not dragged in by anything CALLING them. They are dragged in by a lookup TABLE:
//cipher_wrap.c holds an mbedtls_cipher_base_t per algorithm whose members are function
//pointers, so the address of every camellia_*/aria_* entry is taken at file scope and the
//members link whether or not any path selects those ciphers. The reference is data, not code.
//
//⭐ WHY NOTHING CAN SELECT THEM -- traced 2026-09-13, not assumed:
//  * The camellia/aria symbols are reached only from the static *_wrap() shims inside
//    cipher_wrap.c (camellia_crypt_ecb_wrap, aria_setkey_enc_wrap, ccm_camellia_setkey_wrap,
//    ...). A shim runs only if something picks its table row.
//  * A row is picked by mbedtls_cipher_info_from_type/_from_values. In THIS image the only
//    callers are ccm.c, cmac.c, gcm.c and wpa_supplicant's crypto_mbedtls.c.
//  * Of those, the only one on a live path is omac1_aes_vector(). Disassembled: it derives the
//    cipher type ARITHMETICALLY from the key length -- 16 -> 2, 24 -> 3, 32 -> 4, i.e.
//    MBEDTLS_CIPHER_AES_128/192/256_ECB. There is no input that makes it produce any other
//    value, so no CAMELLIA_* or ARIA_* row is reachable from it.
//  * cmac.c's own selectors are dead: mbedtls_cipher_cmac and mbedtls_aes_cmac_prf_128 are
//    referenced by NOTHING in the link, and the rest are self-tests.
//  (misc\claude\lightbulb-size-audit\bench\whoref.py and callsites.py reproduce all of this.)
//
//⭐ AND -- unlike the WPA3 cut -- this one does not NEED that argument to be airtight, because
//every stub here FAILS rather than lies. setkey returns an error, so a caller that somehow did
//select Camellia gets a clean "this cipher is unavailable" at setup and never reaches a crypt
//call. ⛔ That property is the whole safety case: a stub that returned 0 from a crypt function
//while leaving `output` untouched would hand back the plaintext as ciphertext. None do.
//
//⇒ Safe for any project. A build that uses mbedtls TLS and negotiates a Camellia ciphersuite
//would merely find it unavailable, which is also the truth.

#if defined(ARDUINO_ARCH_ESP32) && defined(NO_CAMELLIA_ARIA)

//Only ever passed as pointers, so incomplete types are exact and keep mbedtls's headers out.
struct mbedtls_camellia_context;
struct mbedtls_aria_context;

//mbedtls/camellia.h:25 and mbedtls/aria.h:36, esp32-libs 3.3.10.
#define LEIF_ERR_CAMELLIA_BAD_INPUT_DATA	(-0x0024)
#define LEIF_ERR_ARIA_BAD_INPUT_DATA		(-0x005C)

extern "C"
{

//--- Camellia ---------------------------------------------------------------------------------
//Prototypes recovered from the shipped .debug_info of libmbedcrypto.a (dwarfproto.py), not from
//the headers -- crypt_ecb takes a `mode` here and aria's does not, which a hand-written pair
//would have got wrong.

void mbedtls_camellia_init(struct mbedtls_camellia_context *ctx)
{
	(void) ctx;
	STUB_HIT();
	//The real body only zeroes the context. Skipping that is safe because setkey below always
	//fails, so no path ever reads a field of it.
}

void mbedtls_camellia_free(struct mbedtls_camellia_context *ctx)
{
	(void) ctx;
	STUB_HIT();
	//The context owns no allocation -- it is a plain struct -- so there is nothing to release.
}

int mbedtls_camellia_setkey_enc(struct mbedtls_camellia_context *ctx, const unsigned char *key,
		unsigned int keybits)
{
	(void) ctx; (void) key; (void) keybits;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;	//refuse the key: Camellia is not in this image
}

int mbedtls_camellia_setkey_dec(struct mbedtls_camellia_context *ctx, const unsigned char *key,
		unsigned int keybits)
{
	(void) ctx; (void) key; (void) keybits;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;
}

//⛔ Every crypt function below returns an ERROR and leaves `output` untouched. Returning 0
//would present unwritten memory as ciphertext -- the one failure mode that would be worse than
//the 4,114 bytes this saves.
int mbedtls_camellia_crypt_ecb(struct mbedtls_camellia_context *ctx, int mode,
		const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) mode; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;
}

int mbedtls_camellia_crypt_cbc(struct mbedtls_camellia_context *ctx, int mode, size_t length,
		unsigned char *iv, const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) mode; (void) length; (void) iv; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;
}

int mbedtls_camellia_crypt_cfb128(struct mbedtls_camellia_context *ctx, int mode, size_t length,
		size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) mode; (void) length; (void) iv_off; (void) iv; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;
}

int mbedtls_camellia_crypt_ctr(struct mbedtls_camellia_context *ctx, size_t length,
		size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
		const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) length; (void) nc_off; (void) nonce_counter; (void) stream_block;
	(void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_CAMELLIA_BAD_INPUT_DATA;
}

//--- ARIA -------------------------------------------------------------------------------------
//⛔ aria_crypt_ecb takes THREE parameters, not four -- it has no `mode`. Camellia's does.

void mbedtls_aria_init(struct mbedtls_aria_context *ctx)
{
	(void) ctx;
	STUB_HIT();
}

void mbedtls_aria_free(struct mbedtls_aria_context *ctx)
{
	(void) ctx;
	STUB_HIT();
}

int mbedtls_aria_setkey_enc(struct mbedtls_aria_context *ctx, const unsigned char *key,
		unsigned int keybits)
{
	(void) ctx; (void) key; (void) keybits;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

int mbedtls_aria_setkey_dec(struct mbedtls_aria_context *ctx, const unsigned char *key,
		unsigned int keybits)
{
	(void) ctx; (void) key; (void) keybits;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

int mbedtls_aria_crypt_ecb(struct mbedtls_aria_context *ctx, const unsigned char *input,
		unsigned char *output)
{
	(void) ctx; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

int mbedtls_aria_crypt_cbc(struct mbedtls_aria_context *ctx, int mode, size_t length,
		unsigned char *iv, const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) mode; (void) length; (void) iv; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

int mbedtls_aria_crypt_cfb128(struct mbedtls_aria_context *ctx, int mode, size_t length,
		size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output)
{
	(void) ctx; (void) mode; (void) length; (void) iv_off; (void) iv; (void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

int mbedtls_aria_crypt_ctr(struct mbedtls_aria_context *ctx, size_t length, size_t *nc_off,
		unsigned char *nonce_counter, unsigned char *stream_block, const unsigned char *input,
		unsigned char *output)
{
	(void) ctx; (void) length; (void) nc_off; (void) nonce_counter; (void) stream_block;
	(void) input; (void) output;
	STUB_HIT();
	return LEIF_ERR_ARIA_BAD_INPUT_DATA;
}

}	//extern "C"

#endif	//ARDUINO_ARCH_ESP32 && NO_CAMELLIA_ARIA


//=============================================================================================
//NO_PPP -- 19,351 B. lcp, ipcp, fsm, vj, pppos, ppp, upap, auth, magic in liblwip.a.
//=============================================================================================
//
//The best ratio on the whole table: nine members for seven stubs, because PPP is a closed
//subsystem that talks almost entirely to itself. Six of the seven are reached only from
//esp_netif_lwip_ppp.c -- esp_netif_new_ppp(), esp_netif_start_ppp(), esp_netif_stop_ppp(),
//esp_netif_destroy_ppp(), esp_netif_ppp_set_auth_internal() -- and every one of those runs only
//for an esp_netif created with a PPP base. This board creates a WiFi STA netif and nothing else,
//so no PPP pcb is ever constructed. (callsites.py names each enclosing function.)
//
//⛔ ppp_init() is the exception and it matters: it is called from lwip_init(), so it runs at
//EVERY boot on every board. Disassembled -- its whole body is memp_init_pool() over PPP's own
//private memory pools. Skipping that leaves those pools' free lists unbuilt, which is harmless
//precisely because nothing ever allocates from them; the pools' storage lives in memp.c and is
//not what this cut removes. ⇒ It is the one stub here that must NOT count a hit, or the alarm
//would read non-zero on a perfectly healthy board from the first second and mean nothing.

#if defined(ARDUINO_ARCH_ESP32) && defined(NO_PPP)

//lwIP spellings. err_t is s8_t: lwip/err.h:97, with no LWIP_ERR_T override in the esp32 cc.h.
typedef int8_t err_t_lwip;
struct ppp_pcb_s;
struct netif;
struct pbuf;

extern "C"
{

int ppp_init(void)
{
	//⛔ NOT counted -- see the note above. This one legitimately runs at every boot.
	return 0;						//lwIP ignores the value; 0 is its own success return
}

err_t_lwip ppp_connect(struct ppp_pcb_s *pcb, uint16_t holdoff)
{
	(void) pcb; (void) holdoff;
	STUB_HIT();
	return -1;						//ERR_MEM -- there is no PPP stack to connect with
}

err_t_lwip ppp_close(struct ppp_pcb_s *pcb, uint8_t nocarrier)
{
	(void) pcb; (void) nocarrier;
	STUB_HIT();
	return -1;
}

err_t_lwip ppp_free(struct ppp_pcb_s *pcb)
{
	(void) pcb;
	STUB_HIT();
	return -1;
}

void ppp_set_auth(struct ppp_pcb_s *pcb, uint8_t authtype, const char *user, const char *passwd)
{
	(void) pcb; (void) authtype; (void) user; (void) passwd;
	STUB_HIT();
}

void ppp_set_notify_phase_callback(struct ppp_pcb_s *pcb, void *notify_phase_cb)
{
	(void) pcb; (void) notify_phase_cb;
	STUB_HIT();
}

err_t_lwip ppp_ioctl(struct ppp_pcb_s *pcb, uint8_t cmd, void *arg)
{
	(void) pcb; (void) cmd; (void) arg;
	STUB_HIT();
	return -1;
}

//⭐ Returns NULL, and that is the value the cut depends on: esp_netif_new_ppp() checks it and
//fails the netif creation, so nothing downstream ever holds a half-built pcb.
struct ppp_pcb_s * pppos_create(struct netif *pppif, void *output_cb, void *link_status_cb,
		void *ctx_cb)
{
	(void) pppif; (void) output_cb; (void) link_status_cb; (void) ctx_cb;
	STUB_HIT();
	return 0;
}

//⛔ The two that the FIRST stub set missed, and the link error that found them is the lesson:
//`pppos_input_sys` is referenced from libesp_netif.a's OWN `ppp.c.obj` -- a different member
//that happens to share a basename with liblwip.a's, and which the ld map spells
//`esp_netif_lwip_ppp.c.obj`. Keying the analysis off the map's names made that member's
//references invisible. `bench\stubany.py` now over-approximates the keep set instead.
//Both call sites are PPP-only: pppapi_do_ppp_ioctl() and pppos_input_tcpip_as_ram_pbuf().
err_t_lwip pppos_input_sys(struct pbuf *p, struct netif *inp)
{
	(void) p; (void) inp;
	STUB_HIT();
	return -1;						//ERR_MEM: there is no PPP netif to hand this pbuf to
}

}	//extern "C"

#endif	//ARDUINO_ARCH_ESP32 && NO_PPP


//=============================================================================================
//ESP8266 -- NO_SOFT_AP (3,088 B) and NO_HOST_AP (7,397 B flash + 73 B RAM).
//=============================================================================================
//
//The ESP8266 original, moved here from arduino\Lightbulb\LinkStubs.cpp on 2026-09-13 so that
//both platforms live in one file. //Leif, 2026-09-13: "really it should just be one file:
//linkstubs.cpp and then we can have ifdefs to apply it to the two different platforms."
//Reasoning and measurements: misc\docs\plans\lightbulb-esp8266-strip-plan.md.
//
//⛔ These do NOT count a stub hit, and that is deliberate. LEIF_LINKSTUBS_ANY is set only on
//ESP32, so on ESP8266 LeifGetLinkStubHits() stays the inline zero in LinkStubs.h and this move
//changes nothing about what a bulb executes. Giving the ESP8266 stubs the same alarm is worth
//doing, but it is a behaviour change to a live fleet image and is not what this move was.
//
//⛔ #include <ESP8266WiFi.h> below is load-bearing and must stay ahead of the canary: the
//LEIF_NO_SOFT_AP_PATCH the canary tests for is defined by the vendored core itself, in
//ESP8266WiFiAP.h:33, and is only visible once that header has been pulled in.

#if defined(ARDUINO_ARCH_ESP8266) && defined(NO_SOFT_AP)

#include <ESP8266WiFi.h>

//A bulb is station-only, so lwIP never brings up an AP netif and never calls these.
//Without them, lwip-git.o pulls in LwipDhcpServer -- a DHCP SERVER, 3,088 B.
//The matching half is the NO_SOFT_AP gate vendored into the core's ESP8266WiFiAP.cpp;
//this canary makes an SDK update that loses it a compile error rather than a bigger image.
#ifndef LEIF_NO_SOFT_AP_PATCH
#error "ESP8266WiFiAP.cpp lost its NO_SOFT_AP patch -- see misc/docs/plans/lightbulb-esp8266-strip-plan.md"
#endif

extern "C" void dhcps_start_LWIP2(void * info, void * apnetif)
{
	(void)info;
	(void)apnetif;
}

extern "C" void dhcps_stop(void)
{
}

#endif	//ARDUINO_ARCH_ESP8266 && NO_SOFT_AP

#if defined(ARDUINO_ARCH_ESP8266) && defined(NO_HOST_AP)

#ifndef NO_SOFT_AP
#error "NO_HOST_AP cuts the AP-side receive and beacon code, so the soft AP cannot work -- set NO_SOFT_AP too"
#endif

//Eight definitions that between them satisfy every reference into the SDK's
//ieee80211_hostap.o from outside it, so the linker never opens that member:
//7,397 B of flash and 73 B of RAM a station-only bulb cannot use.
//The five variables are storage-identical to the SDK's; what disappears is the AP-side
//code that wrote them, and none of that runs in station mode.

extern "C"
{
	//.bss in the SDK, so zero is the SDK's own starting value.
	unsigned char BcnWithMcastSendCnt = 0;
	unsigned char BcnEb_update = 0;
	unsigned char ap_freq_force_to_scan = 0;
	unsigned char PendFreeBcnEb = 0;

	//⛔ 1 is the SDK's initial value and it is load-bearing. pp.o's receive dispatch
	//reads this byte once and calls hostap_input only when it is NOT 1, so 1 is what
	//makes the call below unreachable. Only the AP-side code we are cutting ever
	//cleared it, which is why today's station-only image never reaches hostap_input
	//either. Disassembly is in misc/docs/plans/lightbulb-esp8266-strip-plan.md.
	unsigned char TmpSTAAPCloseAP = 1;

	void ppRecycleRxPkt(void *pkt);

	//Unreachable while TmpSTAAPCloseAP is 1. It still recycles the buffer, which is
	//what pp.o itself does with a frame it declines to hand to the AP side -- so if a
	//future SDK ever did reach here, it leaks nothing.
	void hostap_input(void *conn, void *pkt, signed char rssi, int flag)
	{
		(void)conn;
		(void)rssi;
		(void)flag;
		ppRecycleRxPkt(pkt);
	}

	//Called only for opmode 2 (SOFTAP) and 3 (STATIONAP); a bulb is opmode 1. Both
	//call sites discard the return value.
	int wifi_softap_start(void)
	{
		return 0;
	}

	int wifi_softap_stop(void)
	{
		return 0;
	}
}

#endif	//ARDUINO_ARCH_ESP8266 && NO_HOST_AP


//⛔ Defined ONLY when a cut was taken. The no-cut case is an inline zero in the header, so a
//project that took no cut never has to carry this file -- see LinkStubs.h for the 102 of
//105 projects that stopped linking when it did (counted by makefile inspection
//2026-09-13; ESP8266 included, because the old declaration had no arch guard).
#ifdef LEIF_LINKSTUBS_ANY
uint32_t LeifGetLinkStubHits()
{
	return g_LinkStubHits;
}
#endif

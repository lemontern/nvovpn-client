#include "wireguard-go-version.h"
#include "3rd/amneziawg-apple/Sources/WireGuardKitGo/wireguard.h"
#include "3rd/amneziawg-apple/Sources/WireGuardKitC/WireGuardKitC.h"

#include <stdbool.h>
#include <stdint.h>

#define WG_KEY_LEN (32)
#define WG_KEY_LEN_BASE64 (45)
#define WG_KEY_LEN_HEX (65)

void key_to_base64(char base64[WG_KEY_LEN_BASE64],
                   const uint8_t key[WG_KEY_LEN]);
bool key_from_base64(uint8_t key[WG_KEY_LEN], const char* base64);

void key_to_hex(char hex[WG_KEY_LEN_HEX], const uint8_t key[WG_KEY_LEN]);
bool key_from_hex(uint8_t key[WG_KEY_LEN], const char* hex);

bool key_eq(const uint8_t key1[WG_KEY_LEN], const uint8_t key2[WG_KEY_LEN]);

void write_msg_to_log(const char* tag, const char* msg);

// hev-socks5-tunnel (C): мост SOCKS→packetFlow для xray (HevSocksTunnel.swift).
void hev_socks5_tunnel_quit(void);
int hev_socks5_tunnel_main(const char* configFile, int fd);

// NvoVPN 14.09.2026: C-API движка xray (amnezia_xray_configure/_start/_stop/_setsockcallback/_free +
// typedef amnezia_xray_sockcallback). Заголовок из conan-пакета amnezia-xray-bindings (include-путь
// пробрасывается линковкой amnezia::xray-bindings) — как в macOS NE.
#include "amnezia_xray.h"

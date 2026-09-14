from conan import ConanFile

class AmneziaVPN(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualBuildEnv", "CMakeConfigDeps"

    options = {
        "macos_ne": [True, False]
    }
    default_options = {
        "macos_ne": False,
        # libssh на Android использует mbedTLS-backend (см. requirements); он требует threading.
        # На других платформах mbedtls в граф не входит — опция там безвредно игнорируется.
        "mbedtls/*:enable_threading": True,
    }

    def requirements(self):
        os = str(self.settings.os)

        has_ne = os == "iOS" or (os == "Macos" and self.options.macos_ne)
        has_service = os == "Windows" or os == "Linux" or (os == "Macos" and not has_ne)

        if has_service:
            if os == "Windows":
                self.requires("awg-windows/0.1.8")
                self.requires("tap-windows6/9.27.0")
                self.requires("win-split-tunnel/1.2.5.0")
                self.requires("wintun/0.14.1")
            else:
                self.requires("awg-go/0.2.16")

            self.requires("amnezia-xray-bindings/1.1.0")
            self.requires("tun2socks/2.6.0")
            self.requires("openvpn/2.7.0")
            self.requires("v2ray-rules-dat/202603162227")

        if has_ne:
            self.requires("awg-apple/2.0.1")
            # NvoVPN: VLESS/xray в NE на macOS И iOS (14.09.2026: iOS без VLESS бесполезен для РФ — ТСПУ режет
            # awg до Европы, обход держится на VLESS/транзитах). Движок — amnezia-xray-bindings: тонкая C-обвязка
            # над upstream github.com/xtls/xray-core (НЕ amnezia-xray-core, в бинаре нет amnezia-путей; App Store 4.3
            # в 07.2026 ловил именно чужие движки + Amnezia-символы, они остались вырезанными). hev (C) — мост
            # SOCKS→packetFlow. Go-конфликт xray↔wg-go решён локализацией cgo-символов в CI (nvovpn-ci.yml,
            # шаги «Fix Go cgo symbol clash in libamnezia_xray.a» для macOS и iOS).
            self.requires("amnezia-xray-bindings/1.1.0")
            # as_framework=True — hev упаковывается как HevSocks5Tunnel.xcframework через
            # package_framework/location (рабочий путь линковки; ветка as_framework=False
            # в recipe битая — cpp_info.libraries вместо .libs). Так же требовал оригинал.
            self.requires("hev-socks5-tunnel/2.15.0", options={"as_framework": True})
            # openvpnadapter остаётся только на macOS NE (на iOS OpenVPN — заглушка, App Store 4.3).
            if os == "Macos":
                self.requires("openvpnadapter/1.0.0")

        if os == "Android":
            self.requires("amnezia-libxray/1.0.0")
            self.requires("awg-android/1.1.7")
            self.requires("openvpn-pt-android/1.0.0")

        # expicitly use libssh@amnezia to prevent it from being downloaded from conan-center
        # На Android OpenSSL-backend libssh не находит библиотеку 'ssl' при сборке (на других
        # платформах OpenSSL резолвится штатно) — переключаем libssh на mbedTLS (чистый C, портируемый).
        # libssh нужен только self-hosted-флоу (SSH внутрь сервера); боевой NvoVPN-путь его не вызывает.
        if os == "Android":
            # mbedtls затянется транзитивно (libssh crypto_backend=mbedtls); enable_threading задан в default_options.
            self.requires("libssh/0.11.3@amnezia", options={"crypto_backend": "mbedtls"})
        else:
            self.requires("libssh/0.11.3@amnezia")
        self.requires("openssl/3.6.2")
        self.requires("zlib/1.3.2")

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
            # NvoVPN: iOS NE — ТОЛЬКО AmneziaWG (App Store 4.3: чужие движки Xray/OpenVPN
            # вырезаны — их бинарные блобы идентичны AmneziaVPN и другим форкам).
            # hev-socks5-tunnel (Go-Xray) убран совсем (macOS его тоже не линкует).
            # openvpnadapter остаётся только на macOS NE (там OpenVPN-код ещё компилируется).
            if os == "Macos":
                self.requires("openvpnadapter/1.0.0")
                # SPIKE (macOS VLESS): macOS раздаётся с сайта (.dmg), не App Store → 4.3 не применяется.
                # ГЛАВНЫЙ вопрос спайка — линкуются ли ДВА Go-рантайма в одном бинаре extension:
                # libwg-go (внутри awg-apple/WireGuardKit) + libamnezia_xray (движок xray, тоже Go).
                # Историю (коммит 16e8151) диагностировали как «Go-конфликт»; проверяем это напрямую,
                # force_load'ом вытягивая все объекты xray, чтобы дублирующиеся cgo-символы всплыли на линковке.
                # hev (C, мост SOCKS→packetFlow) НЕ добавляем на этом шаге — он не создаёт Go-конфликта,
                # это лишняя переменная; вернём, когда вопрос двух Go-рантаймов будет закрыт.
                self.requires("amnezia-xray-bindings/1.1.0")

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

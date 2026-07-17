enable_language(Swift)
message("Client message >> macos build >> AmneziaVPNNetworkExtension")
set(CLIENT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)

add_executable(AmneziaVPNNetworkExtension)

message("executable_path is: @executable_path/../../Frameworks")
set_target_properties(AmneziaVPNNetworkExtension PROPERTIES
    # NvoVPN: System Extension (вместо app-extension) — для раздачи .dmg вне App Store (Developer ID).
    # App Extension VPN авторизуется только для App Store; Developer ID требует system-extension.
    XCODE_PRODUCT_TYPE com.apple.product-type.system-extension
    BUNDLE_EXTENSION systemextension
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${APPLE_PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_STRING "AmneziaVPNNetworkExtension"
    MACOSX_BUNDLE_BUNDLE_NAME "AmneziaVPNNetworkExtension"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUILD_IOS_APP_IDENTIFIER}.network-extension"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_NAME "${BUILD_IOS_APP_IDENTIFIER}.network-extension"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ${CMAKE_CURRENT_SOURCE_DIR}/AmneziaVPNNetworkExtension.entitlements
    XCODE_ATTRIBUTE_MARKETING_VERSION "${APP_MAJOR_VERSION}"
    XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CMAKE_PROJECT_VERSION_TWEAK}"
    XCODE_ATTRIBUTE_PRODUCT_NAME "AmneziaVPNNetworkExtension"

    # APPLICATION_EXTENSION_API_ONLY убран — system extension это не app extension.
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "11.0"

    XCODE_ATTRIBUTE_INFOPLIST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/../../../../Frameworks @loader_path/../../../../Frameworks"
)

if(DEPLOY)
    message("DEPLOY is ON")
    set_target_properties(AmneziaVPNNetworkExtension PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY[variant=Debug] "Apple Development"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Manual
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "distr macos.com.nvovpn.app.NE"
        XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER[variant=Debug] "dev macos.com.nvovpn.app.NE"
    )
else()
    set_target_properties(AmneziaVPNNetworkExtension PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
    )
endif()

set_target_properties(AmneziaVPNNetworkExtension PROPERTIES
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES "YES"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/WireGuardNetworkExtension-Bridging-Header.h"
    XCODE_ATTRIBUTE_SWIFT_OPTIMIZATION_LEVEL "-Onone"
    XCODE_ATTRIBUTE_SWIFT_PRECOMPILE_BRIDGING_HEADER "NO"
)

set_target_properties("AmneziaVPNNetworkExtension" PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "Q9PKZ8C922"
)

find_library(FW_ASSETS_LIBRARY AssetsLibrary)
find_library(FW_MOBILE_CORE MobileCoreServices)
find_library(FW_UI_KIT UIKit)
find_library(FW_LIBRESOLV libresolv.9.tbd)


# Set the root directory
set(CLIENT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)

target_link_libraries(AmneziaVPNNetworkExtension PRIVATE ${FW_LIBRESOLV})

# SPIKE (macOS VLESS): диагностический тест ГЛАВНОГО блокера — уживаются ли два Go-рантайма
# (libwg-go внутри awg-apple + libamnezia_xray, движок xray) в одном бинаре extension.
# Swift-обвязка пока заглушка (XrayStub) и НЕ ссылается на символы xray, поэтому без -force_load
# линкер выкинул бы объекты xray (dead-strip) и конфликт не всплыл бы — тест был бы ложно-зелёным.
# -force_load вытягивает ВСЕ объекты libamnezia_xray → дублирующиеся cgo-символы с libwg-go
# всплывают на линковке, если конфликт реален. Фикс (локализация символов) — следующим шагом,
# по точному списку из лога CI. hev здесь намеренно НЕ линкуется (C, не влияет на Go-вопрос).
find_package(amnezia-xray-bindings REQUIRED)
target_link_libraries(AmneziaVPNNetworkExtension PRIVATE amnezia::xray-bindings)
target_link_options(AmneziaVPNNetworkExtension PRIVATE "-Wl,-force_load,$<TARGET_FILE:amnezia::xray-bindings>")

target_compile_options(AmneziaVPNNetworkExtension PRIVATE -DGROUP_ID=\"${BUILD_IOS_GROUP_IDENTIFIER}\")
target_compile_options(AmneziaVPNNetworkExtension PRIVATE -DNETWORK_EXTENSION=1)

set(WG_APPLE_SOURCE_DIR ${CLIENT_ROOT_DIR}/3rd/amneziawg-apple/Sources)

message("WG_APPLE_SOURCE_DIR is: ${WG_APPLE_SOURCE_DIR}")
message("CLIENT_ROOT_DIR is: ${CLIENT_ROOT_DIR}")

target_sources(AmneziaVPNNetworkExtension PRIVATE
    # NvoVPN: точка входа main() для System Extension (standalone executable; app-extension
    # использовал системный _NSExtensionMain, у system extension его нет).
    ${CMAKE_CURRENT_SOURCE_DIR}/main.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/WireGuardAdapter.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/PacketTunnelSettingsGenerator.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/DNSResolver.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardNetworkExtension/ErrorNotifier.swift
    ${WG_APPLE_SOURCE_DIR}/Shared/Keychain.swift
    ${WG_APPLE_SOURCE_DIR}/Shared/Model/TunnelConfiguration+WgQuickConfig.swift
    ${WG_APPLE_SOURCE_DIR}/Shared/Model/NETunnelProviderProtocol+Extension.swift
    ${WG_APPLE_SOURCE_DIR}/Shared/Model/String+ArrayConversion.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/TunnelConfiguration.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/IPAddressRange.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/Endpoint.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/DNSServer.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/InterfaceConfiguration.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/PeerConfiguration.swift
    ${WG_APPLE_SOURCE_DIR}/Shared/FileManager+Extension.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKitC/x25519.c
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/Array+ConcurrentMap.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/IPAddress+AddrInfo.swift
    ${WG_APPLE_SOURCE_DIR}/WireGuardKit/PrivateKey.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/NELogController.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/Log.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/LogRecord.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/PacketTunnelProvider.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/PacketTunnelProvider+WireGuard.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/PacketTunnelProvider+OpenVPN.swift
    # NvoVPN: AmneziaWG-only. Xray + hev-socks5-tunnel убраны (Go-конфликт wg-go/hev при линковке).
    # Вместо PacketTunnelProvider+Xray.swift / XrayConfig.swift / HevSocksTunnel.swift — заглушка:
    ${CLIENT_ROOT_DIR}/platforms/ios/PacketTunnelProvider+XrayStub.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/WGConfig.swift
    ${CLIENT_ROOT_DIR}/platforms/ios/iosglue.mm
)

target_sources(AmneziaVPNNetworkExtension PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PrivacyInfo.xcprivacy
)

set_property(TARGET AmneziaVPNNetworkExtension APPEND PROPERTY RESOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/PrivacyInfo.xcprivacy
)

target_include_directories(AmneziaVPNNetworkExtension PRIVATE ${CLIENT_ROOT_DIR})
target_include_directories(AmneziaVPNNetworkExtension PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

find_package(openvpnadapter REQUIRED)
target_link_libraries(AmneziaVPNNetworkExtension PRIVATE amnezia::openvpnadapter)

find_package(awg-apple REQUIRED)
target_link_libraries(AmneziaVPNNetworkExtension PRIVATE amnezia::awg-apple)

# NvoVPN: hev-socks5-tunnel (Go) убран — конфликтует с libwg-go (Go) на линковке macOS (duplicate cgo symbols).
# Нужен только для Xray, который мы не используем (AmneziaWG-only).

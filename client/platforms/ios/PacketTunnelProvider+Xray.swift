import Darwin
import Foundation
import NetworkExtension
import os

// NvoVPN: индекс активного физического интерфейса живёт В ПРОЦЕССЕ, а не в объекте провайдера.
//
// Указатель на sock-колбэк отдаётся Go-рантайму xray, и тот зовёт колбэк из СВОИХ потоков на
// каждый исходящий сокет — в том числе после остановки туннеля и после освобождения провайдера
// (в одном системном расширении macOS последовательно живут несколько сессий: awg → VLESS →
// смена страны). Раньше в колбэк отдавался Unmanaged.passUnretained(self), поэтому обращение к
// уже освобождённому провайдеру роняло расширение целиком:
//   EXC_BAD_ACCESS (SIGQUIT), KERN_INVALID_ADDRESS at 0x20
//   closure #1 in PacketTunnelProvider.setupAndStartXray ← amnezia_xray_invokesockcallback
// (крэш AmneziaVPNNetworkExtension 01.09.2026, туннель умирал через несколько минут работы).
//
// Колбэк больше НЕ разыменовывает объекты — только читает это значение, поэтому пережить
// провайдера он не может. Заодно снимается гонка: индекс пишется на pathMonitorQueue,
// а читается из потока xray.
private let nvoIfaceIdxLock: UnsafeMutablePointer<os_unfair_lock_s> = {
    let lock = UnsafeMutablePointer<os_unfair_lock_s>.allocate(capacity: 1)
    lock.initialize(to: os_unfair_lock_s())
    return lock
}()

private let nvoIfaceIdxBox: UnsafeMutablePointer<UInt32> = {
    let box = UnsafeMutablePointer<UInt32>.allocate(capacity: 1)
    box.initialize(to: 0)
    return box
}()

func nvoSetActiveIfaceIdx(_ value: UInt32) {
    os_unfair_lock_lock(nvoIfaceIdxLock)
    nvoIfaceIdxBox.pointee = value
    os_unfair_lock_unlock(nvoIfaceIdxLock)
}

func nvoActiveIfaceIdx() -> UInt32 {
    os_unfair_lock_lock(nvoIfaceIdxLock)
    let value = nvoIfaceIdxBox.pointee
    os_unfair_lock_unlock(nvoIfaceIdxLock)
    return value
}

// Привязка исходящего сокета xray к активному физическому интерфейсу (защита от петли
// маршрутизации). Свободная функция без захвата контекста — вызывается из потоков Go.
func nvoBindSocketToActiveInterface(_ fd: uintptr_t) {
    var idx = nvoActiveIfaceIdx()
    guard idx != 0 else { return }

    withUnsafePointer(to: &idx) { ptr in
        setsockopt(Int32(fd), IPPROTO_IP, IP_BOUND_IF, ptr, socklen_t(MemoryLayout<UInt32>.size))
        setsockopt(Int32(fd), IPPROTO_IPV6, IPV6_BOUND_IF, ptr, socklen_t(MemoryLayout<UInt32>.size))
    }
}

enum XrayErrors: Error {
    case noXrayConfig
    case xrayConfigIsWrong
    case cantSaveXrayConfig
    case cantParseListenAndPort
    case cantAcquireLocalPort
    case cantSaveHevSocksConfig
}

extension Constants {
    static let cachesDirectory: URL = {
        if let cachesDirectoryURL = FileManager.default.urls(for: .cachesDirectory,
                                                             in: .userDomainMask).first {
            return cachesDirectoryURL
        } else {
            fatalError("Unable to retrieve caches directory.")
        }
    }()
}

extension PacketTunnelProvider {
    /// TCP port chosen by the OS on IPv6 loopback (::1), matching inbound listen address.
    private func acquireFreeLocalPort() throws -> Int {
        let fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)
        guard fd != -1 else {
            throw XrayErrors.cantAcquireLocalPort
        }
        defer { close(fd) }
        var reuse: Int32 = 1
        _ = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, socklen_t(MemoryLayout<Int32>.size))
        var addr = sockaddr_in6()
        addr.sin6_len = UInt8(MemoryLayout<sockaddr_in6>.size)
        addr.sin6_family = sa_family_t(AF_INET6)
        addr.sin6_port = in_port_t(0).bigEndian
        addr.sin6_addr = in6addr_loopback
        addr.sin6_scope_id = 0
        let bindResult = withUnsafePointer(to: &addr) { ptr in
            ptr.withMemoryRebound(to: sockaddr.self, capacity: 1) { p in
                bind(fd, p, socklen_t(MemoryLayout<sockaddr_in6>.size))
            }
        }
        guard bindResult == 0 else {
            throw XrayErrors.cantAcquireLocalPort
        }
        var bound = sockaddr_in6()
        var len = socklen_t(MemoryLayout<sockaddr_in6>.size)
        let gr = withUnsafeMutablePointer(to: &bound) { p in
            p.withMemoryRebound(to: sockaddr.self, capacity: 1) { bp in
                getsockname(fd, bp, &len)
            }
        }
        guard gr == 0 else {
            throw XrayErrors.cantAcquireLocalPort
        }
        return Int(bound.sin6_port.byteSwapped)
    }

    private func applyXraySplitTunnel(_ xrayConfig: XrayConfig,
                                      settings: NEPacketTunnelNetworkSettings) {
        guard let splitTunnelType = xrayConfig.splitTunnelType else {
            return
        }

        guard let splitTunnelSites = xrayConfig.splitTunnelSites else {
            xrayLog(.error, message: "Split tunnel sites are not set")
            return
        }

        if splitTunnelType == 1 {
            var ipv4IncludedRoutes = [NEIPv4Route]()

            for allowedIPString in splitTunnelSites {
                if let allowedIP = IPAddressRange(from: allowedIPString) {
                    ipv4IncludedRoutes.append(NEIPv4Route(
                        destinationAddress: "\(allowedIP.address)",
                        subnetMask: "\(allowedIP.subnetMask())"))
                }
            }

            settings.ipv4Settings?.includedRoutes = ipv4IncludedRoutes
        } else if splitTunnelType == 2 {
            var ipv4ExcludedRoutes = [NEIPv4Route]()

            for excludedIPString in splitTunnelSites {
                if let excludedIP = IPAddressRange(from: excludedIPString) {
                    ipv4ExcludedRoutes.append(NEIPv4Route(
                        destinationAddress: "\(excludedIP.address)",
                        subnetMask: "\(excludedIP.subnetMask())"))
                }
            }

            settings.ipv4Settings?.excludedRoutes = ipv4ExcludedRoutes
        }
    }

    func startXray(completionHandler: @escaping (Error?) -> Void) {

        // Xray configuration
        guard let protocolConfiguration = self.protocolConfiguration as? NETunnelProviderProtocol,
              let providerConfiguration = protocolConfiguration.providerConfiguration,
              let configData = providerConfiguration[Constants.xrayConfigKey] as? Data else {
            xrayLog(.error, message: "Can't get xray configuration")
            completionHandler(XrayErrors.noXrayConfig)
            return
        }

        // Tunnel settings
        let ipv6Enabled = false
        let hideVPNIcon = false

        let settings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: "254.1.1.1")
        settings.mtu = 9000

        settings.ipv4Settings = {
            let settings = NEIPv4Settings(addresses: ["198.18.0.1"], subnetMasks: ["255.255.0.0"])
            settings.includedRoutes = [NEIPv4Route.default()]
            return settings
        }()

        settings.ipv6Settings = {
            guard ipv6Enabled else {
                return nil
            }
            let settings = NEIPv6Settings(addresses: ["fd6e:a81b:704f:1211::1"], networkPrefixLengths: [64])
            settings.includedRoutes = [NEIPv6Route.default()]
            if hideVPNIcon {
                settings.excludedRoutes = [NEIPv6Route(destinationAddress: "::", networkPrefixLength: 128)]
            }
            return settings
        }()

        do {
            let xrayConfig = try JSONDecoder().decode(XrayConfig.self,
                                                      from: configData)

            var dnsArray = [String]()
            if let dns1 = xrayConfig.dns1 {
                dnsArray.append(dns1)
            }
            if let dns2 = xrayConfig.dns2 {
                dnsArray.append(dns2)
            }

            settings.dnsSettings = !dnsArray.isEmpty
            ? NEDNSSettings(servers: dnsArray)
            : NEDNSSettings(servers: ["1.1.1.1"])
            applyXraySplitTunnel(xrayConfig, settings: settings)

            let xrayConfigData = xrayConfig.config.data(using: .utf8)

            guard let xrayConfigData else {
                xrayLog(.error, message: "Can't encode config to data")
                completionHandler(XrayErrors.xrayConfigIsWrong)
                return
            }

            let jsonDict = try JSONSerialization.jsonObject(with: xrayConfigData,
                                                            options: []) as? [String: Any]

            guard var jsonDict else {
                xrayLog(.error, message: "Can't parse address and port for hevSocks")
                completionHandler(XrayErrors.cantParseListenAndPort)
                return
            }

            let port = try acquireFreeLocalPort()
            let address = "::1"

            // Extract existing SOCKS5 credentials or generate new ones per session.
            let socksCredentials = ensureInboundAuth(jsonDict: &jsonDict, port: port, address: address)

            let updatedData = try JSONSerialization.data(withJSONObject: jsonDict, options: [])

            setTunnelNetworkSettings(settings) { [weak self] error in
                if let error {
                    completionHandler(error)
                    return
                }

                self?.updateActiveInterfaceIndexForCurrentPath()

                // Launch xray
                self?.setupAndStartXray(configData: updatedData) { xrayError in
                    if let xrayError {
                        completionHandler(xrayError)
                        return
                    }

                    // Launch hevSocks
                    self?.setupAndRunTun2socks(configData: updatedData,
                                               address: address,
                                               port: port,
                                               username: socksCredentials.username,
                                               password: socksCredentials.password,
                                               completionHandler: completionHandler)
                }
            }
        } catch {
            completionHandler(error)
            return
        }
    }

    func stopXray(completionHandler: () -> Void) {
        Socks5Tunnel.quit()
        if let err = amnezia_xray_stop() {
            xrayLog(.error, message: "amnezia_xray_stop: \(String(cString: err))")
            amnezia_xray_free(UnsafeMutableRawPointer(err))
        }
        completionHandler()
    }

    func sockCallback(fd: uintptr_t) {
        nvoBindSocketToActiveInterface(fd)
    }

    private struct SocksCredentials {
        let username: String
        let password: String
    }

    private func indexOfSocksInbound(in inboundsArray: [[String: Any]]) -> Int? {
        for (i, inbound) in inboundsArray.enumerated() {
            guard let proto = inbound["protocol"] as? String else { continue }
            if proto.caseInsensitiveCompare("socks") == .orderedSame {
                return i
            }
        }
        return nil
    }

    // Returns existing SOCKS5 credentials from the inbound config, or generates and injects
    // new random ones. Also sets port and address on the socks inbound entry.
    private func ensureInboundAuth(jsonDict: inout [String: Any], port: Int, address: String) -> SocksCredentials {
        var inboundsArray = jsonDict["inbounds"] as? [[String: Any]] ?? []

        if let socksIdx = indexOfSocksInbound(in: inboundsArray) {
            var inbound = inboundsArray[socksIdx]
            inbound["port"] = port
            inbound["listen"] = address

            var settings = inbound["settings"] as? [String: Any] ?? [:]
            if let accounts = settings["accounts"] as? [[String: Any]],
               let first = accounts.first,
               let user = first["user"] as? String, !user.isEmpty,
               let pass = first["pass"] as? String, !pass.isEmpty {
                // Re-use existing credentials, but always enforce auth mode in case the
                // imported config had accounts but auth: "noauth" (or no auth field).
                settings["auth"] = "password"
                inbound["settings"] = settings
                inboundsArray[socksIdx] = inbound
                jsonDict["inbounds"] = inboundsArray
                return SocksCredentials(username: user, password: pass)
            }

            // Generate new random credentials for this session
            let user = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased().prefix(16)
            let pass = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
            settings["auth"] = "password"
            settings["accounts"] = [["user": String(user), "pass": pass]]
            inbound["settings"] = settings
            inboundsArray[socksIdx] = inbound
            jsonDict["inbounds"] = inboundsArray
            return SocksCredentials(username: String(user), password: pass)
        }

        // Fallback: no socks inbound — generate credentials but can't inject
        let user = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased().prefix(16)
        let pass = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
        return SocksCredentials(username: String(user), password: pass)
    }

    private func setupAndStartXray(configData: Data,
                                   completionHandler: @escaping (Error?) -> Void) {
        // Пишем конфиг на диск для диагностики; движок конфигурируется СОДЕРЖИМЫМ (не путём).
        let path = Constants.cachesDirectory.appendingPathComponent("config.json", isDirectory: false).path
        FileManager.default.createFile(atPath: path, contents: configData)

        guard let configString = String(data: configData, encoding: .utf8) else {
            xrayLog(.error, message: "Can't decode xray config to string")
            completionHandler(XrayErrors.xrayConfigIsWrong)
            return
        }

        updateActiveInterfaceIndexForCurrentPath()

        // Колбэк защиты сокетов xray от петли маршрутизации (bind исходящих к активному интерфейсу).
        // Указатель на провайдера сюда НЕ передаётся: xray зовёт колбэк из своих потоков и после
        // остановки туннеля, а провайдер к тому моменту уже освобождён — это роняло расширение
        // (см. nvoBindSocketToActiveInterface). Контекстом отдаём вечно живущий бокс индекса,
        // сам колбэк его не читает.
        let ctx = UnsafeMutableRawPointer(nvoIfaceIdxBox)
        let cb: amnezia_xray_sockcallback = { (fd, _) in
            nvoBindSocketToActiveInterface(fd)
        }
        if let err = amnezia_xray_setsockcallback(cb, ctx) {
            let msg = String(cString: err); amnezia_xray_free(UnsafeMutableRawPointer(err))
            xrayLog(.error, message: "amnezia_xray_setsockcallback: \(msg)")
            completionHandler(XrayErrors.xrayConfigIsWrong)
            return
        }

        // amnezia_xray_configure принимает содержимое конфига как C-строку (JSON).
        var cfg = Array(configString.utf8CString)
        if let err = amnezia_xray_configure(&cfg) {
            let msg = String(cString: err); amnezia_xray_free(UnsafeMutableRawPointer(err))
            xrayLog(.error, message: "amnezia_xray_configure: \(msg)")
            completionHandler(XrayErrors.xrayConfigIsWrong)
            return
        }

        if let err = amnezia_xray_start() {
            let msg = String(cString: err); amnezia_xray_free(UnsafeMutableRawPointer(err))
            xrayLog(.error, message: "amnezia_xray_start: \(msg)")
            completionHandler(XrayErrors.xrayConfigIsWrong)
            return
        }

        completionHandler(nil)
        xrayLog(.info, message: "Xray started")
    }

    private func setupAndRunTun2socks(configData: Data,
                                      address: String,
                                      port: Int,
                                      username: String,
                                      password: String,
                                      completionHandler: @escaping (Error?) -> Void) {
        let config = """
        tunnel:
          mtu: 9000
        socks5:
          port: \(port)
          address: \(address)
          username: \(username)
          password: \(password)
          udp: 'udp'
        misc:
          task-stack-size: 20480
          connect-timeout: 5000
          read-write-timeout: 60000
          log-file: stderr
          log-level: error
          limit-nofile: 65535
        """

        let configurationFilePath = Constants.cachesDirectory.appendingPathComponent("config.yml", isDirectory: false).path
        guard FileManager.default.createFile(atPath: configurationFilePath, contents: config.data(using: .utf8)!) else {
            xrayLog(.info, message: "Cant save hevSocks configuration")
            completionHandler(XrayErrors.cantSaveHevSocksConfig)
            return
        }

        DispatchQueue.global().async {
            xrayLog(.info, message: "Hev socks started")
            completionHandler(nil)
            Socks5Tunnel.run(withConfig: configurationFilePath)
        }
    }
}

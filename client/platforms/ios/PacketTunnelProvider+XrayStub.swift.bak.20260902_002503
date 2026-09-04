import Foundation
import NetworkExtension

// NvoVPN: продукт работает ТОЛЬКО на AmneziaWG. Реальные Xray + hev-socks5-tunnel убраны из
// сетевого расширения, т.к. libhev-socks5-tunnel (Go) конфликтует с libwg-go (Go) при линковке
// на macOS (ld-prime, Xcode 15+): "duplicate cgo symbols". Эти заглушки оставлены, чтобы switch
// по protoType компилировался; фактически не вызываются — бэкенд отдаёт только amneziawg.
extension PacketTunnelProvider {
    func startXray(completionHandler: @escaping (Error?) -> Void) {
        completionHandler(NSError(domain: "NvoVPN", code: -1,
            userInfo: [NSLocalizedDescriptionKey: "Xray is not supported (AmneziaWG only)"]))
    }

    func stopXray(completionHandler: () -> Void) {
        completionHandler()
    }
}

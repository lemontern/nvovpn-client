import Foundation
import NetworkExtension

// NvoVPN: продукт работает ТОЛЬКО на AmneziaWG. Реальный OpenVPN (openvpnadapter) убран из
// iOS сетевого расширения — его бинарный блоб идентичен AmneziaVPN и другим форкам
// (App Store Guideline 4.3 «similar binary»). Заглушки нужны, чтобы switch по protoType
// компилировался; фактически не вызываются — бэкенд отдаёт только amneziawg.
// Гейт !canImport: на macOS NE (где openvpnadapter линкуется) файл выпадает из компиляции.
#if !canImport(OpenVPNAdapter)
extension PacketTunnelProvider {
    func startOpenVPN(completionHandler: @escaping (Error?) -> Void) {
        completionHandler(NSError(domain: "NvoVPN", code: -1,
            userInfo: [NSLocalizedDescriptionKey: "OpenVPN is not supported (AmneziaWG only)"]))
    }

    func stopOpenVPN(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        completionHandler()
    }

    func handleOpenVPNStatusMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)? = nil) {
        completionHandler?(nil)
    }
}
#endif

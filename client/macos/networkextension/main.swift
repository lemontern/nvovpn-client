import Foundation
import NetworkExtension

// NvoVPN: System Extension — это standalone executable (в отличие от App Extension, который
// использовал системный _NSExtensionMain). Точка входа запускает режим system extension;
// провайдер (PacketTunnelProvider) резолвится из Info.plist NetworkExtension → NEProviderClasses.
autoreleasepool {
    NEProvider.startSystemExtensionMode()
}

dispatchMain()

import Foundation

/// What IosController::setupTrustTunnel() sends under the "trusttunnel"
/// providerConfiguration key. `config` is the complete TrustTunnelClient TOML
/// (with [listener.tun]); the route lists repeat what it carries so the
/// extension can install them without parsing TOML.
struct TrustTunnelConfig: Decodable {
    let dns1: String?
    let dns2: String?
    let mtu: Int
    let includedRoutes: [String]
    let excludedRoutes: [String]
    let dnsUpstreams: [String]
    let addresses: [String]
    let config: String
}

import Foundation
import Network
import NetworkExtension
import VpnClientFramework

enum TrustTunnelErrors: Error {
    case noConfig
    case configIsWrong
    case createFailed
    case startFailed
}

/// TrustTunnelClient's VpnState (platform/apple/TrustTunnelClient/VpnState.swift).
private enum TrustTunnelState: Int32 {
    case disconnected = 0
    case connecting = 1
    case connected = 2
    case waitingForRecovery = 3
    case recovering = 4
    case waitingForNetwork = 5
}

extension Constants {
    static let trustTunnelConfigKey = "trusttunnel"
    /// The core's in-tunnel DNS, served when the endpoint lists dns_upstreams
    /// (same address upstream's AGPacketTunnelProvider uses).
    static let trustTunnelFakeDns = "198.18.53.53"
}

/// TrustTunnel via TrustTunnelClient's VpnClientFramework. Unlike Xray there
/// is no SOCKS hop: VpnClient reads and writes NEPacketTunnelFlow directly,
/// binds its own outbound sockets to the uplink (IP_BOUND_IF), and follows
/// network changes itself, so this extension only installs the network
/// settings and maps states.
extension PacketTunnelProvider {
    func startTrustTunnel(completionHandler: @escaping (Error?) -> Void) {
        guard let protocolConfiguration = self.protocolConfiguration as? NETunnelProviderProtocol,
              let providerConfiguration = protocolConfiguration.providerConfiguration,
              let configData = providerConfiguration[Constants.trustTunnelConfigKey] as? Data else {
            neLog(.error, title: "TrustTunnel", message: "Can't get configuration")
            completionHandler(TrustTunnelErrors.noConfig)
            return
        }

        let ttConfig: TrustTunnelConfig
        do {
            ttConfig = try JSONDecoder().decode(TrustTunnelConfig.self, from: configData)
        } catch {
            neLog(.error, title: "TrustTunnel", message: "Can't decode configuration: \(error)")
            completionHandler(TrustTunnelErrors.configIsWrong)
            return
        }

        let settings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: trustTunnelRemoteAddress(ttConfig.addresses))
        settings.mtu = NSNumber(value: ttConfig.mtu)
        applyTrustTunnelRoutes(ttConfig, settings: settings)

        // The core answers DNS itself only when the endpoint names upstreams;
        // otherwise use the app's DNS, which then travels through the tunnel.
        var dnsServers = [String]()
        if !ttConfig.dnsUpstreams.isEmpty {
            dnsServers = [Constants.trustTunnelFakeDns]
        } else {
            dnsServers = [ttConfig.dns1, ttConfig.dns2].compactMap { $0 }.filter { !$0.isEmpty }
            if dnsServers.isEmpty {
                dnsServers = ["1.1.1.1"]
            }
        }
        let dnsSettings = NEDNSSettings(servers: dnsServers)
        dnsSettings.matchDomains = [""]
        settings.dnsSettings = dnsSettings

        setTunnelNetworkSettings(settings) { [weak self] error in
            guard let self else { return }
            if let error {
                neLog(.error, title: "TrustTunnel", message: "Can't apply network settings: \(error)")
                completionHandler(error)
                return
            }
            self.trustTunnelQueue.async {
                self.launchTrustTunnelClient(config: ttConfig.config, completionHandler: completionHandler)
            }
        }
    }

    func stopTrustTunnel(completionHandler: @escaping () -> Void) {
        trustTunnelQueue.async {
            self.stopTrustTunnelClient()
            completionHandler()
        }
    }

    func trustTunnelSleep() {
        trustTunnelQueue.async {
            self.trustTunnelClient?.notify_sleep()
        }
    }

    func trustTunnelWake() {
        trustTunnelQueue.async {
            self.trustTunnelClient?.notify_wake()
        }
    }

    // Runs on trustTunnelQueue.
    private func launchTrustTunnelClient(config: String, completionHandler: @escaping (Error?) -> Void) {
        trustTunnelStartHandled = false

        let client = TrustTunnelClientFactory.makeClient(
            config: config,
            connectionInfoHandler: { _ in },
            stateChangeHandler: { [weak self] rawState in
                guard let self else { return }
                self.trustTunnelQueue.async {
                    self.handleTrustTunnelState(rawState, completionHandler: completionHandler)
                }
            })

        guard let client else {
            neLog(.error, title: "TrustTunnel", message: "VpnClient rejected the configuration")
            completionHandler(TrustTunnelErrors.createFailed)
            return
        }
        trustTunnelClient = client

        if !client.start(packetFlow) {
            neLog(.error, title: "TrustTunnel", message: "VpnClient failed to start")
            stopTrustTunnelClient()
            completionHandler(TrustTunnelErrors.startFailed)
            return
        }
        neLog(.info, title: "TrustTunnel", message: "Client started, connecting")
    }

    // Runs on trustTunnelQueue. The tunnel is up only once the core reports
    // connected; disconnected before that is a failed start.
    private func handleTrustTunnelState(_ rawState: Int32, completionHandler: @escaping (Error?) -> Void) {
        guard let state = TrustTunnelState(rawValue: rawState) else {
            neLog(.debug, title: "TrustTunnel", message: "Unknown state \(rawState)")
            return
        }
        neLog(.info, title: "TrustTunnel", message: "State \(state)")

        switch state {
        case .connected:
            reasserting = false
            if !trustTunnelStartHandled {
                trustTunnelStartHandled = true
                completionHandler(nil)
            }
        case .disconnected:
            stopTrustTunnelClient()
            if !trustTunnelStartHandled {
                trustTunnelStartHandled = true
                completionHandler(TrustTunnelErrors.startFailed)
            } else {
                cancelTunnelWithError(nil)
            }
        case .waitingForRecovery, .recovering, .waitingForNetwork:
            reasserting = true
        case .connecting:
            break
        }
    }

    // Runs on trustTunnelQueue.
    private func stopTrustTunnelClient() {
        if let client = trustTunnelClient {
            _ = client.stop()
            trustTunnelClient = nil
        }
    }

    /// NEPacketTunnelNetworkSettings wants an IP; endpoint addresses are
    /// "host:port" and may be names. Same fallback as upstream.
    private func trustTunnelRemoteAddress(_ addresses: [String]) -> String {
        for address in addresses {
            if IPv4Address(trustTunnelHost(address)) != nil || IPv6Address(trustTunnelHost(address)) != nil {
                return trustTunnelHost(address)
            }
        }
        return "127.0.0.1"
    }

    /// "[v6]:port" -> v6, "host:port" -> host, a bare v6 literal unchanged.
    private func trustTunnelHost(_ address: String) -> String {
        if address.hasPrefix("["), let close = address.firstIndex(of: "]") {
            return String(address[address.index(after: address.startIndex)..<close])
        }
        let colons = address.filter { $0 == ":" }.count
        if colons == 1, let colon = address.firstIndex(of: ":") {
            return String(address[..<colon])
        }
        return address
    }

    /// Install the routes the app computed, minus what can never be routed,
    /// using the core's own CIDR arithmetic (VpnClient.excludeCidr) so the
    /// extension and the core agree on what the tunnel carries.
    private func applyTrustTunnelRoutes(_ ttConfig: TrustTunnelConfig, settings: NEPacketTunnelNetworkSettings) {
        let nonRoutable = ["0.0.0.0/8", "224.0.0.0/3"]
        let routes = VpnClient.excludeCidr(ttConfig.includedRoutes,
                                           excludeRoutes: ttConfig.excludedRoutes + nonRoutable)

        var v4 = [NEIPv4Route]()
        var v6 = [NEIPv6Route]()
        for route in routes {
            let parts = route.split(separator: "/")
            guard parts.count == 2, let prefix = Int(parts[1]) else { continue }
            let ip = String(parts[0])
            if ip.contains(":") {
                v6.append(NEIPv6Route(destinationAddress: ip, networkPrefixLength: NSNumber(value: prefix)))
            } else {
                v4.append(NEIPv4Route(destinationAddress: ip, subnetMask: ipv4Mask(prefix)))
            }
        }

        let ipv4 = NEIPv4Settings(addresses: ["10.0.0.2"], subnetMasks: ["255.255.255.0"])
        ipv4.includedRoutes = v4
        settings.ipv4Settings = ipv4

        if !v6.isEmpty {
            let ipv6 = NEIPv6Settings(addresses: ["fd00::1"], networkPrefixLengths: [64])
            ipv6.includedRoutes = v6
            settings.ipv6Settings = ipv6
        }
    }

    private func ipv4Mask(_ prefix: Int) -> String {
        let mask: UInt32 = prefix == 0 ? 0 : ~UInt32(0) << (32 - UInt32(min(prefix, 32)))
        return "\(mask >> 24 & 0xff).\(mask >> 16 & 0xff).\(mask >> 8 & 0xff).\(mask & 0xff)"
    }
}

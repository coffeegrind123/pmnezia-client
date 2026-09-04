import Foundation
import NetworkExtension
import Network
import os
import Darwin

enum TunnelProtoType: String {
  case wireguard, xray

}

struct Constants {
  static let kDefaultPathKey = "defaultPath"
  static let processQueueName = "org.amnezia.process-packets"
  static let kActivationAttemptId = "activationAttemptId"
  static let xrayConfigKey = "xray"
  static let wireGuardConfigKey = "wireguard"
  static let loggerTag = "NET"
  
  static let kActionStart = "start"
  static let kActionRestart = "restart"
  static let kActionStop = "stop"
  static let kActionGetTunnelId = "getTunnelId"
  static let kActionStatus = "status"
  static let kActionIsServerReachable = "isServerReachable"
  static let kMessageKeyAction = "action"
  static let kMessageKeyTunnelId = "tunnelId"
  static let kMessageKeyConfig = "config"
  static let kMessageKeyErrorCode = "errorCode"
  static let kMessageKeyHost = "host"
  static let kMessageKeyPort = "port"
  static let kMessageKeyOnDemand = "is-on-demand"
  static let kMessageKeySplitTunnelType = "SplitTunnelType"
  static let kMessageKeySplitTunnelSites = "SplitTunnelSites"
}

class PacketTunnelProvider: NEPacketTunnelProvider {
    var wgAdapter: WireGuardAdapter?
    private let pathMonitorQueue = DispatchQueue(label: Constants.processQueueName + ".path-monitor")
    private let networkChangeQueue = DispatchQueue(label: Constants.processQueueName + ".network-change")
    private let pathMonitor = NWPathMonitor()
    private var didReceiveInitialPathUpdate = false
    private var currentPath: Network.NWPath?
    private var currentPathSignature: String?
    private var pendingNetworkChangeWorkItem: DispatchWorkItem?
    private var isApplyingNetworkChange = false

    var splitTunnelType: Int?
    var splitTunnelSites: [String]?


    var startHandler: ((Error?) -> Void)?
    var stopHandler: (() -> Void)?
    var protoType: TunnelProtoType?
    
    var activeIfaceIdx: UInt32 = 0

    override init() {
        super.init()
        pathMonitor.pathUpdateHandler = { [weak self] path in
            guard let self else { return }
            self.currentPath = path
            let signature = self.pathSignature(for: path)
            let hasMeaningfulChange = self.currentPathSignature != signature
            self.currentPathSignature = signature
            self.updateActiveInterfaceIndex(for: path)

            guard self.didReceiveInitialPathUpdate else {
                self.didReceiveInitialPathUpdate = true
                return
            }

            guard hasMeaningfulChange, let proto = self.protoType else { return }

            // WireGuard/AWG manages network changes internally in its own adapter.
            if proto == .wireguard {
                return
            }

            if self.isApplyingNetworkChange || self.reasserting {
                xrayLog(.debug, message: "Ignoring path change while xray restart is in progress")
                return
            }

            self.scheduleNetworkChangeHandling(for: proto, path: path)
        }
        pathMonitor.start(queue: pathMonitorQueue)

        currentPath = pathMonitor.currentPath
        currentPathSignature = pathSignature(for: pathMonitor.currentPath)
    }

    func updateActiveInterfaceIndex(for path: Network.NWPath?) {
        guard let path else {
            activeIfaceIdx = 0
            return
        }

        let preferredTypes: [NWInterface.InterfaceType] = [.wiredEthernet, .wifi, .cellular, .other]

        let nonLoopbackInterfaces = path.availableInterfaces.filter { $0.type != .loopback }
        let activeInterfaces = nonLoopbackInterfaces.filter { path.usesInterfaceType($0.type) }

        let candidate = preferredTypes.compactMap { type in
            activeInterfaces.first { $0.type == type }
        }.first ?? activeInterfaces.first ?? nonLoopbackInterfaces.first

        if let candidate {
            activeIfaceIdx = UInt32(candidate.index)
        } else {
            activeIfaceIdx = 0
        }
    }

    func updateActiveInterfaceIndexForCurrentPath() {
        if let currentPath {
            currentPathSignature = pathSignature(for: currentPath)
            updateActiveInterfaceIndex(for: currentPath)
            return
        }

        currentPath = pathMonitor.currentPath
        currentPathSignature = pathSignature(for: pathMonitor.currentPath)
        updateActiveInterfaceIndex(for: pathMonitor.currentPath)
    }

  override func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)? = nil) {
      if messageData.count == 1 && messageData[0] == 0 {
          guard let completionHandler else { return }
          if protoType == .wireguard {
              handleWireguardAppMessage(messageData, completionHandler: completionHandler)
          } else {
              completionHandler(nil)
          }
          return
      }

      guard let message = String(data: messageData, encoding: .utf8) else {
          if let completionHandler {
              completionHandler(nil)
          }
          return
      }

      neLog(.info, title: "App said: ", message: message)

      guard let message = try? JSONSerialization.jsonObject(with: messageData, options: []) as? [String: Any] else {
          if protoType == .wireguard {
              handleWireguardAppMessage(messageData, completionHandler: completionHandler)
              return
          }
          neLog(.error, message: "Failed to serialize message from app")
          return
      }

      guard let completionHandler else {
          neLog(.error, message: "Missing message completion handler")
          return
      }

      guard let action = message[Constants.kMessageKeyAction] as? String else {
          neLog(.error, message: "Missing action key in app message")
          completionHandler(nil)
          return
      }

      if action == Constants.kActionStatus {
          handleStatusAppMessage(messageData,
                                 completionHandler: completionHandler)
      }
  }

    override func startTunnel(options: [String : NSObject]? = nil,
                              completionHandler: @escaping ((any Error)?) -> Void) {
        let activationAttemptId = options?[Constants.kActivationAttemptId] as? String
        let errorNotifier = ErrorNotifier(activationAttemptId: activationAttemptId)

        neLog(.info, message: "Start tunnel")
        if let vpnProto = protocolConfiguration as? NEVPNProtocol {
            if #available(iOS 14.0, macOS 11.0, *) {
                var details = "includeAllNetworks=\(vpnProto.includeAllNetworks)"
                if #available(iOS 14.2, macOS 11.0, *) {
                    details += " excludeLocalNetworks=\(vpnProto.excludeLocalNetworks)"
                }
                neLog(.info, title: "Protocol", message: details)
            }
        }

        if let protocolConfiguration = protocolConfiguration as? NETunnelProviderProtocol {
            let providerConfiguration = protocolConfiguration.providerConfiguration
            let providerKeys = providerConfiguration?.keys.sorted().joined(separator: ",") ?? ""
            var protocolDetails = "bundleId=\(protocolConfiguration.providerBundleIdentifier ?? "") keys=[\(providerKeys)]"
            neLog(.info, title: "Protocol", message: protocolDetails)

            if (providerConfiguration?[Constants.wireGuardConfigKey] as? Data) != nil {
                protoType = .wireguard
            } else if (providerConfiguration?[Constants.xrayConfigKey] as? Data) != nil {
                protoType = .xray
            }
        }

        guard let protoType else {
            let error = NSError(domain: "Protocol is not selected", code: 0)
            completionHandler(error)
            return
        }

        cancelPendingNetworkChangeHandling()
        didReceiveInitialPathUpdate = false
        updateActiveInterfaceIndexForCurrentPath()

        switch protoType {
        case .wireguard:
            startWireguard(activationAttemptId: activationAttemptId,
                           errorNotifier: errorNotifier,
                           completionHandler: completionHandler)
        case .xray:
            startXray(completionHandler: completionHandler)

        }
    }

  
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        cancelPendingNetworkChangeHandling()

        guard let protoType else {
            completionHandler()
            return
        }

        switch protoType {
        case .wireguard:
            stopWireguard(with: reason,
                          completionHandler: completionHandler)
        case .xray:
            stopXray(completionHandler: completionHandler)
        }
    }
  
    func handleStatusAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)? = nil) {
        guard let protoType else {
            completionHandler?(nil)
            return
        }

        switch protoType {
        case .wireguard:
            handleWireguardStatusMessage(messageData, completionHandler: completionHandler)
        case .xray:
            break;
        }
    }
  
    // MARK: Network observing methods
    override func observeValue(forKeyPath keyPath: String?,
                               of object: Any?,
                               change: [NSKeyValueChangeKey: Any]?,
                               context: UnsafeMutableRawPointer?) {
        guard Constants.kDefaultPathKey == keyPath else {
            return
        }
    }
  
    private func handle(networkChange changePath: Network.NWPath, completion: @escaping (Error?) -> Void) {
        guard protoType == .xray else {
            updateActiveInterfaceIndex(for: changePath)
            completion(nil)
            return
        }

        updateActiveInterfaceIndex(for: changePath)
        reasserting = true
        xrayLog(.info, message: "Applying network change to xray tunnel")
        stopXray { }
        startXray { [weak self] error in
            self?.reasserting = false
            completion(error)
        }
    }

    private func scheduleNetworkChangeHandling(for proto: TunnelProtoType, path: Network.NWPath) {
        guard proto == .xray else { return }

        pendingNetworkChangeWorkItem?.cancel()

        let workItem = DispatchWorkItem { [weak self] in
            guard let self else { return }
            self.pendingNetworkChangeWorkItem = nil

            if self.isApplyingNetworkChange || self.reasserting {
                xrayLog(.debug, message: "Skipping network change while restart is already in progress")
                return
            }

            self.isApplyingNetworkChange = true
            DispatchQueue.main.async {
                self.handle(networkChange: path) { [weak self] _ in
                    self?.networkChangeQueue.async {
                        self?.isApplyingNetworkChange = false
                    }
                }
            }
        }

        pendingNetworkChangeWorkItem = workItem
        networkChangeQueue.asyncAfter(deadline: .now() + 1.0, execute: workItem)
    }

    private func cancelPendingNetworkChangeHandling() {
        pendingNetworkChangeWorkItem?.cancel()
        pendingNetworkChangeWorkItem = nil
        isApplyingNetworkChange = false
    }
}

private extension PacketTunnelProvider {
    func pathSignature(for path: Network.NWPath) -> String {
        var signatureComponents = [String(describing: path.status)]
        signatureComponents.append(path.isExpensive ? "exp" : "noexp")
        signatureComponents.append(path.isConstrained ? "con" : "nocon")

        // Ignore loopback and tunnel-style `.other` interfaces so Xray does not
        // react to its own utun lifecycle as if the physical uplink changed.
        let preferredTypes: [NWInterface.InterfaceType] = [.wiredEthernet, .wifi, .cellular]
        let externalInterfaces = path.availableInterfaces.filter { interface in
            interface.type == .wiredEthernet || interface.type == .wifi || interface.type == .cellular
        }

        let sortedInterfaces = externalInterfaces.sorted { lhs, rhs in
            if lhs.type == rhs.type {
                return lhs.index < rhs.index
            }

            let lhsOrder = preferredTypes.firstIndex(of: lhs.type) ?? preferredTypes.count
            let rhsOrder = preferredTypes.firstIndex(of: rhs.type) ?? preferredTypes.count

            if lhsOrder == rhsOrder {
                return lhs.index < rhs.index
            }

            return lhsOrder < rhsOrder
        }

        for interface in sortedInterfaces {
            let typeName: String
            switch interface.type {
            case .wiredEthernet: typeName = "ethernet"
            case .wifi: typeName = "wifi"
            case .cellular: typeName = "cellular"
            case .loopback, .other:
                continue
            @unknown default: typeName = "unknown"
            }
            signatureComponents.append("\(typeName):\(interface.index)")
        }

        // Include currently used interface preference ordering
        for type in preferredTypes {
            let usesType = path.usesInterfaceType(type)
            signatureComponents.append("uses-\(type):\(usesType)")
        }

        return signatureComponents.joined(separator: "|")
    }
}

extension WireGuardLogLevel {
  var osLogLevel: OSLogType {
    switch self {
    case .verbose:
      return .debug
    case .error:
      return .error
    }
  }
}

extension NEProviderStopReason {
  var amneziaDescription: String {
    switch self {
    case .none:
      return "No specific reason"
    case .userInitiated:
      return "The user stopped the NE"
    case .providerFailed:
      return "The NE failed to function correctly"
    case .noNetworkAvailable:
      return "No network connectivity is currently available"
    case .unrecoverableNetworkChange:
      return "The device’s network connectivity changed"
    case .providerDisabled:
      return "The NE was disabled"
    case .authenticationCanceled:
      return "The authentication process was canceled"
    case .configurationFailed:
      return "The VPNC is invalid"
    case .idleTimeout:
      return "The session timed out"
    case .configurationDisabled:
      return "The VPNC was disabled"
    case .configurationRemoved:
      return "The VPNC was removed"
    case .superceded:
      return "VPNC was superceded by a higher-priority VPNC"
    case .userLogout:
      return "The user logged out"
    case .userSwitch:
      return "The current console user changed"
    case .connectionFailed:
      return "The connection failed"
    case .internalError:
      return "The network extension reported an internal error"
    case .sleep:
      return "A stop reason indicating the VPNC enabled disconnect on sleep and the device went to sleep"
    case .appUpdate:
      return "appUpdat"
    @unknown default:
      return "@unknown default"
    }
  }
}

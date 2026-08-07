// SPDX-License-Identifier: GPL-3.0-or-later
//
// Engine — the public façade of the QQ-DNS native client engine.
//
// A faithful in-process Qt port of the QQ-Tunnel client (reference:
// awg-easy-rs `src/qqdns/engine.rs`, itself a port of QQ-Tunnel's `main.py`).
// It carries a raw UDP datapath — the AmneziaWG datapath itself — inside DNS
// query names, so the tunnel survives an egress blackout where only port 53
// escapes.
//
//   AmneziaWG (Endpoint = 127.0.0.1:localUdpPort)
//        ↓  (raw WireGuard UDP)
//   h_in socket  ──►  base32 + fragment  ──►  DNS queries
//                                               ↓  send sockets (port spread)
//   public resolvers → operator's NS-delegated server → awg-easy-rs qqdns
//        ▲                                               │
//   wan listener (:53) ◄── DNS queries ◄────────────────┘  (return direction)
//
// Unlike MasterDnsVPN's engine (which exposes a SOCKS5 port bridged by
// tun2socks), this exposes a **local UDP port**: AmneziaWG runs on top of it,
// so QqDnsProtocol composes under Awg rather than replacing tun2socks. The
// engine never spawns subprocesses, never bundles a foreign binary, and uses
// only Qt (QUdpSocket).
//
// Calling pattern mirrors MasterDnsVpnEngine:
//
//     auto engine = std::make_unique<amnezia::qqdns::Engine>();
//     QObject::connect(engine.get(), &Engine::stateChanged, ...);
//     engine->start(configJson);   // binds sockets; State::Connected when ready
//     quint16 port = engine->localUdpPort();  // AmneziaWG Endpoint target
//     ...
//     engine->stop();

#ifndef QQDNS_ENGINE_H
#define QQDNS_ENGINE_H

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <memory>

namespace amnezia::qqdns {

class EnginePrivate;

class Engine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,      // not started, or cleanly stopped
        Starting,  // start() called; binding sockets
        Connected, // sockets bound; local UDP port live, ready for AmneziaWG
        Stopping,  // stop() in progress
        Failed     // a fatal error stopped the engine; lastError() has detail
    };
    Q_ENUM(State)

    explicit Engine(QObject *parent = nullptr);
    ~Engine() override;

    // Bind the tunnel sockets. `config` is the inner engine blob (see the
    // key list in engine.cpp): dns_ips, send_domains, recv_domains, ports,
    // wire-shape params. Returns false synchronously on a structurally invalid
    // config or a bind failure; on success the engine is Connected and
    // localUdpPort() is valid.
    bool start(const QJsonObject &config);

    // Tear down all sockets. Idempotent; the engine may be reused via start().
    void stop();

    State state() const;
    QString lastError() const;

    // Local loopback UDP port the engine listens on for the app (AmneziaWG
    // points its Endpoint at 127.0.0.1:<this>). 0 until Connected.
    quint16 localUdpPort() const;

    quint64 bytesReceived() const; // tunnel → app
    quint64 bytesSent() const;     // app → tunnel

signals:
    void stateChanged(amnezia::qqdns::Engine::State newState);
    void bytesChanged(quint64 receivedDelta, quint64 sentDelta);

private:
    Q_DISABLE_COPY_MOVE(Engine)
    std::unique_ptr<EnginePrivate> d;
};

} // namespace amnezia::qqdns

#endif // QQDNS_ENGINE_H

// SPDX-License-Identifier: GPL-3.0-or-later

#include "qqDnsProtocol.h"

#include "core/protocols/awgProtocol.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/ipcClient.h"
#include "ipc.h"

#include <QJsonDocument>

QqDnsProtocol::QqDnsProtocol(const QJsonObject &configuration, QObject *parent)
    : VpnProtocol(configuration, parent)
{
    // The wrapper the model stores is already the engine's snake_case blob plus
    // an embedded "awg" object; the engine ignores keys it doesn't recognise,
    // so we pass the whole thing through as its config.
    m_engineConfig =
            configuration.value(ProtocolUtils::key_proto_config_data(Proto::QqDns)).toObject();
}

QqDnsProtocol::~QqDnsProtocol()
{
    qDebug() << "QqDnsProtocol::~QqDnsProtocol()";
    QqDnsProtocol::stop();
}

QJsonObject QqDnsProtocol::buildInnerAwgConfig(quint16 enginePort) const
{
    const QJsonObject wrapper = m_engineConfig;
    const QJsonObject awgWrapper = wrapper.value(configKey::qqAwg).toObject();

    QJsonObject awgConfigData =
            awgWrapper.value(ProtocolUtils::key_proto_config_data(Proto::Awg)).toObject();
    // Rewrite the WireGuard endpoint to the loopback engine port. Loopback is
    // never routed through the TUN, so the handshake reaches the engine.
    const QString loopback = QStringLiteral("127.0.0.1");
    awgConfigData[configKey::hostName] = loopback;
    awgConfigData[configKey::port] = enginePort;

    // Inherit the user's runtime preferences (split tunnel, killswitch, DNS,
    // client id, …) from the outer config; swap the protocol + data to awg.
    QJsonObject awgRaw = m_rawConfig;
    awgRaw.remove(ProtocolUtils::key_proto_config_data(Proto::QqDns));
    awgRaw[QStringLiteral("protocol")] = ProtocolUtils::protoToString(Proto::Awg);
    awgRaw[ProtocolUtils::key_proto_config_data(Proto::Awg)] = awgConfigData;
    awgRaw[configKey::hostName] = loopback;
    awgRaw[configKey::port] = enginePort;
    return awgRaw;
}

ErrorCode QqDnsProtocol::start()
{
    qDebug() << "QqDnsProtocol::start()";

    if (m_engineConfig.isEmpty()) {
        qCritical() << "QqDns config wrapper is empty";
        return ErrorCode::InternalError;
    }

    const QString configJson =
            QString::fromUtf8(QJsonDocument(m_engineConfig).toJson(QJsonDocument::Compact));

    return IpcClient::withInterface(
            [&](QSharedPointer<IpcInterfaceReplica> iface) -> ErrorCode {
                auto startReply = iface->qqDnsStart(configJson);
                if (!startReply.waitForFinished() || !startReply.returnValue()) {
                    qCritical() << "Failed to start QqDns engine in service";
                    return ErrorCode::InternalError;
                }

                auto portReply = iface->qqDnsLocalPort();
                if (!portReply.waitForFinished()) {
                    qCritical() << "Failed to fetch QqDns local UDP port";
                    return ErrorCode::InternalError;
                }
                const quint16 enginePort = portReply.returnValue();
                if (enginePort == 0) {
                    qCritical() << "QqDns engine did not bind a local UDP port";
                    return ErrorCode::InternalError;
                }

                // Bring AmneziaWG up on top of the loopback engine port.
                const QJsonObject awgRaw = buildInnerAwgConfig(enginePort);
                m_awg = new Awg(awgRaw, this);
                connect(m_awg.data(), &VpnProtocol::connectionStateChanged, this,
                        [this](Vpn::ConnectionState s) { setConnectionState(s); });
                connect(m_awg.data(), &VpnProtocol::protocolError, this,
                        [this](ErrorCode e) { setLastError(e); });

                const ErrorCode ec = m_awg->start();
                if (ec != ErrorCode::NoError) {
                    qCritical() << "Inner AmneziaWG failed to start (qqdns flow):" << ec;
                }
                return ec;
            },
            []() { return ErrorCode::AmneziaServiceConnectionFailed; });
}

void QqDnsProtocol::stop()
{
    qDebug() << "QqDnsProtocol::stop()";

    if (m_awg) {
        m_awg->blockSignals(true);
        m_awg->stop();
        m_awg->deleteLater();
        m_awg.clear();
    }

    IpcClient::withInterface([](QSharedPointer<IpcInterfaceReplica> iface) {
        auto stopReply = iface->qqDnsStop();
        if (!stopReply.waitForFinished() || !stopReply.returnValue()) {
            qWarning() << "Failed to stop QqDns engine in service";
        }
    });

    setConnectionState(Vpn::ConnectionState::Disconnected);
}

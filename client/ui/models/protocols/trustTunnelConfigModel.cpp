// SPDX-License-Identifier: GPL-3.0-or-later

#include "trustTunnelConfigModel.h"

#include <QRegularExpression>

#include "core/utils/constants/protocolConstants.h"

using namespace amnezia;
namespace tt = amnezia::serialization::trusttunnel;

namespace
{
// QML hands lists over as a QStringList, a JS array or a comma/newline string.
QStringList toStringList(const QVariant &value)
{
    QStringList raw;
    if (value.canConvert<QStringList>() && value.typeId() != QMetaType::QString) {
        raw = value.toStringList();
    } else {
        raw = value.toString().split(QRegularExpression(QStringLiteral("[,\\n]")));
    }
    QStringList out;
    for (const QString &s : raw) {
        const QString t = s.trimmed();
        if (!t.isEmpty()) {
            out.append(t);
        }
    }
    return out;
}
} // namespace

TrustTunnelConfigModel::TrustTunnelConfigModel(QObject *parent) : QAbstractListModel(parent) {}

tt::Endpoint &TrustTunnelConfigModel::endpoint()
{
    if (!m_protocolConfig.clientConfig.has_value()) {
        m_protocolConfig.clientConfig = TrustTunnelClientConfig {};
    }
    return m_protocolConfig.clientConfig->endpoint;
}

const tt::Endpoint &TrustTunnelConfigModel::endpoint() const
{
    static const tt::Endpoint empty;
    return m_protocolConfig.clientConfig.has_value() ? m_protocolConfig.clientConfig->endpoint : empty;
}

int TrustTunnelConfigModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

bool TrustTunnelConfigModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }

    tt::Endpoint &e = endpoint();
    switch (role) {
    case AddressesRole: {
        const QStringList addresses = toStringList(value);
        if (addresses.isEmpty()) {
            return false;
        }
        e.addresses = addresses;
        break;
    }
    case CustomSniRole: e.customSni = value.toString().trimmed(); break;
    case UpstreamProtocolRole: {
        const QString proto = value.toString();
        if (proto != QLatin1String(protocols::trustTunnel::upstreamHttp2)
            && proto != QLatin1String(protocols::trustTunnel::upstreamHttp3)) {
            return false;
        }
        e.upstreamProtocol = proto;
        m_protocolConfig.serverConfig.transportProto =
                proto == QLatin1String(protocols::trustTunnel::upstreamHttp3) ? QStringLiteral("udp")
                                                                             : QStringLiteral("tcp");
        break;
    }
    case AntiDpiRole: e.antiDpi = value.toBool(); break;
    case HasIpv6Role: e.hasIpv6 = value.toBool(); break;
    case SkipVerificationRole: e.skipVerification = value.toBool(); break;
    case DnsUpstreamsRole: e.dnsUpstreams = toStringList(value); break;
    default: return false;
    }
    emit dataChanged(index, index, QList { role });
    return true;
}

QVariant TrustTunnelConfigModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    const tt::Endpoint &e = endpoint();
    switch (role) {
    case HostnameRole: return e.hostname;
    case AddressesRole: return e.addresses;
    case CustomSniRole: return e.customSni;
    case UsernameRole: return e.username;
    case UpstreamProtocolRole: return e.upstreamProtocol;
    case AntiDpiRole: return e.antiDpi;
    case HasIpv6Role: return e.hasIpv6;
    case SkipVerificationRole: return e.skipVerification;
    case HasPinnedCertificateRole: return !e.certificate.isEmpty();
    case DnsUpstreamsRole: return e.dnsUpstreams;
    case ClientRandomRole: return e.clientRandom;
    case ProfileNameRole: return e.name;
    }
    return QVariant();
}

QString TrustTunnelConfigModel::deepLink() const
{
    if (!m_protocolConfig.clientConfig.has_value()) {
        return {};
    }
    return tt::encodeDeepLink(endpoint());
}

void TrustTunnelConfigModel::updateModel(amnezia::DockerContainer container,
                                         const amnezia::TrustTunnelProtocolConfig &protocolConfig)
{
    beginResetModel();
    m_container = container;
    m_protocolConfig = protocolConfig;
    endResetModel();
}

amnezia::TrustTunnelProtocolConfig TrustTunnelConfigModel::getProtocolConfig()
{
    return m_protocolConfig;
}

QHash<int, QByteArray> TrustTunnelConfigModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[HostnameRole] = "hostname";
    roles[AddressesRole] = "addresses";
    roles[CustomSniRole] = "customSni";
    roles[UsernameRole] = "username";
    roles[UpstreamProtocolRole] = "upstreamProtocol";
    roles[AntiDpiRole] = "antiDpi";
    roles[HasIpv6Role] = "hasIpv6";
    roles[SkipVerificationRole] = "skipVerification";
    roles[HasPinnedCertificateRole] = "hasPinnedCertificate";
    roles[DnsUpstreamsRole] = "dnsUpstreams";
    roles[ClientRandomRole] = "clientRandom";
    roles[ProfileNameRole] = "profileName";
    return roles;
}

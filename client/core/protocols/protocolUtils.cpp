#include "protocolUtils.h"

#include <QRandomGenerator>
#include <QJsonObject>
#include <QObject>

using namespace amnezia;

QString ProtocolUtils::transportProtoToString(TransportProto proto, Proto p)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<TransportProto>();
    QString protoKey = metaEnum.valueToKey(static_cast<int>(proto));
    return protoKey.toLower();
}

Proto ProtocolUtils::protoFromString(QString proto)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<Proto>();
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        Proto p = static_cast<Proto>(i);
        if (proto == protoToString(p))
            return p;
    }
    return Proto::Unknown;
}

QString ProtocolUtils::protoToString(Proto p)
{
    if (p == Proto::Unknown)
        return "";

    QMetaEnum metaEnum = QMetaEnum::fromType<Proto>();
    QString protoKey = metaEnum.valueToKey(static_cast<int>(p));
    return protoKey.toLower();
}

QMap<Proto, QString> ProtocolUtils::protocolHumanNames()
{
    return { { Proto::WireGuard, "WireGuard" },
             { Proto::Awg, "AmneziaWG" },
             { Proto::Xray, "XRay" },
             { Proto::MasterDnsVpn, "MasterDnsVPN" },
             { Proto::QqDns, "QQ-DNS" },
    };
}

ServiceType ProtocolUtils::protocolService(Proto p)
{
    return p == Proto::Unknown ? ServiceType::None : ServiceType::Vpn;
}

TransportProto ProtocolUtils::defaultTransportProto(Proto p)
{
    switch (p) {
    case Proto::Xray: return TransportProto::Tcp;
    // WireGuard and AmneziaWG are UDP; both DNS tunnels ride DNS over UDP.
    default: return TransportProto::Udp;
    }
}

QString ProtocolUtils::key_proto_config_data(Proto p)
{
    return protoToString(p) + "_config_data";
}

QString ProtocolUtils::key_proto_config_path(Proto p)
{
    return protoToString(p) + "_config_path";
}

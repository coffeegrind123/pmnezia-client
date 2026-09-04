#ifndef PROTOCOLENUM_H
#define PROTOCOLENUM_H

#include <QMetaEnum>
#include <QObject>

namespace amnezia
{
    namespace ProtocolEnumNS
    {
        Q_NAMESPACE

        enum TransportProto {
            Udp,
            Tcp,
            TcpAndUdp
        };
        Q_ENUM_NS(TransportProto)

        enum Proto {
            Unknown = 0,
            WireGuard,
            Awg,
            Xray,
            MasterDnsVpn,
            QqDns,
        };
        Q_ENUM_NS(Proto)

        enum ServiceType {
            None = 0,
            Vpn
        };
        Q_ENUM_NS(ServiceType)
    } // namespace ProtocolEnumNS

    using namespace ProtocolEnumNS;
}

#endif // PROTOCOLENUM_H



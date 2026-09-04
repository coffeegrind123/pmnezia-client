#include "containerUtils.h"

#include <QMetaEnum>
#include <QObject>
#include <QJsonDocument>

using namespace amnezia;

DockerContainer ContainerUtils::containerFromString(const QString &container)
{
    QMetaEnum metaEnum = QMetaEnum::fromType<DockerContainer>();
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        DockerContainer c = static_cast<DockerContainer>(i);
        if (container == containerToString(c))
            return c;
    }
    return DockerContainer::None;
}

QString ContainerUtils::containerToString(DockerContainer c)
{
    if (c == DockerContainer::None)
        return "none";
    if (c == DockerContainer::Awg)
        return "amnezia-awg";
    if (c == DockerContainer::Awg2)
        return "amnezia-awg2";
    QMetaEnum metaEnum = QMetaEnum::fromType<DockerContainer>();
    QString containerKey = metaEnum.valueToKey(static_cast<int>(c));

    return "amnezia-" + containerKey.toLower();
}

QString ContainerUtils::containerTypeToString(DockerContainer c)
{
    if (c == DockerContainer::None)
        return "none";
    if (c == DockerContainer::Awg)
        return "awg";
    if (c == DockerContainer::Awg2)
        return "awg";
    QMetaEnum metaEnum = QMetaEnum::fromType<DockerContainer>();
    QString containerKey = metaEnum.valueToKey(static_cast<int>(c));

    return containerKey.toLower();
}

QList<DockerContainer> ContainerUtils::allContainers()
{
    QMetaEnum metaEnum = QMetaEnum::fromType<DockerContainer>();
    QList<DockerContainer> all;
    for (int i = 0; i < metaEnum.keyCount(); ++i) {
        all.append(static_cast<DockerContainer>(i));
    }

    return all;
}

QMap<DockerContainer, QString> ContainerUtils::containerHumanNames()
{
    return { { DockerContainer::None, "Not installed" },
             { DockerContainer::WireGuard, "WireGuard" },
             { DockerContainer::Awg, "AmneziaWG" },
             { DockerContainer::Awg2, "AmneziaWG" },
             { DockerContainer::Xray, "XRay" },
             { DockerContainer::MasterDnsVpn, QObject::tr("MasterDnsVPN") },
             { DockerContainer::QqDns, QObject::tr("QQ-DNS") },
    };
}

QMap<DockerContainer, QString> ContainerUtils::containerDescriptions()
{
    return { { DockerContainer::WireGuard,
               QObject::tr("WireGuard - popular VPN protocol with high performance, high speed and low power "
                           "consumption.") },
             { DockerContainer::Awg,
               QObject::tr("AmneziaWG is a special protocol from Amnezia based on WireGuard. "
                           "It provides high connection speed and ensures stable operation even in the most challenging network conditions.") },
             { DockerContainer::Awg2,
               QObject::tr("AmneziaWG is a special protocol from Amnezia based on WireGuard. "
                           "It provides high connection speed and ensures stable operation even in the most challenging network conditions.") },
             { DockerContainer::Xray,
               QObject::tr("XRay with REALITY masks VPN traffic as web traffic and protects against active probing. "
                           "It is highly resistant to detection and offers high speed.") },
             { DockerContainer::MasterDnsVpn,
               QObject::tr("MasterDnsVPN tunnels TCP traffic inside DNS queries that traverse public resolvers. "
                           "Designed to keep working when only DNS leaves the network - useful in heavily filtered or "
                           "captive-portal environments.") },
             { DockerContainer::QqDns,
               QObject::tr("QQ-DNS carries the AmneziaWG datapath itself inside DNS query names. "
                           "Reaches the low-latency Gaming config when only port 53 escapes the network.") },
    };
}

QMap<DockerContainer, QString> ContainerUtils::containerDetailedDescriptions()
{
    return {
        { DockerContainer::WireGuard,
          QObject::tr("WireGuard is a modern, streamlined VPN protocol offering stable connectivity and excellent performance across all devices. "
                      "It uses fixed encryption settings, delivering lower latency and higher data transfer speeds compared to OpenVPN. "
                      "However, WireGuard is easily identifiable by DPI systems due to its distinctive packet signatures, making it susceptible to blocking.\n"
                      "\nFeatures:\n"
                      "* Available on all AmneziaVPN platforms\n"
                      "* Low power consumption on mobile devices\n"
                      "* Minimal configuration required\n"
                      "* Easily detected by DPI systems (susceptible to blocking)\n"
                      "* Operates over UDP protocol") },
        { DockerContainer::Awg2,
          QObject::tr("AmneziaWG is a modern VPN protocol based on WireGuard, "
                      "combining simplified architecture with high performance across all devices. "
                      "It addresses WireGuard's main vulnerability (easy detection by DPI systems) through advanced obfuscation techniques, "
                      "making VPN traffic indistinguishable from regular internet traffic.\n"
                      "\nAmneziaWG is an excellent choice for those seeking a fast, stealthy VPN connection.\n"
                      "\nFeatures:\n"
                      "* Available on all AmneziaVPN platforms\n"
                      "* Low battery consumption on mobile devices\n"
                      "* Minimal settings required\n"
                      "* Undetectable by traffic analysis systems (DPI)\n"
                      "* Operates over UDP protocol") },
        { DockerContainer::Xray,
          QObject::tr("REALITY is an innovative protocol developed by the creators of XRay, designed specifically to combat high levels of internet censorship. "
                      "REALITY identifies censorship systems during the TLS handshake, "
                      "redirecting suspicious traffic seamlessly to legitimate websites like google.com while providing genuine TLS certificates. "
                      "This allows VPN traffic to blend indistinguishably with regular web traffic without special configuration."
                      "\nUnlike older protocols such as VMess, VLESS, and XTLS-Vision, REALITY incorporates an advanced built-in \"friend-or-foe\" detection mechanism, "
                      "effectively protecting against DPI and other traffic analysis methods.\n"
                      "\nFeatures:\n"
                      "* Resistant to active probing and DPI detection\n"
                      "* No special configuration required to disguise traffic\n"
                      "* Highly effective in heavily censored regions\n"
                      "* Minimal battery consumption on devices\n"
                      "* Operates over TCP protocol") },
        { DockerContainer::MasterDnsVpn,
          QObject::tr("MasterDnsVPN is a DNS-tunnel transport: the client encrypts and fragments TCP traffic into "
                      "DNS queries that travel through public DNS resolvers, and the server listens on UDP/53 for "
                      "the tunnel envelopes via an NS-delegated subdomain. Optimised for harsh networks where only "
                      "DNS leaves the host (captive portals, deep filtering, lossy or paid mobile data).\n"
                      "\nFeatures:\n"
                      "* Survives blackout networks where only DNS resolves\n"
                      "* Encrypted with operator-chosen cipher (XOR / ChaCha20 / AES-128/192/256-GCM)\n"
                      "* Resilient to packet loss via ARQ retransmission and per-resolver MTU discovery\n"
                      "* Higher latency and lower throughput than direct VPN protocols (DNS-frame overhead)\n"
                      "* Operator must own a domain and create an NS delegation pointing to the server") },

        { DockerContainer::QqDns,
          QObject::tr("QQ-DNS tunnels raw UDP - the AmneziaWG datapath itself - inside DNS query names, so the "
                      "low-latency Gaming config keeps working when only port 53 escapes the network. Both ends are "
                      "authoritative for an NS-delegated subdomain.\n"
                      "\nFeatures:\n"
                      "* Carries the native AmneziaWG datapath, not a SOCKS proxy\n"
                      "* Survives blackout networks where only DNS resolves\n"
                      "* Blackout survival, not low latency - base32 encoding and fragmentation multiply overhead\n"
                      "* One server instance serves one client endpoint\n"
                      "* Operator must own a domain and create an NS delegation pointing to the server") },
    };
}

ServiceType ContainerUtils::containerService(DockerContainer c)
{
    return ProtocolUtils::protocolService(defaultProtocol(c));
}

Proto ContainerUtils::defaultProtocol(DockerContainer c)
{
    switch (c) {
    case DockerContainer::WireGuard: return Proto::WireGuard;
    case DockerContainer::Awg:
    case DockerContainer::Awg2: return Proto::Awg;
    case DockerContainer::Xray: return Proto::Xray;
    case DockerContainer::MasterDnsVpn: return Proto::MasterDnsVpn;
    case DockerContainer::QqDns: return Proto::QqDns;
    case DockerContainer::None:
    default: return Proto::Unknown;
    }
}

QString ContainerUtils::containerTypeToProtocolString(DockerContainer c)
{
    if (c == DockerContainer::None)
        return "none";

    Proto p = defaultProtocol(c);
    return ProtocolUtils::protoToString(p);
}

bool ContainerUtils::isSupportedByCurrentPlatform(DockerContainer c)
{
#if defined(Q_OS_MAC) && !defined(MACOS_NE)
    // The mdnsvpn binary has no macOS build yet.
    return c != DockerContainer::MasterDnsVpn;
#else
    Q_UNUSED(c)
    return true;
#endif
}

bool ContainerUtils::isEasySetupContainer(DockerContainer container)
{
    return container == DockerContainer::Awg2;
}

int ContainerUtils::easySetupOrder(DockerContainer container)
{
    return container == DockerContainer::Awg2 ? 1 : 0;
}

bool ContainerUtils::isAwgContainer(DockerContainer container)
{
    return container == DockerContainer::Awg || container == DockerContainer::Awg2;
}

QJsonObject ContainerUtils::getProtocolConfigFromContainer(const Proto protocol, const QJsonObject &containerConfig)
{
    QString protocolConfigString = containerConfig.value(ProtocolUtils::protoToString(protocol))
    .toObject()
            .value(configKey::lastConfig)
            .toString();

    return QJsonDocument::fromJson(protocolConfigString.toUtf8()).object();
}

int ContainerUtils::installPageOrder(DockerContainer container)
{
    switch (container) {
    case DockerContainer::Awg2: return 1;
    case DockerContainer::WireGuard: return 2;
    case DockerContainer::Xray: return 3;
    case DockerContainer::MasterDnsVpn: return 9;
    case DockerContainer::QqDns: return 10;
    default: return 0;
    }
}

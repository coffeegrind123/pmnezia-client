// TrustTunnel: tt:// deep links, the two TOML shapes, the client TOML the
// service renders, the protocol model behind ProtocolConfig, and import.
//
// Vectors A and B are the output of upstream's scripts/config_to_deeplink.py
// (TrustTunnel v1.1.0), which byte-matches its Rust encoder.

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "core/models/containerConfig.h"
#include "core/models/protocolConfig.h"
#include "core/models/protocols/trustTunnelProtocolConfig.h"
#include "core/utils/serialization/trustTunnel.h"
#include "secureQSettings.h"
#include "utils/testCoreController.h"
#include "vpnConnection.h"

using namespace amnezia;
namespace tt = amnezia::serialization::trusttunnel;

namespace
{

const QString vectorA = QStringLiteral("tt://?AAEBAQ92cG4uZXhhbXBsZS5jb20FBWFsaWNlBglzZWNyZXQxMjMCCzEuMi4zLjQ6NDQz");
const QString vectorB = QStringLiteral(
        "tt://?AAEBARZzZWN1cmUudnBuLmV4YW1wbGUuY29tBQxwcmVtaXVtX3VzZXIGFHZlcnlfc2VjcmV0X3Bhc3N3b3JkAhAxOTIuMTY4LjEuMTo4ND"
        "QzAgwxMC4wLjAuMTo0NDMDD2Nkbi5leGFtcGxlLm9yZwQBAAoBAQkBAgwGTXkgVlBODR4HMS4xLjEuMRV0bHM6Ly9kbnMuZXhhbXBsZS5jb20");

QString linkFromPayload(const QByteArray &payload)
{
    return QStringLiteral("tt://?")
            + QString::fromLatin1(payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// Minimal TLV payload: version, hostname, user, password, one address.
QByteArray basePayload(quint8 version)
{
    QByteArray p;
    p.append('\x00').append('\x01').append(char(version));
    p.append('\x01').append('\x01').append('h');
    p.append('\x05').append('\x01').append('u');
    p.append('\x06').append('\x01').append('p');
    p.append('\x02').append('\x05').append("a:443");
    return p;
}

// Two fake "certificates": DER SEQUENCEs with short- and long-form lengths.
QByteArray fakeDerChain()
{
    QByteArray first;
    first.append('\x30').append('\x03').append("abc");
    QByteArray second;
    second.append('\x30').append('\x81').append(char(200)).append(QByteArray(200, 'x'));
    return first + second;
}

} // namespace

class TestTrustTunnel : public QObject
{
    Q_OBJECT

private slots:
    // ---- deep links ----

    void decodesVectorA()
    {
        QString error;
        const auto e = tt::decodeDeepLink(vectorA, &error);
        QVERIFY2(e.has_value(), qPrintable(error));
        QCOMPARE(e->version, 1);
        QCOMPARE(e->hostname, QStringLiteral("vpn.example.com"));
        QCOMPARE(e->username, QStringLiteral("alice"));
        QCOMPARE(e->password, QStringLiteral("secret123"));
        QCOMPARE(e->addresses, QStringList { QStringLiteral("1.2.3.4:443") });
        QVERIFY(e->hasIpv6);
        QVERIFY(!e->skipVerification);
        QVERIFY(!e->antiDpi);
        QCOMPARE(e->upstreamProtocol, QStringLiteral("http2"));
        QVERIFY(e->certificate.isEmpty());
    }

    void decodesVectorB()
    {
        QString error;
        const auto e = tt::decodeDeepLink(vectorB, &error);
        QVERIFY2(e.has_value(), qPrintable(error));
        QCOMPARE(e->hostname, QStringLiteral("secure.vpn.example.com"));
        QCOMPARE(e->username, QStringLiteral("premium_user"));
        QCOMPARE(e->password, QStringLiteral("very_secret_password"));
        QCOMPARE(e->addresses, (QStringList { QStringLiteral("192.168.1.1:8443"), QStringLiteral("10.0.0.1:443") }));
        QCOMPARE(e->customSni, QStringLiteral("cdn.example.org"));
        QVERIFY(!e->hasIpv6);
        QVERIFY(e->antiDpi);
        QCOMPARE(e->upstreamProtocol, QStringLiteral("http3"));
        QCOMPARE(e->name, QStringLiteral("My VPN"));
        QCOMPARE(e->dnsUpstreams, (QStringList { QStringLiteral("1.1.1.1"), QStringLiteral("tls://dns.example.com") }));
    }

    void reencodesVectorsByteForByte()
    {
        for (const QString &link : { vectorA, vectorB }) {
            const auto e = tt::decodeDeepLink(link);
            QVERIFY(e.has_value());
            QCOMPARE(tt::encodeDeepLink(*e), link);
        }
    }

    void acceptsLegacyPrefixWithoutQuestionMark()
    {
        QString legacy = vectorA;
        legacy.replace(QStringLiteral("tt://?"), QStringLiteral("tt://"));
        const auto e = tt::decodeDeepLink(legacy);
        QVERIFY(e.has_value());
        QCOMPARE(e->hostname, QStringLiteral("vpn.example.com"));
    }

    void acceptsVersionTwoAndItsFields()
    {
        QByteArray p = basePayload(2);
        p.append('\x0E').append('\x14').append("https://sub.example/");
        p.append('\x0F').append('\x04').append("abcd");
        QString error;
        const auto e = tt::decodeDeepLink(linkFromPayload(p), &error);
        QVERIFY2(e.has_value(), qPrintable(error));
        QCOMPARE(e->version, 2);
        QCOMPARE(e->subscriptionUrl, QStringLiteral("https://sub.example/"));
        QCOMPARE(e->clientRandomAuthKey, QStringLiteral("abcd"));
    }

    void rejectsNewerVersion()
    {
        QString error;
        QVERIFY(!tt::decodeDeepLink(linkFromPayload(basePayload(3)), &error).has_value());
        QVERIFY(error.contains(QStringLiteral("version")));
    }

    void ignoresUnknownTags()
    {
        QByteArray p = basePayload(1);
        p.append('\x2A').append('\x02').append("zz");
        QVERIFY(tt::decodeDeepLink(linkFromPayload(p)).has_value());
    }

    void lastDuplicateWinsExceptAddresses()
    {
        QByteArray p = basePayload(1);
        p.append('\x01').append('\x01').append('H');
        p.append('\x02').append('\x05').append("b:443");
        const auto e = tt::decodeDeepLink(linkFromPayload(p));
        QVERIFY(e.has_value());
        QCOMPARE(e->hostname, QStringLiteral("H"));
        QCOMPARE(e->addresses, (QStringList { QStringLiteral("a:443"), QStringLiteral("b:443") }));
    }

    void rejectsMissingRequiredField()
    {
        QByteArray p;
        p.append('\x00').append('\x01').append('\x01');
        p.append('\x01').append('\x01').append('h');
        QString error;
        QVERIFY(!tt::decodeDeepLink(linkFromPayload(p), &error).has_value());
        QVERIFY(error.contains(QStringLiteral("missing")));
    }

    void rejectsTruncatedAndMalformed()
    {
        QByteArray p = basePayload(1);
        p.append('\x0C').append('\x10').append("short");
        QVERIFY(!tt::decodeDeepLink(linkFromPayload(p)).has_value());
        QVERIFY(!tt::decodeDeepLink(QStringLiteral("tt://?not base64!")).has_value());
        QVERIFY(!tt::decodeDeepLink(QStringLiteral("vless://x")).has_value());
    }

    void certificateRoundTripsThroughPem()
    {
        const QByteArray der = fakeDerChain();
        const QString pem = tt::derChainToPem(der);
        QCOMPARE(pem.count(QStringLiteral("-----BEGIN CERTIFICATE-----")), 2);
        QCOMPARE(tt::pemChainToDer(pem), der);

        QByteArray p = basePayload(1);
        p.append('\x08').append(char(0x40 | (der.size() >> 8))).append(char(der.size() & 0xFF)).append(der);
        const auto e = tt::decodeDeepLink(linkFromPayload(p));
        QVERIFY(e.has_value());
        QCOMPARE(tt::pemChainToDer(e->certificate), der);
        QCOMPARE(tt::encodeDeepLink(*e), linkFromPayload(p));
    }

    // ---- TOML ----

    void parsesEndpointExportToml()
    {
        // The endpoint's `-c <user> --format toml` shape: flat keys,
        // client_random_prefix, the PEM in a multi-line string.
        const QString pem = tt::derChainToPem(fakeDerChain());
        const QString toml = QStringLiteral(
                "# This file was automatically generated by endpoint and could be used in vpn client.\n\n"
                "# Endpoint host name\nhostname = \"vpn.example.com\"\n\n"
                "addresses = [\n    \"203.0.113.5:443\", # primary\n    '[2001:db8::1]:443',\n]\n"
                "custom_sni = \"\"\nhas_ipv6 = false\nusername = \"alice\"\npassword = \"p\\\"w\\\\d\"\n"
                "client_random_prefix = \"aabb/ff00\"\nskip_verification = false\n"
                "certificate = \"\"\"\n%1\"\"\"\nupstream_protocol = \"http3\"\nanti_dpi = true\n"
                "name = \"Office\"\ndns_upstreams = [\"tls://1.1.1.1\"]\n")
                                     .arg(pem);
        QString error;
        const auto e = tt::parseToml(toml, &error);
        QVERIFY2(e.has_value(), qPrintable(error));
        QCOMPARE(e->hostname, QStringLiteral("vpn.example.com"));
        QCOMPARE(e->addresses, (QStringList { QStringLiteral("203.0.113.5:443"), QStringLiteral("[2001:db8::1]:443") }));
        QVERIFY(!e->hasIpv6);
        QCOMPARE(e->password, QStringLiteral("p\"w\\d"));
        QCOMPARE(e->clientRandom, QStringLiteral("aabb/ff00"));
        QCOMPARE(e->certificate, pem);
        QCOMPARE(e->upstreamProtocol, QStringLiteral("http3"));
        QVERIFY(e->antiDpi);
        QCOMPARE(e->name, QStringLiteral("Office"));
        QVERIFY(tt::looksLikeEndpointToml(toml));
    }

    void parsesClientConfigToml()
    {
        const QString toml = QStringLiteral(
                "loglevel = \"info\"\nvpn_mode = \"general\"\nexclusions = []\n\n"
                "[endpoint]\nhostname = \"vpn.example.com\"\naddresses = [\"1.2.3.4:443\"]\n"
                "username = \"bob\"\npassword = 'literal\\no-escape'\nclient_random = \"\"\n"
                "upstream_protocol = \"http2\"\n\n"
                "[listener.tun]\nincluded_routes = [\"0.0.0.0/0\"]\nmtu_size = 1350\n");
        QString error;
        const auto e = tt::parseToml(toml, &error);
        QVERIFY2(e.has_value(), qPrintable(error));
        QCOMPARE(e->username, QStringLiteral("bob"));
        QCOMPARE(e->password, QStringLiteral("literal\\no-escape"));
        QVERIFY(e->hasIpv6);
    }

    void rejectsBadToml()
    {
        QVERIFY(!tt::parseToml(QStringLiteral("hostname = \"unterminated\n")).has_value());
        QVERIFY(!tt::parseToml(QStringLiteral("hostname = \"h\"\naddresses = [\"a:1\"]\nusername = \"u\"\n")).has_value());
        QVERIFY(!tt::parseToml(QStringLiteral("hostname = \"h\"\naddresses = [\"a:1\"]\nusername = \"u\"\n"
                                              "password = \"p\"\nupstream_protocol = \"http9\"\n"))
                         .has_value());
    }

    void clientTomlRoundTripsHostileStrings()
    {
        tt::Endpoint e;
        e.hostname = QStringLiteral("vpn.example.com");
        e.addresses = { QStringLiteral("1.2.3.4:443") };
        e.username = QStringLiteral("u\"ser");
        e.password = QStringLiteral("pa\\ss\nword\x01]\n[listener.tun]");
        e.certificate = tt::derChainToPem(fakeDerChain());
        e.upstreamProtocol = QStringLiteral("http3");
        e.dnsUpstreams = { QStringLiteral("tls://1.1.1.1") };

        const QString toml = tt::buildClientToml(e, { QStringLiteral("127.0.0.1:41000"), QStringLiteral("su"), QStringLiteral("sp") });
        const auto tables = tt::toml::parse(toml);
        QVERIFY(tables.has_value());
        QVERIFY(!tables->contains(QStringLiteral("listener.tun")));
        QCOMPARE(tables->value(QStringLiteral("listener.socks")).value(QStringLiteral("address")).toString(),
                 QStringLiteral("127.0.0.1:41000"));
        QCOMPARE(tables->value(QString()).value(QStringLiteral("killswitch_enabled")).toBool(true), false);

        const auto back = tt::parseToml(toml);
        QVERIFY(back.has_value());
        QCOMPARE(back->username, e.username);
        QCOMPARE(back->password, e.password);
        QCOMPARE(back->certificate, e.certificate);
        QCOMPARE(back->upstreamProtocol, e.upstreamProtocol);
    }

    // ---- model ----

    void protocolConfigAnswersEveryVisitor()
    {
        const auto e = tt::decodeDeepLink(vectorB);
        QVERIFY(e.has_value());
        ProtocolConfig pc { TrustTunnelProtocolConfig::fromEndpoint(*e) };
        QCOMPARE(pc.type(), Proto::TrustTunnel);
        QVERIFY(pc.hasClientConfig());
        QVERIFY(pc.isThirdPartyConfig());
        QCOMPARE(pc.port(), QStringLiteral("8443"));
        QCOMPARE(pc.transportProto(), QStringLiteral("udp"));
        QCOMPARE(pc.clientId(), QStringLiteral("premium_user"));
        QCOMPARE(pc.getClientConfigJson().value(QStringLiteral("hostname")).toString(),
                 QStringLiteral("secure.vpn.example.com"));
        pc.clearClientConfig();
        QVERIFY(!pc.hasClientConfig());
    }

    void containerConfigSurvivesStorage()
    {
        const auto e = tt::decodeDeepLink(vectorA);
        QVERIFY(e.has_value());
        ContainerConfig cc;
        cc.container = DockerContainer::TrustTunnel;
        cc.protocolConfig = ProtocolConfig { TrustTunnelProtocolConfig::fromEndpoint(*e) };

        const ContainerConfig back = ContainerConfig::fromJson(cc.toJson());
        QCOMPARE(back.container, DockerContainer::TrustTunnel);
        const TrustTunnelProtocolConfig *proto = back.getTrustTunnelProtocolConfig();
        QVERIFY(proto);
        QVERIFY(proto->hasClientConfig());
        QCOMPARE(tt::encodeDeepLink(proto->clientConfig->endpoint), vectorA);
    }

    // ---- import ----

    void importsDeepLinkAsConnectableServer()
    {
        SecureQSettings settings("AmneziaVPN-Test-" + QUuid::createUuid().toString(), "amnezia-client", nullptr, false);
        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);
        TestCoreController core(vpnConnection, &settings, nullptr);

        const auto result = core.m_importCoreController->extractConfigFromData(vectorB);
        QCOMPARE(result.errorCode, ErrorCode::NoError);
        QCOMPARE(result.config.value(QStringLiteral("description")).toString(), QStringLiteral("My VPN"));
        QCOMPARE(result.config.value(QStringLiteral("dns1")).toString(), QStringLiteral("1.1.1.1"));

        QSignalSpy finished(core.m_importCoreController, &ImportController::importFinished);
        core.m_importCoreController->importConfig(result.config);
        QCOMPARE(finished.count(), 1);

        const QString serverId = core.m_serversRepository->defaultServerId();
        QVERIFY(!serverId.isEmpty());
        QCOMPARE(core.m_serversController->getDefaultContainer(serverId), DockerContainer::TrustTunnel);
        const ContainerConfig cc = core.m_serversController->getContainerConfig(serverId, DockerContainer::TrustTunnel);
        QVERIFY(cc.protocolConfig.hasClientConfig());
        QCOMPARE(cc.protocolConfig.getClientConfigJson().value(QStringLiteral("username")).toString(),
                 QStringLiteral("premium_user"));

        settings.clearSettings();
    }

    void importsTomlAndQrText()
    {
        SecureQSettings settings("AmneziaVPN-Test-" + QUuid::createUuid().toString(), "amnezia-client", nullptr, false);
        auto vpnConnection = QSharedPointer<VpnConnection>::create(nullptr, nullptr);
        TestCoreController core(vpnConnection, &settings, nullptr);

        const QString toml = QStringLiteral("hostname = \"vpn.example.com\"\naddresses = [\"1.2.3.4:443\"]\n"
                                            "username = \"alice\"\npassword = \"secret\"\nupstream_protocol = \"http2\"\n");
        const auto fromToml = core.m_importCoreController->extractConfigFromData(toml);
        QCOMPARE(fromToml.errorCode, ErrorCode::NoError);

        const auto fromQr = core.m_importCoreController->extractConfigFromQr(vectorA.toUtf8());
        QCOMPARE(fromQr.errorCode, ErrorCode::NoError);
        QCOMPARE(fromQr.config.value(QStringLiteral("hostName")).toString(), QStringLiteral("vpn.example.com"));

        settings.clearSettings();
    }
};

QTEST_MAIN(TestTrustTunnel)
#include "testTrustTunnel.moc"

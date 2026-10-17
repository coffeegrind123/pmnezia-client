// Pure-model tests for QqDnsProtocolConfig behind the ProtocolConfig variant.
// ConnectionController::prepareConnection() refuses any VPN container whose
// protocolConfig.hasClientConfig() is false, and createConnectionConfiguration()
// hands getClientConfigJson() to the protocol; QQ-DNS must answer both.

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "core/models/containerConfig.h"
#include "core/models/protocolConfig.h"
#include "core/models/protocols/qqDnsProtocolConfig.h"
#include "core/protocols/protocolUtils.h"
#include "core/utils/constants/configKeys.h"

using namespace amnezia;

namespace
{

QqDnsProtocolConfig importedConfig()
{
    QqDnsProtocolConfig c;
    c.dnsIps = QJsonArray { QStringLiteral("1.1.1.1"), QStringLiteral("8.8.8.8:53") };
    c.sendDomains = QJsonArray { QStringLiteral("s.example.com") };
    c.recvDomains = QJsonArray { QStringLiteral("r.example.com") };
    c.receivePort = 5353;

    QJsonObject awgData;
    awgData[configKey::hostName] = QStringLiteral("203.0.113.7");
    awgData[configKey::port] = 51820;
    QJsonObject awg;
    awg[ProtocolUtils::key_proto_config_data(Proto::Awg)] = awgData;
    c.awg = awg;

    c.id = QStringLiteral("qq-client-1");
    c.isThirdPartyConfig = true;
    return c;
}

} // namespace

class TestQqDnsConfig : public QObject
{
    Q_OBJECT

private slots:
    void importedConfigCountsAsClientConfig()
    {
        const ProtocolConfig pc { importedConfig() };
        QCOMPARE(pc.type(), Proto::QqDns);
        QVERIFY(pc.hasClientConfig());
    }

    void blankConfigHasNoClientConfig()
    {
        const ProtocolConfig pc { QqDnsProtocolConfig {} };
        QVERIFY(!pc.hasClientConfig());
        QVERIFY(pc.getClientConfigJson().isEmpty());
    }

    void clientConfigJsonCarriesTheEngineBlob()
    {
        const QqDnsProtocolConfig src = importedConfig();
        const ProtocolConfig pc { src };

        const QJsonObject json = pc.getClientConfigJson();
        QCOMPARE(json.value(configKey::qqDnsIps).toArray(), src.dnsIps);
        QCOMPARE(json.value(configKey::qqSendDomains).toArray(), src.sendDomains);
        QCOMPARE(json.value(configKey::qqRecvDomains).toArray(), src.recvDomains);
        QCOMPARE(json.value(configKey::qqAwg).toObject(), src.awg);
    }

    void scalarAccessorsAnswer()
    {
        const ProtocolConfig pc { importedConfig() };
        QCOMPARE(pc.clientId(), QStringLiteral("qq-client-1"));
        QCOMPARE(pc.port(), QStringLiteral("5353"));
        QCOMPARE(pc.transportProto(), QStringLiteral("udp"));
    }

    void setClientConfigJsonReplacesEngineFields()
    {
        ProtocolConfig pc { QqDnsProtocolConfig {} };
        pc.setClientConfigJson(importedConfig().toJson());
        QVERIFY(pc.hasClientConfig());
        QCOMPARE(pc.getClientConfigJson().value(configKey::qqSendDomains).toArray(),
                 importedConfig().sendDomains);
    }

    void clearClientConfigDropsIt()
    {
        ProtocolConfig pc { importedConfig() };
        pc.clearClientConfig();
        QVERIFY(!pc.hasClientConfig());
    }

    void thirdPartyMarkerSurvivesStorage()
    {
        ContainerConfig cc;
        cc.container = DockerContainer::QqDns;
        cc.protocolConfig = ProtocolConfig { importedConfig() };

        const ContainerConfig back = ContainerConfig::fromJson(cc.toJson());
        QCOMPARE(back.container, DockerContainer::QqDns);
        QVERIFY(back.protocolConfig.isThirdPartyConfig());
        QVERIFY(back.protocolConfig.hasClientConfig());
        QCOMPARE(back.protocolConfig.clientId(), QStringLiteral("qq-client-1"));
    }
};

QTEST_MAIN(TestQqDnsConfig)
#include "testQqDnsConfig.moc"

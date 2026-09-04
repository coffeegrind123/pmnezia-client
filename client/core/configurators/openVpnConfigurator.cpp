#include "openVpnConfigurator.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>
#include <QTemporaryFile>
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    #include <QGuiApplication>
#else
    #include <QApplication>
#endif

#include "core/utils/errorCodes.h"
#include "core/utils/routeModes.h"
#include "core/utils/commonStructs.h"
#include "core/utils/networkUtilities.h"
#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/utils/utilities.h"
#include "core/models/protocols/openVpnProtocolConfig.h"

using namespace amnezia;



OpenVpnConfigurator::OpenVpnConfigurator(QObject *parent)
    : ConfiguratorBase(parent)
{
}

ProtocolConfig OpenVpnConfigurator::processConfigWithLocalSettings(const ConnectionSettings &settings,
                                                                   ProtocolConfig protocolConfig)
{
    applyDnsToNativeConfig(settings.dns, protocolConfig);

    QString config = protocolConfig.nativeConfig();

    {
        QRegularExpression regex("redirect-gateway.*");
        config.replace(regex, "");

        if (settings.dns.primaryDns.contains(protocols::dns::amneziaDnsIp)) {
            QRegularExpression dnsRegex("dhcp-option DNS " + settings.dns.secondaryDns);
            config.replace(dnsRegex, "");
        }

        if (!settings.splitTunneling.isSitesSplitTunnelingEnabled) {
            config.append("\nredirect-gateway def1 ipv6 bypass-dhcp\n");
            config.append("block-ipv6\n");
        } else if (settings.splitTunneling.routeMode == RouteMode::VpnOnlyForwardSites) {
            // no redirect-gateway
        } else if (settings.splitTunneling.routeMode == RouteMode::VpnAllExceptSites) {
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
            config.append("\nredirect-gateway ipv6 !ipv4 bypass-dhcp\n");
#endif
            config.append("block-ipv6\n");
        }
    }

#ifndef MZ_WINDOWS
    config.replace("block-outside-dns", "");
#endif

#if (defined(MZ_MACOS) || defined(MZ_LINUX))
    config.append(QString("\nscript-security 2\n"
                         "up %1/update-resolv-conf.sh\n"
                         "down %1/update-resolv-conf.sh\n")
                  .arg(qApp->applicationDirPath()));
#endif

    protocolConfig.setNativeConfig(config);
    return protocolConfig;
}

ProtocolConfig OpenVpnConfigurator::processConfigWithExportSettings(const ExportSettings &settings,
                                                                    ProtocolConfig protocolConfig)
{
    applyDnsToNativeConfig(settings.dns, protocolConfig);

    QString config = protocolConfig.nativeConfig();

    QRegularExpression regex("redirect-gateway.*");
    config.replace(regex, "");

    if (settings.dns.primaryDns.contains(protocols::dns::amneziaDnsIp)) {
        QRegularExpression dnsRegex("dhcp-option DNS " + settings.dns.secondaryDns);
        config.replace(dnsRegex, "");
    }

    config.append("\nredirect-gateway def1 ipv6 bypass-dhcp\n");
    config.append("block-ipv6\n");
    config.replace("block-outside-dns", "");

    protocolConfig.setNativeConfig(config);
    return protocolConfig;
}

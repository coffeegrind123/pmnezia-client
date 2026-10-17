// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TRUSTTUNNELCONFIGMODEL_H
#define TRUSTTUNNELCONFIGMODEL_H

#include <QAbstractListModel>

#include "core/models/protocols/trustTunnelProtocolConfig.h"
#include "core/utils/containerEnum.h"
#include "core/utils/protocolEnum.h"

// One row: the imported endpoint profile. Identity (hostname, credentials,
// pinned certificate) is read-only here, since it must match what the
// endpoint issued; transport knobs the client chooses are editable.
class TrustTunnelConfigModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        HostnameRole = Qt::UserRole + 1,
        AddressesRole,
        CustomSniRole,
        UsernameRole,
        UpstreamProtocolRole,
        AntiDpiRole,
        HasIpv6Role,
        SkipVerificationRole,
        HasPinnedCertificateRole,
        DnsUpstreamsRole,
        ClientRandomRole,
        ProfileNameRole
    };

    explicit TrustTunnelConfigModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // The current profile as a tt:// link, for sharing to another device.
    Q_INVOKABLE QString deepLink() const;

public slots:
    void updateModel(amnezia::DockerContainer container, const amnezia::TrustTunnelProtocolConfig &protocolConfig);
    amnezia::TrustTunnelProtocolConfig getProtocolConfig();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    amnezia::serialization::trusttunnel::Endpoint &endpoint();
    const amnezia::serialization::trusttunnel::Endpoint &endpoint() const;

    amnezia::DockerContainer m_container = amnezia::DockerContainer::None;
    amnezia::TrustTunnelProtocolConfig m_protocolConfig;
};

#endif // TRUSTTUNNELCONFIGMODEL_H

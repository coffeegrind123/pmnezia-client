// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef QQDNSCONFIGMODEL_H
#define QQDNSCONFIGMODEL_H

#include <QAbstractListModel>

#include "core/models/protocols/qqDnsProtocolConfig.h"
#include "core/utils/containerEnum.h"
#include "core/utils/protocolEnum.h"

class QqDnsConfigModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        DnsIpsRole,
        SendDomainsRole,
        RecvDomainsRole,
        ReceivePortRole,
        MaxDomainLenRole,
        MaxSubLenRole,
        RetriesRole
    };

    explicit QqDnsConfigModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void updateModel(amnezia::DockerContainer container,
                     const amnezia::QqDnsProtocolConfig &protocolConfig);
    amnezia::QqDnsProtocolConfig getProtocolConfig();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    amnezia::DockerContainer m_container = amnezia::DockerContainer::None;
    amnezia::QqDnsProtocolConfig m_protocolConfig;
};

#endif // QQDNSCONFIGMODEL_H

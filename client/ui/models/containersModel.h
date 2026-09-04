#ifndef CONTAINERS_MODEL_H
#define CONTAINERS_MODEL_H

#include <QAbstractListModel>
#include <QJsonObject>

#include "core/utils/containerEnum.h"
#include "core/utils/containers/containerUtils.h"
#include "core/utils/protocolEnum.h"
#include "core/models/containerConfig.h"

class ContainersModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ContainersModel(QObject *parent = nullptr);

    enum Roles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        DetailedDescriptionRole,
        ServiceTypeRole,
        ConfigRole,
        IsThirdPartyConfigRole,
        DockerContainerRole,
        ContainerStringRole,

        IsEasySetupContainerRole,
        EasySetupOrderRole,

        IsInstalledRole,
        IsCurrentlyProcessedRole,
        IsSupportedRole,

        InstallPageOrderRole,

        IsVpnContainerRole,
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant data(const int index, int role) const;

public slots:
    void updateModel(const QMap<amnezia::DockerContainer, amnezia::ContainerConfig> &containers);

    void setProcessedContainerIndex(int containerIndex);

    QString getProcessedContainerName();

    QJsonObject getContainerConfig(const int containerIndex);

    bool isSupportedByCurrentPlatform(const int containerIndex);

    bool hasInstalledProtocols();

protected:
    QHash<int, QByteArray> roleNames() const override;

signals:
    void containersModelUpdated();

private:
    QMap<amnezia::DockerContainer, amnezia::ContainerConfig> m_containers;

    int m_processedContainerIndex = -1;
};

#endif // CONTAINERS_MODEL_H

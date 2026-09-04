#ifndef SERVERSUICONTROLLER_H
#define SERVERSUICONTROLLER_H

#include <QObject>

#include <QSet>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

#include "core/controllers/serversController.h"
#include "core/models/serverDescription.h"
#include "core/controllers/settingsController.h"
#include "ui/models/serversModel.h"
#include "ui/models/containersModel.h"
#include "ui/models/protocolsModel.h"
#include "ui/models/protocols/awgConfigModel.h"
#include "ui/models/protocols/wireguardConfigModel.h"
#include "ui/models/protocols/xrayConfigModel.h"
#include "ui/models/protocols/masterDnsVpnConfigModel.h"
#include "ui/models/protocols/qqDnsConfigModel.h"

#include <QRegularExpression>

class ServersUiController : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString defaultServerId READ getDefaultServerId NOTIFY defaultServerIdChanged)

    Q_PROPERTY(QString defaultServerName READ getDefaultServerName NOTIFY defaultServerIdChanged)
    Q_PROPERTY(QString defaultServerDefaultContainerName READ getDefaultServerDefaultContainerName NOTIFY defaultServerIdChanged)
    Q_PROPERTY(QString defaultServerDescriptionCollapsed READ getDefaultServerDescriptionCollapsed NOTIFY defaultServerIdChanged)
    Q_PROPERTY(QString defaultServerDescriptionExpanded READ getDefaultServerDescriptionExpanded NOTIFY defaultServerIdChanged)
    Q_PROPERTY(bool isDefaultServerDefaultContainerHasSplitTunneling READ isDefaultServerDefaultContainerHasSplitTunneling NOTIFY defaultServerIdChanged)
    
    Q_PROPERTY(QString processedServerId READ getProcessedServerId WRITE setProcessedServerId NOTIFY processedServerIdChanged)
    Q_PROPERTY(int processedContainerIndex READ getProcessedContainerIndex WRITE setProcessedContainerIndex NOTIFY processedContainerIndexChanged)
    
public:
    explicit ServersUiController(ServersController* serversController,
                                 SettingsController* settingsController,
                                 ServersModel* serversModel,
                                 ContainersModel* containersModel,
                                 ContainersModel* defaultServerContainersModel,
                                 ProtocolsModel* protocolsModel,
                                 AwgConfigModel* awgConfigModel,
                                 WireGuardConfigModel* wireGuardConfigModel,
                                 XrayConfigModel* xrayConfigModel,
                                 MasterDnsVpnConfigModel* masterDnsVpnConfigModel,
                                 QqDnsConfigModel* qqDnsConfigModel,
                                 QObject *parent = nullptr);

public slots:
    void removeServer(const QString &serverId);

    // Reloads ProtocolsModel from the stored container config.
    Q_INVOKABLE void updateProtocols(const QString &serverId, int containerIndex);
    // Loads the per-protocol config model so a client-settings page can bind to it.
    void openClientSettings(const QString &serverId, int containerIndex, int protocolIndex);
    // Saves edits made on a client-settings page back into the stored config.
    void updateClientConfig(const QString &serverId, int containerIndex, int protocolIndex, bool closePage = true);

    QRegularExpression ipAddressRegExp();
    void removeServerAtIndex(int index);

    void editServerName(const QString &serverId, const QString &name);

    void setDefaultServer(const QString &serverId);
    void setDefaultServerAtIndex(int index);

    void setDefaultContainer(const QString &serverId, int containerIndex);
    void setDefaultContainerAtIndex(int index, int containerIndex);

    void onDefaultServerChanged(const QString &defaultServerId);
    
    // Getters for properties
    QString getDefaultServerId() const;
    QString getDefaultServerName() const;
    QString getDefaultServerDefaultContainerName() const;
    QString getDefaultServerDescriptionCollapsed() const;
    QString getDefaultServerDescriptionExpanded() const;
    bool isDefaultServerDefaultContainerHasSplitTunneling() const;


    QString serverName(const QString &serverId) const;
    QString serverHostName(const QString &serverId) const;
    int serverDefaultContainer(const QString &serverId) const;
    bool serverHasInstalledContainers(const QString &serverId) const;
    
    QString getProcessedServerId() const;
    void setProcessedServerId(const QString &serverId);

    int getProcessedContainerIndex() const;
    void setProcessedContainerIndex(int index);
    
    bool isDefaultServerCurrentlyProcessed() const;
    
    QString getServerId(int index) const;
    int getServerIndexById(const QString &serverId) const;
    int getServersCount() const;

signals:
    void updateContainerFinished(const QString &message, bool closePage);
    void updateContainerErrorOccurred(ErrorCode errorCode);
    void removeServerFinished(const QString &finishedMessage);

    void errorOccurred(const QString &errorMessage);
    void finished(const QString &message);
    void defaultServerIdChanged(const QString &serverId);
    void processedServerIdChanged(const QString &serverId);
    void processedContainerIndexChanged(int index);

public:
    void updateModel();
    
private:
    const ServerDescription &serverDescriptionById(const QString &serverId) const;
    const ServerDescription &processedServerDescription() const;
    int serverIndexForId(const QString &serverId) const;

    void updateContainersModel();
    void updateDefaultServerContainersModel();

    bool buildContainerConfigFromModel(int containerIndex, int protocolIndex, ContainerConfig &containerConfig);
    void updateProtocolConfigModel(const QString &serverId, int containerIndex, int protocolIndex);

    ServersController* m_serversController;
    SettingsController* m_settingsController;
    ServersModel* m_serversModel;
    ContainersModel* m_containersModel;
    ContainersModel* m_defaultServerContainersModel;
    ProtocolsModel* m_protocolsModel;
    AwgConfigModel* m_awgConfigModel;
    WireGuardConfigModel* m_wireGuardConfigModel;
    XrayConfigModel* m_xrayConfigModel;
    MasterDnsVpnConfigModel* m_masterDnsVpnConfigModel;
    QqDnsConfigModel* m_qqDnsConfigModel;

    QVector<amnezia::ServerDescription> m_orderedServerDescriptions;
    
    QString m_processedServerId;
    int m_processedContainerIndex = -1;
};

#endif // SERVERSUICONTROLLER_H


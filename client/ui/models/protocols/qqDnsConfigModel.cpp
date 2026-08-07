// SPDX-License-Identifier: GPL-3.0-or-later

#include "qqDnsConfigModel.h"

#include <QJsonArray>
#include <QRegularExpression>

using namespace amnezia;

namespace {
// Normalise QML input (QStringList, array literal, or comma/newline string)
// into a QJsonArray of trimmed non-empty strings.
QJsonArray toJsonArray(const QVariant &value)
{
    QJsonArray arr;
    if (value.canConvert<QStringList>() && value.typeId() != QMetaType::QString) {
        for (const QString &s : value.toStringList()) {
            const QString t = s.trimmed();
            if (!t.isEmpty()) {
                arr.append(t);
            }
        }
    } else {
        const QString raw = value.toString();
        for (const QString &part : raw.split(QRegularExpression(QStringLiteral("[,\\n ]")))) {
            const QString t = part.trimmed();
            if (!t.isEmpty()) {
                arr.append(t);
            }
        }
    }
    return arr;
}

QStringList fromJsonArray(const QJsonArray &arr)
{
    QStringList out;
    for (const QJsonValue &v : arr) {
        if (v.isString()) {
            out.append(v.toString());
        }
    }
    return out;
}
} // namespace

QqDnsConfigModel::QqDnsConfigModel(QObject *parent) : QAbstractListModel(parent) {}

int QqDnsConfigModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

bool QqDnsConfigModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }
    switch (role) {
    case Roles::DnsIpsRole:
        m_protocolConfig.dnsIps = toJsonArray(value);
        break;
    case Roles::SendDomainsRole:
        m_protocolConfig.sendDomains = toJsonArray(value);
        break;
    case Roles::RecvDomainsRole:
        m_protocolConfig.recvDomains = toJsonArray(value);
        break;
    case Roles::ReceivePortRole:
        m_protocolConfig.receivePort = value.toInt();
        break;
    case Roles::MaxDomainLenRole:
        m_protocolConfig.maxDomainLen = value.toInt();
        break;
    case Roles::MaxSubLenRole:
        m_protocolConfig.maxSubLen = value.toInt();
        break;
    case Roles::RetriesRole:
        m_protocolConfig.retries = value.toInt();
        break;
    default:
        return false;
    }
    emit dataChanged(index, index, QList { role });
    return true;
}

QVariant QqDnsConfigModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }
    switch (role) {
    case Roles::DnsIpsRole:
        return fromJsonArray(m_protocolConfig.dnsIps);
    case Roles::SendDomainsRole:
        return fromJsonArray(m_protocolConfig.sendDomains);
    case Roles::RecvDomainsRole:
        return fromJsonArray(m_protocolConfig.recvDomains);
    case Roles::ReceivePortRole:
        return m_protocolConfig.receivePort;
    case Roles::MaxDomainLenRole:
        return m_protocolConfig.maxDomainLen;
    case Roles::MaxSubLenRole:
        return m_protocolConfig.maxSubLen;
    case Roles::RetriesRole:
        return m_protocolConfig.retries;
    }
    return QVariant();
}

void QqDnsConfigModel::updateModel(amnezia::DockerContainer container,
                                   const amnezia::QqDnsProtocolConfig &protocolConfig)
{
    beginResetModel();
    m_container = container;
    m_protocolConfig = protocolConfig;
    endResetModel();
}

amnezia::QqDnsProtocolConfig QqDnsConfigModel::getProtocolConfig()
{
    return m_protocolConfig;
}

QHash<int, QByteArray> QqDnsConfigModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DnsIpsRole] = "dnsIps";
    roles[SendDomainsRole] = "sendDomains";
    roles[RecvDomainsRole] = "recvDomains";
    roles[ReceivePortRole] = "receivePort";
    roles[MaxDomainLenRole] = "maxDomainLen";
    roles[MaxSubLenRole] = "maxSubLen";
    roles[RetriesRole] = "retries";
    return roles;
}

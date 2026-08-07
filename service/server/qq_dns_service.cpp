// SPDX-License-Identifier: GPL-3.0-or-later

#include "qq_dns_service.h"

#include "engine.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

QqDnsService &QqDnsService::getInstance()
{
    static QqDnsService instance;
    return instance;
}

QqDnsService::QqDnsService() = default;
QqDnsService::~QqDnsService() = default;

bool QqDnsService::start(const QString &configJson)
{
    qDebug() << "QqDnsService::start";

    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "QqDnsService::start: invalid JSON:" << err.errorString();
        return false;
    }

    if (m_engine) {
        m_engine->stop();
        m_engine.reset();
    }

    m_engine = std::make_unique<amnezia::qqdns::Engine>();
    if (!m_engine->start(doc.object())) {
        qWarning() << "QqDnsService::start: engine start failed:" << m_engine->lastError();
        m_engine.reset();
        return false;
    }
    return true;
}

bool QqDnsService::stop()
{
    qDebug() << "QqDnsService::stop";
    if (!m_engine) {
        return true;
    }
    m_engine->stop();
    m_engine.reset();
    return true;
}

quint16 QqDnsService::localUdpPort() const
{
    return m_engine ? m_engine->localUdpPort() : 0;
}

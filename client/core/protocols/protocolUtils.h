#ifndef PROTOCOLUTILS_H
#define PROTOCOLUTILS_H

#include <QList>
#include <QMap>
#include <QString>
#include <QJsonObject>

#include "core/utils/protocolEnum.h"
#include "core/utils/constants/configKeys.h"
#include "core/utils/constants/protocolConstants.h"

namespace amnezia
{
    namespace ProtocolUtils
    {
        QString transportProtoToString(TransportProto proto, Proto p = Proto::Unknown);

        Proto protoFromString(QString p);
        QString protoToString(Proto p);

        QMap<Proto, QString> protocolHumanNames();

        ServiceType protocolService(Proto p);

        TransportProto defaultTransportProto(Proto p);

        QString key_proto_config_data(Proto p);
        QString key_proto_config_path(Proto p);

    }
}

#endif // PROTOCOLUTILS_H



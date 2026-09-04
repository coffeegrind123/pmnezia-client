#include "errorStrings.h"

using namespace amnezia;

QString errorString(ErrorCode code) {
    QString errorMessage;

    switch (code) {

    // General error codes
    case(ErrorCode::NoError): errorMessage = QObject::tr("No error"); break;
    case(ErrorCode::UnknownError): errorMessage = QObject::tr("Unknown error"); break;
    case(ErrorCode::NotImplementedError): errorMessage = QObject::tr("Function not implemented"); break;
    case(ErrorCode::AmneziaServiceNotRunning): errorMessage = QObject::tr("Background service is not running"); break;
    case(ErrorCode::NotSupportedOnThisPlatform): errorMessage = QObject::tr("The selected protocol is not supported on the current platform"); break;

    // Distro errors
    case (ErrorCode::AmneziaServiceConnectionFailed): errorMessage = QObject::tr("Amnezia helper service error"); break;
    case (ErrorCode::OpenSslFailed): errorMessage = QObject::tr("OpenSSL failed"); break;

    case (ErrorCode::ImportInvalidConfigError): errorMessage = QObject::tr("The config does not contain any containers and credentials for connecting to the server"); break;
    case (ErrorCode::ImportBackupFileUseRestoreInstead): errorMessage = QObject::tr("Backup files cannot be imported here. Use 'Restore from backup' instead."); break;
    case (ErrorCode::RestoreBackupInvalidError): errorMessage = QObject::tr("Backup file is corrupted or has invalid format"); break;
    case (ErrorCode::ImportOpenConfigError): errorMessage = QObject::tr("Unable to open config file"); break;
    case (ErrorCode::NoInstalledContainersError): errorMessage = QObject::tr("VPN Protocols is not installed.\n Please install VPN container at first"); break;

    // Android errors
    case (ErrorCode::AndroidError): errorMessage = QObject::tr("VPN connection error"); break;

    // QFile errors
    case(ErrorCode::OpenError): errorMessage = QObject::tr("QFile error: The file could not be opened"); break;
    case(ErrorCode::ReadError): errorMessage = QObject::tr("QFile error: An error occurred when reading from the file"); break;
    case(ErrorCode::PermissionsError): errorMessage = QObject::tr("QFile error: The file could not be accessed"); break;
    case(ErrorCode::UnspecifiedError): errorMessage =  QObject::tr("QFile error: An unspecified error occurred"); break;
    case(ErrorCode::FatalError): errorMessage =  QObject::tr("QFile error: A fatal error occurred"); break;
    case(ErrorCode::AbortError): errorMessage =  QObject::tr("QFile error: The operation was aborted"); break;

    case(ErrorCode::InternalError):
    default:
        errorMessage = QObject::tr("Internal error"); break;
    }

    return QObject::tr("ErrorCode: %1. ").arg(code) + errorMessage;
}

QDebug operator<<(QDebug debug, const ErrorCode &e)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "ErrorCode::" << int(e) << "(" << errorString(e) << ")";

    return debug;
}

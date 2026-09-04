#ifndef ERRORCODES_H
#define ERRORCODES_H

#include <QMetaEnum>
#include <QObject>

namespace amnezia
{
    namespace error_code_ns
    {
      Q_NAMESPACE
      // TODO: change to enum class
      enum ErrorCode {
        // General error codes
        NoError = 0,
        UnknownError = 100,
        InternalError = 101,
        NotImplementedError = 102,
        AmneziaServiceNotRunning = 103,
        NotSupportedOnThisPlatform = 104,

        // Distro errors
        AmneziaServiceConnectionFailed = 603,
        ExecutableMissing = 604,
        XrayExecutableMissing = 605,
        Tun2SockExecutableMissing = 606,

        // 3rd party utils errors
        OpenSslFailed = 800,
        XrayExecutableCrashed = 803,
        Tun2SockExecutableCrashed = 804,

        // import and install errors
        ImportInvalidConfigError = 900,
        ImportOpenConfigError = 901,
        NoInstalledContainersError = 902,
        ImportBackupFileUseRestoreInstead = 903,
        RestoreBackupInvalidError = 904,

        // Android errors
        AndroidError = 1000,

        // QFile errors
        OpenError = 1200,
        ReadError = 1201,
        PermissionsError = 1202,
        UnspecifiedError = 1203,
        FatalError = 1204,
        AbortError = 1205,

      };
      Q_ENUM_NS(ErrorCode)
    }

    using ErrorCode = error_code_ns::ErrorCode;
}

Q_DECLARE_METATYPE(amnezia::ErrorCode)

#endif // ERRORCODES_H

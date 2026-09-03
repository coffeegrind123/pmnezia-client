#include "updateController.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>

#include "amneziaApplication.h"
#include "logger.h"
#include "core/utils/appUiConfig.h"
#include "core/utils/selfhosted/scriptsRegistry.h"

#if defined(Q_OS_ANDROID)
    #include "platforms/android/android_controller.h"
#endif

namespace
{
    Logger logger("UpdateController");

#if defined(Q_OS_WINDOWS)
    const QLatin1String kInstallerRemoteFileNamePattern("AmneziaVPN_%1_windows_x64.exe");
    const QString kInstallerLocalPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN_installer.exe";
#elif defined(Q_OS_MACOS) && !defined(MACOS_NE)
    const QLatin1String kInstallerRemoteFileNamePattern("AmneziaVPN_%1_macos_x64.pkg");
    const QString kInstallerLocalPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.pkg";
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    const QLatin1String kInstallerRemoteFileNamePattern("AmneziaVPN_%1_linux_x64.run");
    const QString kInstallerLocalPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/AmneziaVPN.run";
#endif

}

UpdateController::UpdateController(SecureAppSettingsRepository* appSettingsRepository, QObject *parent)
    : QObject(parent), m_appSettingsRepository(appSettingsRepository)
{
}

QString UpdateController::getVersion() const
{
    return m_version;
}

QString UpdateController::getReleaseDate() const
{
    return m_releaseDate;
}

QString UpdateController::getDescription() const
{
    return m_description;
}

QStringList UpdateController::getTags() const
{
    return m_tags;
}

QStringList UpdateController::getNewFeatures() const
{
    return m_newFeatures;
}

QStringList UpdateController::getImprovements() const
{
    return m_improvements;
}

QStringList UpdateController::getBugFixes() const
{
    return m_bugFixes;
}

UpdateState::State UpdateController::getUpdateState() const
{
    return m_updateState;
}

bool UpdateController::isStoreUpdate() const
{
#if defined(Q_OS_IOS) || defined(MACOS_NE)
    return true;
#elif defined(Q_OS_ANDROID)
    return AndroidController::instance()->isPlay();
#else
    return false;
#endif
}

void UpdateController::setUpdateState(UpdateState::State state)
{
    if (m_updateState == state) {
        return;
    }
    m_updateState = state;
    emit updateStateChanged();
}

bool UpdateController::isUpdateCheckRunning() const
{
    return m_updateCheckRunning;
}

void UpdateController::setUpdateCheckRunning(bool running)
{
    if (m_updateCheckRunning == running) {
        return;
    }
    m_updateCheckRunning = running;
    emit updateCheckRunningChanged();
}

void UpdateController::checkForUpdates()
{
    // Auto-update discovery via the Amnezia gateway has been removed: the
    // v1/app_update request carried the installation uuid, os/app version and
    // distribution tag. This fork ships through GitHub releases, so the in-app
    // update check reports "up to date" without contacting any endpoint.
    setUpdateCheckRunning(false);
    emit updateNotFound();
}

void UpdateController::openStorePage() const
{
#if defined(Q_OS_IOS)
    QDesktopServices::openUrl(QUrl(QLatin1String(APP_IOS_STORE_URL_FALLBACK)));
#elif defined(MACOS_NE)
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://apps.apple.com/app/id1600529900")));
#elif defined(Q_OS_ANDROID)
    QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://play.google.com/store/apps/details?id=%1").arg(QLatin1String(APP_ANDROID_PACKAGE))));
#endif
}

void UpdateController::startUpdate()
{
    if (isStoreUpdate()) {
        openStorePage();
        return;
    }

#if defined(Q_OS_ANDROID)
    // GitHub build on Android: open the release page in a browser, the system downloads the APK.
    if (m_releasePageUrl.isEmpty()) {
        logger.error() << "Release page URL is empty";
        setUpdateState(UpdateState::State::DownloadError);
        return;
    }
    QDesktopServices::openUrl(QUrl(m_releasePageUrl));
#elif !defined(Q_OS_IOS) && !defined(MACOS_NE)
    downloadInstaller();
#endif
}

QString UpdateController::composeDownloadUrl() const
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
    if (m_downloadBaseUrl.isEmpty() || m_version.isEmpty()) {
        return QString();
    }
    const QString fileName = QString(kInstallerRemoteFileNamePattern).arg(m_version);
    return m_downloadBaseUrl + "/" + fileName;
#else
    return QString();
#endif
}

void UpdateController::downloadInstaller()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
    if (m_updateState == UpdateState::State::Downloading) {
        return;
    }

    const QString downloadUrl = composeDownloadUrl();
    if (downloadUrl.isEmpty()) {
        logger.error() << "Download URL is empty";
        setUpdateState(UpdateState::State::DownloadError);
        return;
    }

    setUpdateState(UpdateState::State::Downloading);

    QNetworkRequest request;
    request.setTransferTimeout(30000);
    request.setUrl(QUrl(downloadUrl));

    QNetworkReply *reply = amnApp->networkManager()->get(request);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            logger.error() << "Installer download failed, network error:" << static_cast<int>(reply->error())
                           << reply->errorString();
            logger.error() << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            setUpdateState(UpdateState::State::DownloadError);
            return;
        }

        QFile file(kInstallerLocalPath);
        if (!file.open(QIODevice::WriteOnly)) {
            logger.error() << "Failed to open installer file for writing:" << kInstallerLocalPath
                           << "Error:" << file.errorString();
            setUpdateState(UpdateState::State::DownloadError);
            return;
        }

        if (file.write(reply->readAll()) == -1) {
            logger.error() << "Failed to write installer data to file:" << kInstallerLocalPath
                           << "Error:" << file.errorString();
            file.close();
            setUpdateState(UpdateState::State::DownloadError);
            return;
        }

        file.close();
        setUpdateState(UpdateState::State::ReadyToInstall);
    });
#endif
}

void UpdateController::installUpdate()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS) && !defined(MACOS_NE)
    if (m_updateState != UpdateState::State::ReadyToInstall) {
        logger.error() << "Installer is not downloaded yet";
        return;
    }

    #if defined(Q_OS_WINDOWS)
    runWindowsInstaller(kInstallerLocalPath);
    #elif defined(Q_OS_MACOS) && !defined(MACOS_NE)
    runMacInstaller(kInstallerLocalPath);
    #elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    runLinuxInstaller(kInstallerLocalPath);
    #endif
#endif
}

#if defined(Q_OS_WINDOWS)
int UpdateController::runWindowsInstaller(const QString &installerPath)
{
    qint64 pid;
    bool success = QProcess::startDetached(installerPath, QStringList(), QString(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_MACOS) && !defined(MACOS_NE)
int UpdateController::runMacInstaller(const QString &installerPath)
{
    // Create temporary directory for extraction
    QTemporaryDir extractDir;
    extractDir.setAutoRemove(false);
    if (!extractDir.isValid()) {
        logger.error() << "Failed to create temporary directory";
        return -1;
    }
    logger.info() << "Temporary directory created:" << extractDir.path();

    // Create script file in the temporary directory
    QString scriptPath = extractDir.path() + "/mac_installer.sh";
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly)) {
        logger.error() << "Failed to create script file";
        return -1;
    }

    // Get script content from registry
    QString scriptContent = amnezia::scriptData(amnezia::ClientScriptType::mac_installer);
    if (scriptContent.isEmpty()) {
        logger.error() << "macOS installer script content is empty";
        scriptFile.close();
        return -1;
    }

    scriptFile.write(scriptContent.toUtf8());
    scriptFile.close();
    logger.info() << "Script file created:" << scriptPath;

    // Make script executable
    QFile::setPermissions(scriptPath, QFile::permissions(scriptPath) | QFile::ExeUser);

    // Start detached process
    qint64 pid;
    bool success =
            QProcess::startDetached("/bin/bash", QStringList() << scriptPath << extractDir.path() << installerPath, extractDir.path(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
int UpdateController::runLinuxInstaller(const QString &installerPath)
{
    QFile::setPermissions(installerPath, QFile::permissions(installerPath) | QFile::ExeUser);

    qint64 pid;
    bool success = QProcess::startDetached(installerPath, QStringList(), QString(), &pid);

    if (success) {
        logger.info() << "Installation process started with PID:" << pid;
    } else {
        logger.error() << "Failed to start installation process";
        return -1;
    }

    return 0;
}
#endif

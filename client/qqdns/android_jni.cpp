// SPDX-License-Identifier: GPL-3.0-or-later
//
// JNI bridge for the Android VpnService → native QQ-DNS engine path.
//
// Compiled into the main Qt-for-Android shared library, so the Java loader
// resolves Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_* via the same .so
// the activity already pulls in (no extra loadLibrary).
//
// Android differs from desktop: there is no privileged service daemon — the
// VpnService runs in the same process as the Qt SO, and the engine runs
// in-process inside it. Where desktop composes the engine under the `Awg`
// protocol via IPC, the Android VpnService (QqDnsService.kt) must instead run
// the WireGuard tunnel itself, pointed at the loopback UDP port this bridge
// exposes via nativeLocalUdpPort(), and VpnService.protect() the engine's
// resolver sockets so they don't loop through the tun. That handoff is the
// device-tested Kotlin work; this bridge only exposes the engine.

#include "engine.h"

#include <jni.h>

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>
#include <mutex>

namespace {

// One Engine per JVM process — Android UX is "one tunnel at a time".
std::mutex g_engineMutex;
std::unique_ptr<amnezia::qqdns::Engine> g_engine;

QString jstringToQString(JNIEnv *env, jstring s)
{
    if (!s) {
        return {};
    }
    const char *raw = env->GetStringUTFChars(s, nullptr);
    QString out = QString::fromUtf8(raw);
    env->ReleaseStringUTFChars(s, raw);
    return out;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeStart(JNIEnv *env, jclass /*clazz*/,
                                                            jstring configJson)
{
    const QString json = jstringToQString(env, configJson);
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "qqdns JNI: bad config JSON:" << err.errorString();
        return JNI_FALSE;
    }

    std::lock_guard<std::mutex> lock(g_engineMutex);
    if (g_engine) {
        g_engine->stop();
        g_engine.reset();
    }
    g_engine = std::make_unique<amnezia::qqdns::Engine>();
    if (!g_engine->start(doc.object())) {
        qWarning() << "qqdns JNI: engine start failed:" << g_engine->lastError();
        g_engine.reset();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeStop(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    if (g_engine) {
        g_engine->stop();
        g_engine.reset();
    }
}

// The loopback UDP port AmneziaWG (run by the VpnService) points its endpoint
// at. 0 until the engine is Connected.
JNIEXPORT jint JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeLocalUdpPort(JNIEnv * /*env*/,
                                                                  jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    return g_engine ? static_cast<jint>(g_engine->localUdpPort()) : 0;
}

// Mirrors Engine::State integer values; Kotlin treats them as a plain ordinal.
JNIEXPORT jint JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeState(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    return g_engine ? static_cast<jint>(g_engine->state())
                    : static_cast<jint>(amnezia::qqdns::Engine::State::Idle);
}

JNIEXPORT jstring JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeLastError(JNIEnv *env, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    if (!g_engine) {
        return env->NewStringUTF("");
    }
    return env->NewStringUTF(g_engine->lastError().toUtf8().constData());
}

JNIEXPORT jlong JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeBytesReceived(JNIEnv * /*env*/,
                                                                   jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    return g_engine ? static_cast<jlong>(g_engine->bytesReceived()) : 0;
}

JNIEXPORT jlong JNICALL
Java_org_amnezia_vpn_protocol_qqdns_QqDnsNative_nativeBytesSent(JNIEnv * /*env*/, jclass /*clazz*/)
{
    std::lock_guard<std::mutex> lock(g_engineMutex);
    return g_engine ? static_cast<jlong>(g_engine->bytesSent()) : 0;
}

} // extern "C"

package org.amnezia.vpn.protocol.qqdns

/**
 * JNI bridge to the native QQ-DNS engine.
 *
 * The native implementation lives in client/qqdns/android_jni.cpp and is
 * compiled into the main Qt-for-Android shared library the activity already
 * loads — no separate System.loadLibrary call needed here.
 *
 * All methods serialise on a process-global mutex on the C++ side; safe to
 * call from any Kotlin context.
 */
internal object QqDnsNative {

    /** Engine::State enum, mirrored from C++. */
    const val STATE_IDLE = 0
    const val STATE_STARTING = 1
    const val STATE_CONNECTED = 2
    const val STATE_STOPPING = 3
    const val STATE_FAILED = 4

    /**
     * Spin up the native engine with the qqdns wrapper JSON. Returns true on a
     * successful synchronous start (the loopback UDP listener binds shortly
     * after); false when the config fails validation.
     */
    @JvmStatic
    external fun nativeStart(configJson: String): Boolean

    /** Tear down the engine. Idempotent. */
    @JvmStatic
    external fun nativeStop()

    /**
     * The loopback UDP port AmneziaWG points its endpoint at (0 until bound).
     * Caller polls this after nativeStart() returns true.
     */
    @JvmStatic
    external fun nativeLocalUdpPort(): Int

    /** Engine state ordinal — see STATE_* constants above. */
    @JvmStatic
    external fun nativeState(): Int

    /** Human-readable last error from the engine (empty string when none). */
    @JvmStatic
    external fun nativeLastError(): String

    @JvmStatic
    external fun nativeBytesReceived(): Long

    @JvmStatic
    external fun nativeBytesSent(): Long
}

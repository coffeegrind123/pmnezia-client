# libMasterDnsVpn

Thin Gradle-only subproject that wraps `libmasterdnsvpn.aar` — the Android
Archive produced by `gomobile bind` over the bundled MasterDnsVPN client core.

## What ships in the AAR

The AAR exposes a single Kotlin/Java-callable façade:

```
package org.amnezia.vpn.protocol.masterdnsvpn.libMasterDnsVpn

class LibMasterDnsVpn {
    /**
     * Start the bundled MasterDnsVPN client core. Reads the operator's
     * client_config.toml from `configPath`, opens a SOCKS5 listener on the
     * `LISTEN_PORT` declared in that file (default 18000, bound to 127.0.0.1),
     * and runs MTU discovery + ARQ session setup against the resolvers in
     * `RESOLVERS = [...]`.
     *
     * Returns null on success, or a non-empty error string describing why the
     * core failed to start. The function blocks only long enough to spin up the
     * background goroutines — the SOCKS5 listener is bound asynchronously and
     * the caller must poll its bind via TCP connect (see MasterDnsVpn.kt).
     */
    static String startClient(String configPath);

    /**
     * Tear down the running core. Returns null on success, or a non-empty
     * error string. Safe to call when no core is running (returns null).
     */
    static String stopClient();

    /**
     * True when the core is alive and listening. Mainly diagnostic — the
     * Kotlin side already polls the SOCKS5 listener.
     */
    static boolean isRunning();
}
```

## Source of the AAR

The wrapper Go source lives at `gomobile/mobile.go` in this directory.
Building the .aar requires the gomobile toolchain:

```sh
cd gomobile
go install golang.org/x/mobile/cmd/gomobile@latest
gomobile init
gomobile bind \
    -target=android/arm64,arm,amd64,386 \
    -androidapi 21 \
    -trimpath \
    -ldflags='-s -w' \
    -o ../libmasterdnsvpn.aar \
    .
```

The conan recipe `amnezia-mdnsvpn-android/<ver>` automates these steps in CI
and drops the resulting AAR at this directory's root.

## Why the AAR is not committed

`libmasterdnsvpn.aar` is a build-output artifact ~3-4 MB per architecture.
Following the same convention as `client/android/xray/libXray/libxray.aar`
(provisioned by conan via `amnezia-libxray/1.0.0`), the binary is materialised
out of the conan cache at build time rather than being kept in source control.

A fresh checkout that hasn't run conan / gomobile will see this directory
without `libmasterdnsvpn.aar`. The Gradle build will then error with a
"missing artifact" message that points back to the conan recipe — explicit
failure rather than silent miscompilation.

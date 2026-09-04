# PmneziaVPN

[![Build](https://github.com/coffeegrind123/pmnezia-client/actions/workflows/deploy.yml/badge.svg?branch=dev)](https://github.com/coffeegrind123/pmnezia-client/actions/workflows/deploy.yml?query=branch:dev)
[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![Downloads](https://img.shields.io/badge/downloads-dev--latest-brightgreen)](https://github.com/coffeegrind123/pmnezia-client/releases/tag/dev-latest)

A VPN client for [**coffeeblack-vpn**](https://github.com/coffeegrind123/coffeeblack-vpn) servers, forked from
[`amnezia-vpn/amnezia-client`](https://github.com/amnezia-vpn/amnezia-client) and reduced to the transports that
server actually issues.

coffeeblack-vpn provisions itself and hands out client configurations through its own web interface. This client
therefore does one job: **import a configuration and connect.** Everything upstream carries for provisioning a
server over SSH has been removed, along with every protocol coffeeblack-vpn does not serve.

> **Unofficial fork.** Not affiliated with or endorsed by the Amnezia team. For the official client, support and
> signed builds, see [amnezia.org](https://amnezia.org).

---

## Contents

- [Supported transports](#supported-transports)
- [Installation](#installation)
- [Importing a configuration](#importing-a-configuration)
- [Differences from upstream](#differences-from-upstream)
- [Building from source](#building-from-source)
- [Branding](#branding)
- [Releases](#releases)
- [License](#license)

---

## Supported transports

| Transport | Role | Notes |
| --- | --- | --- |
| **AmneziaWG** | Primary | Obfuscated WireGuard over UDP. Low latency, full tunnel. Containers `amnezia-awg` and `amnezia-awg2`. |
| **WireGuard** | Fallback | coffeeblack-vpn's AmneziaWG 3 obfuscation knobs are off by default, so a configuration issued without them carries no `Jc`/`S1–S4`/`H1–H4` lines and imports as plain WireGuard. Same protocol implementation. |
| **XRay** | Censorship-resistant | VLESS + Reality + Vision over TCP, and the `xhttp` stream mode (HTTP/2 over a secret path). |
| **MasterDnsVPN** | Blackout survival | DNS tunnel carrying TCP/SOCKS5 inside DNS queries. Native C++ engine. |
| **QQ-DNS** | Blackout survival | UDP-over-DNS carrying the AmneziaWG datapath itself, for networks where only port 53 escapes. |

The protocol name **AmneziaWG** is upstream's and is deliberately preserved — coffeeblack-vpn serves it under that
name and the configuration format uses it.

---

## Installation

Pre-built binaries are published as a rolling pre-release:

**[→ Latest build](https://github.com/coffeegrind123/pmnezia-client/releases/tag/dev-latest)**

| Platform | Artifacts |
| --- | --- |
| Windows | `PmneziaVPN_<ver>_windows_x64.exe` (IFW), `PmneziaVPN_<ver>_windows_x64.msi` (WiX) |
| Linux | `PmneziaVPN_<ver>_linux_x64.run` (IFW) |
| Android | `PmneziaVPN_<ver>_android9+_universal.apk`, per-ABI APKs (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`), and `.aab` |

The application checks this repository's releases on start-up and can download and run the installer itself, so a
`dev-latest` build updates in place.

macOS and iOS builds are not produced by default; they require Apple signing credentials and are gated behind a
workflow input. Android builds carry no Google Play dependencies and run on devices without Play Services.

> Because this is a fork, GitHub does not show a *Releases* widget on the repository home page and the build is
> marked as a pre-release, so it never receives the "Latest" badge. Use the link above.

---

## Importing a configuration

Accepted formats:

| Source | Format |
| --- | --- |
| AmneziaWG / WireGuard | `.conf` file |
| XRay | `vless://` URL, or native JSON |
| MasterDnsVPN | `mdnsvpn://b64?…` blob, or `client_config.json` |
| QQ-DNS | transport JSON |
| Any of the above | the Amnezia `vpn://` envelope, or a QR code |

Open the app, choose **Add server**, and supply the file, link, QR code or pasted text. No credentials are required
— the client never contacts the server outside the tunnel it establishes.

---

## Differences from upstream

**Removed.** The SSH server-deployment stack (container install, configuration and removal, the provisioning
scripts, the credentials and installation wizard, client management and configuration sharing); the Amnezia gateway
API, subscriptions, premium tiers and in-app advertising; and the OpenVPN, IKEv2/IPsec, Cloak, Shadowsocks,
MTProxy, TProxy, SFTP, SOCKS5, Tor-website and AmneziaDNS protocols and services, together with their
`vmess://`, `trojan://`, `ss://` and `ssd://` import paths.

**Added.**

| Area | Detail |
| --- | --- |
| MasterDnsVPN engine | Native C++ port of the upstream Go client (`client/masterdnsvpn/`): DNS and wire framing, per-stream ARQ with adaptive RTO and bounded NACKs, an eight-strategy resolver pool, MTU probing, ZSTD/LZ4/raw-deflate compression, a SOCKS5 server, and an Android JNI bridge. Parity-tested against upstream's own Go test vectors. |
| QQ-DNS engine | Native UDP-over-DNS (`client/qqdns/`) carrying the AmneziaWG datapath, with its own Android `VpnService` module. |
| xhttp transport | XRay VLESS + Reality over HTTP/2 on a secret path ([upstream PR #2339](https://github.com/amnezia-vpn/amnezia-client/pull/2339)). |
| Play-free Android | ML Kit barcode scanning replaced with zxing-cpp (Apache-2.0). |
| In-app updates | Update checks and installer download via this repository's GitHub releases. |
| Reduced dependencies | qt5compat dropped tree-wide; the background service is GUI-free; Qt Xml, Multimedia, Image Formats, `Qt.labs.platform` and libssh removed. |

Everything else tracks upstream `dev`, which this fork merges from regularly.

---

## Building from source

```bash
git clone https://github.com/coffeegrind123/pmnezia-client.git
cd pmnezia-client
git submodule update --init --recursive
```

### Requirements

- [CMake](https://cmake.org/download/)
- [Qt 6.10+](https://www.qt.io/download-open-source) (CI builds against 6.10.1) with the **Qt Remote Objects** and
  **Qt Shader Tools** modules. Qt 5 Compatibility, Multimedia and Image Formats are *not* required.
- [Conan 2](https://conan.io/downloads) on `PATH` (a Homebrew or virtualenv install is fine on macOS)
- A platform toolchain: GCC/Make (Linux), Visual Studio 2022 or its Build Tools (Windows), Android SDK plus
  [Ninja](https://ninja-build.org/) (Android), Xcode (Apple)
- Optional, for installers: [Qt Installer Framework](https://www.qt.io/download-open-source) and the
  [WiX toolset](https://github.com/wixtoolset/wix/releases)

If dependencies are outside default locations, point the build scripts at them with `QT_INSTALL_DIR`,
`QT_ROOT_PATH`, `QIF_ROOT_PATH` or `ANDROID_HOME`; see `deploy/` for the full list.

### Build

```bash
deploy/build.sh                      # host executables
deploy/build.sh --installer all      # executables and installer
deploy/build.sh -t android --aab     # Android APK and AAB
deploy/build.sh -h                   # all options
```

On Windows, use `deploy\build.bat` with the same `--installer all` flag. Artifacts are written to `deploy/build/`
as `PmneziaVPN_<ver>_<platform>_x64.<ext>`.

To reproduce a CI build exactly, apply the brand first — see below.

### IDEs

Any CMake- and Qt-aware IDE works. For Xcode or Android Studio, configure with CMake first (via Qt Creator, for
example) and open the generated project from the build directory; note that edits there live under the build
directory and must be copied back by hand.

---

## Branding

The application ships as **PmneziaVPN**, but the source tree deliberately retains upstream's Amnezia naming so that
merges from `amnezia-vpn/amnezia-client` continue to apply. The rename is applied per build:

```bash
deploy/rebrand.sh            # apply, in place
deploy/rebrand.sh --check    # verify only
```

`deploy/rebrand.sh` reads `deploy/brand.env`, which is the single source of truth — the release workflow reads the
same file, so artifact names stay consistent with what the build produces. To change the brand, edit `brand.env`
and nothing else.

Five identifiers are deliberately preserved, and the script fails the build if any is renamed:

| Preserved | Reason |
| --- | --- |
| `AmneziaWG` | The protocol, not the application. coffeeblack-vpn serves it under this name and the configuration format uses it. |
| `amnezia-libxray`, `amnezia-xray-bindings`, `amnezia::` | Conan package names and the CMake imported-target namespace they export. |
| `artifactory.amnezia.org` | The Conan remote host. |
| `namespace amnezia` | Internal C++ namespace, spelled identically to the CMake target namespace above. |
| `AMNEZIAVPN_VERSION` | Read from `CMakeLists.txt` by the release job. |

Substitution tokens are chosen so none of the above can be matched: `AmneziaVPN` is disjoint from `AmneziaWG`, and
bare `Amnezia` is never substituted. After rewriting, the script asserts that each preserved identifier survives,
that no rewritten URL contains a doubled scheme, and that every file referenced by a `.qrc` manifest exists.

---

## Releases

The **Build and Release** workflow (`.github/workflows/deploy.yml`) runs on every push to `dev` or `main`, building
Linux, Windows and Android in parallel and refreshing the rolling `dev-latest` pre-release.

To build Apple targets or publish under a different tag, dispatch it manually from the
[Actions tab](https://github.com/coffeegrind123/pmnezia-client/actions/workflows/deploy.yml) or:

```bash
gh workflow run deploy.yml --repo coffeegrind123/pmnezia-client --ref dev
```

| Input | Default | Purpose |
| --- | --- | --- |
| `release_tag` | `dev-latest` | Rolling tag; an existing release on this tag is deleted and recreated. |
| `release_name` | `Dev build (masterdnsvpn + xhttp)` | Release title; version and short SHA are appended. |
| `prerelease` | `true` | Mark as pre-release. Push-triggered builds are always pre-releases. |
| `enable_apple_builds` | `false` | Also build iOS, macOS and macOS-NE. Requires Apple signing credentials. |

> **Artifact naming is a contract.** CPack emits `PmneziaVPN_<ver>_<platform>_x64.<ext>` and `upload-artifact` runs
> with `archive: false`, so each artifact is named after its file. The upload globs and the download pattern in
> `deploy.yml` derive from `deploy/brand.env`; keep them consistent if CPack output names change.

`tag-deploy.yml` and `tag-upload.yml` are upstream leftovers — the old tag-driven release and an S3 upload. Both
are dispatch-only, pin Qt 6.4.1 and require Amnezia's credentials; neither is used here.

---

## License

GNU General Public License v3.0 — see [`LICENSE`](LICENSE). Third-party components are distributed under their own
terms; see [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

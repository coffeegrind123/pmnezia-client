# AmneziaVPN (coffeegrind123 fork)

[![Build Status](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml/badge.svg?branch=dev)](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml?query=branch:dev)

A fork of [`amnezia-vpn/amnezia-client`](https://github.com/amnezia-vpn/amnezia-client), cut down to a dedicated client for [`coffeegrind123/awg-easy-rs`](https://github.com/coffeegrind123/awg-easy-rs). It ships its own rolling pre-release builds for Windows, Linux and Android.

- Upstream project: <https://github.com/amnezia-vpn/amnezia-client>
- Upstream documentation: <https://docs.amnezia.org>

---

## What this fork is

awg-easy-rs deploys itself and hands out client configs through its own web UI, so this client **only imports configs and connects**. Everything upstream carries for provisioning a server over SSH is gone, as is every protocol awg-easy-rs does not serve.

**Transports kept** — exactly the set awg-easy-rs issues configs for:

| Transport | Notes |
| --- | --- |
| **AmneziaWG** | `amnezia-awg` and `amnezia-awg2`. The low-latency datapath. |
| **WireGuard** | An awg-easy-rs config with the AWG3 obfuscation knobs left off carries no `Jc`/`S1-S4`/`H1-H4` lines, so it imports as plain WireGuard. Same protocol implementation as AmneziaWG. |
| **XRay** | VLESS + Reality + Vision over TCP, and the `xhttp` stream mode (HTTP/2 over a secret path) that awg-easy-rs also serves. |
| **MasterDnsVPN** | DNS-tunnel transport (`Proto::MasterDnsVpn`), a native C++ `VpnProtocol`. Carries TCP/SOCKS5 inside DNS queries. |
| **QQ-DNS** | UDP-over-DNS (`Proto::QqDns`) — carries the AmneziaWG datapath itself when only port 53 escapes. |

**Removed:** the SSH server-deployment stack (container install/configure/remove, `server_scripts/`, the setup wizard's credential and install pages, client management and config sharing); the Amnezia gateway API, subscriptions, premium and in-app ads; and the OpenVPN, IKEv2/IPsec, Cloak, Shadowsocks, MTProxy, TProxy, SFTP, SOCKS5, Tor-website and AmneziaDNS protocols and services, along with their `vmess://` / `trojan://` / `ss://` / `ssd://` import paths.

Config import still accepts: AmneziaWG and WireGuard `.conf`, `vless://`, `mdnsvpn://b64?`, QQ-DNS JSON, the native Amnezia `vpn://` envelope, and QR codes of any of them.

### Added on top of upstream

| Area | What |
| --- | --- |
| **MasterDnsVPN engine** | A full native C++ port of the upstream Go client, in-tree at `client/masterdnsvpn/` — DNS/wire framing, per-stream ARQ with adaptive RTO and bounded NACKs, an 8-strategy resolver pool, MTU probing, ZSTD/LZ4/raw-deflate compression, a SOCKS5 server, and an Android JNI bridge. Parity-tested against upstream's own Go test vectors. |
| **QQ-DNS engine** | Native UDP-over-DNS at `client/qqdns/`, carrying the AmneziaWG datapath itself, plus its own Android `VpnService` module for the QQ-DNS-under-AmneziaWG handoff. |
| **xhttp transport** | XRay VLESS + Reality over HTTP/2 on a secret path, alongside the TCP/Vision path ([upstream PR #2339](https://github.com/amnezia-vpn/amnezia-client/pull/2339)). |
| **No Google Play** | ML Kit barcode scanning replaced with zxing-cpp (Apache-2.0), so the app builds and runs on devices without Play Services. |
| **In-app updates** | The app checks this fork's GitHub Releases and can download and run the installer itself — the Play-store update path it replaces is gone. |
| **Leaner deps** | qt5compat dropped tree-wide (`Qt5Compat.GraphicalEffects` replaced by in-tree shaders), the service is Gui-free, and Qt Xml / Multimedia / ImageFormats, `Qt.labs.platform` and libssh are gone. |

Everything else tracks upstream `dev`; this fork is rebased/merged against it regularly.

---

## Downloads

Pre‑built binaries are published as a **rolling pre‑release** on this fork:

➡️ **<https://github.com/coffeegrind123/amnezia-client/releases/tag/dev-latest>**

> [!NOTE]
> Because this is a **fork**, GitHub does not show the *Releases* widget on the repository home page, and the build is marked as a pre‑release (so it never gets the "Latest" badge). Use the direct link above or the [Releases tab](https://github.com/coffeegrind123/amnezia-client/releases) — the tag alone on the home page is expected.

Each `dev-latest` build contains:

| Platform | Assets |
| --- | --- |
| Windows | `AmneziaVPN_<ver>_windows_x64.exe` (IFW installer), `AmneziaVPN_<ver>_windows_x64.msi` (WIX installer) |
| Linux | `AmneziaVPN_<ver>_linux_x64.run` (IFW installer) |
| Android | `AmneziaVPN_<ver>_android9+_universal.apk`, per‑ABI APKs (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`), and `AmneziaVPN_<ver>.aab` |

macOS / iOS builds are **not** produced by default — they require Apple signing secrets and are gated behind a workflow input (see below). For signed Apple builds, use the upstream releases.

The app checks this repository's releases on startup and can download and run the installer itself, so a `dev-latest` build updates in place.

---

## Building from source

Pull submodules after cloning:

```bash
git submodule update --init --recursive
```

### Requirements

- [`CMake`](https://cmake.org/download/)
- A toolchain for your target:
  - **Linux** — `gcc`/`make`
  - **Windows** — [Visual Studio 2022](https://aka.ms/vs/17/release/vs_community.exe) or VS 2022 Build Tools
  - **Android** — Android SDK + [`Ninja`](https://ninja-build.org/)
  - **Apple** — [Xcode](https://developer.apple.com/xcode/) / command‑line tools
- [`Qt 6.10+`](https://www.qt.io/download-open-source) (CI builds against 6.10.1) with the **Qt Remote Objects** and **Qt Shader Tools** additional modules on top of a Core install. This fork does **not** need Qt 5 Compatibility, Multimedia or Image Formats — `Qt5Compat.GraphicalEffects` was replaced by in-tree components and shaders.
- [`Conan 2`](https://conan.io/downloads) package manager (must be on `PATH`, except on macOS where a Homebrew/`.venv` install is fine)
- Optional, for installers: [Qt Installer Framework](https://www.qt.io/download-open-source) (Windows/Linux) and the [WiX toolset](https://github.com/wixtoolset/wix/releases) (Windows)

If dependencies live in non‑default paths, point the build scripts at them via `QT_INSTALL_DIR`, `QT_ROOT_PATH`, `QIF_ROOT_PATH`, `ANDROID_HOME` (see the scripts in `deploy/` for the full list).

### Build scripts

Unix‑like (`deploy/build.sh`):

```bash
# Host-platform executables
deploy/build.sh

# Executables + installer
deploy/build.sh --installer all

# Android APK + AAB
deploy/build.sh -t android --aab

# Help
deploy/build.sh -h
```

Windows (`deploy/build.bat`):

```batch
:: Executables only
deploy\build.bat

:: Executables + IFW and WIX installers
deploy\build.bat --installer all
```

Build artifacts (including CPack installers) land under `deploy/build/`, named `AmneziaVPN_<ver>_<platform>_x64.<ext>`.

### IDEs

Any CMake/Qt‑aware IDE works (Qt Creator, VS Code with the Qt Extension Pack, etc.). For Xcode/Android Studio, configure the project with CMake (e.g. via Qt Creator) first, then open the generated project from the build directory — note that edits there live under the build directory and must be copied back manually.

---

## Producing release builds (maintainers)

Releases are cut by the **Build and Release workflow** (`.github/workflows/deploy.yml`).

It runs **automatically on every push to `dev` or `main`**, building Linux, Windows and Android in parallel and refreshing the rolling `dev-latest` pre-release. Apple jobs stay off on a push (their `if: inputs.enable_apple_builds` is falsy without dispatch inputs).

To build Apple targets, or to publish under a different tag, dispatch it manually from the [Actions tab](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml) → *Run workflow*, or:

```bash
gh workflow run deploy.yml --repo coffeegrind123/amnezia-client --ref dev
```

Dispatch inputs:

| Input | Default | Purpose |
| --- | --- | --- |
| `release_tag` | `dev-latest` | Rolling tag; an existing release on this tag is deleted and recreated. |
| `release_name` | `Dev build (masterdnsvpn + xhttp)` | Release title (the project version and short SHA are appended). |
| `prerelease` | `true` | Mark the release as a pre‑release. Push-triggered builds are always pre‑releases. |
| `enable_apple_builds` | `false` | Also build iOS/macOS/macOS‑NE (requires Apple signing secrets). |

`Publish-Release` then collects every `AmneziaVPN_*.*` artifact and attaches it to the release. Translations (`AmneziaVPN_translations`) are intentionally excluded.

> Artifact naming is a contract: CPack emits `AmneziaVPN_<ver>_<platform>_x64.<ext>` and `upload-artifact` runs with `archive: false` (so each artifact is named after its file). The upload globs and the download pattern in `deploy.yml` must match that naming — keep them in sync if you touch CPack output names.

The other two workflows in `.github/workflows/` (`tag-deploy.yml`, `tag-upload.yml`) are upstream leftovers — the old tag-driven release and the S3 upload. Both are dispatch-only, pin Qt 6.4.1, and need Amnezia's own secrets; neither is used by this fork.

---

## License

GNU General Public License v3.0 — see [`LICENSE`](LICENSE). Third‑party components are distributed under their own terms; see [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

This is an unofficial fork. It is not affiliated with or endorsed by the Amnezia team. For the official project, support channels and signed builds, go to [amnezia.org](https://amnezia.org) and [`amnezia-vpn/amnezia-client`](https://github.com/amnezia-vpn/amnezia-client).

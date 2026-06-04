# AmneziaVPN (coffeegrind123 fork)

[![Build Status](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml/badge.svg?branch=dev)](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml?query=branch:dev)

A fork of [`amnezia-vpn/amnezia-client`](https://github.com/amnezia-vpn/amnezia-client) that adds two extra masking transports — **MasterDnsVPN** (DNS‑tunnel mode) and **xhttp** (VLESS + Reality over HTTP/2) — and ships its own rolling pre‑release builds for Windows, Linux and Android.

Everything from upstream still applies: AmneziaVPN is an open‑source client that deploys and manages a self‑hosted VPN server over SSH (OpenVPN, WireGuard, AmneziaWG, IKEv2, XRay, Shadowsocks, Cloak). For the full product description, end‑user documentation and official signed builds, see upstream:

- Project: <https://github.com/amnezia-vpn/amnezia-client>
- Documentation: <https://docs.amnezia.org>

---

## What this fork adds

| Feature | Description | Source |
| --- | --- | --- |
| **MasterDnsVPN** | A third proxy transport that tunnels traffic over DNS (`Proto::MasterDnsVpn`), implemented as a native C++ `VpnProtocol`. Useful where only DNS egress is available. | `client/core/protocols/masterDnsVpnProtocol.cpp`, `client/core/models/protocols/masterDnsVpnProtocolConfig.cpp` |
| **xhttp transport** | An additional XRay stream mode carrying VLESS + Reality over HTTP/2, alongside the existing TCP/Reality path. | `client/core/configurators/xrayConfigurator.cpp` (`normalizeXhttpMode`, Reality key handling) |

Both transports are wired through the standard container/protocol model (`client/core/models/containerConfig.cpp`, `protocolConfig.cpp`) so they appear and configure like any other Amnezia protocol.

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
- [`Qt 6.10+`](https://www.qt.io/download-open-source) with: Core (for your target), Qt 5 Compatibility, Qt Remote Objects, Qt Shader Tools
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

Releases are cut by the **Deploy workflow** (`.github/workflows/deploy.yml`), which is **dispatch‑only** (no automatic push trigger). Run it from the [Actions tab](https://github.com/coffeegrind123/amnezia-client/actions/workflows/deploy.yml) → *Run workflow*, or:

```bash
gh workflow run deploy.yml --repo coffeegrind123/amnezia-client --ref dev
```

Inputs:

| Input | Default | Purpose |
| --- | --- | --- |
| `release_tag` | `dev-latest` | Rolling tag; an existing release on this tag is deleted and recreated. |
| `release_name` | `Dev build (masterdnsvpn + xhttp)` | Release title. |
| `prerelease` | `true` | Mark the release as a pre‑release. |
| `enable_apple_builds` | `false` | Also build iOS/macOS/macOS‑NE (requires Apple signing secrets). |

The workflow builds Linux, Windows and Android in parallel, then `Publish-Release` collects every `AmneziaVPN_*.*` artifact and attaches it to the release. Translations (`AmneziaVPN_translations`) are intentionally excluded from the release.

> Artifact naming is a contract: CPack emits `AmneziaVPN_<ver>_<platform>_x64.<ext>` and `upload-artifact` runs with `archive: false` (so each artifact is named after its file). The upload globs and the download pattern in `deploy.yml` must match that naming — keep them in sync if you touch CPack output names.

---

## License

GNU General Public License v3.0 — see [`LICENSE`](LICENSE). Third‑party components are distributed under their own terms; see [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md).

This is an unofficial fork. It is not affiliated with or endorsed by the Amnezia team. For the official project, support channels and signed builds, go to [amnezia.org](https://amnezia.org) and [`amnezia-vpn/amnezia-client`](https://github.com/amnezia-vpn/amnezia-client).

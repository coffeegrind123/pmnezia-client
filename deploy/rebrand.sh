#!/usr/bin/env bash
#
# Build-time rebrand: rewrites the working tree from the upstream Amnezia
# naming to this fork's brand, as defined in deploy/brand.env.
#
# Run once, from the repo root, before configuring the build:
#
#     deploy/rebrand.sh            # apply
#     deploy/rebrand.sh --check    # verify only, change nothing
#
# The tree is deliberately NOT stored rebranded: keeping upstream's names
# in git is what lets merges from amnezia-vpn/amnezia-client keep applying,
# so the rename happens per build instead.
#
# What is deliberately NOT renamed, and why:
#
#   AmneziaWG / amneziawg   The protocol, not the app. awg-easy-rs serves it
#                           under this name, the config format uses it, and
#                           the spec is upstream's. Renaming it would leave
#                           this client calling the protocol something no
#                           other tool does - including the server that
#                           issued the config.
#   amnezia-libxray         Conan package names. These resolve from a remote;
#   amnezia-xray-bindings   renaming them makes conan look for packages that
#                           do not exist.
#   amnezia::               CMake imported-target namespace exported by those
#                           same conan packages.
#   amnezia-vpn/            The upstream GitHub org, in submodule URLs.
#   namespace amnezia       Internal C++ namespace. Invisible to users, and
#                           spelled identically to the CMake target namespace
#                           above, so renaming it cannot be done by string
#                           substitution without breaking the build.
#   AMNEZIAVPN_VERSION      Read by .github/workflows/deploy.yml to derive the
#                           release version.
#
# The replacement tokens are chosen so that none of the above can be hit:
# "AmneziaVPN" is disjoint from "AmneziaWG", and bare "Amnezia" is never
# substituted.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

CHECK_ONLY=0
[[ "${1:-}" == "--check" ]] && CHECK_ONLY=1

# shellcheck disable=SC1091
source deploy/brand.env

: "${BRAND_APP_NAME:?deploy/brand.env must define BRAND_APP_NAME}"
: "${BRAND_LOWER:?deploy/brand.env must define BRAND_LOWER}"
: "${BRAND_ANDROID_PACKAGE:?deploy/brand.env must define BRAND_ANDROID_PACKAGE}"
: "${BRAND_ANDROID_DIR:?deploy/brand.env must define BRAND_ANDROID_DIR}"
: "${BRAND_APPLE_ID_PREFIX:?deploy/brand.env must define BRAND_APPLE_ID_PREFIX}"

say() { printf '  %s\n' "$*"; }

# ---------------------------------------------------------------- guards ----
# Already applied? Re-running must be a no-op rather than a corruption.
ALREADY=0
if grep -q "$BRAND_APP_NAME" client/cmake/branding/common.cmake 2>/dev/null; then
    echo "rebrand: already applied ($BRAND_APP_NAME present in branding/common.cmake); skipping rewrite"
    ALREADY=1
    CHECK_ONLY=1
fi

if [[ $CHECK_ONLY -eq 1 && $ALREADY -eq 0 ]]; then
    echo "rebrand: --check, verifying only; no files will be modified"
fi

# Files eligible for content rewriting. Excludes vendored code, git internals
# and build output. Translation catalogues ARE included: their <source> strings
# must keep matching the rewritten QML, or every translated string that named
# the app would silently fall back to English.
eligible_files() {
    find . \
        -path ./.git -prune -o \
        -path ./client/3rd -prune -o \
        -path ./deploy/build -prune -o \
        -path ./deploy/rebrand.sh -prune -o \
        -path ./deploy/brand.env -prune -o \
        -type f \
        \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.mm' -o -name '*.m' \
           -o -name '*.qml' -o -name '*.qrc' -o -name '*.kt' -o -name '*.kts' -o -name '*.java' \
           -o -name '*.swift' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name '*.xml' \
           -o -name '*.in' -o -name '*.plist' -o -name '*.entitlements' -o -name '*.desktop' \
           -o -name '*.service' -o -name '*.sh' -o -name '*.bat' -o -name '*.cmd' -o -name '*.js' \
           -o -name '*.ts' \) \
        -print
}

# ------------------------------------------------------------- rewriting ----
if [[ $CHECK_ONLY -eq 0 ]]; then
    echo "rebrand: applying $BRAND_APP_NAME"

    mapfile -t FILES < <(eligible_files)

    # Identity tokens. Order matters only in that the longest, most specific
    # forms come first; none of these can match AmneziaWG.
    for f in "${FILES[@]}"; do
        LC_ALL=C sed -i \
            -e "s/AmneziaVPN/${BRAND_APP_NAME}/g" \
            -e "s/amneziavpn/${BRAND_LOWER}/g" \
            -e "s/org\.amnezia\.vpn/${BRAND_ANDROID_PACKAGE}/g" \
            -e "s/org\.amnezia\.${BRAND_APP_NAME}/${BRAND_APPLE_ID_PREFIX}.${BRAND_APP_NAME}/g" \
            -e "s/org\.amnezia\.amneziaVPN/${BRAND_APPLE_ID_PREFIX}.${BRAND_LOWER}/g" \
            "$f"
    done
    say "rewrote identity tokens in ${#FILES[@]} files"

    # -- upstream links: a rebranded app must not point at Amnezia's support --
    for f in "${FILES[@]}"; do
        LC_ALL=C sed -i \
            -e "s|https://docs\.amnezia\.org[^\"']*|${BRAND_HOMEPAGE}|g" \
            -e "s|https://storage\.googleapis\.com/amnezia/amnezia\.org[^\"']*|${BRAND_HOMEPAGE}|g" \
            -e "s|https://amnezia\.org[^\"']*|${BRAND_HOMEPAGE}|g" \
            -e "s|mailto:support@amnezia\.org|${BRAND_SUPPORT_URL}|g" \
            -e "s|support@amnezia\.org|${BRAND_SUPPORT_URL}|g" \
            -e "s|https://amnezia\.host[^\"']*|${BRAND_HOMEPAGE}|g" \
            -e "s|https://t\.me/amnezia[a-z_]*|${BRAND_SUPPORT_URL}|g" \
            -e "s|https://telegram\.me/amnezia[a-z_]*|${BRAND_SUPPORT_URL}|g" \
            "$f"
    done

    # Bare domains with no scheme are display text ("amnezia.org" shown to the
    # user) and only ever appear in QML. Rewriting them tree-wide also hits
    # infrastructure hostnames that merely end in the same domain - notably the
    # conan remote at artifactory.amnezia.org, which is not ours to rename and
    # whose mangling breaks dependency resolution outright.
    for f in "${FILES[@]}"; do
        [[ "$f" == *.qml ]] || continue
        LC_ALL=C sed -i \
            -e "s|amnezia\\.host|${BRAND_HOMEPAGE}|g" \
            -e "s|amnezia\\.org|${BRAND_HOMEPAGE}|g" \
            "$f"
    done
    say "repointed upstream links to ${BRAND_HOMEPAGE}"

    # -- android package directories -----------------------------------------
    while IFS= read -r d; do
        [[ -d "$d" ]] || continue
        mv "$d" "$(dirname "$d")/${BRAND_ANDROID_DIR}"
        say "moved $d -> $(dirname "$d")/${BRAND_ANDROID_DIR}"
    done < <(find client/android -type d -name amnezia)

    # -- translation catalogues ----------------------------------------------
    for ts in client/translations/amneziavpn_*.ts; do
        [[ -e "$ts" ]] || continue
        mv "$ts" "client/translations/${BRAND_LOWER}_${ts##*amneziavpn_}"
    done
    say "renamed translation catalogues to ${BRAND_LOWER}_*.ts"

    # -- packaging assets whose FILENAME carries the brand --------------------
    while IFS= read -r p; do
        [[ -e "$p" ]] || continue
        mv "$p" "$(dirname "$p")/$(basename "$p" | sed "s/AmneziaVPN/${BRAND_APP_NAME}/")"
    done < <(find deploy -name 'AmneziaVPN*' -not -path './deploy/build/*')
    say "renamed packaging assets under deploy/data"
fi

# ------------------------------------------------------------ validation ----
# Anything below surviving is correct. Anything below MISSING means the
# rebrand ate an identifier the build depends on.
echo "rebrand: verifying protected identifiers survived"
fail=0
require() {
    local needle="$1" where="$2" desc="$3"
    if ! grep -rq -- "$needle" $where 2>/dev/null; then
        echo "  FAIL: '$needle' no longer present in $where ($desc)"
        fail=1
    else
        say "ok: $needle ($desc)"
    fi
}
require 'amnezia-xray-bindings' 'conanfile.py'                  'conan package'
require 'amnezia-libxray'       'conanfile.py'                  'conan package'
require 'amnezia::'             'service/server/CMakeLists.txt' 'conan cmake target namespace'
require 'AmneziaWG'             'client/core/utils/containers/containerUtils.cpp' 'protocol name'
require 'namespace amnezia'     'client/core/utils/protocolEnum.h' 'internal C++ namespace'
require 'AMNEZIAVPN_VERSION'    'CMakeLists.txt'                'version var read by CI'
require 'artifactory.amnezia.org' 'cmake/recipes_bootstrap.cmake' 'conan remote host'

# A substitution that ate a hostname leaves a doubled scheme behind. That is
# exactly how artifactory.amnezia.org was first broken, so it is an assertion.
if grep -rn 'https://[^"'"'"' ]*https://' --include='*.cmake' --include='*.cpp' \
        --include='*.h' --include='*.qml' --include='*.yml' . 2>/dev/null \
        | grep -v '^./.git/' | head -5 | grep -q .; then
    echo "  FAIL: a rewritten URL contains a doubled scheme:" >&2
    grep -rn 'https://[^"'"'"' ]*https://' --include='*.cmake' --include='*.cpp' \
        --include='*.h' --include='*.qml' --include='*.yml' . 2>/dev/null \
        | grep -v '^./.git/' | head -5 >&2
    fail=1
fi

if [[ $fail -ne 0 ]]; then
    echo "rebrand: FAILED - a protected identifier was renamed; refusing to continue" >&2
    exit 1
fi

echo "rebrand: done"

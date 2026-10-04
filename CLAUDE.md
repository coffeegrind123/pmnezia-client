# pmnezia-client — agent notes

Fork of `amnezia-vpn/amnezia-client`, stripped to import-and-connect for
coffeeblack-vpn servers (`coffeegrind123/coffeeblack-vpn`, local
`~/coffeeblack-vpn`). Remote `origin` = `coffeegrind123/pmnezia-client`,
`upstream` = `amnezia-vpn/amnezia-client`. Mainline branch is **`dev`**. Read
`README.md` ("Differences from upstream", "Branding", "Releases") first.

## Never compile on this box

No Qt, conan, Go or Android builds locally, in this container or a sibling one —
see the global CLAUDE.md. Verification is the `deploy.yml` workflow: every push to
`dev` builds Linux, Windows and Android and refreshes the `dev-latest`
pre-release. Apple targets only build on
`gh workflow run deploy.yml --ref dev -f enable_apple_builds=true` (needs signing
secrets). Locally: edit, `python3 -m py_compile` recipes, `git merge-tree`, and
read sources.

Watch a run: `gh run watch <id> -R coffeegrind123/pmnezia-client --exit-status`.
When a step fails before compiling, `--log-failed` can be empty; fetch the job log
with `gh api repos/coffeegrind123/pmnezia-client/actions/jobs/<job>/logs`.

## Fork invariants

- The tree keeps upstream's Amnezia naming; `deploy/rebrand.sh` applies the brand
  per build from `deploy/brand.env`. Never rebrand files in git — it breaks merges.
- Artwork is replaced the same way: `deploy/brand/overlay/` mirrors the repo paths
  of every upstream image it replaces and `rebrand.sh` copies it in. Never edit
  upstream images in git. A merge that adds or resizes a branded image needs a
  matching entry in `deploy/brand/generate.py`; regenerate with the venv recipe in
  `deploy/brand/README.md` (rendering, not compiling).
- `rebrand.sh` must run on macOS's bash 3.2 and BSD tools: no `mapfile`, no
  `grep -P`, `sed -i` only through its `sed_inplace` helper.
- Premium / gateway / news / IAP / telemetry code stays deleted. Update checks go
  to this repo's GitHub releases and must not send identifiers.
- Fork features that must survive every merge: MasterDnsVPN (`client/masterdnsvpn/`),
  QQ-DNS (`client/qqdns/`), the xhttp transport, zxing-cpp instead of ML Kit, the
  non-matrix Android job and `Publish-Release` in `deploy.yml`.
- Tests live in `client/tests/` (upstream #3172, adopted in merge `8d88ae23a`) and
  build only with `AMNEZIA_BUILD_TESTS=ON`, which the CI build jobs set. The harness
  `utils/testCoreController.h` exposes only the getters the fork's `CoreController`
  has; upstream suites for deleted code (`api/`, `selfHostedAdmin/`) stay out. Fork-only
  suites: `testMasterDnsVpnEngine`, `testMasterDnsVpnConfig`, `testComplexOperations`.
  `deploy.yml` runs them in `test-linux` / `test-windows`, and `Publish-Release`
  waits for both. `coffeegrind123/amnezia-client-tests` is superseded.

## Upstream upgrade protocol

Run when asked to bring the fork up to date. Previous passes are the merge commits
`ed0b1fa9a`, `316fd266a`, `8722c6d20`; their messages are the model.

### 1. Inventory (read-only)

```sh
git fetch upstream origin
git rev-list --left-right --count upstream/dev...dev       # behind, ahead
git log --oneline $(git merge-base dev upstream/dev)..upstream/dev
git merge-tree --write-tree --name-only --no-messages dev upstream/dev   # trial merge: conflicting files
```

Also compare every `recipes/*/conanfile.py` pin with upstream's and with the
upstream projects' newest tags (`gh api repos/amnezia-vpn/<repo>/tags`). For the
AmneziaWG recipes the source of truth is coffeeblack-vpn: the client's
amneziawg-go must not lag that repo's `Dockerfile` `AWG_GO_TAG`, and every AWG key
coffeeblack-vpn emits (`src/wg/config_gen.rs`, `src/wg/cb3.rs`) must appear in
`configKey::awgProtocolKeys()` with the same spelling.

### 2. Merge, don't rebase

`git merge upstream/dev` into `dev`, message `Merge upstream/dev (<n> commits) into
fork dev`. Resolve by class, and write each class into the merge message:

- modify/delete on premium/gateway/telemetry units: stay deleted; also drop new
  upstream files that only reference deleted code.
- files both sides changed: take upstream's change and re-apply the fork's intent,
  never a blanket `--ours`/`--theirs`. After resolving, check that no upstream change
  outside the deleted stack was lost (`git diff upstream/dev -- <file>`).
- `deploy.yml`: keep the fork's jobs and secrets set; adopt upstream's toolchain and
  action fixes (Qt host, action pins).
- `.ts` translations: take upstream's.

Then verify statically: every `.qrc` entry exists on disk, no QML/C++ reference to a
deleted class or page, fork features still wired through every protocol dispatch
file, and `deploy/rebrand.sh --check` logic still matches `brand.env`.

### 3. Recipes

Bump a recipe pin with its source hash (the zip's sha256; for a git clone, the tag).
Where an upstream platform repo (amneziawg-android/-windows/-apple) embeds an older
amneziawg-go than the one coffeeblack-vpn ships, the recipe repins it in `source()`
via `pin_awg_go()` — update those constants, and delete the block once a platform
tag embeds the version.

### 4. Ship

Push `dev` and watch the run to green on Linux, Windows and Android. Report what
CI could not cover (Apple unless dispatched). A pass is not finished while CI is
red.

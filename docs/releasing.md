# Popeye G&W release workflow

This is the maintained procedure behind the `popeye-gw-build-audit`,
`popeye-gw-release` and `popeye-gw-appstore` skills (`.claude/skills/`,
mirrored in `.agents/skills/`). It implements PRD sections 12, 13 and 15. The PRD
was retired after v1.0.2; read those sections with `git show v1.0.2:PRD.md`.

Identity lives in the `apps` map in [release-config.json](release-config.json).
Select `popeye-gw` (default) or `popeye-gw-clock` explicitly for the entire
workflow. `tools.upload_store.app_config(APP)` resolves the common store hosts
and selected app values. Never borrow the other app's ID, assets or journal.
`uuid` identifies the PBW; `store_app_id` is assigned at first registration.

## Select the app

Run commands from the repository root. Set these variables once and keep them
for the entire workflow; the examples below use them:

```sh
APP=popeye-gw                 # use popeye-gw-clock for the watchface
V=X.Y.Z
PROJECT=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["project_dir"])' "$APP")
ARTIFACT=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["artifact_name"])' "$APP")
PREFIX=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["tag_prefix"])' "$APP")
RELEASE_DIR=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["release_dir"])' "$APP")
NAME=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["display_name"])' "$APP")
D="$RELEASE_DIR/$V"
TAG="$PREFIX$V"
```

| Value | Game (default) | Watchface |
|---|---|---|
| Project | `.` | `watchface` |
| Changelog | `CHANGELOG.md` | `watchface/CHANGELOG.md` |
| Tag | `vX.Y.Z` | `clock-vX.Y.Z` |
| Frozen candidate | `.release/X.Y.Z/` | `.release/popeye-gw-clock/X.Y.Z/` |
| Registration journal | `.release/registration.json` | `.release/popeye-gw-clock/registration.json` |
| Category / type | Games / watchapp | Faces / watchface |
| Listing material | `docs/releases/` | `docs/releases/clock/` |

The watchface's owner-approved scope is recorded in `docs/v1.1-notes.md`;
PRD game milestone gates do not apply to it. Its release must not bump the
game version or replace the game's GitHub Latest release. Faces is confirmed
by the [current RePebble watchface catalog listing](https://apps.repebble.com/85378f447ebf4507ae9cd857)
(2026-10-06); recheck the Dashboard form before first registration.

**Every publication needs the owner's explicit approval of the exact frozen
PBW (its SHA-256), its version and its destinations, after the owner's
play-test on the watch. Installing the build is not approval.** Loading,
reading or editing these skills authorizes nothing.

## Budgets

| Item | Limit | Where it is checked |
|---|---|---|
| App image (`.text + .data + .bss`) | 61,440 bytes | Every SDK build (`tools/check_app_size.py`) |
| Resources (`emery/app_resources.pbpack`) | 200,000 bytes | Audit, from the PBW |
| PBW | 2,000,000 bytes | Audit |
| Free heap while playing (game only) | ≥ 20 % | Audit, measured in the emulator |

Never raise a budget to make a release pass; that is an owner decision.

## Essential validation (default for every release)

- Always: selected app's normal and sanitizer host suites, one clean SDK build, PBW identity and budgets, and
  the identity review of new or changed text and art.
- Emulator: launch plus the flows the diff touches. At most five screenshots,
  and only for layout or art changes. Allow one emulator retry (`pebble kill`,
  `pebble wipe` only with owner permission), then report the gap instead of debugging at length.
- Watch: the owner plays the frozen PBW (PRD 12.6). The agent never reports
  play-test results it did not observe.
- Listing-only edits need no build or emulator run.
- Reuse evidence only for identical bytes; never attribute old results to a
  new PBW. Required failures stay failures; only the owner can accept an
  exception, never elapsed time.
- Remote checks: two attempts about a minute apart, then report what is
  still pending.

## 1. Preflight

```sh
git fetch --tags origin
git status --short                      # must be empty
git rev-parse --abbrev-ref HEAD         # main
git rev-list --left-right --count origin/main...HEAD
git tag --list "$PREFIX*" --sort=-v:refname | head -3
gh release list --limit 3
gh run list --branch main --limit 3 --json headSha,status,conclusion,name
```

- Work on `main` with a clean tree. Do not stash, reset or commit unrelated
  work to pass a check; stop and ask.
- Host tests must be green in CI for the commit being released.
- **Game 1.0.0 gate only:** M1–M5 accepted (PRD 15), including the owner's art approval
  and M5 play-test sign-off. Ask the owner rather than inferring it.
- Review `git log --oneline PREVIOUS_APP_TAG..HEAD` and `git diff --stat PREVIOUS_APP_TAG..HEAD`.
  Choose the version by semver from that diff and tell the owner before
  editing anything. The first release is `1.0.0`, which the selected project’s `package.json`
  already holds.

## 2. Release preparation commit

Limit edits to:

- selected project’s `package.json` `version`;
- configured `changelog_file`: a `## X.Y.Z — YYYY-MM-DD` section. Its body is the release
  note used for GitHub and the store;
- the README status line;
- configured `description_file`, only when its wording changes.

Do not change game code, tuning or art during release preparation. Run the
identity check (PRD 14) on the changelog, README, description, listing
assets and every string in `src/`. Verify the selected app’s configured display name and
artifact names and the approved Popeye, Olive Oyl and Brutus cast. Commit with
`build: prepare APP X.Y.Z`.

## 3. Build audit

Run on the preparation commit with a clean tree:

```sh
if [ "$APP" = popeye-gw-clock ]; then
  ./watchface/test.sh && ./watchface/test.sh --sanitize || exit 1
else
  ./tools/test.sh && ./tools/test.sh --sanitize || exit 1
fi
# For tooling or shared-code changes, run the full tools/test.sh suites too.
mkdir -p "$RELEASE_DIR"
# `pebble clean` can leave a stale bundle (clock 1.0.3 rebuilt the old PBW).
# Delete the ignored build directory instead.
git check-ignore -q "$PROJECT/build" && rm -rf "$PROJECT/build" || exit 1
if (cd "$PROJECT" && TERM=xterm pebble build) >"$RELEASE_DIR/build.log" 2>&1; then
  rg -q "'build' finished" "$RELEASE_DIR/build.log" || exit 1
else
  cat "$RELEASE_DIR/build.log"
  exit 1
fi
rg "App image|warning|error" "$RELEASE_DIR/build.log"
# Remove only the selected project's .lock-waf* files without reading them.
```

Both the exit status must be 0 and `'build' finished` must appear, and the
PBW identity check below must report the new version. The `.lock-waf*` files
contain the host environment: never print or commit them.

PBW identity and sizes:

```sh
python3 - "$APP" <<'EOF'
import hashlib, json, sys, zipfile
from pathlib import Path
from tools.upload_store import app_config
cfg = app_config(sys.argv[1])
project = Path(cfg["project_dir"])
pbw = project / "build" / cfg["artifact_name"]
pkg = json.loads((project / "package.json").read_text())
with zipfile.ZipFile(pbw) as z:
    info = json.loads(z.read("appinfo.json"))
    sizes = {i.filename: i.file_size for i in z.infolist()}
data = open(pbw, "rb").read()
checks = {
    "uuid": info["uuid"] == cfg["uuid"],
    "version": info["versionLabel"] == pkg["version"],
    "platforms": info["targetPlatforms"] == cfg["platforms"],
    "type": info["watchapp"].get("watchface") is (cfg["app_type"] == "watchface"),
    "name": info["shortName"] == info["longName"] == cfg["display_name"],
    "pbw_budget": len(data) <= cfg["budgets"]["pbw_bytes"],
    "resources_budget": 0 < sizes.get("emery/app_resources.pbpack", 0)
        <= cfg["budgets"]["resources_bytes"],
}
print(json.dumps({"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                  "resources": sizes.get("emery/app_resources.pbpack", 0),
                  "checks": checks}, indent=2))
raise SystemExit(0 if all(checks.values()) else 1)
EOF
```

- **Game launcher icon:** the menu icon must be a real 25 × 25 PNG canvas
  (`sips -g pixelWidth -g pixelHeight FILE`) and legible in an actual
  launcher screenshot. Compile success does not validate it.
- **Game heap:** play both modes in the emulator while reading
  `pebble logs --emulator emery`. The app must log `heap_bytes_free()` and
  `heap_bytes_used()`; record the lowest free share. If the app logs nothing,
  report heap as unmeasured. Never substitute linker figures for it.
- **Clock:** use [watchface-playtest.md](watchface-playtest.md). Verify time,
  continuous Lively and optional Classic/Still modes, focus/Quiet Time/low-battery
  stops, phone settings persistence, data freshness and readable layouts.
  No sound; optional disconnect vibration must respect Quiet Time.
  Gameplay fairness, gameplay heap and a game launcher icon are not clock gates.
- **Screenshots (store releases with visual changes):** PRD 9.4 game scenes
  or clock static/demo scenes,
  captured natively at 200 × 228 with
  `pebble screenshot --emulator emery --no-open FILE`.

## 4. Freeze the candidate and install it

Freeze once, from the audited build, into the ignored `.release/` folder:

```sh
mkdir -p "$RELEASE_DIR"
mkdir "$D" || exit 1           # refuse an existing candidate
cp "$PROJECT/build/$ARTIFACT" "$D/$ARTIFACT"
```

Write `$D/notes.md` (the changelog section body: LF line endings, no trailing
newline) and `$D/manifest.json` with: `app`, `version`, `commit` (`git rev-parse
HEAD`), `pbw_bytes`, `pbw_sha256`, the app image line, `resources_bytes`, the
heap result or `"unmeasured"` (clock: `"not applicable"`), both test results, `notes_sha256`,
`description_sha256`, the SHA-256 of each listing asset for a first
registration, `store_app_id`, `destinations: ["github", "appstore"]` and `frozen_at`.

From this point the frozen file is the only source. Never copy from `build/`
again. If source, notes or listing change, freeze a new candidate; any earlier
approval no longer applies.

Install the frozen bytes on the owner's Pebble Time 2:

```sh
pebble install --cloudpebble "$D/$ARTIFACT"
```

## 5. Approval

Stop and report the version, commit, PBW path, bytes and SHA-256, budgets and
heap, test results, screenshots, release notes and destinations. Destinations
are the GitHub release `$TAG` and the RePebble listing (Dashboard New for
the first registration, otherwise the saved `store_app_id`). For a first
registration, also report the complete proposed listing. For the clock use `docs/watchface-playtest.md`. For the game ask the owner to run the
PRD 12.6 play-test on that installed build (both modes, pause and resume,
alarm while closed and while playing, settings round trip). Then they reply
with approval of that SHA-256 and those destinations.

Record the approval in `manifest.json` (`approval`: SHA-256, destinations,
owner's words, time) only after it is given. If the session already holds
approval for this exact digest and these destinations, continue without
asking again. An approval for other bytes does not carry over.

Use the existing manifest keys: `approval.pbw_sha256`,
`approval.destinations`, `approval.owner_words`, `approval.time`,
`installed_on_pt2_at`, and `owner_playtest`. The store helper checks these
records; it never creates approval or play-test evidence.

## 6. Publish GitHub

Recheck first: the frozen file's SHA-256 equals the approved one,
`manifest.commit` is on `main`, and no tag or release `$TAG` exists.
If one exists with the same PBW, treat that step as done and resume. If it
differs, stop.

```sh
C=$(python3 -c "import json;print(json.load(open('$D/manifest.json'))['commit'])")
git push origin main
git tag -a "$TAG" -m "$NAME $V" "$C"
git push origin "$TAG"
# Clock only: add --latest=false. Game: preserve the existing default.
if [ "$APP" = popeye-gw-clock ]; then
  gh release create "$TAG" "$D/$ARTIFACT" --verify-tag --latest=false \
    --title "$NAME $V" --notes-file "$D/notes.md"
else
  gh release create "$TAG" "$D/$ARTIFACT" --verify-tag \
    --title "$NAME $V" --notes-file "$D/notes.md"
fi
```

## 7. Publish the store

Follow the `popeye-gw-appstore` skill: Dashboard **New** for the first
registration (section 8), otherwise the programmatic existing-listing helper
(section 9). Prefer API read-backs and public HTTP verification throughout.
Use the browser for login or operations not supported by the inspected helper,
and state the specific limitation. Upload only the frozen PBW. Never run top-level
`pebble publish`: it rebuilds the PBW and can create a listing on its own.

## 8. First registration (Dashboard New)

1. **Discover first.** A missing local ID is not proof that no listing exists.
   Check `GET {appstore_api}/api/v1/apps/uuid/{uuid}` and the owner's
   logged-in Dashboard app list. If an app matches the UUID and the source
   repository (a name match alone is not enough), adopt it: record its ID
   (step 4) and switch to section 9. Do not replace its artwork.
2. **Journal intent** in the selected app’s configured `registration_file` before opening the
   form (create its parent directory first): `status: submitting`, `app`, `uuid`, `version`, `pbw_sha256`, `started_at`.
   A `submitting` or `uncertain` journal blocks any further New for this app,
   even for another version. Never delete the journal to get around this.
3. **Submit** at `{dashboard}/dashboard/submit` in the owner's logged-in
   browser. Upload the frozen PBW. Check that the extracted UUID, version,
   configured app type and Emery-only hardware are right. Use the frozen title,
   description, configured category, listed visibility, source repository URL, both
   own icons (so no icon is generated), native screenshots, banner and notes.
   Leave the companion-app fields empty. Review the whole form before
   **Submit**. New publishes at once (`isPublished=true`), so submission is
   the public release.
4. **Record the ID immediately** when the response or Dashboard App
   Information shows it, before any other step. Set the journal to `created`
   with `app_id`. Set `store_app_id` and
   `store_listing_url` (`{public_store}/{id}`) in the selected `apps[APP]` entry of `release-config.json`.
   Also record the assigned `store_app_id` in this candidate’s manifest, without
   changing frozen bytes or approval. This records the destination created by
   the approved first registration.
5. **Lost or unclear response:** mark the journal `uncertain` and do not
   submit again. Recover the ID from the Dashboard list or UUID lookup, then
   continue from step 4.

## 9. Update the existing listing

Use `tools/upload_store.py` for routine release updates. It uses the existing
Pebble Firebase login, creates an in-memory Dashboard session, and wraps the
installed SDK's `_upload_release` method with a restricted transport. It
does not build, create a developer account or app, change listing text or
artwork, or replace screenshots. The default command checks local files only.

Run with the Python interpreter from the installed `pebble` script's shebang
(locally `~/.local/share/uv/tools/pebble-tool/bin/python3`):

```sh
PEBBLE_PYTHON=$(sed -n '1s/^#!//p' "$(command -v pebble)")
"$PEBBLE_PYTHON" tools/upload_store.py "$D" --app "$APP"
"$PEBBLE_PYTHON" tools/upload_store.py "$D" --app "$APP" --inspect
# Only after section 5 approval is recorded:
"$PEBBLE_PYTHON" tools/upload_store.py "$D" --app "$APP" --publish
```

If credentials are missing, the owner runs `pebble login`; never request or
copy tokens into command arguments. If the SDK signatures or API schema
change, inspect and update the helper before using it. Offline helper tests:
`PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_upload_store.py`.
The same command with the Pebble Python also exercises the installed SDK
uploader using a fake POST transport, without credentials or network traffic.

The helper performs these steps:

1. Read back the authenticated Dashboard API app. Its ID and UUID must
   both match. Save a baseline in `$D/store-baseline.json`: title,
   description, website, source, category, visibility, companions, icons,
   banners, screenshots and every prior release (version, notes, published
   state). Keep this first baseline across retries.
2. Stop for review if any of these holds:
   - version `X.Y.Z` already exists as a draft;
   - it exists with a different PBW;
   - a newer version exists.

   If it exists, is published and has the same PBW, the step is already done.
3. Upload the frozen PBW with the approved notes. The helper checks both SDK
   method signatures and the actual multipart fields/bytes, posts once only
   to `{appstore_api}/api/dashboard/apps/{id}/releases`, refuses redirects,
   keeps the token in memory and compares against the original baseline.
   Public PBW downloads use fresh unauthenticated requests and may follow a
   bounded redirect to the observed official R2 storage host. Authenticated
   upload and Dashboard requests still reject redirects.
   `store-attempt.json` is written before POST. If the response is lost, rerun
   only after read-back: a published version with identical bytes and notes
   completes without another POST. If still absent or a draft, keep pending
   and reconcile; don't delete the journal or switch to browser submission.
4. Read back again. Everything in the baseline must be unchanged apart from the
   added release. Restore any change before retrying; never reset the
   baseline to hide one.
5. Change description or artwork only when the owner asked and approved the
   exact text or files. This helper intentionally supports release uploads
   only. Prefer a separately inspected API operation; otherwise use Dashboard
   Edit Listing and check that all other fields survived.

Old browser-created `store-baseline.json` files have a different schema.
Do not replace them to make this helper pass. Finish those candidates using
their original evidence or explicitly migrate and review the baseline.

## 10. Verify (read-only)

```sh
T=$(mktemp -d)
gh release view "$TAG" --json tagName,isDraft,isPrerelease,assets
gh release download "$TAG" -p "$ARTIFACT" -D "$T" && shasum -a 256 "$T/$ARTIFACT"
UUID=$(python3 -c 'import sys; from tools.upload_store import app_config; print(app_config(sys.argv[1])["uuid"])' "$APP")
curl -fsS "https://appstore-api.repebble.com/api/v1/apps/uuid/$UUID?hardware=emery" -o "$T/emery.json"
curl -fsS "https://appstore-api.repebble.com/api/v1/apps/uuid/$UUID" -o "$T/general.json"
```

In both catalog responses, `data[0]` must match `store_app_id` and the UUID.
`latest_release.version` must equal `X.Y.Z` and `release_notes` the frozen
notes. `hardware_platforms` holds objects, so match Emery by name. Download
`latest_release.pbw_file`: its SHA-256 must equal the approved one. Open
`store_listing_url` and check that the version and changelog appear. The GitHub
asset must also hash to the approved SHA-256.

Use two attempts about a minute apart, then report any surface still pending.
The Emery catalog stands in for the phone's My Apps; only the owner looking at
the phone confirms what it shows.

## 11. Record

When everything is verified, add or update the README's download section.
Link only the verified destinations: the store listing and the GitHub
release. Set `manifest.verified_at`. Commit
`docs: record Popeye G&W X.Y.Z publication` and push it. This commit belongs
to an authorized release, never to maintaining this workflow.

## Recovery

- Resume from `$D/manifest.json` and the selected app’s `registration_file`;
  never rebuild a frozen candidate.
- A partial release stays pending until every approved destination verifies.
  Finish the remaining destinations with the same file.
- A tag that points to a different commit, a release asset with a different
  hash, or a store version with a different PBW: stop and report. Never
  overwrite or delete a published version to make the workflow pass.
- Credentials stay outside the repository. The owner logs in; never ask for
  passwords or tokens, never print them, and send tokens only to
  `developer.repebble.com`, `appstore-api.repebble.com` and the official
  sign-in provider.

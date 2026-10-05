# Harbor Catch release workflow

This is the maintained procedure behind the `harbor-catch-build-audit`,
`harbor-catch-release` and `harbor-catch-appstore` skills (`.claude/skills/`,
mirrored in `.agents/skills/`). It implements PRD sections 12, 13 and 15.

Identity lives in [release-config.json](release-config.json). `uuid` identifies
the PBW; `store_app_id` is a separate ID the RePebble store assigns at first
registration and stays `null` until then. The bundle is always
`build/harbor-catch.pbw`, whatever the checkout directory is called.

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
| Free heap while playing | ≥ 20 % | Audit, measured in the emulator |

Never raise a budget to make a release pass; that is an owner decision.

## Essential validation (default for every release)

- Always: both host suites, one clean SDK build, PBW identity and budgets, and
  the originality scan of new or changed text and art.
- Emulator: launch plus the flows the diff touches. At most five screenshots,
  and only for layout or art changes. Allow one emulator retry (`pebble kill`,
  `pebble wipe`), then report the gap instead of debugging at length.
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
git tag --list 'v*' --sort=-v:refname | head -3
gh release list --limit 3
gh run list --branch main --limit 3 --json headSha,status,conclusion,name
```

- Work on `main` with a clean tree. Do not stash, reset or commit unrelated
  work to pass a check; stop and ask.
- Host tests must be green in CI for the commit being released.
- **1.0.0 gate:** M1–M5 accepted (PRD 15), including the owner's art approval
  and M5 play-test sign-off. Ask the owner rather than inferring it.
- Review `git log --oneline vPREV..HEAD` and `git diff --stat vPREV..HEAD`.
  Choose the version by semver from that diff and tell the owner before
  editing anything. The first release is `1.0.0`, which `package.json`
  already holds.

## 2. Release preparation commit

Limit edits to:

- `package.json` `version`;
- `CHANGELOG.md`: a `## X.Y.Z — YYYY-MM-DD` section. Its body is the release
  note used for GitHub and the store;
- the README status line;
- `docs/releases/store-description.txt`, only when its wording changes.

Do not change game code, tuning or art during release preparation. Run the
originality check (PRD 14) on the changelog, README, description, listing
assets and every string in `src/`. Search them for the franchise that the
historical repository name refers to, its characters, its publisher and its
maker, and for any other third-party mark. Commit with
`build: prepare Harbor Catch X.Y.Z`.

## 3. Build audit

Run on the preparation commit with a clean tree:

```sh
./tools/test.sh
./tools/test.sh --sanitize
mkdir -p .release
pebble clean
TERM=xterm pebble build >.release/build.log 2>&1; echo "exit $?"
grep -c "'build' finished" .release/build.log
grep -E 'App image|warning|error' .release/build.log
rm -f .lock-waf*
```

Both the exit status must be 0 and `'build' finished` must appear. The
`.lock-waf*` files contain the host environment: never print or commit them.

PBW identity and sizes:

```sh
python3 - <<'EOF'
import hashlib, json, zipfile
pbw = "build/harbor-catch.pbw"
cfg = json.load(open("docs/release-config.json"))
pkg = json.load(open("package.json"))
with zipfile.ZipFile(pbw) as z:
    info = json.loads(z.read("appinfo.json"))
    sizes = {i.filename: i.file_size for i in z.infolist()}
data = open(pbw, "rb").read()
checks = {
    "uuid": info["uuid"] == cfg["uuid"],
    "version": info["versionLabel"] == pkg["version"],
    "platforms": info["targetPlatforms"] == cfg["platforms"],
    "watchapp": info["watchapp"].get("watchface") is False,
    "name": info["shortName"] == info["longName"] == cfg["display_name"],
    "pbw_budget": len(data) <= cfg["budgets"]["pbw_bytes"],
    "resources_budget": sizes.get("emery/app_resources.pbpack", 0)
        <= cfg["budgets"]["resources_bytes"],
}
print(json.dumps({"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                  "resources": sizes.get("emery/app_resources.pbpack", 0),
                  "checks": checks}, indent=2))
raise SystemExit(0 if all(checks.values()) else 1)
EOF
```

- **Launcher icon:** the menu icon must be a real 25 × 25 PNG canvas
  (`sips -g pixelWidth -g pixelHeight FILE`) and legible in an actual
  launcher screenshot. Compile success does not validate it.
- **Heap:** play both modes in the emulator while reading
  `pebble logs --emulator emery`. The app must log `heap_bytes_free()` and
  `heap_bytes_used()`; record the lowest free share. If the app logs nothing,
  report heap as unmeasured. Never substitute linker figures for it.
- **Screenshots (store releases with visual changes):** the PRD 9.4 scenes,
  captured natively at 200 × 228 with
  `pebble screenshot --emulator emery --no-open FILE`.

## 4. Freeze the candidate and install it

Freeze once, from the audited build, into the ignored `.release/` folder:

```sh
V=X.Y.Z; D=.release/$V
mkdir -p "$D"
cp build/harbor-catch.pbw "$D/harbor-catch.pbw"
```

Write `$D/notes.md` (the changelog section body: LF line endings, no trailing
newline) and `$D/manifest.json` with: `version`, `commit` (`git rev-parse
HEAD`), `pbw_bytes`, `pbw_sha256`, the app image line, `resources_bytes`, the
heap result or `"unmeasured"`, both test results, `notes_sha256`,
`description_sha256`, the SHA-256 of each listing asset for a first
registration, `destinations: ["github", "appstore"]` and `frozen_at`.

From this point the frozen file is the only source. Never copy from `build/`
again. If source, notes or listing change, freeze a new candidate; any earlier
approval no longer applies.

Install the frozen bytes on the owner's Pebble Time 2:

```sh
pebble install --cloudpebble "$D/harbor-catch.pbw"
```

## 5. Approval

Stop and report the version, commit, PBW path, bytes and SHA-256, budgets and
heap, test results, screenshots, release notes and destinations. Destinations
are the GitHub release `vX.Y.Z` and the RePebble listing (Dashboard New for
the first registration, otherwise the saved `store_app_id`). For a first
registration, also report the complete proposed listing. Ask the owner to run the
PRD 12.6 play-test on that installed build (both modes, pause and resume,
alarm while closed and while playing, settings round trip). Then they reply
with approval of that SHA-256 and those destinations.

Record the approval in `manifest.json` (`approval`: SHA-256, destinations,
owner's words, time) only after it is given. If the session already holds
approval for this exact digest and these destinations, continue without
asking again. An approval for other bytes does not carry over.

## 6. Publish GitHub

Recheck first: the frozen file's SHA-256 equals the approved one,
`manifest.commit` is on `main`, and no tag or release `vX.Y.Z` exists.
If one exists with the same PBW, treat that step as done and resume. If it
differs, stop.

```sh
V=X.Y.Z; D=.release/$V
C=$(python3 -c "import json;print(json.load(open('$D/manifest.json'))['commit'])")
git push origin main
git tag -a "v$V" -m "Harbor Catch $V" "$C"
git push origin "v$V"
gh release create "v$V" "$D/harbor-catch.pbw" --verify-tag \
  --title "Harbor Catch $V" --notes-file "$D/notes.md"
```

## 7. Publish the store

Follow the `harbor-catch-appstore` skill: Dashboard **New** for the first
registration (section 8), otherwise an upload to the existing listing
(section 9). Upload only the frozen PBW. Never run top-level
`pebble publish`: it rebuilds the PBW and can create a listing on its own.

## 8. First registration (Dashboard New)

1. **Discover first.** A missing local ID is not proof that no listing exists.
   Check `GET {appstore_api}/api/v1/apps/uuid/{uuid}` and the owner's
   logged-in Dashboard app list. If an app matches the UUID and the source
   repository (a name match alone is not enough), adopt it: record its ID
   (step 4) and switch to section 9. Do not replace its artwork.
2. **Journal intent** in `.release/registration.json` before opening the
   form: `{"status": "submitting", "version", "pbw_sha256", "started_at"}`.
   A `submitting` or `uncertain` journal blocks any further New submission,
   even for another version. Never delete the journal to get around this.
3. **Submit** at `{dashboard}/dashboard/submit` in the owner's logged-in
   browser. Upload the frozen PBW. Check that the extracted UUID, version,
   watchapp type and Emery-only hardware are right. Use the frozen title,
   description, category Games, listed visibility, source repository URL, both
   own icons (so no icon is generated), native screenshots, banner and notes.
   Leave the companion-app fields empty. Review the whole form before
   **Submit**. New publishes at once (`isPublished=true`), so submission is
   the public release.
4. **Record the ID immediately** when the response or Dashboard App
   Information shows it, before any other step. Set the journal to `created`
   with `app_id`. Set `store_app_id` and
   `store_listing_url` (`{public_store}/{id}`) in `release-config.json`.
5. **Lost or unclear response:** mark the journal `uncertain` and do not
   submit again. Recover the ID from the Dashboard list or UUID lookup, then
   continue from step 4.

## 9. Update the existing listing

1. In the owner's logged-in Dashboard, read back the app. Its ID and UUID must
   both match. Save a baseline in `$D/store-baseline.json`: title,
   description, website, source, category, visibility, companions, icons,
   banners, screenshots and every prior release (version, notes, published
   state). Keep this first baseline across retries.
2. Stop for review if any of these holds:
   - version `X.Y.Z` already exists as a draft;
   - it exists with a different PBW;
   - a newer version exists.

   If it exists, is published and has the same PBW, the step is already done.
3. Upload the frozen PBW with the approved notes. The installed Pebble Tool's
   `PublishCommand._upload_release` is the inspected upload path that the
   sibling KasugaBus project wraps in `guarded_store_publisher` and
   `upload_store`. Don't call it ad hoc. Before the first update release, port
   that guarded helper into `tools/` with offline tests. It must check the
   method signature, post only to `{appstore_api}/api/dashboard/apps/{id}/releases`,
   refuse redirects, keep the token in memory and compare against the
   baseline. Alternatively, use an official Dashboard release upload if the
   current UI offers one, after inspecting it.
4. Read back again. Everything in the baseline must be unchanged apart from the
   added release. Restore any change before retrying; never reset the
   baseline to hide one.
5. Change description or artwork only when the owner asked and approved the
   exact text or files. Use Dashboard Edit Listing and check that all other
   fields survived.

## 10. Verify (read-only)

```sh
V=X.Y.Z; D=.release/$V; T=$(mktemp -d)
gh release view "v$V" --json tagName,isDraft,isPrerelease,assets
gh release download "v$V" -p harbor-catch.pbw -D "$T" && shasum -a 256 "$T/harbor-catch.pbw"
UUID=$(python3 -c "import json;print(json.load(open('docs/release-config.json'))['uuid'])")
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
`docs: record Harbor Catch X.Y.Z publication` and push it. This commit belongs
to an authorized release, never to maintaining this workflow.

## Recovery

- Resume from `.release/X.Y.Z/manifest.json` and `.release/registration.json`;
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

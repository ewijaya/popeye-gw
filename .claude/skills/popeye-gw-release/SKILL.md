---
name: popeye-gw-release
description: Prepares, freezes, publishes and verifies a Popeye G&W game or Clock watchface release to its GitHub release and RePebble listing, under the owner's approval of the exact PBW after a play-test on the watch, including the guarded first-registration handoff. Use only when the user explicitly asks to release, ship, publish or verify a Popeye G&W version; not for ordinary builds or audits. Loading or maintaining this skill does not authorize publication.
---

# Popeye G&W release

Accept an app parameter: `popeye-gw` (default) or `popeye-gw-clock`.
Resolve common configuration plus `apps[APP]` from release-config.json using
`tools.upload_store.app_config(APP)`. Read docs/releasing.md “Select the app”.
Use that app's project, artifact, UUID, display name, type, category, budgets,
changelog, description, listing assets, tag prefix, release directory and
registration journal throughout. Never reuse another app's listing or journal.

Read [docs/releasing.md](../../../docs/releasing.md),
[release-config.json](../../../docs/release-config.json), PRD sections 12–15,
the [build audit](../popeye-gw-build-audit/SKILL.md) and the
[store skill](../popeye-gw-appstore/SKILL.md) before acting.

Read identity and registration state from the selected app configuration and
its registration journal. A null ID does not prove no listing exists.
Destinations are the repository's GitHub release and that app's RePebble listing.
The clock uses `clock-vX.Y.Z` and `gh release create --latest=false`, preserving
the game's Latest release. Never bump the game when releasing the clock.

## Workflow

1. **Preflight** (releasing.md 1): clean `main`, CI green on the release
   commit, existing tags and releases checked. For game 1.0.0 only, confirm with the
   owner that M1–M5 are accepted; don't infer it from the code.
2. **Version:** choose semver from the diff since the previous tag and tell the
   owner before editing. The first release is 1.0.0, already in
   the selected project’s `package.json`.
3. **Prepare** (releasing.md 2): edit only the version, the configured changelog section,
   the README status and, if its wording changes, the store description. No
   game, tuning or art changes. Run the identity check, then commit.
4. **Audit** the preparation commit with the build-audit skill. One clean
   build.
5. **Freeze once** into the configured `release_dir` plus `/X.Y.Z/` with `manifest.json` and
   `notes.md`, record `app` in its manifest, then install **that file** on the owner's watch with
   `pebble install --cloudpebble`.
6. **Stop for approval** (below). Use `docs/watchface-playtest.md` for the
   clock, PRD 12.6 for the game; never infer device results.
7. **Publish** the GitHub release, then the store (releasing.md 6–9), using
   only the frozen PBW. For the existing store listing, use
   `tools/upload_store.py --app APP` by default, with the installed Pebble Tool's Python.
   Use programmatic read-backs and verification. Browser automation is a
   fallback for login, first registration, or a specific operation the helper
   cannot safely perform; explain the limitation before falling back.
8. **Verify** read-only (releasing.md 10): two attempts, then report what is
   still pending.
9. **Record** the verified destinations in the README, then commit and push
   (releasing.md 11).

**Publication requires the owner's explicit approval of the exact frozen
candidate: its SHA-256, version and destinations, plus the full proposed
listing for a first registration. The approval comes after the owner's
play-test of that installed build. Installation alone is not approval.**
Loading or maintaining this skill does not provide that approval. Before
pushing the tag, creating the GitHub release or touching the store, stop if
approval is missing. Quote this requirement and give the candidate's path,
SHA-256, bytes, budgets, heap, test results, notes and destinations for review.
Never report play-test results you did not observe. If the session already
holds approval of this exact digest and these destinations, continue without
asking again. Approval of other bytes, or an older version, does not carry
over. Record approval in the manifest only after it is given.

A request to "publish this new app everywhere" covers the first registration,
listing and publication, still subject to that exact-candidate approval; don't
ask separately for permission to register.

## Guards

- Never rebuild during or after freezing; never use `build/` output after the
  freeze. A source, notes or listing change means a new candidate and a new
  approval.
- Never run top-level `pebble publish`. It rebuilds and can create a listing.
- A failed or timed-out programmatic upload must be reconciled read-only before
  any browser fallback or retry; never submit the same release blindly.
- Never overwrite or delete an existing tag, release asset or store version.
  The same bytes mean that step is done; different bytes mean stop. Never
  publish a store draft silently.
- Preserve the existing listing's metadata, artwork and earlier releases (store
  skill). Never repeat Dashboard New while a registration is uncertain.
- A partial release stays pending until every approved destination verifies.
  Advertise only verified destinations.
- The owner logs in. Never ask for passwords or tokens, never print them, and
  send tokens only to the official RePebble hosts.
- Keep evidence honest: unmeasured heap, emulator-only checks and pending
  propagation are reported as such.

Validation follows releasing.md "Essential validation". Aim for about ten
minutes of agent time; allow one emulator retry, then report the gap. Required
failures stay failures unless the owner explicitly accepts an exception.

Final report: version, commit and tag, PBW SHA-256 and bytes, budgets and heap,
test and play-test status, a URL and status for each destination, preserved
listing fields, README changes and the final Git state.

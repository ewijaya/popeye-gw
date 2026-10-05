---
name: harbor-catch-release
description: Prepares, freezes, publishes and verifies a Harbor Catch release to its GitHub release and RePebble listing, under the owner's approval of the exact PBW after a play-test on the watch, including the guarded first-registration handoff. Use only when the user explicitly asks to release, ship, publish or verify a Harbor Catch version; not for ordinary builds or audits. Loading or maintaining this skill does not authorize publication.
---

# Harbor Catch release

Read [docs/releasing.md](../../../docs/releasing.md),
[release-config.json](../../../docs/release-config.json), PRD sections 12–15,
the [build audit](../harbor-catch-build-audit/SKILL.md) and the
[store skill](../harbor-catch-appstore/SKILL.md) before acting.

Identity: display name Harbor Catch, UUID
`6b7c8c28-36f0-47ca-a073-3e8f33efcaf0`, Emery only, interactive watchapp,
category Games, bundle `build/harbor-catch.pbw`. Read `store_app_id` from the
configuration and `.release/registration.json`. Don't assume registration
status from this skill. Destinations are the repository's GitHub release and
the RePebble listing.

## Workflow

1. **Preflight** (releasing.md 1): clean `main`, CI green on the release
   commit, existing tags and releases checked. For 1.0.0, confirm with the
   owner that M1–M5 are accepted; don't infer it from the code.
2. **Version:** choose semver from the diff since the previous tag and tell the
   owner before editing. The first release is 1.0.0, already in
   `package.json`.
3. **Prepare** (releasing.md 2): edit only the version, the CHANGELOG section,
   the README status and, if its wording changes, the store description. No
   game, tuning or art changes. Run the originality check, then commit.
4. **Audit** the preparation commit with the build-audit skill. One clean
   build.
5. **Freeze once** into `.release/X.Y.Z/` with `manifest.json` and
   `notes.md`, then install **that file** on the owner's watch with
   `pebble install --cloudpebble`.
6. **Stop for approval** (below).
7. **Publish** the GitHub release, then the store (releasing.md 6–9), using
   only the frozen PBW.
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

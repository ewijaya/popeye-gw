---
name: popeye-gw-build-audit
description: Audits the Popeye G&W game or Clock watchface with the selected host suites, a clean Emery SDK build, PBW identity checks and the app-image, resource, PBW and heap budgets without publishing or changing versions. Use when the user asks to test, build, audit or measure Popeye G&W (tools/test.sh, pebble build, app image bytes, PBW size or SHA-256, resources, heap, launcher icon), or as the validation step of a release.
---

# Popeye G&W build audit

Accept an app parameter: `popeye-gw` (default) or `popeye-gw-clock`.
Resolve common configuration plus `apps[APP]` from release-config.json using
`tools.upload_store.app_config(APP)`. Read docs/releasing.md “Select the app”.
Use that app's project, artifact, UUID, display name, type, category, budgets,
changelog, description, listing assets, tag prefix, release directory and
registration journal throughout. Never reuse another app's listing or journal.

Follow [docs/releasing.md](../../../docs/releasing.md) section 3 and the
environment notes in [CLAUDE.md](../../../CLAUDE.md). Budgets are in
[release-config.json](../../../docs/release-config.json). An audit never bumps
versions, commits, tags, pushes, installs on the watch or publishes.

Record `pebble --version`, the active SDK (4.33.1), the commit and whether the
tree is clean. Audit a clean tree; otherwise label the results as unreleasable.

1. **Host suites:** game: `./tools/test.sh` and `./tools/test.sh --sanitize`;
   clock: `./watchface/test.sh` and `./watchface/test.sh --sanitize`. For the game, the
   fairness bot must report zero unavoidable misses over 10,000 seeds per
   mode. Read failure output; never hide an exit status in a pipeline.
2. **Clean build:** delete the configured `project_dir`'s ignored `build/` directory
   (`pebble clean` can leave a stale PBW), then `TERM=xterm pebble build` into a log.
   It passes only with exit status 0 **and** the literal `'build' finished`.
   Every build enforces the 61,440-byte app image; record the printed
   `App image` line. Report whether the linker dropped unused engine code, so a
   shell app's size isn't mistaken for the playable app's.
3. **PBW identity:** read `appinfo.json` from the selected project's
   `build/<artifact_name>`. UUID, name and platform must match configuration;
   versionLabel must match that project's package.json. `watchapp.watchface`
   must be true for the clock, false for the game. Record bytes, SHA-256 and
   resource pack size against selected budgets using releasing.md's snippet.
4. **Game heap only:** measure in the emulator during both modes from logged
   `heap_bytes_free()` and `heap_bytes_used()`; at least 20 % must stay free.
   With no log, report "unmeasured". Never substitute linker free RAM for it,
   or emulator results for observations on the watch.
5. **Game launcher icon** (once art exists): a real 25 × 25 PNG canvas that is
   legible in an actual launcher screenshot. A successful compile does not
   prove it.
6. **Identity (PRD 14):** verify the configured display name and
   artifact names and approved Popeye, Olive Oyl and Brutus character art.
   Check the LCD presentation and remove stale product or cast identifiers.
7. **Clock behaviour:** follow `docs/watchface-playtest.md`: renders, keeps
   time, selected animation mode (continuous Lively by default; once per
   minute in Classic), covered/Quiet Time/low-battery stops, Clay settings
   round trip and persistence, data freshness and accessibility layouts.
   No sound; optional disconnect vibration respects Quiet Time.
   Game fairness/heap/icon gates are not applicable.
8. **Cleanup:** delete `.lock-waf*` after the build. They hold the host
   environment, so never print, quote or commit them.

Take emulator screenshots only when asked or when layout or art changed: at
most five, native 200 × 228. Allow one emulator retry (`pebble kill`, then
`pebble wipe` only with owner permission), then report the gap.

Report a compact table: tests, build, app image and headroom, PBW bytes and
SHA-256, resources, heap, icon, identity, and anything unmeasured. A
required failure stays a failure.

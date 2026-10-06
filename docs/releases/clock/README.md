# Popeye G&W Clock 1.0.0 preparation

Status: **1.0.0 published and verified**. See [publication record](1.0.0.md).

The current watchface includes Clay settings, independent continuous animation,
four layout presets, information rows, accessible text and Japanese dates.
The four native gallery screenshots were refreshed from the release build.
See [the implementation audit](../../watchface-audit.md) and the final review
under `.release/popeye-gw-clock/1.0.0/` in the isolated release checkout.

| Listing field | Proposed value |
|---|---|
| Name | Popeye G&W Clock |
| Type / platform | Watchface / Emery (Pebble Time 2) |
| Category / visibility | Faces / listed |
| Version | 1.0.0 |
| UUID | `9f808f24-978d-4e7d-9af1-16bd31e4b8c5` |
| Description | [store-description.txt](store-description.txt) |
| Gallery, banner and icons | [Listing assets](listing/README.md) |
| Website / source | https://github.com/ewijaya/popeye-gw |
| Companion apps | None |
| Store App ID | `8e8e130581e7467d9d7b0138` — registered and verified |
| GitHub release tag | `clock-v1.0.0`, `--latest=false` |
| Release notes | [Watchface changelog](../../../watchface/CHANGELOG.md) |

On 2026-10-06 the original minute-demo development PBW was installed successfully through
CloudPebble: 14,572 bytes, SHA-256
`12bf420c966e08e227daba870e8a6e8fb353cd021f330ccda435586d8ed420a8`.
Asked about readability, the roughly 10-second minute demo, clipping, timing
and unwanted vibration/sound, the owner replied **“everything looks great!”**.
This is positive normal-use feedback, not evidence for every edge case or
approval to publish. See the [remaining device checklist](../../watchface-playtest.md).

Verification for the release-tooling change:

- Full normal and sanitizer host suites passed, including watchface tests.
- Each fairness run covered 10,000 seeds per game mode through 1,000 points,
  with zero unavoidable misses.
- All 14 uploader tests passed using Pebble Python, including the installed
  SDK integration with fake transport. No store requests were made.
- Native emulator screenshots show the clock and live minute demo.
- The existing development PBW's UUID, name, version, type and platform matched
  the clock configuration. No new release build or freeze was performed.

Before publication: finish the unobserved device checks, settle the local
changes, obtain an explicit merge/push/release request, merge to main and
wait for green CI, then audit and freeze the exact candidate. Install and
play-test that frozen PBW. Owner approval must cover its hash, version,
GitHub/store destinations and this complete listing before registration or
publication. The game's version and Latest release stay unchanged.

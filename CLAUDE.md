# Popeye G&W repository conventions

## Product and scope

- The PRD was retired after v1.0.2; read the archived copy with
  `git show v1.0.2:PRD.md` before changing game rules or architecture. "PRD
  section N" references point to it. Record later decisions and interpretations
  in `docs/` (e.g. `docs/v1.1-notes.md`), and ask the owner about scope changes.
- Popeye G&W is a fan-made LCD-style catch game for Pebble Time 2 only:
  Emery, 200 × 228, 64 colours, inspired by Nintendo’s Popeye Game & Watch.
  Use the approved cast: Popeye, Olive Oyl and Brutus.
- Follow PRD section 14 for the nostalgic LCD presentation and product identity.
  Display name: `Popeye G&W`; repository, package and bundle slug: `popeye-gw`.
- Work only on the requested milestone. Store publishing requires the owner's
  explicit approval of the exact build after the required watch play-test.
- Releases follow `docs/releasing.md` through the `popeye-gw-build-audit`,
  `popeye-gw-release` and `popeye-gw-appstore` skills. Keep
  `.claude/skills/` and `.agents/skills/` identical.

## Code and verification

- Keep `src/c/game.c` and `game.h` pure C99 with no Pebble includes, allocation,
  wall-clock reads or platform calls. Use deterministic xorshift randomness.
- Put pacing values in `src/c/tuning.h`. Keep game rules independent of pixels,
  rendering, persistent storage and app lifecycle. Keep input immediate.
- Test rules with host C tests, including deterministic replay and the fairness
  bot over 10,000 seeds per mode through 1,000 points with zero unavoidable misses.
- Run host tests on every push through the repository's automated workflow.
  Keep SDK builds local. Run the host suite and a clean SDK build before pushing
  game logic changes. Inspect failures; do not hide command failures in pipelines.
- Use small conventional commits (`feat:`, `test:`, `fix:`, `docs:`, `ci:`,
  `build:`). Do not add AI attribution or co-author trailers.

## Local environment

- Pebble Tool 5.0.40, active SDK 4.33.1, on an arm64 host. SDK root:
  `~/Library/Application Support/Pebble SDK/SDKs/4.33.1`.
- Use the SDK's bundled compiler at
  `toolchain/arm-none-eabi/bin/arm-none-eabi-gcc` under that root; do not install
  another compiler. Host tests use `/usr/bin/cc` explicitly because `cc` may be
  aliased. Treat the shell's default `node` as broken; this C-only project needs
  no Node tooling. Check any explicitly selected runtime before using it.
- Check both the build exit status and the literal `'build' finished` in its
  output. Terminal/tput output has previously hidden a failed build. If needed,
  use `TERM=xterm pebble build` and inspect the saved log.
- SDK app image hard limit: 65,535 bytes. Repository budget: **61,440 bytes** for
  `.text + .data + .bss`. Measure the `dec` column of:

  ```sh
  "$HOME/Library/Application Support/Pebble SDK/SDKs/4.33.1/toolchain/arm-none-eabi/bin/arm-none-eabi-size" build/emery/pebble-app.elf
  ```

  Report whether unused engine code was removed by the linker, so a foundation
  app's size is not mistaken for the future playable app's size.
- Emulator commands (run only when needed for the requested milestone):

  ```sh
  pebble install --emulator emery build/popeye-gw.pbw
  pebble screenshot --emulator emery --no-open screenshot.png
  ```

  If installs stall, run `pebble kill`, then `pebble wipe`, then retry. `wipe`
  resets local emulator data; do not use `--everything` for routine recovery.
- Physical Pebble Time 2 installation through the phone connection:

  ```sh
  pebble install --cloudpebble build/popeye-gw.pbw
  ```

  Pass the PBW path explicitly: the CLI's default is based on the checkout
  directory name, whereas this project builds `popeye-gw.pbw`.
  Never publish to the RePebble store without explicit approval of that exact
  build.

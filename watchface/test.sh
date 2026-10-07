#!/bin/sh
# Host tests for the scene, settings/data lifecycle and phone companion.
set -eu

watch_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$watch_dir/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/popeye-gw-clock-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

case "${1:-}" in
  '') set -- -O2 ;;
  --sanitize) set -- -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  *) printf '%s\n' 'Usage: watchface/test.sh [--sanitize]' >&2; exit 2 ;;
esac

# The shared sources and art are referenced, never copied: fail if they move.
for shared in src/c/scene.c src/c/clock.c src/c/game.c resources/images/backdrop.png \
    resources/images/segments.png; do
  test -f "$repo_dir/$shared"
  grep -q "$(basename "$shared")" "$watch_dir/wscript" "$watch_dir/package.json"
done
if find "$watch_dir" \( -path "$watch_dir/build" -o -type d -name node_modules \) -prune \
    -o \( -name 'scene.c' -o -name 'clock.c' -o -name 'game.c' -o -name '*.png' \) \
    ! -path "$watch_dir/resources/images/menu-icon.png" -print | grep -q .; then
  echo 'watchface/ must not contain copies of shared sources or art' >&2
  exit 1
fi

# shellcheck disable=SC2086
/usr/bin/cc -std=c99 -Wall -Wextra -Werror -pedantic "$@" -I"$repo_dir/src/c" -I"$watch_dir/src/c" \
  "$repo_dir/src/c/game.c" "$repo_dir/src/c/scene.c" "$repo_dir/src/c/clock.c" \
  "$watch_dir/src/c/face.c" "$watch_dir/tests/test_face.c" -o "$test_dir/test_face"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_face"

/usr/bin/cc -std=c99 -Wall -Wextra -Werror -pedantic "$@" -I"$watch_dir/src/c" \
  "$watch_dir/src/c/settings.c" "$watch_dir/src/c/data.c" \
  "$watch_dir/tests/test_settings.c" -o "$test_dir/test_settings"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_settings"

if ! test -f "$watch_dir/tests/test_pkjs.js"; then
  printf '%s\n' 'Phone tests missing: watchface/tests/test_pkjs.js' >&2
  exit 1
fi
if ! test -d "$watch_dir/node_modules/@rebble/clay"; then
  printf '%s\n' 'Clay dependency missing; install watchface dependencies with npm ci first.' >&2
  exit 1
fi
if test -f "$watch_dir/tests/test_pkjs.js"; then
  watchface_node=${WATCHFACE_NODE:-}
  if test -z "$watchface_node"; then
    for candidate in /Users/e_wijaya_ap/.nvm/versions/node/v20.19.5/bin/node /opt/homebrew/opt/node@20/bin/node /opt/homebrew/bin/node /usr/local/bin/node "$(command -v node || true)"; do
      if test -x "$candidate" && "$candidate" -e 'if (!JSON || !require("assert")) process.exit(1)' >/dev/null 2>&1; then
        watchface_node=$candidate
        break
      fi
    done
  fi
  if test -z "$watchface_node" || ! "$watchface_node" -e 'require("assert")' >/dev/null 2>&1; then
    printf '%s\n' 'A working Node runtime is required; set WATCHFACE_NODE to its executable.' >&2
    exit 1
  fi
  (cd "$watch_dir" && "$watchface_node" tests/test_pkjs.js)
fi

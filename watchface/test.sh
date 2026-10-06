#!/bin/sh
# Host tests for the watchface scene logic. Uses the app's shared modules by path.
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
for shared in src/c/scene.c src/c/clock.c src/c/game.c resources/images/backdrop-ghosts.png \
    resources/images/segments.png; do
  test -f "$repo_dir/$shared"
  grep -q "$(basename "$shared")" "$watch_dir/wscript" "$watch_dir/package.json"
done
if find "$watch_dir" -path "$watch_dir/build" -prune -o \( -name 'scene.c' -o -name 'clock.c' \
    -o -name 'game.c' -o -name '*.png' \) -print | grep -q .; then
  echo 'watchface/ must not contain copies of shared sources or art' >&2
  exit 1
fi

# shellcheck disable=SC2086
/usr/bin/cc -std=c99 -Wall -Wextra -Werror -pedantic "$@" -I"$repo_dir/src/c" -I"$watch_dir/src/c" \
  "$repo_dir/src/c/game.c" "$repo_dir/src/c/scene.c" "$repo_dir/src/c/clock.c" \
  "$watch_dir/src/c/face.c" "$watch_dir/tests/test_face.c" -o "$test_dir/test_face"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_face"

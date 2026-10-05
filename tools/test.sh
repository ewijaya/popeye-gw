#!/bin/sh
# Host-only C99 suite plus the art pipeline check (Python 3 standard library);
# no SDK or Node runtime required.
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/popeye-gw-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

case "${1:-}" in
  '') set -- -O2 ;;
  --sanitize) set -- -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  *) printf '%s\n' 'Usage: tools/test.sh [--sanitize]' >&2; exit 2 ;;
esac

python3 "$repo_dir/tools/build_art.py" --check

cflags="-std=c99 -Wall -Wextra -Werror -pedantic"
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/src/c" "$repo_dir/src/c/game.c" \
  "$repo_dir/src/c/scene.c" "$repo_dir/tests/test_scene.c" -o "$test_dir/test_scene"
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/src/c" "$repo_dir/src/c/game.c" \
  "$repo_dir/tests/test_game.c" -o "$test_dir/test_game"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_scene"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_game"

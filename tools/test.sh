#!/bin/sh
# Host-only C99 suite plus the art pipeline check (Python 3 standard library);
# no SDK or Node runtime required.
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/popeye-gw-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

watch_args=${1:+"$1"}
case "${1:-}" in
  '') set -- -O2 ;;
  --sanitize) set -- -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer ;;
  *) printf '%s\n' 'Usage: tools/test.sh [--sanitize]' >&2; exit 2 ;;
esac

python3 "$repo_dir/tools/build_art.py" --check
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s "$repo_dir/tests" -p test_upload_store.py

cflags="-std=c99 -Wall -Wextra -Werror -pedantic"
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/src/c" "$repo_dir/src/c/game.c" \
  "$repo_dir/src/c/scene.c" "$repo_dir/tests/test_scene.c" -o "$test_dir/test_scene"
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/src/c" "$repo_dir/src/c/game.c" \
  "$repo_dir/tests/test_game.c" -o "$test_dir/test_game"
# Compile the real persistence/wakeup adapters against a small in-memory SDK fake.
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/tests/fake_pebble" -I"$repo_dir/src/c" \
  "$repo_dir/src/c/store.c" "$repo_dir/src/c/storage.c" "$repo_dir/src/c/clock.c" \
  "$repo_dir/src/c/alarm.c" "$repo_dir/src/c/glance.c" "$repo_dir/src/c/orientation.c" "$repo_dir/src/c/scene.c" "$repo_dir/src/c/game.c" \
  "$repo_dir/tests/fake_pebble/fake.c" "$repo_dir/tests/test_m4.c" -o "$test_dir/test_m4"
# Compile the actual finite feedback timer/haptics adapter against the SDK fake.
# shellcheck disable=SC2086
/usr/bin/cc $cflags "$@" -I"$repo_dir/tests/fake_pebble" -I"$repo_dir/src/c" \
  "$repo_dir/src/c/feedback.c" "$repo_dir/src/c/feedback_service.c" \
  "$repo_dir/src/c/clock.c" "$repo_dir/src/c/scene.c" "$repo_dir/src/c/game.c" \
  "$repo_dir/tests/fake_pebble/fake.c" "$repo_dir/tests/test_m5.c" -o "$test_dir/test_m5"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_m5"
TZ=UTC UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_m4"
TZ=America/New_York UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_m4"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_scene"
UBSAN_OPTIONS=halt_on_error=1 "$test_dir/test_game"
# Companion watchface scene logic and its no-copy guard.
# shellcheck disable=SC2086
sh "$repo_dir/watchface/test.sh" $watch_args

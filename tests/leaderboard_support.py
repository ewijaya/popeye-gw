"""Shared helpers: the native verifier and the replay generator, from the real C sources.

tools/test.sh builds both (sanitized under --sanitize) and passes them in POPEYE_VERIFIER and
POPEYE_MAKE_REPLAY; run on their own, the tests compile them into a temporary directory."""
import atexit
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CC = "/usr/bin/cc"
CFLAGS = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2", "-I" + str(ROOT / "src/c")]
ENGINE = [str(ROOT / "src/c/game.c"), str(ROOT / "src/c/replay.c")]
_built = {}


def _build(name, source, env_name):
    if os.environ.get(env_name):
        return os.environ[env_name]
    if name not in _built:
        if not os.path.exists(CC):
            raise unittest.SkipTest("/usr/bin/cc is missing: cannot build the native replay verifier")
        directory = tempfile.mkdtemp(prefix="popeye-gw-lb.")
        atexit.register(shutil.rmtree, directory, True)
        target = os.path.join(directory, name)
        subprocess.run([CC] + CFLAGS + [str(ROOT / source)] + ENGINE + ["-o", target], check=True)
        _built[name] = target
    return _built[name]


def verifier_path():
    return _build("replay_verify", "tools/replay_verify.c", "POPEYE_VERIFIER")


def make_replay(date, bot_seed=1, noise=0, swapped=False, *options):
    """Replay bytes of one imperfect-bot Daily round on the given date (YYYYMMDD). Options such
    as "seed=5", "mode=A" or "limit=30000" record a round that is not a real Daily."""
    program = _build("make_replay", "tests/make_replay.c", "POPEYE_MAKE_REPLAY")
    args = [program, str(date), str(bot_seed), str(noise)] + (["swapped"] if swapped else []) + list(options)
    return subprocess.run(args, check=True, stdout=subprocess.PIPE).stdout


def fnv1a(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def reseal(data):
    """Recompute the trailing checksum so a forged replay is well formed."""
    body = bytes(data[:-4])
    return body + fnv1a(body).to_bytes(4, "little")


def set_u32(data, offset, value):
    data = bytearray(data)
    data[offset:offset + 4] = value.to_bytes(4, "little")
    return bytes(data)

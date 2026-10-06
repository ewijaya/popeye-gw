"""Runs tests/test_pkjs.js with node when a working one exists (the SDK build needs none)."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


def working_node():
    for candidate in (os.environ.get("NODE"), shutil.which("node")):
        if not candidate:
            continue
        try:
            done = subprocess.run([candidate, "--version"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=20)
        except (OSError, subprocess.SubprocessError):
            continue
        if done.returncode == 0 and re.match(rb"^v\d+", done.stdout):
            return candidate
    return None


class PhoneSideTest(unittest.TestCase):
    def test_phone_javascript(self):
        node = working_node()
        if node is None:
            self.skipTest("no working node (set NODE=/path/to/node): phone JS tests not run")
        done = subprocess.run([node, str(ROOT / "tests/test_pkjs.js")], stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=60)
        self.assertEqual(done.returncode, 0, done.stdout.decode())
        self.assertIn(b"pkjs tests passed", done.stdout)

    def test_message_keys_match_the_watch_protocol(self):
        # Runs without node: the keys named in JS, the C adapter and package.json agree.
        import json
        keys = json.loads((ROOT / "package.json").read_text())["pebble"]["messageKeys"]
        js = (ROOT / "src/js/pebble-js-app.js").read_text()
        c = (ROOT / "src/c/online_net.c").read_text()
        for key in keys:
            self.assertIn("'%s'" % key, js, key)
            self.assertIn("MESSAGE_KEY_%s" % key, c, key)
        self.assertEqual(set(re.findall(r"'(LB_[A-Z]+)'", js)), set(keys))
        self.assertEqual(set(re.findall(r"MESSAGE_KEY_(LB_[A-Z]+)", c)), set(keys))
        self.assertIn("var LEADERBOARD_URL = '';", js)


if __name__ == "__main__":
    unittest.main()

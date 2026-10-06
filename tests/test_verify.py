"""The native Daily replay verifier (tools/replay_verify.c)."""
import json
import subprocess
import unittest

import leaderboard_support as support

DATE = 20261006


def run(data):
    done = subprocess.run([support.verifier_path()], input=data, stdout=subprocess.PIPE, timeout=10)
    lines = done.stdout.decode().splitlines()
    assert len(lines) == 1, done.stdout
    return done.returncode, json.loads(lines[0])


class VerifierTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.replay = support.make_replay(DATE, 7, 8)

    def failure(self, data, error):
        status, result = run(data)
        self.assertEqual((status, result), (1, {"ok": False, "error": error}))

    def test_valid_replay(self):
        status, result = run(self.replay)
        self.assertEqual(status, 0)
        self.assertEqual(sorted(result), ["date", "misses", "ok", "score"])
        self.assertIs(result["ok"], True)
        self.assertEqual(result["date"], DATE)
        self.assertGreater(result["score"], 0)
        self.assertIn(result["misses"], (1, 2, 3))

    def test_reads_a_file_argument(self):
        import os
        import tempfile
        with tempfile.NamedTemporaryFile(delete=False) as handle:
            handle.write(self.replay)
        try:
            done = subprocess.run([support.verifier_path(), handle.name], stdout=subprocess.PIPE)
            self.assertEqual(done.returncode, 0)
            self.assertEqual(json.loads(done.stdout)["date"], DATE)
            done = subprocess.run([support.verifier_path(), handle.name + ".missing"], stdout=subprocess.PIPE)
            self.assertEqual((done.returncode, json.loads(done.stdout)["error"]), (1, "unreadable"))
        finally:
            os.unlink(handle.name)

    def test_swapped_controls_still_verify(self):
        status, result = run(support.make_replay(DATE, 3, 5, swapped=True))
        self.assertEqual((status, result["ok"]), (0, True))

    def test_tampered_score_with_valid_checksum(self):
        score = int.from_bytes(self.replay[16:20], "little")
        for forged in (score + 1, score * 10, 0):
            self.failure(support.reseal(support.set_u32(self.replay, 16, forged)), "score_mismatch")

    def test_tampered_score_without_resealing(self):
        self.failure(support.set_u32(self.replay, 16, 999), "bad_checksum")

    def test_wrong_seed(self):
        # Forging the header seed breaks the replay itself; a genuine round played on another
        # seed is well formed and verifies, so the seed rule has to catch it.
        status, result = run(support.reseal(support.set_u32(self.replay, 8, 12345)))
        self.assertEqual((status, result["ok"]), (1, False))
        self.failure(support.make_replay(DATE, 7, 8, False, "seed=12345"), "bad_seed")
        self.failure(support.make_replay(DATE, 7, 8, False, "seed=%d" % DATE), "bad_seed")

    def test_wrong_date_for_the_seed(self):
        self.failure(support.reseal(support.set_u32(self.replay, 12, DATE + 1)), "bad_seed")

    def test_impossible_date(self):
        # Not a calendar day (the seed is derived from it, so it is the date rule that fires).
        self.failure(support.make_replay(20261340, 7, 8), "bad_date")

    def test_wrong_mode_and_limit(self):
        self.failure(support.make_replay(DATE, 7, 8, False, "mode=A"), "bad_mode")
        self.failure(support.make_replay(DATE, 7, 8, False, "limit=30000"), "bad_limit")
        self.failure(support.make_replay(DATE, 7, 8, False, "limit=0"), "bad_limit")

    def test_truncated(self):
        for length in (len(self.replay) - 1, len(self.replay) // 2, 30, 26, 4, 1):
            status, result = run(self.replay[:length])
            self.assertEqual(status, 1)
            self.assertIn(result["error"], ("bad_checksum", "bad_format"))
        self.failure(b"", "empty")

    def test_trailing_garbage_and_oversize(self):
        status, result = run(self.replay + b"\0")
        self.assertEqual((status, result["ok"]), (1, False))
        self.failure(bytes(2049), "too_long")
        self.failure(bytes(100000), "too_long")

    def test_overflow_flag_is_unverifiable(self):
        data = bytearray(self.replay)
        data[2] |= 2
        self.failure(support.reseal(bytes(data)), "unverifiable")

    def test_reserved_and_unknown_flags(self):
        data = bytearray(self.replay)
        data[2] |= 4
        self.failure(support.reseal(bytes(data)), "bad_format")
        data = bytearray(self.replay)
        data[3] = 1
        self.failure(support.reseal(bytes(data)), "bad_format")


if __name__ == "__main__":
    unittest.main()

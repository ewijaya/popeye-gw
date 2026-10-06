"""The Daily leaderboard server (server/leaderboard.py) against the real native verifier."""
import base64
import contextlib
import datetime
import hashlib
import hmac
import http.client
import json
import logging
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import threading
import unittest

import leaderboard_support as support

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "server"))
import leaderboard  # noqa: E402

TODAY = datetime.date(2026, 10, 6)
SECRET = b"test-secret-not-for-production"
TOKEN_A, TOKEN_B, TOKEN_C = "a" * 32, "b" * 32, "c" * 32


def run_verifier(replay):
    done = subprocess.run([support.verifier_path()], input=replay, stdout=subprocess.PIPE)
    return json.loads(done.stdout)


class Clock:
    def __init__(self):
        self.now = 1000.0

    def __call__(self):
        return self.now


class ServerCase(unittest.TestCase):
    rate_limit = 1000
    verifier = None

    @classmethod
    def setUpClass(cls):
        logging.disable(logging.CRITICAL)  # The error paths log on purpose.
        cls.addClassCleanup(logging.disable, logging.NOTSET)
        cls.verifier_binary = support.verifier_path()  # Skips cleanly without a compiler.

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.db_path = os.path.join(self.directory.name, "board.sqlite3")
        self.clock = Clock()
        self.ticks = iter(range(1000, 10 ** 9))  # Every write is one millisecond after the last.
        self.board = leaderboard.Board(self.db_path, self.verifier or self.verifier_binary, SECRET, lambda: TODAY,
                                       lambda: next(self.ticks))
        self.server = leaderboard.make_server(self.board, "127.0.0.1", 0, self.rate_limit, clock=self.clock)
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.02}, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(5)
        self.directory.cleanup()

    def request(self, method, path, body=None, headers=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=10)
        try:
            connection.request(method, path, body=body, headers=headers or {})
            response = connection.getresponse()
            data = response.read()
            return response.status, json.loads(data), response
        finally:
            connection.close()

    def submit(self, replay, player=TOKEN_A, name="Popeye", extra=None):
        body = {"player": player, "name": name, "replay": base64.b64encode(replay).decode()}
        body.update(extra or {})
        return self.request("POST", "/v1/daily", json.dumps(body), {"Content-Type": "application/json"})

    def daily(self, date, query=""):
        return self.request("GET", "/v1/daily/%d%s" % (date, query))

    def rows(self):
        with contextlib.closing(sqlite3.connect(self.db_path)) as db:
            return db.execute("SELECT date, player, name, score FROM scores ORDER BY score DESC").fetchall()


class SubmitTest(ServerCase):
    def test_accepts_a_verified_replay_and_ranks_it(self):
        replay = support.make_replay(20261006, 7, 8)
        score = run_verifier(replay)["score"]
        status, body, response = self.submit(replay)
        self.assertEqual((status, body), (200, {"rank": 1, "total": 1, "best": score}))
        self.assertEqual(response.getheader("Content-Type"), "application/json")
        self.assertEqual(response.getheader("Cache-Control"), "no-store")
        self.assertEqual(self.daily(20261006)[1],
                         {"date": 20261006, "total": 1, "top": [{"rank": 1, "name": "Popeye", "score": score}]})

    def test_ranks_players_by_verified_score(self):
        good, middle, poor = (support.make_replay(20261006, 7, noise) for noise in (0, 8, 2))
        scores = [run_verifier(r)["score"] for r in (good, middle, poor)]
        self.assertTrue(scores[0] > scores[1] > scores[2] > 0, scores)
        self.assertEqual(self.submit(middle, TOKEN_B, "Olive")[1], {"rank": 1, "total": 1, "best": scores[1]})
        self.assertEqual(self.submit(poor, TOKEN_C, "Brutus")[1], {"rank": 2, "total": 2, "best": scores[2]})
        self.assertEqual(self.submit(good, TOKEN_A, "Popeye")[1], {"rank": 1, "total": 3, "best": scores[0]})
        top = self.daily(20261006)[1]["top"]
        self.assertEqual([(r["rank"], r["name"], r["score"]) for r in top],
                         [(1, "Popeye", scores[0]), (2, "Olive", scores[1]), (3, "Brutus", scores[2])])
        # Matching Olive's score later ranks behind her.
        self.assertEqual(self.submit(middle, TOKEN_C, "Brutus")[1], {"rank": 3, "total": 3, "best": scores[1]})

    def test_a_tie_ranks_the_earlier_submission_first(self):
        replay = support.make_replay(20261006, 7, 8)
        self.assertEqual(self.submit(replay, TOKEN_A)[1]["rank"], 1)
        self.assertEqual(self.submit(replay, TOKEN_B)[1]["rank"], 2)
        self.assertEqual(self.submit(replay, TOKEN_A)[1]["rank"], 1)

    def test_keeps_only_the_players_best_per_date(self):
        high, low = support.make_replay(20261006, 7, 0), support.make_replay(20261006, 7, 2)
        best = run_verifier(high)["score"]
        self.assertEqual(self.submit(low)[1]["best"], run_verifier(low)["score"])
        self.assertEqual(self.submit(high)[1], {"rank": 1, "total": 1, "best": best})
        self.assertEqual(self.submit(low)[1], {"rank": 1, "total": 1, "best": best})
        self.assertEqual(self.submit(high)[1], {"rank": 1, "total": 1, "best": best})
        self.assertEqual([row[3] for row in self.rows()], [best])
        # A new date is a new board for the same player.
        other = support.make_replay(20261005, 7, 0)
        self.assertEqual(self.submit(other)[1]["total"], 1)
        self.assertEqual(len(self.rows()), 2)

    def test_the_stored_score_is_the_verified_one(self):
        replay = support.make_replay(20261006, 7, 8)
        score = run_verifier(replay)["score"]
        status, body, _ = self.submit(replay, extra={"score": 9999, "rank": 1, "best": 9999})
        self.assertEqual((status, body["best"]), (200, score))

    def test_tampered_replays_are_rejected(self):
        replay = support.make_replay(20261006, 7, 8)
        score = int.from_bytes(replay[16:20], "little")
        forged = support.reseal(support.set_u32(replay, 16, score + 50))
        self.assertEqual(self.submit(forged)[:2], (422, {"error": "replay_rejected"}))
        self.assertEqual(self.submit(support.set_u32(replay, 16, score + 50))[:2], (422, {"error": "replay_rejected"}))
        wrong_seed = support.reseal(support.set_u32(replay, 8, 77))
        self.assertEqual(self.submit(wrong_seed)[:2], (422, {"error": "replay_rejected"}))
        self.assertEqual(self.submit(replay[:-9])[:2], (422, {"error": "replay_rejected"}))
        overflow = bytearray(replay)
        overflow[2] |= 2
        self.assertEqual(self.submit(support.reseal(bytes(overflow)))[:2], (422, {"error": "replay_rejected"}))
        not_daily = support.make_replay(20261006, 7, 8, False, "mode=A")
        self.assertEqual(self.submit(not_daily)[:2], (422, {"error": "replay_rejected"}))
        self.assertEqual(self.rows(), [])

    def test_bad_base64_and_empty_replays(self):
        for replay in ("not base64!!", "", "AAA", 12, None):
            body = json.dumps({"player": TOKEN_A, "name": "Popeye", "replay": replay})
            self.assertEqual(self.request("POST", "/v1/daily", body)[:2], (400, {"error": "bad_replay"}), replay)
        body = json.dumps({"player": TOKEN_A, "name": "Popeye"})
        self.assertEqual(self.request("POST", "/v1/daily", body)[:2], (400, {"error": "bad_replay"}))
        self.assertEqual(self.submit(bytes(2049))[:2], (400, {"error": "bad_replay"}))

    def test_oversize_bodies(self):
        status, body, _ = self.request("POST", "/v1/daily", "x" * (8 * 1024 + 1))
        self.assertEqual((status, body), (413, {"error": "too_large"}))
        big = json.dumps({"player": TOKEN_A, "name": "Popeye", "replay": "A" * 9000})
        self.assertEqual(self.request("POST", "/v1/daily", big)[:2], (413, {"error": "too_large"}))
        # A body under the cap but over the replay cap is still refused.
        huge = base64.b64encode(bytes(3000)).decode()
        body = json.dumps({"player": TOKEN_A, "name": "Popeye", "replay": huge})
        self.assertLess(len(body), 8 * 1024)
        self.assertEqual(self.request("POST", "/v1/daily", body)[:2], (400, {"error": "bad_replay"}))
        self.assertEqual(self.rows(), [])

    def test_malformed_requests(self):
        post = lambda body, headers=None: self.request("POST", "/v1/daily", body, headers)[:2]
        self.assertEqual(post("{not json"), (400, {"error": "bad_json"}))
        self.assertEqual(post("[1, 2]"), (400, {"error": "bad_request"}))
        self.assertEqual(post('"text"'), (400, {"error": "bad_request"}))
        self.assertEqual(post(b"\xff\xfe"), (400, {"error": "bad_json"}))
        self.assertEqual(post("", {"Content-Length": "abc"}), (411, {"error": "length_required"}))
        self.assertEqual(self.request("POST", "/v1/nope", "{}")[:2], (404, {"error": "not_found"}))
        self.assertEqual(self.request("PUT", "/v1/daily", "{}")[:2], (405, {"error": "method_not_allowed"}))
        self.assertEqual(self.request("DELETE", "/v1/daily/20261006")[:2], (405, {"error": "method_not_allowed"}))

    def test_bad_names(self):
        replay = support.make_replay(20261006, 7, 8)
        for name in ("", "x" * 13, "bad!", "a.b", "Ünï", "tab\t", "new\nline", None, 7, ["a"], "<b>"):
            self.assertEqual(self.submit(replay, name=name)[:2], (400, {"error": "bad_name"}), repr(name))
        for name in ("a", "x" * 12, "Sailor 1A2B", "a_b-c", "Z9"):
            self.assertEqual(self.submit(replay, name=name)[0], 200, name)

    def test_bad_player_tokens(self):
        replay = support.make_replay(20261006, 7, 8)
        for player in ("", "short", "x" * 129, "has space in it", "tab\ttoken1234", "Ünïcode-token", None, 5):
            self.assertEqual(self.submit(replay, player=player)[:2], (400, {"error": "bad_player"}), repr(player))

    def test_date_window_is_utc_yesterday_to_tomorrow(self):
        for date, accepted in ((20261003, False), (20261004, False), (20261005, True), (20261006, True),
                               (20261007, True), (20261008, False), (20270101, False)):
            replay = support.make_replay(date, 7, 8)
            status, body, _ = self.submit(replay)
            if accepted:
                self.assertEqual(status, 200, date)
            else:
                self.assertEqual((status, body), (422, {"error": "date_out_of_range"}), date)
        self.assertEqual([row[0] for row in self.rows()].count(20261004), 0)

    def test_date_window_across_a_month_end(self):
        self.board.today = lambda: datetime.date(2026, 12, 31)
        for date, accepted in ((20261230, True), (20261231, True), (20270101, True), (20270102, False), (20261229, False)):
            self.assertEqual(self.submit(support.make_replay(date, 7, 8))[0], 200 if accepted else 422, date)

    def test_verifier_failure_is_reported_without_details(self):
        self.board.verifier = os.path.join(self.directory.name, "missing")
        status, body, response = self.submit(support.make_replay(20261006, 7, 8))
        self.assertEqual((status, body), (500, {"error": "verifier_unavailable"}))
        self.assertNotIn(b"Traceback", json.dumps(body).encode())
        broken = os.path.join(self.directory.name, "broken.sh")
        Path(broken).write_text("#!/bin/sh\necho garbage\n")
        os.chmod(broken, 0o755)
        self.board.verifier = broken
        self.assertEqual(self.submit(support.make_replay(20261006, 7, 8))[:2], (500, {"error": "verifier_unavailable"}))

    def test_internal_errors_hide_the_trace(self):
        def explode(body):
            raise RuntimeError("secret internal detail /Users/someone/path")
        self.board.submit = explode
        status, body, response = self.submit(support.make_replay(20261006, 7, 8))
        self.assertEqual((status, body), (500, {"error": "server_error"}))
        self.assertNotIn("internal", json.dumps(body))


class PrivacyTest(ServerCase):
    def test_only_an_hmac_of_the_player_token_is_stored(self):
        replay = support.make_replay(20261006, 7, 8)
        token = "raw-account-token-0123456789abcdef"
        self.assertEqual(self.submit(replay, player=token)[0], 200)
        expected = hmac.new(SECRET, token.encode(), hashlib.sha256).hexdigest()
        self.assertEqual([(row[1], row[2]) for row in self.rows()], [(expected, "Popeye")])
        self.assertNotEqual(expected, hashlib.sha256(token.encode()).hexdigest())  # keyed, not a bare hash
        # The raw token appears in no database file, including the write-ahead log.
        for name in os.listdir(self.directory.name):
            self.assertNotIn(token.encode(), Path(self.directory.name, name).read_bytes(), name)
        # Nor does a replay or the token reach the schema's other columns.
        with contextlib.closing(sqlite3.connect(self.db_path)) as db:
            columns = [row[1] for row in db.execute("PRAGMA table_info(scores)")]
        self.assertEqual(columns, ["date", "player", "name", "score", "submitted"])

    def test_the_same_token_is_the_same_player_and_another_secret_is_another_player(self):
        replay = support.make_replay(20261006, 7, 8)
        self.submit(replay, player=TOKEN_A)
        self.submit(replay, player=TOKEN_A)
        self.assertEqual(len(self.rows()), 1)
        other = leaderboard.Board(os.path.join(self.directory.name, "other.sqlite3"), self.verifier_binary, b"another-secret-value")
        self.assertNotEqual(other.player_hash(TOKEN_A), self.board.player_hash(TOKEN_A))

    def test_no_empty_secret(self):
        with self.assertRaises(ValueError):
            leaderboard.Board(os.path.join(self.directory.name, "x.sqlite3"), self.verifier_binary, b"")

    def test_refuses_to_start_without_a_secret(self):
        env = {k: v for k, v in os.environ.items() if not k.startswith("LEADERBOARD_")}
        done = subprocess.run([sys.executable, str(ROOT / "server/leaderboard.py"), "--verifier", self.verifier_binary,
                               "--db", os.path.join(self.directory.name, "y.sqlite3"), "--port", "0"],
                              env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(b"LEADERBOARD_SECRET", done.stderr)
        env["LEADERBOARD_SECRET"] = "short"
        done = subprocess.run([sys.executable, str(ROOT / "server/leaderboard.py"), "--verifier", self.verifier_binary],
                              env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        self.assertNotEqual(done.returncode, 0)
        env["LEADERBOARD_SECRET"] = "x" * 20
        done = subprocess.run([sys.executable, str(ROOT / "server/leaderboard.py"), "--verifier", "/nonexistent/verify"],
                              env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(b"--verifier", done.stderr)


class ReadTest(ServerCase):
    def fill(self, count, date=20261006):
        with contextlib.closing(sqlite3.connect(self.db_path)) as db:
            for i in range(count):
                db.execute("INSERT INTO scores VALUES (?, ?, ?, ?, ?)", (date, "h%03d" % i, "P%d" % i, 1000 - i, i))
            db.commit()

    def test_limit_default_cap_and_validation(self):
        self.fill(80)
        status, body, _ = self.daily(20261006)
        self.assertEqual((status, body["total"], len(body["top"])), (200, 80, 10))
        self.assertEqual(body["top"][0], {"rank": 1, "name": "P0", "score": 1000})
        self.assertEqual(len(self.daily(20261006, "?limit=3")[1]["top"]), 3)
        self.assertEqual(len(self.daily(20261006, "?limit=50")[1]["top"]), 50)
        self.assertEqual(len(self.daily(20261006, "?limit=51")[1]["top"]), 50)
        self.assertEqual(len(self.daily(20261006, "?limit=999999")[1]["top"]), 50)
        for query in ("?limit=0", "?limit=-1", "?limit=abc", "?limit=", "?limit=1&limit=2", "?limit=9999999"):
            self.assertEqual(self.daily(20261006, query)[:2], (400, {"error": "bad_limit"}), query)

    def test_empty_and_invalid_dates(self):
        self.assertEqual(self.daily(20261006)[1], {"date": 20261006, "total": 0, "top": []})
        self.assertEqual(self.daily(20261340)[:2], (400, {"error": "bad_date"}))
        self.assertEqual(self.daily(20260230)[:2], (400, {"error": "bad_date"}))
        for path in ("/v1/daily/2026100", "/v1/daily/abcdefgh", "/v1/daily/", "/v2/daily/20261006", "/"):
            self.assertEqual(self.request("GET", path)[:2], (404, {"error": "not_found"}), path)

    def test_reads_other_dates_than_today(self):
        self.fill(2, 20250101)
        self.assertEqual(self.daily(20250101)[1]["total"], 2)


class RateLimitTest(ServerCase):
    rate_limit = 3

    def test_limits_each_client_per_minute(self):
        for _ in range(3):
            self.assertEqual(self.daily(20261006)[0], 200)
        status, body, _ = self.daily(20261006)
        self.assertEqual((status, body), (429, {"error": "rate_limited"}))
        self.assertEqual(self.submit(support.make_replay(20261006, 7, 8))[0], 429)
        self.clock.now += 61
        self.assertEqual(self.daily(20261006)[0], 200)

    def test_limiter_is_per_client_and_bounded(self):
        limiter = leaderboard.RateLimiter(2, 60, self.clock)
        self.assertTrue(limiter.allow("a") and limiter.allow("a") and not limiter.allow("a"))
        self.assertTrue(limiter.allow("b"))
        for i in range(10100):
            limiter.allow("client%d" % i)
        self.clock.now += 120
        limiter.allow("fresh")
        self.assertLess(len(limiter.hits), 10100)


class ProxyTest(ServerCase):
    def test_forwarded_header_is_ignored_unless_trusted(self):
        self.server.limiter = leaderboard.RateLimiter(2, 60, self.clock)
        headers = {"X-Forwarded-For": "1.2.3.4"}
        codes = [self.request("GET", "/v1/daily/20261006", headers=headers)[0] for _ in range(3)]
        self.assertEqual(codes, [200, 200, 429])  # Spoofing the header does not buy a fresh budget.
        self.server.trust_proxy = True
        self.assertEqual(self.request("GET", "/v1/daily/20261006", headers={"X-Forwarded-For": "9.9.9.9"})[0], 200)


if __name__ == "__main__":
    unittest.main()

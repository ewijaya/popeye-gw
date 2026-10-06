#!/usr/bin/env python3
"""Popeye G&W verified Daily leaderboard. Python 3 standard library only.

    LEADERBOARD_SECRET=... python3 server/leaderboard.py --verifier build/replay_verify

POST /v1/daily          {"player": token, "name": "...", "replay": base64}  -> {rank, total, best}
GET  /v1/daily/<date>?limit=10                                              -> {date, total, top}

A submitted replay is re-simulated by the native verifier (built from the real game.c and
replay.c); the score stored is the one the verifier computed, never one the client claims.
See server/README.md for the security model and deployment notes."""
import argparse
import base64
import binascii
from collections import deque
import contextlib
import datetime
import hashlib
import hmac
import json
import logging
import os
import re
import sqlite3
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

MAX_BODY = 8 * 1024
MAX_REPLAY = 2048
MAX_LIMIT = 50
DEFAULT_LIMIT = 10
NAME_RE = re.compile(r"^[A-Za-z0-9 _-]{1,12}$")
PLAYER_RE = re.compile(r"^[\x21-\x7e]{8,128}$")
DATE_RE = re.compile(r"^[0-9]{8}$")
VERIFIER_TIMEOUT = 5
LOG = logging.getLogger("leaderboard")

SCHEMA = """
CREATE TABLE IF NOT EXISTS scores (
  date INTEGER NOT NULL,
  player TEXT NOT NULL,        -- HMAC-SHA256 of the client token, hex; never the token
  name TEXT NOT NULL,
  score INTEGER NOT NULL,
  submitted INTEGER NOT NULL,  -- unix milliseconds the best score arrived (earlier wins a tie, then the hash)
  PRIMARY KEY (date, player)
);
CREATE INDEX IF NOT EXISTS scores_rank ON scores (date, score DESC, submitted ASC, player ASC);
"""


class ApiError(Exception):
    def __init__(self, status, code):
        super().__init__(code)
        self.status, self.code = status, code


def valid_date(value):
    try:
        datetime.date(value // 10000, value // 100 % 100, value % 100)
    except ValueError:
        return False
    return True


def date_number(day):
    return day.year * 10000 + day.month * 100 + day.day


class RateLimiter:
    """At most `limit` requests per `window` seconds for each client address, in memory."""

    def __init__(self, limit, window=60.0, clock=time.monotonic):
        self.limit, self.window, self.clock = limit, window, clock
        self.hits = {}
        self.lock = threading.Lock()

    def allow(self, key):
        now = self.clock()
        with self.lock:
            if len(self.hits) > 10000:
                self.hits = {k: v for k, v in self.hits.items() if v and now - v[-1] < self.window}
            queue = self.hits.setdefault(key, deque())
            while queue and now - queue[0] >= self.window:
                queue.popleft()
            if len(queue) >= self.limit:
                return False
            queue.append(now)
            return True


class Board:
    """Verification and storage, independent of HTTP."""

    def __init__(self, db_path, verifier, secret, today=None, now_ms=None):
        if not secret:
            raise ValueError("a non-empty secret is required")
        self.db_path, self.verifier, self.secret = db_path, verifier, secret
        self.today = today or (lambda: datetime.datetime.now(datetime.timezone.utc).date())
        self.now_ms = now_ms or (lambda: time.time_ns() // 1000000)
        self.lock = threading.Lock()
        with self.connect() as db:
            db.executescript(SCHEMA)

    @contextlib.contextmanager
    def connect(self):
        """A connection that commits on success, rolls back on error and always closes."""
        db = sqlite3.connect(self.db_path, timeout=10)
        try:
            db.execute("PRAGMA journal_mode=WAL")
            with db:
                yield db
        finally:
            db.close()

    def player_hash(self, player):
        return hmac.new(self.secret, player.encode("ascii"), hashlib.sha256).hexdigest()

    def verify(self, replay):
        try:
            done = subprocess.run([self.verifier], input=replay, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, timeout=VERIFIER_TIMEOUT)
            result = json.loads(done.stdout.decode("ascii"))
        except (OSError, subprocess.SubprocessError, ValueError):
            LOG.error("verifier failed to run")
            raise ApiError(500, "verifier_unavailable")
        if not isinstance(result, dict) or result.get("ok") is not True or done.returncode != 0:
            raise ApiError(422, "replay_rejected")
        try:
            date, score = int(result["date"]), int(result["score"])
        except (KeyError, TypeError, ValueError):
            raise ApiError(500, "verifier_unavailable")
        return date, score

    def submit(self, body):
        if not isinstance(body, dict):
            raise ApiError(400, "bad_request")
        player, name, encoded = body.get("player"), body.get("name"), body.get("replay")
        if not isinstance(player, str) or not PLAYER_RE.match(player):
            raise ApiError(400, "bad_player")
        if not isinstance(name, str) or not NAME_RE.match(name):
            raise ApiError(400, "bad_name")
        if not isinstance(encoded, str) or len(encoded) > MAX_REPLAY * 4 // 3 + 8:
            raise ApiError(400, "bad_replay")
        try:
            replay = base64.b64decode(encoded, validate=True)
        except (binascii.Error, ValueError):
            raise ApiError(400, "bad_replay")
        if not replay or len(replay) > MAX_REPLAY:
            raise ApiError(400, "bad_replay")
        date, score = self.verify(replay)
        today = self.today()
        window = [date_number(today + datetime.timedelta(days=d)) for d in (-1, 0, 1)]
        if date not in window:
            raise ApiError(422, "date_out_of_range")
        if score <= 0:
            raise ApiError(422, "no_score")
        key, now = self.player_hash(player), self.now_ms()
        with self.lock, self.connect() as db:
            row = db.execute("SELECT score FROM scores WHERE date = ? AND player = ?", (date, key)).fetchone()
            if row is None:
                db.execute("INSERT INTO scores VALUES (?, ?, ?, ?, ?)", (date, key, name, score, now))
            elif score > row[0]:
                db.execute("UPDATE scores SET name = ?, score = ?, submitted = ? WHERE date = ? AND player = ?",
                           (name, score, now, date, key))
            else:
                db.execute("UPDATE scores SET name = ? WHERE date = ? AND player = ?", (name, date, key))
            best, submitted = db.execute("SELECT score, submitted FROM scores WHERE date = ? AND player = ?",
                                         (date, key)).fetchone()
            ahead = db.execute("SELECT COUNT(*) FROM scores WHERE date = ? AND (score > ? OR (score = ? AND "
                               "(submitted < ? OR (submitted = ? AND player < ?))))",
                               (date, best, best, submitted, submitted, key)).fetchone()[0]
            total = db.execute("SELECT COUNT(*) FROM scores WHERE date = ?", (date,)).fetchone()[0]
        return {"rank": ahead + 1, "total": total, "best": best}

    def top(self, date, limit):
        with self.connect() as db:
            rows = db.execute("SELECT name, score FROM scores WHERE date = ? ORDER BY score DESC, submitted ASC, player ASC LIMIT ?",
                              (date, limit)).fetchall()
            total = db.execute("SELECT COUNT(*) FROM scores WHERE date = ?", (date,)).fetchone()[0]
        return {"date": date, "total": total,
                "top": [{"rank": i + 1, "name": name, "score": score} for i, (name, score) in enumerate(rows)]}


class Handler(BaseHTTPRequestHandler):
    server_version = "PopeyeGWLeaderboard"
    sys_version = ""
    protocol_version = "HTTP/1.1"
    timeout = 15  # a stalled client cannot hold a thread for long

    def client_key(self):
        if self.server.trust_proxy:
            forwarded = self.headers.get("X-Forwarded-For", "")
            if forwarded:
                return forwarded.split(",")[-1].strip()[:64]  # the proxy's own entry
        return self.client_address[0]

    def reply(self, status, payload):
        data = json.dumps(payload, separators=(",", ":")).encode("ascii")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(data)

    def fail(self, status, code, close=False):
        if close:
            self.close_connection = True
        self.reply(status, {"error": code})

    def guarded(self, action):
        try:
            if not self.server.limiter.allow(self.client_key()):
                return self.fail(429, "rate_limited", close=True)
            action()
        except ApiError as error:
            self.fail(error.status, error.code, close=True)  # The body may be unread.
        except Exception:  # Never leak a traceback to the client.
            LOG.exception("unhandled error")
            self.fail(500, "server_error", close=True)

    def do_POST(self):
        self.guarded(self.post)

    def do_GET(self):
        self.guarded(self.get)

    def do_PUT(self):
        self.fail(405, "method_not_allowed", close=True)

    do_DELETE = do_PATCH = do_PUT

    def post(self):
        if urlsplit(self.path).path != "/v1/daily":
            raise ApiError(404, "not_found")
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError:
            raise ApiError(411, "length_required")
        if length < 0:
            raise ApiError(400, "bad_request")
        if length > MAX_BODY:
            raise ApiError(413, "too_large")
        raw = self.rfile.read(length)
        if len(raw) != length:
            raise ApiError(400, "bad_request")
        try:
            body = json.loads(raw.decode("utf-8"))
        except ValueError:
            raise ApiError(400, "bad_json")
        self.reply(200, self.server.board.submit(body))

    def get(self):
        parts = urlsplit(self.path)
        match = re.match(r"^/v1/daily/([0-9]{8})$", parts.path)
        if match is None:
            raise ApiError(404, "not_found")
        date = int(match.group(1))
        if not valid_date(date):
            raise ApiError(400, "bad_date")
        values = parse_qs(parts.query, keep_blank_values=True).get("limit", [str(DEFAULT_LIMIT)])
        if len(values) != 1 or not re.match(r"^[0-9]{1,6}$", values[0]) or int(values[0]) < 1:
            raise ApiError(400, "bad_limit")
        self.reply(200, self.server.board.top(date, min(int(values[0]), MAX_LIMIT)))

    def log_message(self, format, *args):  # No access log: it would hold client addresses.
        pass


def make_server(board, host="127.0.0.1", port=0, rate_limit=30, trust_proxy=False, clock=time.monotonic):
    server = ThreadingHTTPServer((host, port), Handler)
    server.daemon_threads = True
    server.board = board
    server.limiter = RateLimiter(rate_limit, 60.0, clock)
    server.trust_proxy = trust_proxy
    return server


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--verifier", default=os.environ.get("LEADERBOARD_VERIFIER"),
                        help="path to the compiled replay_verify binary (env LEADERBOARD_VERIFIER)")
    parser.add_argument("--db", default=os.environ.get("LEADERBOARD_DB", "leaderboard.sqlite3"))
    parser.add_argument("--host", default=os.environ.get("LEADERBOARD_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("LEADERBOARD_PORT", "8080")))
    parser.add_argument("--rate-limit", type=int, default=int(os.environ.get("LEADERBOARD_RATE", "30")),
                        help="requests per minute per client address")
    parser.add_argument("--trust-proxy", action="store_true",
                        default=os.environ.get("LEADERBOARD_TRUST_PROXY") == "1",
                        help="take the client address from the last X-Forwarded-For entry")
    args = parser.parse_args(argv)
    secret = os.environ.get("LEADERBOARD_SECRET", "")
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    if len(secret) < 16:
        parser.error("set LEADERBOARD_SECRET to a random string of at least 16 characters")
    if not args.verifier or not os.access(args.verifier, os.X_OK):
        parser.error("--verifier (or LEADERBOARD_VERIFIER) must name the compiled replay_verify binary")
    board = Board(args.db, os.path.abspath(args.verifier), secret.encode("utf-8"))
    server = make_server(board, args.host, args.port, args.rate_limit, args.trust_proxy)
    LOG.info("listening on %s:%d", *server.server_address[:2])
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

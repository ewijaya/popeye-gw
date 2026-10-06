# Popeye G&W Daily leaderboard server

A small, optional service that ranks Daily rounds. It is **not deployed**: the game ships with
the leaderboard disabled (empty endpoint URL in `src/js/pebble-js-app.js`). Nothing here
runs unless the owner starts it.

Python 3 standard library only (`http.server`, `sqlite3`, `hmac`, `subprocess`), plus one
native binary: the replay verifier, compiled from the game's own `game.c` and `replay.c`.
There is no port of the game rules to another language, so the server and the watch cannot
disagree about a score.

## Run locally

```sh
# Build the verifier (strict C99, no SDK needed).
/usr/bin/cc -std=c99 -O2 -Isrc/c tools/replay_verify.c src/c/game.c src/c/replay.c -o replay_verify

LEADERBOARD_SECRET="$(python3 -c 'import secrets; print(secrets.token_hex(32))')" \
  python3 server/leaderboard.py --verifier ./replay_verify --db leaderboard.sqlite3 --port 8080
```

Keep the secret: it keys the player hashes, so changing it makes every player a new player.

| Setting | Flag | Environment | Default |
|---|---|---|---|
| HMAC secret, at least 16 characters (required) | - | `LEADERBOARD_SECRET` | none: refuses to start |
| Verifier binary (required) | `--verifier` | `LEADERBOARD_VERIFIER` | none |
| SQLite file | `--db` | `LEADERBOARD_DB` | `leaderboard.sqlite3` |
| Bind address | `--host` | `LEADERBOARD_HOST` | `127.0.0.1` |
| Port | `--port` | `LEADERBOARD_PORT` | `8080` |
| Requests per minute per client | `--rate-limit` | `LEADERBOARD_RATE` | `30` |
| Use the last `X-Forwarded-For` entry as the client address | `--trust-proxy` | `LEADERBOARD_TRUST_PROXY=1` | off |

Tests: `./tools/test.sh` (they build a verifier, start the server on an ephemeral port with a
temporary database and cover accept, rank, best-only, tampering, size, names, the date window,
limits, the rate limit and the HMAC storage).

## API

Errors are always `{"error": "<code>"}` with an HTTP status; no stack trace or detail ever
reaches the client.

`POST /v1/daily` with JSON `{"player": "<opaque token>", "name": "<name>", "replay": "<base64>"}`

- Body at most 8 KiB (413 otherwise); replay at most 2,048 bytes decoded; strict base64.
- `name` matches `^[A-Za-z0-9 _-]{1,12}$`; `player` is 8-128 printable ASCII characters.
- The replay must pass the verifier (422 `replay_rejected`): well formed, checksum, Game B
  rules, 60 s limit, the seed `game_daily_seed(date)` for a real date, and a re-simulated
  score equal to the claimed one. A replay flagged as overflowed (unverifiable) is rejected.
- Its date must be UTC today, yesterday or tomorrow (422 `date_out_of_range`), which covers every
  time zone. A zero score is refused (`no_score`).
- Success: `{"rank": 12, "total": 140, "best": 73}`. Only the player's best score per date is
  kept; the stored score is the verifier's, never a number from the request. Rank is by score,
  then by who reached it first.

`GET /v1/daily/<YYYYMMDD>?limit=10` returns `{"date", "total", "top": [{"rank", "name", "score"}]}`.
`limit` is 1-50 (larger values are capped at 50).

Other statuses: 400 bad input, 404 unknown path, 405 other methods, 411 no length, 429 rate limited,
500 server or verifier failure.

## Security model

What it prevents:

- **Fabricated scores.** The client sends its inputs, not a score. The server replays them through
  the real engine, so editing the score, the seed, the date or the events (even with a recomputed
  checksum) is rejected. The checksum is an integrity check against accidents, not a secret.
- **Cross-day replays and other modes.** The seed must be the one for the replay's own date.
- **Resource abuse (basic).** Size caps, a verifier timeout, a per-address rate limit in memory.

What it does **not** prevent, and cannot:

- A **bot or an assisted human playing a real game.** A script that plays the 60-second round
  perfectly, or software that reads the screen and presses the buttons, produces a genuine
  replay. So does replaying a known-best input sequence for the day's fixed seed (everyone gets
  the same food order, which is the point of Daily, and the first player's inputs could be reused).
  Treat the board as friendly competition, not a tournament. If needed, remove entries by hand
  in SQLite (`DELETE FROM scores WHERE ...`).
- **Identity.** `player` is an opaque account token from the phone. Anyone who can reach the server
  can invent tokens, so one person can hold many entries. The date window and rate limit only
  slow that down.
- **Name abuse** beyond the character set. Names are plain text; clients must display them as
  text (the watch app does).

## Deployment notes (left to the owner)

Nothing has been deployed, registered or paid for.

- Any small VM or container that can run Python 3 and a C compiler to build the verifier once.
  Run `leaderboard.py` as an unprivileged user behind an HTTPS reverse proxy (Caddy, nginx); the
  Pebble phone app and PebbleKit JS need an `https://` URL.
- Bind to `127.0.0.1`, let the proxy terminate TLS, and start with `--trust-proxy` only when the
  proxy sets `X-Forwarded-For` itself (it must overwrite, not forward, a client-supplied value).
- Put the secret in the service manager's environment, not in the repository or command line.
  Back up the SQLite file; the process is a single writer and a single node.
- Then set `LEADERBOARD_URL` in `src/js/pebble-js-app.js` (for example
  `https://leaderboard.example.org`), rebuild and play-test on a watch before any release.

## Privacy

- The phone sends the watch account token (`Pebble.getAccountToken()`), a nickname and the replay
  (inputs, seed, date, score) of the player's best Daily round, and only when **Online** is on.
- The server stores only: the date, an **HMAC-SHA256 of the token** under the server secret
  (not the token), the nickname, the score and a timestamp in milliseconds. It does not keep
  the replay, an IP address or a request log (the access log is disabled; the rate limiter holds
  client addresses in memory only, never on disk).
- The nickname is public on the board. The default is `Sailor` plus four characters derived
  from the token, so it names nobody.
- A reverse proxy in front of the server may keep its own logs; configure it accordingly.
- To delete a player, remove their rows. The server cannot look them up from the app side because
  it only knows the hash: compute `HMAC-SHA256(secret, token)` for a token the player supplies.

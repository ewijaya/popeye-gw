#ifndef POPEYE_GW_REPLAY_H
#define POPEYE_GW_REPLAY_H

/* Input replay for timed rounds (Daily), pure C99 like game.c: no allocation, no clock.
 * A verifier re-runs the same engine calls and compares the final score.
 *
 * Time base: cumulative ACTIVE milliseconds, i.e. the sum of the elapsed_ms that
 * game_advance consumed while the status was PLAYING or RECOVERING. Paused time,
 * time after game over and unconsumed leftovers never advance it. Because the engine
 * is chunking-independent (tested), a verifier may advance in any pieces.
 *
 * Byte format, all integers little-endian:
 *   0  u8   version = 1
 *   1  u8   mode (0 = Game A rules, 1 = Game B rules)
 *   2  u8   flags: bit0 controls swapped, bit1 overflowed (unverifiable)
 *   3  u8   reserved, zero
 *   4  u32  time limit in active ms (0 = untimed; Daily and Sprint use 60000)
 *   8  u32  seed
 *  12  u32  date YYYYMMDD (local date the round was started)
 *  16  u32  final score as played (true total, not the 3-digit display)
 *  20  u32  end_ms: cumulative active ms when recording stopped
 *  24  u16  event count
 *  26  ...  events, each a LEB128 varint of (delta_ms << 2 | kind), where delta_ms is
 *           the active time since the previous event (since 0 for the first)
 *           kind 0 = UP button edge, 1 = DOWN button edge (toggles the held state:
 *           press when it was up, release when it was down), 2 = pause, 3 = resume
 *  n-4 u32  FNV-1a (32-bit) checksum of every preceding byte
 * Simulation: game_init + game_start_timed(mode, seed, limit), controls_swapped from
 * flags, then for each event game_advance(delta) and the call above; finally
 * game_advance(end_ms - last event time). Daily's seed is game_daily_seed(date). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "game.h"

#define REPLAY_VERSION 1u
#define REPLAY_HEADER_BYTES 26u
#define REPLAY_TRAILER_BYTES 4u
#define REPLAY_MAX_BYTES 2048u   /* a 60 s round needs well under half of this */
#define REPLAY_MAX_EVENTS 1024u
#define REPLAY_MAX_MS 7200000u   /* sanity bound for end_ms and the time limit */

typedef struct {
  uint8_t bytes[REPLAY_MAX_BYTES];
  size_t used;
  uint32_t clock_ms, last_ms, events;
  bool active, overflow;
} Replay;

/* Call right after game_start_timed. */
void replay_begin(Replay *replay, const Game *game, uint32_t seed, uint32_t date);

/* Each performs the engine call and records it. A NULL replay just calls the engine. */
bool replay_input(Replay *replay, Game *game, GameButton button, bool pressed);
bool replay_pause(Replay *replay, Game *game);
bool replay_resume(Replay *replay, Game *game);
bool replay_advance(Replay *replay, Game *game, uint32_t elapsed_ms);
bool replay_step(Replay *replay, Game *game);

/* Seals the buffer and returns its length (also zero if never begun). replay->bytes then
 * holds the format above. An overflowed replay is sealed with the overflow flag and never
 * verifies; check replay->overflow before storing. */
size_t replay_finish(Replay *replay, const Game *game);

typedef enum {
  REPLAY_OK,             /* well formed and the re-run score equals the claimed score */
  REPLAY_BAD_FORMAT,
  REPLAY_BAD_CHECKSUM,
  REPLAY_UNVERIFIABLE,   /* recorder ran out of room */
  REPLAY_SCORE_MISMATCH
} ReplayStatus;

typedef struct {
  ReplayStatus status;
  uint8_t mode;
  bool controls_swapped;
  uint32_t time_limit_ms, seed, date, claimed_score, end_ms;
  uint32_t score, catches, drops, hits, total_misses, time_left_ms;
  GameStatus game_status;
  bool time_up;
} ReplayResult;

/* Header fields are filled once the checksum is valid; the game outcome only when the
 * events replayed. Returns result->status. */
ReplayStatus replay_simulate(const uint8_t *data, size_t length, ReplayResult *result);

#endif

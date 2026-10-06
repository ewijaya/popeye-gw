/* Daily replay verifier: re-runs the real engine (game.c + replay.c) over the recorded input
 * and prints one JSON line. Reads the replay bytes from the file named by argv[1], or from
 * stdin when there is no argument or it is "-". Exit status 0 when verified, 1 otherwise.
 *   {"ok":true,"date":20261006,"score":42,"misses":3}
 *   {"ok":false,"error":"bad_seed"}
 * Beyond replay_simulate, a Daily replay must be Game B rules, a 60 s limit and the seed
 * game_daily_seed(date) for a real calendar date. Strict C99, no allocation. */
#include "game.h"
#include "replay.h"
#include "tuning.h"

#include <stdio.h>
#include <string.h>

static bool valid_date(uint32_t date) {
  static const unsigned days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  unsigned year = date / 10000u, month = date / 100u % 100u, day = date % 100u, maximum;
  if (year < 1970u || year > 9999u || month < 1u || month > 12u) return false;
  maximum = days[month - 1u];
  if (month == 2u && year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ++maximum;
  return day >= 1u && day <= maximum;
}

/* Returns the error code, or NULL when the replay is a valid Daily round (filled in result). */
static const char *verify_replay(const uint8_t *bytes, size_t length, ReplayResult *result) {
  if (length == 0u) return "empty";
  if (length > REPLAY_MAX_BYTES) return "too_long";
  switch (replay_simulate(bytes, length, result)) {
    case REPLAY_OK: break;
    case REPLAY_BAD_CHECKSUM: return "bad_checksum";
    case REPLAY_UNVERIFIABLE: return "unverifiable";
    case REPLAY_SCORE_MISMATCH: return "score_mismatch";
    default: return "bad_format";
  }
  if (result->mode != (uint8_t)GAME_B) return "bad_mode";
  if (result->time_limit_ms != PGW_SPRINT_MS) return "bad_limit";
  if (!valid_date(result->date)) return "bad_date";
  if (result->seed != game_daily_seed(result->date)) return "bad_seed";
  return NULL;
}

int main(int argc, char **argv) {
  static uint8_t bytes[REPLAY_MAX_BYTES + 1u];
  ReplayResult result;
  const char *error;
  FILE *input = stdin;
  size_t length;
  if (argc > 2) {
    printf("{\"ok\":false,\"error\":\"usage\"}\n");
    return 1;
  }
  if (argc == 2 && strcmp(argv[1], "-") != 0 && (input = fopen(argv[1], "rb")) == NULL) {
    printf("{\"ok\":false,\"error\":\"unreadable\"}\n");
    return 1;
  }
  length = fread(bytes, 1u, sizeof(bytes), input);
  if (input != stdin) fclose(input);
  error = verify_replay(bytes, length, &result);
  if (error != NULL) {
    printf("{\"ok\":false,\"error\":\"%s\"}\n", error);
    return 1;
  }
  printf("{\"ok\":true,\"date\":%lu,\"score\":%lu,\"misses\":%lu}\n", (unsigned long)result.date,
         (unsigned long)result.score, (unsigned long)result.total_misses);
  return 0;
}

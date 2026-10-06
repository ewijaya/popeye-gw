#ifndef POPEYE_GW_ONLINE_H
#define POPEYE_GW_ONLINE_H

/* Pure C99 state for the optional Daily leaderboard (no Pebble includes). The watch keeps one
 * record: the best Daily replay is stored by storage_save_replay, and this says whether it has
 * reached the server and where it ranked. The wire protocol is in online_net.h. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ONLINE_RECORD_SIZE 24u
#define ONLINE_MAX_ATTEMPTS 2u /* the first send and one retry at the next launch */

typedef enum { ONLINE_NONE, ONLINE_PENDING, ONLINE_SENT } OnlinePhase;

typedef struct {
  uint8_t phase;
  uint8_t attempts;     /* failed sends of the pending replay */
  uint32_t date, score; /* of the stored replay (zero when none) */
  uint32_t rank, total; /* from the server once sent */
} OnlineState;

void online_defaults(OnlineState *state);
/* Versioned, checksummed, little-endian; a failed decode leaves defaults. */
void online_encode(const OnlineState *state, uint8_t out[ONLINE_RECORD_SIZE]);
bool online_decode(OnlineState *state, const uint8_t *data, size_t size);
/* A new best Daily replay was stored: it is pending again, with fresh attempts. */
void online_queue(OnlineState *state, uint32_t date, uint32_t score);
void online_succeeded(OnlineState *state, uint32_t rank, uint32_t total);
/* A failed send. A permanent refusal (the server rejected the replay) stops retries. */
void online_failed(OnlineState *state, bool permanent);
/* The stored replay is gone or the scores were reset. */
void online_clear(OnlineState *state);
/* Pending and not out of attempts. */
bool online_should_send(const OnlineState *state);
/* Daily high-scores row: "Online #12/140", "Online: not sent", "Online: sending", "Online: off"
 * or "Online: on" (nothing to show for today). today is the local date YYYYMMDD. */
void online_text(char *out, size_t size, bool enabled, bool sending, const OnlineState *state, uint32_t today);

#endif

#ifndef POPEYE_GW_ONLINE_NET_H
#define POPEYE_GW_ONLINE_NET_H

/* Sends the stored Daily replay to the phone over AppMessage, which posts it to the leaderboard.
 * Watch to phone, one message per 256-byte chunk of the replay:
 *   LB_SEQ u8 (chunk index), LB_LEN u16 (total replay bytes), LB_DATA (the chunk).
 * Phone to watch, once the server has answered (or the phone cannot ask):
 *   LB_STATUS u8: 1 ranked (LB_RANK, LB_TOTAL u32), 0 try later, 2 rejected for good,
 *   3 online play is not configured in this build.
 * AppMessage is open only while a transfer runs, and the caller never starts one during a
 * round. A transfer that gets no answer within 30 s fails. */

#include <stdbool.h>
#include <stdint.h>

typedef enum { ONLINE_NET_OK, ONLINE_NET_FAILED, ONLINE_NET_REJECTED, ONLINE_NET_UNAVAILABLE } OnlineNetResult;

typedef void (*OnlineNetDone)(OnlineNetResult result, uint32_t rank, uint32_t total);

/* Returns false (and never calls done) if there is no stored replay, a transfer is already
 * running or AppMessage could not be opened. Otherwise done is called exactly once, later. */
bool online_net_start(OnlineNetDone done);
/* Abandons a running transfer without calling done. */
void online_net_cancel(void);
bool online_net_busy(void);

#endif

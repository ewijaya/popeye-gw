#include <pebble.h>

#include "online_net.h"
#include "storage.h"
#include "settings_wire.h"

enum { INBOX_BYTES = 64, OUTBOX_BYTES = 288, CHUNK_BYTES = 256, ANSWER_TIMEOUT_MS = 30000, RETRY_MS = 2000 };

static OnlineNetDone s_done;
static AppTimer *s_timeout, *s_retry;
static unsigned s_next, s_count;
static uint16_t s_length;
static bool s_busy, s_opened, s_awaiting, s_failed_once;
static const Settings *s_settings;
static PhoneSettingsApply s_apply;
static bool s_config_pending;
static uint32_t s_config_id;
static uint8_t s_config_status;
static bool s_config_receiving;
static uint32_t s_last_patch_id;
static uint8_t s_last_patch_status;
static enum { OUT_NONE, OUT_REPLAY, OUT_SETTINGS } s_outgoing;

static void send_chunk(void *data);
static void pump(void);

static void stop(void) {
  if (s_timeout != NULL) app_timer_cancel(s_timeout);
  if (s_retry != NULL) app_timer_cancel(s_retry);
  s_timeout = s_retry = NULL;
  s_awaiting = false; /* A late callback of an abandoned message is ignored. */
  s_busy = false;
}

static void finish(OnlineNetResult result, uint32_t rank, uint32_t total) {
  OnlineNetDone done = s_done;
  stop();
  s_done = NULL;
  if (done != NULL) done(result, rank, total);
  pump();
}

static void answer_timeout(void *data) {
  s_timeout = NULL;
  finish(ONLINE_NET_FAILED, 0u, 0u);
}

/* One retry of a chunk (a phone that is not ready yet); after that the transfer fails. */
static void chunk_failed(void) {
  if (s_failed_once) { finish(ONLINE_NET_FAILED, 0u, 0u); return; }
  s_failed_once = true;
  s_retry = app_timer_register(RETRY_MS, send_chunk, NULL);
  if (s_retry == NULL) finish(ONLINE_NET_FAILED, 0u, 0u);
}

static void send_chunk(void *data) {
  uint8_t chunk[CHUNK_BYTES];
  DictionaryIterator *out;
  size_t size;
  s_retry = NULL;
  if (!s_busy) return;
  if (s_outgoing != OUT_NONE) return;
  size = storage_replay_chunk(s_next, chunk, sizeof(chunk));
  if (size == 0u || app_message_outbox_begin(&out) != APP_MSG_OK) { chunk_failed(); return; }
  dict_write_uint8(out, MESSAGE_KEY_LB_SEQ, (uint8_t)s_next);
  dict_write_uint16(out, MESSAGE_KEY_LB_LEN, s_length);
  dict_write_data(out, MESSAGE_KEY_LB_DATA, chunk, (uint16_t)size);
  if (app_message_outbox_send() != APP_MSG_OK) chunk_failed();
  else { s_awaiting = true; s_outgoing = OUT_REPLAY; }
}

static void outbox_sent(DictionaryIterator *iterator, void *context) {
  bool replay = s_outgoing == OUT_REPLAY;
  s_outgoing = OUT_NONE;
  if (replay && s_busy && s_awaiting) {
    s_awaiting = false;
    s_failed_once = false;
    ++s_next;
  }
  pump(); /* Settings first, then the next replay chunk, or wait for the answer. */
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  bool replay = s_outgoing == OUT_REPLAY;
  s_outgoing = OUT_NONE;
  if (replay && s_busy && s_awaiting) {
    s_awaiting = false;
    chunk_failed();
  }
  /* The phone retries an unanswered settings request; never spin while disconnected. */
  pump();
}

static void pump(void) {
  DictionaryIterator *out;
  uint8_t values[SETTINGS_WIRE_COUNT];
  if (!s_opened || s_outgoing != OUT_NONE) return;
  if (s_config_pending && s_settings != NULL) {
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    settings_wire_encode(s_settings, values);
    dict_write_data(out, MESSAGE_KEY_CFG_DATA, values, sizeof(values));
    dict_write_uint32(out, MESSAGE_KEY_CFG_ID, s_config_id);
    dict_write_uint8(out, MESSAGE_KEY_CFG_STATUS, s_config_status);
    s_config_pending = false;
    if (app_message_outbox_send() == APP_MSG_OK) s_outgoing = OUT_SETTINGS;
  }
  if (s_outgoing == OUT_NONE && s_busy && !s_awaiting && s_retry == NULL && s_next < s_count)
    send_chunk(NULL);
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  Tuple *request = dict_find(iterator, MESSAGE_KEY_CFG_REQUEST);
  Tuple *patch = dict_find(iterator, MESSAGE_KEY_CFG_PATCH);
  Tuple *id = dict_find(iterator, MESSAGE_KEY_CFG_ID);
  Tuple *status = dict_find(iterator, MESSAGE_KEY_LB_STATUS);
  Tuple *rank = dict_find(iterator, MESSAGE_KEY_LB_RANK);
  Tuple *total = dict_find(iterator, MESSAGE_KEY_LB_TOTAL);
  if ((request != NULL || patch != NULL) && s_settings != NULL) {
    Settings settings = *s_settings;
    if (id == NULL || (id->type != TUPLE_UINT && id->type != TUPLE_INT) || id->length != 4u) return;
    s_config_id = id->value->uint32;
    s_config_status = 0u;
    if (patch != NULL) {
      s_config_receiving = true;
      s_config_status = s_config_id != 0u && s_config_id == s_last_patch_id ? s_last_patch_status :
                       patch->type == TUPLE_BYTE_ARRAY &&
                        settings_wire_apply(&settings, patch->value->data, patch->length)
                        ? s_apply(&settings) : 3u;
      s_last_patch_id = s_config_id;
      s_last_patch_status = s_config_status;
      s_config_receiving = false;
    }
    s_config_pending = true;
    pump();
    return;
  }
  if (!s_busy || status == NULL) return;
  if (status->value->uint8 == 3) { finish(ONLINE_NET_UNAVAILABLE, 0u, 0u); return; } /* Answers the first chunk. */
  if (s_next < s_count) return; /* The replay is still going out. */
  switch (status->value->uint8) {
    case 1:
      if (rank != NULL && total != NULL) finish(ONLINE_NET_OK, rank->value->uint32, total->value->uint32);
      else finish(ONLINE_NET_FAILED, 0u, 0u);
      break;
    case 2: finish(ONLINE_NET_REJECTED, 0u, 0u); break;
    default: finish(ONLINE_NET_FAILED, 0u, 0u); break;
  }
}

bool online_net_init(const Settings *settings, PhoneSettingsApply apply) {
  s_settings = settings;
  s_apply = apply;
  if (!s_opened) {
    app_message_register_inbox_received(inbox_received);
    app_message_register_outbox_sent(outbox_sent);
    app_message_register_outbox_failed(outbox_failed);
    if (app_message_open(INBOX_BYTES, OUTBOX_BYTES) != APP_MSG_OK) {
      app_message_deregister_callbacks();
      return false;
    }
    s_opened = true;
  }
  return true;
}

void online_net_settings_changed(void) {
  if (s_config_receiving) return;
  /* An outstanding reply keeps its request ID/result; its payload always uses live settings. */
  if (!s_config_pending) { s_config_id = 0u; s_config_status = 0u; }
  s_config_pending = true;
  pump();
}

void online_net_deinit(void) {
  online_net_cancel();
  app_message_deregister_callbacks();
  s_opened = s_config_pending = false;
  s_outgoing = OUT_NONE;
}

bool online_net_start(OnlineNetDone done) {
  size_t length = storage_replay_length();
  if (!s_opened || online_net_busy() || length == 0u || length > UINT16_MAX) return false;
  s_busy = true;
  s_done = done;
  s_length = (uint16_t)length;
  s_count = (unsigned)((length + CHUNK_BYTES - 1u) / CHUNK_BYTES);
  s_next = 0u;
  s_failed_once = s_awaiting = false;
  s_timeout = app_timer_register(ANSWER_TIMEOUT_MS, answer_timeout, NULL);
  if (s_timeout == NULL) { s_done = NULL; stop(); return false; }
  pump();
  return true;
}

void online_net_cancel(void) {
  if (!s_busy) return;
  s_done = NULL;
  stop();
}

bool online_net_busy(void) { return s_busy || s_outgoing == OUT_REPLAY; }

#include "fake.h"
#include "online_net.h"
#include "settings_wire.h"
#include "storage.h"
#include "replay.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AppMessageInboxReceived inbox_received;
static AppMessageOutboxSent outbox_sent;
static AppMessageOutboxFailed outbox_failed;
static DictionaryIterator out;
static bool in_flight, disconnected;
static Settings settings;
static unsigned applies, done_calls;
static uint8_t apply_result;

void app_message_register_inbox_received(AppMessageInboxReceived cb) { inbox_received = cb; }
void app_message_register_outbox_sent(AppMessageOutboxSent cb) { outbox_sent = cb; }
void app_message_register_outbox_failed(AppMessageOutboxFailed cb) { outbox_failed = cb; }
void app_message_deregister_callbacks(void) { inbox_received = NULL; outbox_sent = NULL; outbox_failed = NULL; }
AppMessageResult app_message_open(uint32_t in, uint32_t output) {
  assert(in >= 41u && output >= 288u);
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator) {
  if (in_flight) return APP_MSG_BUSY;
  memset(&out, 0, sizeof(out)); *iterator = &out;
  return APP_MSG_OK;
}
AppMessageResult app_message_outbox_send(void) {
  assert(!in_flight);
  if (disconnected) return APP_MSG_NOT_CONNECTED;
  in_flight = true; return APP_MSG_OK;
}
Tuple *dict_find(DictionaryIterator *iterator, uint32_t key) {
  unsigned i;
  for (i = 0; i < iterator->count; ++i) if (iterator->tuples[i].key == key) return &iterator->tuples[i];
  return NULL;
}
void dict_write_data(DictionaryIterator *iterator, uint32_t key, const uint8_t *data, uint16_t size) {
  unsigned i = iterator->count++;
  assert(i < 12u && size <= 256u);
  iterator->tuples[i] = (Tuple){key, TUPLE_BYTE_ARRAY, size, &iterator->values[i]};
  memcpy(iterator->values[i].data, data, size);
}
static void integer(DictionaryIterator *it, uint32_t key, uint32_t value, uint16_t size) {
  dict_write_data(it, key, (const uint8_t *)&value, size);
  it->tuples[it->count - 1u].type = TUPLE_UINT;
}
void dict_write_uint8(DictionaryIterator *it, uint32_t key, uint8_t value) { integer(it, key, value, 1u); }
void dict_write_uint16(DictionaryIterator *it, uint32_t key, uint16_t value) { integer(it, key, value, 2u); }
void dict_write_uint32(DictionaryIterator *it, uint32_t key, uint32_t value) { integer(it, key, value, 4u); }
static void ack(void) { assert(in_flight); in_flight = false; outbox_sent(&out, NULL); }
static uint8_t apply(const Settings *s) {
  ++applies;
  if (apply_result) return apply_result;
  if (!storage_save_settings(s)) return 1u;
  settings = *s;
  online_net_settings_changed(); /* Same path as main.c; must not lose reply ID/status. */
  return 0u;
}
static void receive(uint32_t id, const uint8_t *patch, size_t size) {
  DictionaryIterator in = {0};
  dict_write_uint32(&in, MESSAGE_KEY_CFG_ID, id);
  if (patch) dict_write_data(&in, MESSAGE_KEY_CFG_PATCH, patch, (uint16_t)size);
  else dict_write_uint8(&in, MESSAGE_KEY_CFG_REQUEST, 1u);
  inbox_received(&in, NULL);
}
static void response(uint32_t id, uint8_t status) {
  uint8_t expected[SETTINGS_WIRE_COUNT];
  settings_wire_encode(&settings, expected);
  assert(in_flight);
  assert(dict_find(&out, MESSAGE_KEY_CFG_ID)->value->uint32 == id);
  assert(dict_find(&out, MESSAGE_KEY_CFG_STATUS)->value->uint8 == status);
  assert(memcmp(dict_find(&out, MESSAGE_KEY_CFG_DATA)->value->data, expected, sizeof(expected)) == 0);
}
static void done(OnlineNetResult result, uint32_t rank, uint32_t total) {
  assert(result == ONLINE_NET_OK && rank == 2u && total == 5u); ++done_calls;
}
int main(void) {
  uint8_t patch[SETTINGS_PATCH_SIZE] = {0};
  uint8_t before[SETTINGS_RECORD_SIZE], after[SETTINGS_RECORD_SIZE];
  SaveData loaded;
  Game game;
  Replay replay;
  unsigned i;
  fake_reset(); fake_feedback_reset(); settings_defaults(&settings);
  settings.alarm_on = true; settings.alarm_hour = 19u; settings.alarm_minute = 43u;
  assert(online_net_init(&settings, apply));
  receive(1u, NULL, 0u); response(1u, 0u); ack();
  assert(applies == 0u && !settings.online);
  /* Every field/value round-trips; no added settings, no alarm mutation. */
  for (i = 0; i < SETTINGS_WIRE_COUNT; ++i) {
    unsigned v;
    for (v = 0; v <= (i == 4u ? 3u : 1u); ++v) {
      uint8_t values[SETTINGS_WIRE_COUNT];
      memset(patch, 0, sizeof(patch)); patch[0] = (uint8_t)(1u << i); patch[1] = (uint8_t)((1u << i) >> 8);
      patch[i + 2u] = (uint8_t)v;
      receive(10u + i * 4u + v, patch, sizeof(patch)); response(10u + i * 4u + v, 0u); ack();
      storage_load(&loaded); settings_wire_encode(&loaded.settings, values);
      assert(values[i] == v);
      assert(loaded.settings.alarm_on && loaded.settings.alarm_hour == 19u && loaded.settings.alarm_minute == 43u);
    }
  }
  /* Partial patch keeps later watch edits, including hidden Buttons and remembered volume. */
  settings.buttons_bottom = false; settings.sound_level = 2u;
  memset(patch, 0, sizeof(patch)); patch[0] = 1u << 4;
  receive(100u, patch, sizeof(patch)); response(100u, 0u); ack();
  assert(!settings.sound && settings.sound_level == 2u && !settings.buttons_bottom);
  i = applies;
  settings.sound = true; /* A later watch edit must survive an ACK retry. */
  receive(100u, patch, sizeof(patch)); response(100u, 0u); ack();
  assert(applies == i && settings.sound);
  /* Atomic validation, persistence failure, and an active-round rejection. */
  settings_encode(&settings, before);
  patch[0] = 255u; patch[1] = 3u; patch[11] = 9u;
  receive(101u, patch, sizeof(patch)); response(101u, 3u); ack();
  receive(102u, patch, sizeof(patch) - 1u); response(102u, 3u); ack();
  patch[11] = 0u; patch[1] = 4u;
  receive(103u, patch, sizeof(patch)); response(103u, 3u); ack();
  memset(patch, 0, sizeof(patch)); patch[0] = 1u;
  fake_write_fail = true;
  receive(104u, patch, sizeof(patch)); response(104u, 1u); ack();
  fake_write_fail = false; apply_result = 2u;
  receive(105u, patch, sizeof(patch)); response(105u, 2u); ack(); apply_result = 0u;
  settings_encode(&settings, after); assert(memcmp(before, after, sizeof(before)) == 0);
  /* Disconnection does not spin; the next phone request recovers. */
  disconnected = true; receive(106u, NULL, 0u); assert(!in_flight);
  disconnected = false; receive(106u, NULL, 0u); response(106u, 0u);
  in_flight = false; outbox_failed(&out, APP_MSG_NOT_CONNECTED, NULL);
  assert(!in_flight); receive(106u, NULL, 0u); response(106u, 0u); ack();
  /* Settings replies are serialized between real replay chunks. */
  game_init(&game, GAME_B, 1u); game_start_timed(&game, GAME_B, 1u, 60000u);
  replay_begin(&replay, &game, 1u, 20261008u);
  for (i = 0; i < 300u; ++i) replay_input(&replay, &game, GAME_UP, i % 2u == 0u);
  assert(storage_save_replay(replay.bytes, replay_finish(&replay, &game)));
  assert(online_net_start(done)); assert(dict_find(&out, MESSAGE_KEY_LB_SEQ)->value->uint8 == 0u);
  receive(107u, NULL, 0u); /* Still the replay's outbox until its ACK. */
  assert(dict_find(&out, MESSAGE_KEY_LB_SEQ) != NULL);
  ack(); response(107u, 0u); ack();
  assert(dict_find(&out, MESSAGE_KEY_LB_SEQ)->value->uint8 == 1u); ack();
  {
    DictionaryIterator in = {0};
    dict_write_uint8(&in, MESSAGE_KEY_LB_STATUS, 1u);
    dict_write_uint32(&in, MESSAGE_KEY_LB_RANK, 2u); dict_write_uint32(&in, MESSAGE_KEY_LB_TOTAL, 5u);
    inbox_received(&in, NULL);
  }
  assert(done_calls == 1u && !online_net_busy());
  /* Enabling Online may queue its replay behind a settings notification. */
  receive(108u, NULL, 0u); response(108u, 0u);
  assert(online_net_start(done)); response(108u, 0u);
  ack(); assert(dict_find(&out, MESSAGE_KEY_LB_SEQ)->value->uint8 == 0u);
  online_net_cancel();
  receive(109u, NULL, 0u);
  assert(!online_net_start(done)); /* A cancelled chunk still owns the outbox until ACK. */
  ack(); response(109u, 0u); ack(); assert(!online_net_busy());
  fake_timer_fail = true;
  assert(!online_net_start(done)); assert(!online_net_busy() && !in_flight);
  fake_timer_fail = false;
  online_net_deinit();
  puts("phone settings and shared transport tests passed");
  return 0;
}

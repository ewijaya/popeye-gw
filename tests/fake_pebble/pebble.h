#ifndef TEST_PEBBLE_H
#define TEST_PEBBLE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
typedef int32_t WakeupId;
typedef void (*WakeupHandler)(WakeupId, int32_t);
enum { E_INVALID_ARGUMENT = -4, E_OUT_OF_STORAGE = -6, E_RANGE = -8, E_DOES_NOT_EXIST = -9 };
#define APP_LOG_LEVEL_INFO 1
#define APP_LOG(...) test_log(__VA_ARGS__)
void test_log(int level, const char *format, ...);
int persist_get_size(uint32_t key);
int persist_read_data(uint32_t key, void *data, size_t size);
int persist_write_data(uint32_t key, const void *data, size_t size);
int32_t persist_read_int(uint32_t key);
int32_t persist_write_int(uint32_t key, int32_t value);
int32_t persist_delete(uint32_t key);
void wakeup_service_subscribe(WakeupHandler handler);
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify);
void wakeup_cancel(WakeupId id);
void wakeup_cancel_all(void);
bool wakeup_get_launch_event(WakeupId *id, int32_t *cookie);
bool wakeup_query(WakeupId id, time_t *timestamp);
/* Feedback adapter surface. */
typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data);
void app_timer_cancel(AppTimer *timer);
uint16_t time_ms(time_t *seconds, uint16_t *milliseconds);
typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
bool quiet_time_is_active(void);
void vibes_cancel(void);
void vibes_short_pulse(void);
void vibes_long_pulse(void);
void vibes_enqueue_custom_pattern(VibePattern pattern);
typedef enum { SpeakerWaveformSine, SpeakerWaveformSquare, SpeakerWaveformTriangle } SpeakerWaveform;
typedef struct __attribute__((__packed__)) {
  uint8_t midi_note, waveform;
  uint16_t duration_ms;
  uint8_t velocity, reserved;
} SpeakerNote;
bool speaker_play_notes(const SpeakerNote *notes, uint32_t num_notes, uint8_t volume);
void speaker_stop(void);
bool speaker_is_muted(void);
/* AppMessage surface used by the real game phone adapter in test_phone.c. */
typedef enum { APP_MSG_OK, APP_MSG_BUSY, APP_MSG_NOT_CONNECTED } AppMessageResult;
enum { TUPLE_BYTE_ARRAY, TUPLE_CSTRING, TUPLE_UINT, TUPLE_INT };
enum { MESSAGE_KEY_LB_SEQ, MESSAGE_KEY_LB_LEN, MESSAGE_KEY_LB_DATA,
       MESSAGE_KEY_LB_STATUS, MESSAGE_KEY_LB_RANK, MESSAGE_KEY_LB_TOTAL,
       MESSAGE_KEY_CFG_REQUEST, MESSAGE_KEY_CFG_PATCH, MESSAGE_KEY_CFG_DATA,
       MESSAGE_KEY_CFG_ID, MESSAGE_KEY_CFG_STATUS };
typedef union { uint8_t data[256]; uint8_t uint8; uint16_t uint16; uint32_t uint32; } FakeTupleValue;
typedef struct { uint32_t key; unsigned type; uint16_t length; FakeTupleValue *value; } Tuple;
typedef struct { Tuple tuples[12]; FakeTupleValue values[12]; unsigned count; } DictionaryIterator;
typedef void (*AppMessageInboxReceived)(DictionaryIterator *, void *);
typedef void (*AppMessageOutboxSent)(DictionaryIterator *, void *);
typedef void (*AppMessageOutboxFailed)(DictionaryIterator *, AppMessageResult, void *);
void app_message_register_inbox_received(AppMessageInboxReceived callback);
void app_message_register_outbox_sent(AppMessageOutboxSent callback);
void app_message_register_outbox_failed(AppMessageOutboxFailed callback);
void app_message_deregister_callbacks(void);
AppMessageResult app_message_open(uint32_t inbox, uint32_t outbox);
AppMessageResult app_message_outbox_begin(DictionaryIterator **out);
AppMessageResult app_message_outbox_send(void);
Tuple *dict_find(DictionaryIterator *iterator, uint32_t key);
void dict_write_data(DictionaryIterator *iterator, uint32_t key, const uint8_t *data, uint16_t size);
void dict_write_uint8(DictionaryIterator *iterator, uint32_t key, uint8_t value);
void dict_write_uint16(DictionaryIterator *iterator, uint32_t key, uint16_t value);
void dict_write_uint32(DictionaryIterator *iterator, uint32_t key, uint32_t value);
#endif

#include "fake.h"
#include <assert.h>
#include <string.h>

static uint8_t s_records[4][256];
static int s_sizes[4];
static WakeupHandler s_handler;
time_t fake_now, fake_scheduled;
int fake_schedule_calls, fake_fail_schedules, fake_schedule_error;
bool fake_write_fail, fake_launch;
WakeupId fake_id;
int32_t fake_cookie;

void fake_reset(void) {
  memset(s_records, 0, sizeof(s_records));
  memset(s_sizes, 0, sizeof(s_sizes));
  s_handler = NULL;
  fake_now = 1791180000; /* A fixed October 2026 instant; tests set local dates. */
  fake_scheduled = 0;
  fake_schedule_calls = fake_fail_schedules = 0;
  fake_schedule_error = E_RANGE;
  fake_write_fail = fake_launch = false;
  fake_id = 0; /* Zero is a valid success ID. */
  fake_cookie = 0;
}

void test_log(int level, const char *format, ...) { (void)level; (void)format; }
time_t time(time_t *out) { if (out != NULL) *out = fake_now; return fake_now; }
int persist_get_size(uint32_t key) { assert(key < 4); return s_sizes[key] ? s_sizes[key] : E_DOES_NOT_EXIST; }
int persist_read_data(uint32_t key, void *data, size_t size) {
  assert(key < 4);
  if (s_sizes[key] == 0) return E_DOES_NOT_EXIST;
  if (size > (size_t)s_sizes[key]) size = (size_t)s_sizes[key];
  memcpy(data, s_records[key], size);
  return (int)size;
}
int persist_write_data(uint32_t key, const void *data, size_t size) {
  assert(key < 4 && size <= 256);
  if (fake_write_fail) return E_OUT_OF_STORAGE;
  memcpy(s_records[key], data, size);
  s_sizes[key] = (int)size;
  return (int)size;
}
int32_t persist_read_int(uint32_t key) { int32_t v = 0; persist_read_data(key, &v, sizeof(v)); return v; }
int32_t persist_write_int(uint32_t key, int32_t v) { return persist_write_data(key, &v, sizeof(v)); }
int32_t persist_delete(uint32_t key) { assert(key < 4); s_sizes[key] = 0; return 1; }
void wakeup_service_subscribe(WakeupHandler handler) { s_handler = handler; }
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify) {
  assert(notify);
  ++fake_schedule_calls;
  if (fake_fail_schedules > 0) { --fake_fail_schedules; return fake_schedule_error; }
  if (timestamp <= fake_now) return E_INVALID_ARGUMENT;
  assert(fake_scheduled == 0); /* Single daily event, no leaks/duplicates. */
  fake_scheduled = timestamp;
  fake_cookie = cookie;
  return fake_id;
}
void wakeup_cancel(WakeupId id) { assert(id == fake_id); fake_scheduled = 0; }
void wakeup_cancel_all(void) { fake_scheduled = 0; }
bool wakeup_get_launch_event(WakeupId *id, int32_t *cookie) {
  *id = fake_id; *cookie = fake_cookie; return fake_launch;
}
bool wakeup_query(WakeupId id, time_t *timestamp) {
  if (id != fake_id || fake_scheduled == 0) return false;
  if (timestamp != NULL) *timestamp = fake_scheduled;
  return true;
}
void fake_fire(void) {
  assert(fake_scheduled > 0 && s_handler != NULL);
  fake_now = fake_scheduled;
  fake_scheduled = 0;
  s_handler(fake_id, fake_cookie);
}

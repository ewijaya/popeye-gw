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

/* One timer is sufficient to exercise the feedback service in isolation. */
struct AppTimer { AppTimerCallback callback; void *data; };
static AppTimer s_timer;
static uint16_t s_millis;
bool fake_quiet, fake_timer_fail, fake_timer_pending;
unsigned fake_short_pulses, fake_long_pulses, fake_patterns, fake_vibe_cancels;
uint32_t fake_pattern[5], fake_timer_delay;
bool fake_muted;
unsigned fake_speaker_plays, fake_speaker_stops, fake_speaker_count, fake_speaker_volume;
uint8_t fake_speaker_midi[8], fake_speaker_waveform[8];
uint16_t fake_speaker_ms[8];
void fake_feedback_reset(void) {
  fake_muted = false;
  fake_speaker_plays = fake_speaker_stops = fake_speaker_count = fake_speaker_volume = 0;
  memset(fake_speaker_midi, 0, sizeof(fake_speaker_midi));
  memset(fake_speaker_waveform, 0, sizeof(fake_speaker_waveform));
  memset(fake_speaker_ms, 0, sizeof(fake_speaker_ms));
  s_millis = 0;
  fake_quiet = fake_timer_fail = fake_timer_pending = false;
  fake_short_pulses = fake_long_pulses = fake_patterns = fake_vibe_cancels = 0;
  fake_timer_delay = 0;
  memset(fake_pattern, 0, sizeof(fake_pattern));
}
AppTimer *app_timer_register(uint32_t delay, AppTimerCallback callback, void *data) {
  assert(!fake_timer_pending && delay > 0);
  if (fake_timer_fail) return NULL;
  s_timer.callback = callback; s_timer.data = data;
  fake_timer_delay = delay; fake_timer_pending = true;
  return &s_timer;
}
void app_timer_cancel(AppTimer *timer) {
  assert(timer == &s_timer && fake_timer_pending);
  fake_timer_pending = false;
}
uint16_t time_ms(time_t *seconds, uint16_t *milliseconds) {
  if (seconds != NULL) *seconds = fake_now;
  if (milliseconds != NULL) *milliseconds = s_millis;
  return s_millis;
}
void fake_elapse(uint32_t ms) {
  uint64_t total = (uint64_t)s_millis + ms;
  fake_now += (time_t)(total / 1000u); s_millis = (uint16_t)(total % 1000u);
}
void fake_timer_fire(void) {
  assert(fake_timer_pending);
  fake_elapse(fake_timer_delay);
  fake_timer_pending = false;
  s_timer.callback(s_timer.data);
}
bool quiet_time_is_active(void) { return fake_quiet; }
void vibes_cancel(void) { ++fake_vibe_cancels; }
void vibes_short_pulse(void) { ++fake_short_pulses; }
void vibes_long_pulse(void) { ++fake_long_pulses; }
void vibes_enqueue_custom_pattern(VibePattern pattern) {
  assert(pattern.num_segments == 5);
  memcpy(fake_pattern, pattern.durations, sizeof(fake_pattern));
  ++fake_patterns;
}
bool speaker_is_muted(void) { return fake_muted; }
void speaker_stop(void) { ++fake_speaker_stops; }
bool speaker_play_notes(const SpeakerNote *notes, uint32_t count, uint8_t volume) {
  uint32_t i;
  assert(count >= 1 && count <= 8 && volume <= 100);
  for (i = 0; i < count; ++i) {
    fake_speaker_midi[i] = notes[i].midi_note; fake_speaker_waveform[i] = notes[i].waveform;
    fake_speaker_ms[i] = notes[i].duration_ms;
    assert(notes[i].midi_note <= 127 && notes[i].duration_ms > 0 && notes[i].duration_ms <= 10000);
  }
  fake_speaker_count = count; fake_speaker_volume = volume; ++fake_speaker_plays;
  return true;
}

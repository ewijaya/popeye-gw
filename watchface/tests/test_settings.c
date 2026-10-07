#include "data.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static struct tm day(int year, int month, int date) {
  struct tm local;
  memset(&local, 0, sizeof(local));
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = date;
  local.tm_hour = 12;
  local.tm_isdst = -1;
  assert(mktime(&local) != (time_t)-1);
  return local;
}

static void test_defaults_and_partial_settings(void) {
  FaceSettings s;
  settings_defaults(&s);
  assert(s.preset == FACE_PRESET_EVERYDAY && s.row1 == FACE_ROW_DATE && s.row2 == FACE_ROW_BATTERY);
  assert(s.animation_mode == FACE_ANIMATION_ARCADE && s.animation_speed == 0 && s.animation_pause == 1);
  /* Lively (wire value 0) is retired: a saved or sent 0 becomes Arcade; the others keep their numbers. */
  settings_apply_integer(&s, 34, 0); settings_validate(&s);
  assert(s.animation_mode == FACE_ANIMATION_ARCADE);
  settings_apply_integer(&s, 34, 2); settings_validate(&s);
  assert(s.animation_mode == FACE_ANIMATION_RELAXED);
  settings_apply_integer(&s, 34, 4); settings_validate(&s);
  assert(s.animation_mode == FACE_ANIMATION_STILL);
  settings_apply_integer(&s, 34, 9); settings_validate(&s); /* out of range keeps the previous value */
  assert(s.animation_mode == FACE_ANIMATION_STILL);
  settings_defaults(&s);
  assert(s.character_activity == 7 && !s.weather_enabled && !s.disconnect_alert && !s.blink_colon);
  assert(s.color_artwork && s.celebrate && !s.weather_effects && s.low_battery_cutoff == 20);
  assert(s.theme == 1);
  assert(settings_apply_integer(&s, FACE_KEY_STEP_GOAL, 13500));
  assert(settings_apply_integer(&s, FACE_KEY_THEME, 3));
  assert(settings_apply_text(&s, FACE_KEY_WORLD_LABEL, "HOME\nTokyo"));
  assert(!settings_apply_integer(&s, FACE_KEY_PHONE_READY, 1));
  assert(!settings_apply_text(&s, FACE_KEY_LOCATION_NAME, "Tokyo"));
  settings_validate(&s);
  assert(s.step_goal == 13500 && s.theme == 3 && strcmp(s.world_label, "HOMEToky") == 0);
  /* A later partial change must not silently reapply the saved preset. */
  settings_apply_integer(&s, FACE_KEY_BLINK_COLON, 1);
  settings_validate(&s);
  assert(s.preset == FACE_PRESET_EVERYDAY && s.step_goal == 13500 && s.theme == 3 && s.blink_colon);
}

static void test_validation(void) {
  FaceSettings s;
  settings_defaults(&s);
  s.disconnect_alert = true; s.weather_effects = true;
  s.row1 = -1; s.row2 = INT32_MAX; s.rotate_seconds = 1;
  s.time_format = 100; s.step_goal = INT32_MIN; s.weather_refresh = 0;
  s.animation_speed = 1; s.animation_pause = 500; s.character_activity = -8;
  s.quiet_start = 28; s.quiet_end = -1; s.low_battery_cutoff = 90;
  s.background_color = -1; s.segment_color = INT32_MAX; s.ghost_strength = 100;
  memset(s.world_label, 'Q', sizeof(s.world_label));
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2026-02-29");
  settings_validate(&s);
  assert(!s.disconnect_alert && !s.weather_effects);
  assert(s.row1 == 0 && s.row2 == 6 && s.rotate_seconds == 15 && s.time_format == 2);
  assert(s.step_goal == 100 && s.weather_refresh == 15 && s.animation_speed == 500);
  assert(s.animation_pause == 10 && s.character_activity == 0 && s.quiet_start == 23 && s.quiet_end == 0);
  assert(s.low_battery_cutoff == 50 && s.background_color == 0 && s.segment_color == 0xffffff);
  assert(s.ghost_strength == 2 && strlen(s.world_label) == 8 && s.event_date[0] == '\0');
  s.rotate_seconds = 0; s.animation_speed = 0; s.low_battery_cutoff = 0;
  settings_validate(&s);
  assert(s.rotate_seconds == 0 && s.animation_speed == 0 && s.low_battery_cutoff == 0);
  s.theme = 4;
  settings_validate(&s);
  assert(s.theme == 1); /* Retired Midnight becomes Ivory. */
  s.theme = 0;
  settings_validate(&s);
  assert(s.theme == 0); /* Explicit Original is still selectable. */
}

static void test_preset_preserves_personal_settings(void) {
  FaceSettings s;
  int preset;
  settings_defaults(&s);
  s.step_goal = 22222; s.weather_enabled = true; s.weather_units = 1; s.weather_refresh = 90;
  s.disconnect_alert = true; s.quiet_hours = true; s.quiet_start = 17; s.low_battery_cutoff = 8;
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2028-02-29");
  settings_apply_text(&s, FACE_KEY_WORLD_LABEL, "TOKYO");
  for (preset = 1; preset <= 5; ++preset) {
    if (preset == 3) continue;
    s.animation_mode = FACE_ANIMATION_ARCADE; s.animation_pause = 4; s.character_activity = 5;
    s.theme = 4; s.custom_colors = true; s.reduced_motion = true; s.animation_speed = 4000;
    settings_apply_preset(&s, preset);
    assert(s.preset == preset && s.step_goal == 22222 && s.weather_enabled && s.weather_units == 1);
    assert(s.weather_refresh == 90 && !s.disconnect_alert && s.quiet_hours && s.quiet_start == 17);
    assert(s.low_battery_cutoff == 8 && strcmp(s.world_label, "TOKYO") == 0);
    assert(strcmp(s.event_date, "2028-02-29") == 0 && s.theme == 1 && !s.custom_colors && s.animation_speed == 4000);
    assert(s.animation_mode == FACE_ANIMATION_ARCADE && s.animation_pause == 4 && s.character_activity == 5 && s.reduced_motion);
    if (preset == 1) assert(s.row1 == 0 && s.row2 == 0);
    if (preset == 2) assert(s.row1 == 1 && s.row2 == 2);
    if (preset == 4) assert(s.row1 == 5 && s.row2 == 1);
    if (preset == 5) assert(s.large_time && s.high_contrast);
    else assert(!s.large_time && !s.high_contrast);
  }
  settings_apply_preset(&s, 0);
  assert(s.preset == 0 && s.large_time); /* Custom is a label, never a reset. */
  settings_apply_preset(&s, 3);
  assert(s.preset == 0 && s.large_time && s.animation_mode == FACE_ANIMATION_ARCADE);
  s.animation_mode = FACE_ANIMATION_RELAXED; s.reduced_motion = false;
  settings_apply_preset(&s, FACE_PRESET_CLASSIC);
  assert(s.animation_mode == FACE_ANIMATION_RELAXED && !s.reduced_motion);
}

static void test_storage(void) {
  FaceSettings s, decoded, unchanged;
  uint8_t encoded[FACE_SETTINGS_STORAGE_SIZE], corrupted[FACE_SETTINGS_STORAGE_SIZE];
  size_t i;
  settings_defaults(&s);
  s.theme = 3; s.step_goal = 25000; s.weather_enabled = true; s.event_repeat = true;
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2028-02-29");
  assert(FACE_SETTINGS_STORAGE_SIZE <= 256);
  assert(settings_encode(&s, encoded, sizeof(encoded)) == sizeof(encoded));
  assert(settings_encode(&s, encoded, sizeof(encoded)-1) == 0);
  assert(settings_decode(&decoded, encoded, sizeof(encoded)));
  assert(memcmp(&s, &decoded, sizeof(s)) == 0);
  unchanged = decoded;
  for (i = 0; i < sizeof(encoded); ++i) {
    memcpy(corrupted, encoded, sizeof(encoded));
    corrupted[i] ^= 0x80;
    assert(!settings_decode(&decoded, corrupted, sizeof(corrupted)));
    assert(memcmp(&decoded, &unchanged, sizeof(decoded)) == 0);
  }
  assert(!settings_decode(&decoded, encoded, sizeof(encoded)-1));
  assert(!settings_decode(&decoded, NULL, 0));
}

static void test_dates_and_anniversaries(void) {
  FaceSettings s;
  struct tm local = day(2026, 10, 6);
  int days;
  settings_defaults(&s);
  assert(!settings_event_days(&s, &local, &days));
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2026-10-07");
  assert(settings_event_days(&s, &local, &days) && days == 1);
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2026-10-06");
  assert(settings_event_days(&s, &local, &days) && days == 0);
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2026-10-05");
  assert(!settings_event_days(&s, &local, &days));
  s.event_elapsed = true;
  assert(settings_event_days(&s, &local, &days) && days == -1);
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2028-02-29");
  local = day(2028, 2, 28);
  assert(settings_event_days(&s, &local, &days) && days == 1);
  s.event_repeat = true; s.event_elapsed = false;
  local = day(2027, 2, 28);
  assert(settings_event_days(&s, &local, &days) && days == 0);
  local = day(2027, 3, 1);
  assert(settings_event_days(&s, &local, &days) && days == 365);
  s.event_elapsed = true;
  local = day(2028, 2, 28);
  assert(settings_event_days(&s, &local, &days) && days == -365);
  local = day(2028, 3, 1);
  assert(settings_event_days(&s, &local, &days) && days == -1);
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2100-02-29");
  assert(!settings_event_days(&s, &local, &days));
  settings_apply_text(&s, FACE_KEY_EVENT_DATE, "2000-02-29");
  local = day(2000, 2, 29);
  assert(settings_event_days(&s, &local, &days) && days == 0);
  local.tm_mon = 12;
  assert(!settings_event_days(&s, &local, &days));
}

static void test_formatting_and_freshness(void) {
  FaceSettings s;
  FaceData d;
  struct tm local = day(2026, 10, 6), world;
  char out[64];
  time_t now = 1767225600; /* 2026-01-01 00:00 UTC */
  int language;
  settings_defaults(&s); memset(&d, 0, sizeof(d));
  data_format_date(out, sizeof(out), &s, &local);
  assert(strcmp(out, "Tue 6 Oct") == 0);
  s.show_year = s.show_week = true;
  for (language = 0; language < 4; ++language) {
    s.language = language;
    data_format_date(out, sizeof(out), &s, &local);
    assert(strlen(out) <= 16 && strstr(out, "2026") && strstr(out, "W41"));
  }
  s.language = 4;
  data_format_date(out, sizeof(out), &s, &local);
  assert(strcmp(out, "2026/10/06火W41") == 0);
  s.show_year = s.show_week = false;
  data_format_date(out, sizeof(out), &s, &local);
  assert(strcmp(out, "10月6日(火)") == 0);
  s.date_format = 2;
  data_format_date(out, sizeof(out), &s, &local);
  assert(strcmp(out, "10/06(火)") == 0);
  s.show_year = s.show_week = true;
  local = day(2021, 1, 1);
  s.language = 0;
  s.date_format = 2;
  data_format_date(out, sizeof(out), &s, &local);
  assert(strcmp(out, "Fr01/01/2021 W53") == 0);
  local = day(2018, 12, 31);
  data_format_date(out, sizeof(out), &s, &local);
  assert(strstr(out, "W01"));
  d.world_updated = now; d.world_offset = 540;
  assert(data_world_time(&d, now, &world) && world.tm_hour == 9 && world.tm_mday == 1);
  s.time_format = 1;
  data_format_world(out, sizeof(out), &s, &d, now, true);
  assert(strcmp(out, "HOME 9:00a") == 0);
  s.time_format = 2;
  data_format_world(out, sizeof(out), &s, &d, now, false);
  assert(strcmp(out, "HOME 09:00") == 0);
  d.world_offset = -480;
  assert(data_world_time(&d, now, &world) && world.tm_mday == 31 && world.tm_mon == 11 && world.tm_hour == 16);
  d.world_offset = 330;
  assert(data_world_time(&d, now, &world) && world.tm_hour == 5 && world.tm_min == 30);
  assert(!data_world_stale(&d, now + 48*3600 - 1));
  assert(data_world_stale(&d, now + 48*3600));
  d.world_offset = 841;
  assert(!data_world_time(&d, now, &world));
  s.weather_enabled = true; d.weather_updated = now; d.weather_status = 1;
  assert(!data_weather_stale(&s, &d, now + 3599));
  assert(data_weather_stale(&s, &d, now + 3600));
  d.weather_status = 2;
  assert(data_weather_stale(&s, &d, now));
  d.weather_status = 1; d.weather_unit = 1;
  assert(data_weather_stale(&s, &d, now));
  d.weather_unit = 0; d.weather_updated = now + 301;
  assert(data_weather_stale(&s, &d, now));
  d.health_available = true; d.steps = 5050;
  assert(data_step_percent(&s, &d) == 50);
  d.steps = INT32_MAX;
  assert(data_step_percent(&s, &d) == 100);
  d.health_available = false;
  assert(data_step_percent(&s, &d) == 0);
  local = day(2026, 10, 6);
  local.tm_year = INT_MAX;
  data_format_date(out, sizeof(out), &s, &local);
  assert(out[0] == '\0');
  local.tm_year = INT_MIN;
  data_format_date(out, sizeof(out), &s, &local);
  assert(out[0] == '\0');
}

static void test_quiet_rotation_and_runtime(void) {
  FaceSettings s;
  FaceData d;
  FaceRuntime r;
  struct tm local = day(2026, 10, 6);
  unsigned effects, i;
  uint32_t frame;
  settings_defaults(&s); memset(&d, 0, sizeof(d)); memset(&r, 0, sizeof(r));
  d.battery_percent = 80;
  s.quiet_hours = true;
  local.tm_hour = 22; assert(settings_quiet_now(&s, &local));
  local.tm_hour = 6; assert(settings_quiet_now(&s, &local));
  local.tm_hour = 7; assert(!settings_quiet_now(&s, &local));
  s.quiet_start = s.quiet_end;
  assert(!settings_quiet_now(&s, &local));
  s.quiet_start = 8; s.quiet_end = 17;
  local.tm_hour = 12; assert(settings_quiet_now(&s, &local));
  s.quiet_hours = false;
  effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK);
  assert(effects & FACE_EFFECT_START); assert(r.timer_pending && r.animate);
  for (i = 0; i < 100; ++i) {
    assert(!(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK) & FACE_EFFECT_START));
    frame = r.frame;
    effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_BEAT);
    assert((effects & FACE_EFFECT_START) && r.frame == frame + 1 && r.timer_pending);
  }
  frame = r.frame;
  effects = data_runtime_update(&r, &s, &d, &local, false, false, FACE_RUNTIME_CHECK);
  assert((effects & FACE_EFFECT_CANCEL) && !r.animate && !r.timer_pending);
  local.tm_hour = 23; /* Resume hours later: one start, no frame catchup. */
  effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK);
  assert((effects & FACE_EFFECT_START) && r.frame == frame);
  effects = data_runtime_update(&r, &s, &d, &local, true, true, FACE_RUNTIME_CHECK);
  assert((effects & FACE_EFFECT_CANCEL) && !r.animate);
  data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK);
  d.battery_percent = 20;
  effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK);
  assert((effects & FACE_EFFECT_CANCEL) && !r.animate);
  d.charging = true;
  assert(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK) & FACE_EFFECT_START);
  s.reduced_motion = true;
  assert(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK) & FACE_EFFECT_CANCEL);
  s.reduced_motion = false;
  data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK);
  data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_TIMER_FAILED);
  assert(!r.animate && !r.timer_pending && r.timer_failed);
  for (i = 0; i < 10; ++i)
    assert(!(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK) & FACE_EFFECT_START));
  assert(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_MINUTE) & FACE_EFFECT_START);
  s.animation_mode = FACE_ANIMATION_CLASSIC;
  effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_RESTART);
  assert((effects & FACE_EFFECT_CANCEL) && (effects & FACE_EFFECT_START) && r.frame == 1);
  for (i = 1; i <= 5; ++i) {
    assert(r.frame == i && r.animate && r.timer_pending);
    data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_BEAT);
  }
  assert(!r.animate && !r.timer_pending);
  assert(!(data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_CHECK) & FACE_EFFECT_START));
  s.animation_mode = FACE_ANIMATION_ARCADE;
  assert(data_animation_allowed(&s, &d, &local, true, false));
  /* Still's paused game frame needs no timer: it is redrawn by the minute tick. */
  s.animation_mode = FACE_ANIMATION_STILL;
  assert(!data_animation_allowed(&s, &d, &local, true, false));
  effects = data_runtime_update(&r, &s, &d, &local, true, false, FACE_RUNTIME_MINUTE);
  assert(!(effects & FACE_EFFECT_START) && !r.animate && !r.timer_pending);
  s.rotate_seconds = 15;
  local.tm_hour = 12; local.tm_min = 0; local.tm_sec = 14;
  assert(data_rotation_delay(&s, &local, true) == 1000);
  assert(data_rotating_row(&s, &local) == s.row1);
  local.tm_sec = 15;
  assert(data_rotation_delay(&s, &local, true) == 15000);
  assert(data_rotating_row(&s, &local) == s.row2);
  assert(data_rotation_delay(&s, &local, false) == 0);
  s.reduced_motion = s.quiet_hours = true;
  assert(data_rotation_delay(&s, &local, true) == 15000); /* Data cadence remains usable. */
  s.row2 = FACE_ROW_NONE;
  assert(data_rotation_delay(&s, &local, true) == 0);
}

static void test_request_retry(void) {
  FaceRequestQueue q;
  memset(&q, 0, sizeof(q));
  q.pending = 3;
  assert(data_request_take(&q, 1000, false) == 0 && q.pending == 3);
  assert(data_request_take(&q, 1000, true) == 3 && q.busy && q.inflight == 3 && q.pending == 0);
  q.pending |= 1; /* New request arriving while in flight stays queued. */
  assert(data_request_take(&q, 1060, true) == 0);
  data_request_complete(&q, false);
  assert(q.pending == 3 && !q.busy && q.inflight == 0);
  assert(data_request_take(&q, 1059, true) == 0);
  assert(data_request_take(&q, 1060, true) == 3);
  data_request_complete(&q, true);
  assert(q.pending == 0 && !q.busy);
  q.pending = 2;
  assert(data_request_take(&q, 1119, true) == 0);
  assert(data_request_take(&q, 1120, true) == 2);
  data_request_complete(&q, false); /* Both begin failures and callback failures use this. */
  assert(q.pending == 2 && q.next_attempt == 1180);
}

static void test_data_messages(void) {
  FaceData d, snapshot;
  time_t now = 1767225600;
  memset(&d, 0, sizeof(d)); memset(&snapshot, 0, sizeof(snapshot));
  snapshot.weather_temp = 10; snapshot.weather_high = 15; snapshot.weather_low = 5;
  snapshot.weather_status = 1; snapshot.weather_updated = now;
  strcpy(snapshot.weather_location, "TOKYO");
  assert(data_apply_weather(&d, &snapshot, true, true, now));
  assert(d.weather_temp == 10 && d.weather_status == 1 && d.weather_updated == now);
  snapshot.weather_status = 2; snapshot.weather_updated = now - 4000;
  snapshot.weather_unit = 1; snapshot.weather_temp = 50;
  snapshot.weather_high = 59; snapshot.weather_low = 41;
  assert(data_apply_weather(&d, &snapshot, true, true, now));
  assert(d.weather_temp == 50 && d.weather_unit == 1 && d.weather_status == 2 && d.weather_updated == now-4000);
  snapshot.weather_temp = 123; snapshot.weather_updated = now;
  assert(data_apply_weather(&d, &snapshot, false, false, now));
  assert(d.weather_temp == 50 && d.weather_updated == now-4000); /* Error-only payload preserves cache. */
  snapshot.weather_status = 1; snapshot.weather_updated = now+301;
  assert(!data_apply_weather(&d, &snapshot, true, true, now));
  assert(d.weather_temp == 50 && d.weather_status == 2);
  snapshot.weather_status = 0; snapshot.weather_updated = 0;
  assert(data_apply_weather(&d, &snapshot, false, true, now));
  assert(d.weather_updated == 0 && d.weather_location[0] == '\0');
  assert(data_apply_world(&d, 330, now, true, now));
  assert(d.world_offset == 330 && d.world_updated == now);
  assert(!data_apply_world(&d, 900, now, true, now));
  assert(!data_apply_world(&d, 0, now, false, now));
  assert(d.world_offset == 330 && d.world_updated == now);
  assert(data_apply_world(&d, 0, 0, false, now));
  assert(d.world_updated == 0); /* Explicit unavailable zone invalidates old offset availability. */
}

int main(void) {
  test_defaults_and_partial_settings();
  test_validation();
  test_preset_preserves_personal_settings();
  test_storage();
  test_dates_and_anniversaries();
  test_formatting_and_freshness();
  test_quiet_rotation_and_runtime();
  test_request_retry();
  test_data_messages();
  puts("settings/data/runtime tests passed");
  return 0;
}

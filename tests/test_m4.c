#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "alarm.h"
#include "clock.h"
#include "storage.h"
#include "fake.h"
#include "orientation.h"

static time_t local_date(int year, int month, int day, int hour, int minute, int second) {
  struct tm local = {0};
  local.tm_year = year - 1900; local.tm_mon = month - 1; local.tm_mday = day;
  local.tm_hour = hour; local.tm_min = minute; local.tm_sec = second; local.tm_isdst = -1;
  return mktime(&local);
}

static void test_records(void) {
  Settings settings, restored;
  HighScores scores, loaded;
  uint8_t settings_bytes[SETTINGS_RECORD_SIZE], scores_bytes[SCORES_RECORD_SIZE];
  unsigned i, bit;
  settings_defaults(&settings);
  assert(!settings.swap_buttons && settings.vibration && settings.ghosts && settings.attract);
  assert(!settings.alarm_on && settings.alarm_hour == 7 && settings.alarm_minute == 0);
  settings.landscape = true;
  settings.swap_buttons = true; settings.ghosts = false; settings.attract = false;
  settings.alarm_on = true; settings.alarm_hour = 23; settings.alarm_minute = 59;
  settings_encode(&settings, settings_bytes);
  assert(settings_decode(&restored, settings_bytes, sizeof(settings_bytes)));
  assert(restored.landscape);
  assert(restored.swap_buttons && !restored.ghosts && !restored.attract && restored.alarm_on);
  assert(restored.vibration && restored.alarm_hour == 23 && restored.alarm_minute == 59);
  for (i = 0; i < sizeof(settings_bytes); ++i) {
    for (bit = 0; bit < 8; ++bit) {
      settings_bytes[i] ^= (uint8_t)(1u << bit);
      assert(!settings_decode(&restored, settings_bytes, sizeof(settings_bytes)));
      assert(!restored.alarm_on && restored.alarm_hour == 7 && restored.ghosts);
      settings_bytes[i] ^= (uint8_t)(1u << bit);
    }
    assert(!settings_decode(&restored, settings_bytes, i));
  }
  settings.alarm_hour = 24; settings_encode(&settings, settings_bytes);
  assert(!settings_decode(&restored, settings_bytes, sizeof(settings_bytes)));
  assert(!settings_decode(&restored, NULL, SETTINGS_RECORD_SIZE));
  scores_defaults(&scores);
  assert(scores_record(&scores, 0, 12345, 20261005));
  assert(scores_record(&scores, 1, UINT32_MAX, 20240229));
  assert(!scores_record(&scores, 0, 12345, 20261006)); /* Ties keep the original date. */
  assert(!scores_record(&scores, 0, 12346, 20250229));
  assert(!scores_record(&scores, 2, 12346, 20261006));
  scores_encode(&scores, scores_bytes);
  assert(scores_decode(&loaded, scores_bytes, sizeof(scores_bytes)));
  assert(loaded.best[0] == 12345 && loaded.best[1] == UINT32_MAX);
  assert(loaded.date[0] == 20261005 && loaded.date[1] == 20240229);
  for (i = 0; i < sizeof(scores_bytes); ++i) {
    for (bit = 0; bit < 8; ++bit) {
      scores_bytes[i] ^= (uint8_t)(1u << bit);
      assert(!scores_decode(&loaded, scores_bytes, sizeof(scores_bytes)));
      assert(loaded.best[0] == 0 && loaded.best[1] == 0);
      scores_bytes[i] ^= (uint8_t)(1u << bit);
    }
    assert(!scores_decode(&loaded, scores_bytes, i));
  }
  scores.date[1] = 20261301; scores_encode(&scores, scores_bytes);
  assert(!scores_decode(&loaded, scores_bytes, sizeof(scores_bytes)));
}

static void test_storage(void) {
  SaveData first, second;
  uint8_t bad[40] = {0};
  fake_reset(); storage_load(&first);
  assert(first.settings.ghosts && !first.settings.alarm_on && first.scores.best[0] == 0);
  first.settings.landscape = true;
  first.settings.swap_buttons = true; first.settings.alarm_on = true;
  first.settings.alarm_minute = 31;
  assert(scores_record(&first.scores, 1, 5000, 20261005));
  assert(storage_save_settings(&first.settings) && storage_save_scores(&first.scores));
  memset(&second, 0, sizeof(second)); storage_load(&second);
  assert(second.settings.landscape);
  assert(second.settings.swap_buttons && second.settings.alarm_minute == 31);
  assert(second.scores.best[1] == 5000 && second.scores.date[1] == 20261005);
  fake_write_fail = true;
  first.settings.alarm_on = false; first.scores.best[1] = 7000;
  assert(!storage_save_settings(&first.settings) && !storage_save_scores(&first.scores));
  storage_load(&second);
  assert(second.settings.alarm_on && second.scores.best[1] == 5000);
  fake_write_fail = false;
  persist_write_data(1, bad, sizeof(bad)); /* Reject an oversized/future record. */
  storage_load(&second);
  assert(!second.settings.alarm_on && second.settings.ghosts && second.scores.best[1] == 5000);
  persist_write_data(2, bad, SCORES_RECORD_SIZE); /* Invalid version/checksum. */
  storage_load(&second);
  assert(second.scores.best[1] == 0 && second.settings.ghosts);
}

static unsigned digit(const Scene *scene, unsigned position) {
  static const unsigned patterns[] = { 0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f };
  unsigned mask = 0, bit, i;
  for (bit = 0; bit < 7; ++bit) if (scene_lit(scene, SEG_DIGIT + 7 * position + bit)) mask |= 1u << bit;
  for (i = 0; i < 10; ++i) if (mask == patterns[i]) return i;
  assert(false); return 0;
}

static void test_clock(void) {
  Scene scene;
  time_t now = local_date(2026, 12, 31, 23, 59, 50), next;
  struct tm local = {0};
  next = clock_next_alarm(now, 0, 0);
  assert(next == local_date(2027, 1, 1, 0, 0, 0));
  now = local_date(2026, 2, 28, 7, 0, 0);
  assert(clock_next_alarm(now, 7, 0) == local_date(2026, 3, 1, 7, 0, 0));
  now = local_date(2024, 2, 28, 7, 0, 0);
  assert(clock_next_alarm(now, 7, 0) == local_date(2024, 2, 29, 7, 0, 0));
  assert(clock_next_alarm(now, 24, 0) == (time_t)-1);
  /* Calendar-day recurrence across both DST transitions (runner tests NY too). */
  now = local_date(2026, 3, 7, 8, 0, 0);
  assert(clock_next_alarm(now, 7, 0) == local_date(2026, 3, 8, 7, 0, 0));
  now = local_date(2026, 10, 31, 8, 0, 0);
  assert(clock_next_alarm(now, 7, 0) == local_date(2026, 11, 1, 7, 0, 0));
  local.tm_hour = 0; local.tm_min = 5; local.tm_sec = 0;
  clock_scene(&scene, &local, false, true, true, false);
  assert(digit(&scene, 0) == 1 && digit(&scene, 1) == 2 && digit(&scene, 2) == 0 && digit(&scene, 3) == 5);
  assert(scene_lit(&scene, SEG_AM) && !scene_lit(&scene, SEG_PM) && scene_lit(&scene, SEG_COLON));
  local.tm_hour = 12; local.tm_sec = 1;
  clock_scene(&scene, &local, false, true, false, false);
  assert(scene_lit(&scene, SEG_PM) && !scene_lit(&scene, SEG_AM) && !scene_lit(&scene, SEG_COLON));
  local.tm_hour = 0;
  clock_scene(&scene, &local, true, false, false, false);
  assert(digit(&scene, 0) == 0 && digit(&scene, 1) == 0 && scene_lit(&scene, SEG_COLON));
  assert(!scene_lit(&scene, SEG_AM) && !scene_lit(&scene, SEG_PM) && scene_lit(&scene, SEG_POPEYE + 2));
  clock_scene(&scene, &local, true, false, true, true);
  assert(scene_lit(&scene, SEG_OLIVE_BELL + 1) && !scene_lit(&scene, SEG_OLIVE_READY));
  assert(!scene_lit(&scene, SEG_BELL));
  local.tm_sec = 2;
  clock_scene(&scene, &local, true, true, false, false);
  assert(scene_lit(&scene, SEG_BRUTUS + 3u + GAME_ATTACK_IDLE));
  assert(scene_lit(&scene, SEG_POPEYE + 2) && scene_lit(&scene, SEG_OLIVE_THROW));
  assert(scene_lit(&scene, SEG_CARGO + GAME_CARGO_STEPS));
  clock_scene(&scene, &local, true, false, true, true);
  assert(scene_lit(&scene, SEG_BRUTUS + GAME_ATTACK_IDLE));
  assert(scene_lit(&scene, SEG_OLIVE_BELL) && scene_lit(&scene, SEG_BELL));
}

static unsigned rings;
static void ring(void) { ++rings; }

static void test_alarm(void) {
  Settings settings;
  time_t first;
  fake_reset(); settings_defaults(&settings); rings = 0;
  fake_now = local_date(2026, 10, 5, 6, 30, 0);
  alarm_init(&settings, ring);
  assert(fake_schedule_calls == 0 && alarm_status()->next == 0);
  settings.alarm_on = true; alarm_refresh(&settings, fake_now);
  first = local_date(2026, 10, 5, 7, 0, 0);
  assert(alarm_status()->next == first && fake_schedule_calls == 1);
  alarm_deinit(); alarm_init(&settings, ring); /* App exit/relaunch keeps the event. */
  assert(fake_schedule_calls == 1 && rings == 0);
  fake_now = first; alarm_refresh(&settings, fake_now); /* Tick before callback. */
  assert(fake_schedule_calls == 1);
  fake_fire();
  assert(rings == 1 && alarm_status()->next == local_date(2026, 10, 6, 7, 0, 0));
  settings.alarm_on = false; alarm_refresh(&settings, fake_now);
  assert(fake_scheduled == 0 && alarm_status()->next == 0);

  fake_reset(); fake_now = local_date(2026, 10, 5, 6, 30, 0);
  settings.alarm_on = true; fake_fail_schedules = 1; alarm_init(&settings, ring);
  assert(alarm_status()->next == first + 60 && alarm_status()->adjusted);
  fake_now = first + 30; alarm_refresh(&settings, fake_now);
  assert(fake_schedule_calls == 2 && alarm_status()->next == first + 60);
  fake_fire(); assert(rings == 2 && !alarm_status()->adjusted);

  fake_reset(); fake_now = local_date(2026, 10, 5, 6, 30, 0);
  fake_fail_schedules = 2; alarm_init(&settings, ring);
  assert(alarm_status()->error == E_RANGE && alarm_status()->next == 0 && fake_schedule_calls == 2);
  alarm_refresh(&settings, fake_now);
  assert(alarm_status()->error == 0 && fake_scheduled == first);
  settings.alarm_hour = 8; alarm_refresh(&settings, fake_now);
  assert(fake_scheduled == local_date(2026, 10, 5, 8, 0, 0));

  fake_reset(); fake_now = first; fake_launch = true; fake_cookie = ALARM_COOKIE;
  settings.alarm_hour = 7; alarm_init(&settings, ring);
  assert(rings == 3 && alarm_status()->next == local_date(2026, 10, 6, 7, 0, 0));
  fake_launch = false; alarm_deinit(); alarm_init(&settings, ring);
  assert(rings == 3 && fake_schedule_calls == 1);

  fake_reset(); fake_write_fail = true; alarm_init(&settings, ring);
  assert(alarm_status()->error == E_OUT_OF_STORAGE && fake_scheduled == 0);
  fake_reset(); fake_launch = true; fake_cookie = 99; alarm_init(&settings, ring);
  assert(rings == 3); /* Unrelated cookies must not ring. */
  settings.alarm_on = false; alarm_refresh(&settings, fake_now);
  alarm_deinit();
}

static void test_orientation(void) {
  /* Existing v1 settings migrate without losing alarms or other preferences. */
  const uint8_t legacy[] = { 1, 31, 23, 59, 179, 178, 236, 216 };
  Settings settings;
  uint8_t source[200], target[200 * 52];
  assert(settings_decode(&settings, legacy, sizeof(legacy)));
  assert(!settings.landscape && settings.swap_buttons && settings.alarm_on);
  assert(settings.alarm_hour == 23 && settings.alarm_minute == 59);
  assert(settings.vibration && settings.ghosts && settings.attract);
  memset(source, 0xf3, sizeof(source)); /* Magenta -> transparent. */
  memset(target, 0, sizeof(target));
  source[0] = 0xc0; source[199] = 0xff;
  orientation_pack_row(target, 52, source, 0);
  assert(target[49] == 1 && target[199 * 52 + 49] == 2);
  assert(target[50] == 0 && target[51] == 0); /* Row padding untouched. */
  orientation_pack_row(target, 52, source, 199);
  assert(target[0] == 0x40 && target[199 * 52] == 0x80);
  assert(target[50 * 52] == 0 && target[50 * 52 + 49] == 0);
}

int main(void) {
  test_records(); test_storage(); test_clock(); test_alarm(); test_orientation();
  puts("M4 tests passed: versioned storage, corrupt records, clock, calendar/DST recurrence and wakeup lifecycle");
  return 0;
}

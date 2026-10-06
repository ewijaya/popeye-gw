#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "alarm.h"
#include "clock.h"
#include "glance.h"
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
  assert(!settings.landscape && settings.buttons_bottom && settings.sound);
  settings.landscape = true;
  settings.buttons_bottom = true;
  settings.swap_buttons = true; settings.ghosts = false; settings.attract = false;
  settings.alarm_on = true; settings.alarm_hour = 23; settings.alarm_minute = 59;
  settings_encode(&settings, settings_bytes);
  assert(settings_decode(&restored, settings_bytes, sizeof(settings_bytes)));
  assert(restored.landscape && restored.buttons_bottom);
  assert(restored.swap_buttons && !restored.ghosts && !restored.attract && restored.alarm_on);
  assert(restored.vibration && restored.sound && restored.alarm_hour == 23 && restored.alarm_minute == 59);
  settings.sound = false; settings_encode(&settings, settings_bytes);
  assert(settings_bytes[0] == 5u && (settings_bytes[1] & 0x80u) == 0u);
  assert(settings_decode(&restored, settings_bytes, sizeof(settings_bytes)));
  assert(!restored.sound && restored.vibration && restored.landscape && restored.alarm_on);
  settings.sound = true; settings_encode(&settings, settings_bytes);
  assert(settings_decode(&restored, settings_bytes, sizeof(settings_bytes)) && restored.sound);
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
  first.settings.buttons_bottom = true;
  first.settings.swap_buttons = true; first.settings.alarm_on = true;
  first.settings.alarm_minute = 31;
  first.settings.sound = false;
  assert(scores_record(&first.scores, 1, 5000, 20261005));
  assert(storage_save_settings(&first.settings) && storage_save_scores(&first.scores));
  memset(&second, 0, sizeof(second)); storage_load(&second);
  assert(second.settings.landscape && second.settings.buttons_bottom);
  assert(second.settings.swap_buttons && second.settings.alarm_minute == 31 && !second.settings.sound);
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
  const uint8_t legacy_top[] = { 2, 63, 23, 59, 32, 220, 204, 152 };
  Settings settings;
  uint8_t source[200], target[200 * 52];
  assert(settings_decode(&settings, legacy, sizeof(legacy)));
  assert(!settings.landscape && settings.buttons_bottom && settings.swap_buttons && settings.alarm_on);
  assert(settings.alarm_hour == 23 && settings.alarm_minute == 59);
  assert(settings.vibration && settings.ghosts && settings.attract);
  assert(settings_decode(&settings, legacy_top, sizeof(legacy_top)));
  assert(settings.landscape && !settings.buttons_bottom && settings.swap_buttons && settings.alarm_on);
  assert(settings.alarm_hour == 23 && settings.alarm_minute == 59);
  assert(settings.vibration && settings.ghosts && settings.attract);
  memset(source, 0xf3, sizeof(source)); /* Magenta -> transparent. */
  memset(target, 0, sizeof(target));
  source[0] = 0xc0; source[199] = 0xff;
  source[100] = 0xc4; /* Menu header green must survive landscape rotation. */
  orientation_pack_row(target, 52, source, 0, false);
  assert(target[49] == 1 && target[199 * 52 + 49] == 2);
  assert(target[100 * 52 + 49] == 3);
  assert(target[50] == 0 && target[51] == 0); /* Row padding untouched. */
  orientation_pack_row(target, 52, source, 199, false);
  assert(target[0] == 0x40 && target[199 * 52] == 0x80);
  assert(target[100 * 52] == 0xc0);
  assert(target[50 * 52] == 0 && target[50 * 52 + 49] == 0);
  memset(target, 0, sizeof(target));
  orientation_pack_row(target, 52, source, 0, true);
  assert(target[0] == 0x80 && target[199 * 52] == 0x40);
  assert(target[99 * 52] == 0xc0);
  orientation_pack_row(target, 52, source, 199, true);
  assert(target[49] == 2 && target[199 * 52 + 49] == 1);
  assert(target[99 * 52 + 49] == 3);
  assert(target[50] == 0 && target[51] == 0);
  assert(target[50 * 52] == 0 && target[50 * 52 + 49] == 0);
}

static void write_checksum(uint8_t record[SETTINGS_RECORD_SIZE]) {
  uint32_t hash = UINT32_C(2166136261);
  unsigned i;
  for (i = 0; i < 4; ++i) hash = (hash ^ record[i]) * UINT32_C(16777619);
  for (i = 0; i < 4; ++i) record[4 + i] = (uint8_t)(hash >> (8u * i));
}

static void test_saved_orientation_preferences(void) {
  /* Actual earlier wire formats: v2 Vertical, then v3 Vertical/Top/Bottom.
   * The first two have no saved horizontal side and should start at Bottom. */
  const uint8_t legacy[][SETTINGS_RECORD_SIZE] = {
    { 2, 31, 23, 59, 0, 14, 246, 123 },
    { 3, 31, 23, 59, 33, 17, 8, 50 },
    { 3, 63, 23, 59, 1, 67, 49, 21 },
    { 3, 127, 23, 59, 193, 21, 222, 206 }
  };
  const uint8_t future[] = { 6, 0, 23, 59, 202, 123, 241, 253 };
  Settings settings, restored;
  uint8_t bytes[SETTINGS_RECORD_SIZE];
  unsigned i, bottom;
  for (i = 0; i < sizeof(legacy) / sizeof(legacy[0]); ++i) {
    assert(settings_decode(&settings, legacy[i], sizeof(legacy[i])));
    assert(settings.landscape == (i >= 2u));
    assert(settings.buttons_bottom == (i != 2u));
    assert(settings.swap_buttons && settings.vibration && settings.ghosts && settings.attract);
    assert(settings.alarm_on && settings.alarm_hour == 23u && settings.alarm_minute == 59u);
    settings_encode(&settings, bytes);
    assert(settings_decode(&restored, bytes, sizeof(bytes)));
    assert(restored.landscape == settings.landscape && restored.buttons_bottom == settings.buttons_bottom);
    assert(restored.alarm_on && restored.alarm_hour == 23u && restored.alarm_minute == 59u);
  }
  /* Real v4 records (before Sound): Sound defaults On, everything else is kept. A set
   * bit 7 is invalid before v5, and v5 may clear it. */
  {
    unsigned version, flags;
    for (version = 1; version <= 5; ++version) {
      for (flags = 0; flags < 256; ++flags) {
        uint8_t record[SETTINGS_RECORD_SIZE] = { (uint8_t)version, (uint8_t)flags, 6, 30 };
        bool valid = version == 5u || (flags & (version == 1u ? 0xe0u : version == 2u ? 0xc0u : 0x80u)) == 0u;
        write_checksum(record);
        assert(settings_decode(&settings, record, sizeof(record)) == valid);
        if (!valid) continue;
        assert(settings.sound == (version < 5u || (flags & 0x80u) != 0u));
        assert(settings.swap_buttons == ((flags & 1u) != 0u) && settings.vibration == ((flags & 2u) != 0u));
        assert(settings.alarm_hour == 6u && settings.alarm_minute == 30u && settings.alarm_on == ((flags & 16u) != 0u));
        assert(settings.buttons_bottom == (version >= 4u ? (flags & 64u) != 0u :
                                           !settings.landscape || (version == 3u && (flags & 64u) != 0u)));
      }
    }
  }
  /* A horizontal side survives Vertical, app exit/reload, and unrelated edits. */
  for (bottom = 0; bottom < 2u; ++bottom) {
    fake_reset();
    settings_defaults(&settings);
    settings.landscape = true;
    settings.buttons_bottom = bottom != 0;
    assert(storage_save_settings(&settings));
    settings.landscape = false;
    settings.ghosts = false;
    assert(storage_save_settings(&settings));
    {
      SaveData loaded;
      storage_load(&loaded);
      assert(!loaded.settings.landscape && !loaded.settings.ghosts);
      assert(loaded.settings.buttons_bottom == (bottom != 0));
      loaded.settings.landscape = true;
      assert(storage_save_settings(&loaded.settings));
      storage_load(&loaded);
      assert(loaded.settings.landscape && loaded.settings.buttons_bottom == (bottom != 0));
    }
  }
  assert(!settings_decode(&settings, future, sizeof(future)));
  assert(!settings.landscape && settings.buttons_bottom && !settings.alarm_on);
}

static void test_oriented_controls(void) {
  unsigned bottom, swapped;
  for (bottom = 0; bottom < 2; ++bottom) {
    for (swapped = 0; swapped < 2; ++swapped) {
      Game game;
      bool physical_left_is_up = !bottom;
      GameButton left = orientation_logical_up(physical_left_is_up, bottom) ? GAME_UP : GAME_DOWN;
      GameButton right = orientation_logical_up(!physical_left_is_up, bottom) ? GAME_UP : GAME_DOWN;
      game_init(&game, GAME_A, 3u);
      game.controls_swapped = swapped != 0;
      assert(game_input(&game, left, true));
      assert(game.popeye_pose == (swapped ? 3u : 1u));
      assert(!game_input(&game, left, true)); /* Holding still never repeats. */
      game_input(&game, left, false);
      assert(game.held_buttons == 0u);
      assert(game_input(&game, right, true));
      assert(game.popeye_pose == 2u);
      game_input(&game, right, false);
      assert(game.held_buttons == 0u);
    }
  }
}

static void test_stats_records(void) {
  Stats stats, loaded;
  uint8_t bytes[STATS_RECORD_SIZE];
  SaveData data;
  unsigned i, bit;
  stats_defaults(&stats);
  assert(stats.games[0] == 0 && stats.catches == 0 && stats.play_seconds == 0);
  stats.games[0] = 3; stats.games[1] = 4; stats.catches = 5000; stats.drops = 61; stats.hits = 7;
  stats.bonuses = 9; stats.best_streak = 321; stats.play_seconds = 98765;
  stats_encode(&stats, bytes);
  assert(bytes[0] == 1u);
  assert(stats_decode(&loaded, bytes, sizeof(bytes)));
  assert(loaded.games[0] == 3 && loaded.games[1] == 4 && loaded.catches == 5000 && loaded.drops == 61);
  assert(loaded.hits == 7 && loaded.bonuses == 9 && loaded.best_streak == 321 && loaded.play_seconds == 98765);
  for (i = 0; i < sizeof(bytes); ++i) {
    for (bit = 0; bit < 8; ++bit) {
      bytes[i] ^= (uint8_t)(1u << bit);
      assert(!stats_decode(&loaded, bytes, sizeof(bytes)));
      assert(loaded.catches == 0 && loaded.games[1] == 0 && loaded.play_seconds == 0); /* Defaults. */
      bytes[i] ^= (uint8_t)(1u << bit);
    }
    assert(!stats_decode(&loaded, bytes, i));
  }
  assert(!stats_decode(&loaded, NULL, STATS_RECORD_SIZE) && !stats_decode(&loaded, bytes, sizeof(bytes) + 1));
  stats.best_streak = stats.catches + 1; /* Impossible: a run cannot exceed the catches. */
  stats_encode(&stats, bytes);
  assert(!stats_decode(&loaded, bytes, sizeof(bytes)));
  stats.best_streak = 321; stats.games[0] = UINT32_MAX; stats_encode(&stats, bytes);
  assert(stats_decode(&loaded, bytes, sizeof(bytes)) && loaded.games[0] == UINT32_MAX);

  /* Stored under its own key, independent of settings, scores and the wakeup id. */
  fake_reset(); storage_load(&data);
  assert(data.stats.catches == 0 && data.stats.best_streak == 0);
  data.stats = stats; data.stats.games[0] = 11;
  assert(storage_save_stats(&data.stats));
  assert(persist_get_size(4) == STATS_RECORD_SIZE && persist_get_size(1) < 0 && persist_get_size(2) < 0);
  memset(&data, 0, sizeof(data)); storage_load(&data);
  assert(data.stats.games[0] == 11 && data.stats.play_seconds == 98765 && data.settings.sound);
  fake_write_fail = true;
  data.stats.catches = 1; assert(!storage_save_stats(&data.stats));
  fake_write_fail = false; storage_load(&data);
  assert(data.stats.catches == 5000); /* The failed save changed nothing. */
  persist_write_data(4, bytes, sizeof(bytes) - 1); /* Truncated/corrupt record: defaults, others intact. */
  assert(storage_save_scores(&data.scores) && storage_save_settings(&data.settings));
  storage_load(&data);
  assert(data.stats.catches == 0 && data.settings.sound);
}

static unsigned xorshift(unsigned *state) {
  *state ^= *state << 13; *state ^= *state >> 17; *state ^= *state << 5;
  return *state;
}

static void test_stats_accumulation(void) {
  Stats stats;
  uint32_t streak = 0, carry = 0;
  stats_defaults(&stats);
  stats_note_events(&stats, GAME_EVENT_LAUNCH, &streak);
  assert(stats.catches == 0 && streak == 0);
  stats_note_events(&stats, GAME_EVENT_CATCH | GAME_EVENT_HIGH_SCORE, &streak);
  stats_note_events(&stats, GAME_EVENT_CATCH, &streak);
  stats_note_events(&stats, GAME_EVENT_DROP, &streak); /* A first drop is half a miss: the run goes on. */
  stats_note_events(&stats, GAME_EVENT_CATCH, &streak);
  assert(stats.catches == 3 && stats.drops == 1 && stats.hits == 0 && streak == 3 && stats.best_streak == 3);
  stats_note_events(&stats, GAME_EVENT_DROP | GAME_EVENT_MISS, &streak); /* Second drop: a MISS. */
  assert(stats.drops == 2 && stats.hits == 0 && streak == 0 && stats.best_streak == 3);
  stats_note_events(&stats, GAME_EVENT_CATCH, &streak);
  stats_note_events(&stats, GAME_EVENT_MISS, &streak); /* Brutus. */
  assert(stats.hits == 1 && stats.drops == 2 && streak == 0 && stats.best_streak == 3);
  stats_note_events(&stats, GAME_EVENT_CATCH | GAME_EVENT_BONUS, &streak);
  assert(stats.bonuses == 1 && stats.catches == 5);
  stats_note_events(&stats, GAME_EVENT_MISS | GAME_EVENT_OVER, &streak);
  assert(stats.hits == 2 && streak == 0);
  stats_count_game(&stats, 0); stats_count_game(&stats, 1); stats_count_game(&stats, 1); stats_count_game(&stats, 2);
  assert(stats.games[0] == 1 && stats.games[1] == 2);
  /* Active milliseconds keep their sub-second remainder across calls. */
  stats_add_play_ms(&stats, &carry, 560); stats_add_play_ms(&stats, &carry, 440);
  assert(stats.play_seconds == 1 && carry == 0);
  stats_add_play_ms(&stats, &carry, 1500); stats_add_play_ms(&stats, &carry, 700);
  assert(stats.play_seconds == 3 && carry == 200);
  stats_add_play_ms(&stats, &carry, 999); stats_add_play_ms(&stats, &carry, 1);
  assert(stats.play_seconds == 4 && carry == 200);
  stats.catches = UINT32_MAX; stats.play_seconds = UINT32_MAX - 1; streak = UINT32_MAX;
  stats_note_events(&stats, GAME_EVENT_CATCH, &streak);
  stats_add_play_ms(&stats, &carry, 5000);
  assert(stats.catches == UINT32_MAX && streak == UINT32_MAX && stats.play_seconds == UINT32_MAX);

  /* The totals agree with the real engine's own counters over many imperfect games. */
  {
    unsigned state = 12345u, seed, mode;
    Stats totals;
    uint32_t catches = 0, drops = 0, hits = 0, misses = 0, games = 0, run = 0, best_run = 0, bonuses = 0;
    uint32_t live = 0, play_ms = 0;
    stats_defaults(&totals); carry = 0;
    for (mode = 0; mode < 2; ++mode) {
      for (seed = 1; seed <= 60; ++seed) {
        Game game;
        unsigned steps = 0;
        uint32_t before_catches, before_misses;
        game_init(&game, mode == 0 ? GAME_A : GAME_B, seed);
        live = 0; run = 0;
        while (game.status != GAME_OVER && steps++ < 4000) {
          unsigned target = 2, i;
          uint64_t soonest = UINT64_MAX;
          for (i = 0; i < GAME_MAX_CARGO; ++i)
            if (game.cargo[i].active && game.cargo[i].landing_step < soonest) {
              soonest = game.cargo[i].landing_step;
              target = game_lane_pose(game.cargo[i].lane);
            }
          if (xorshift(&state) % 8u == 0u) target = xorshift(&state) % GAME_POSES; /* Sloppy play. */
          while (game.status == GAME_PLAYING && game.popeye_pose != target) {
            GameButton button = game.popeye_pose > target ? GAME_UP : GAME_DOWN;
            game_input(&game, button, true); game_input(&game, button, false);
          }
          before_catches = game.catches; before_misses = game.total_misses;
          play_ms += game.status == GAME_RECOVERING ? game.recovery_ms_left : game.step_ms_left;
          stats_add_play_ms(&totals, &carry, game.status == GAME_RECOVERING ? game.recovery_ms_left : game.step_ms_left);
          game_step(&game);
          stats_note_events(&totals, game_take_events(&game), &live);
          if (game.total_misses != before_misses) run = 0;
          else if (game.catches != before_catches) { ++run; if (run > best_run) best_run = run; }
          assert(live == run);
        }
        catches += game.catches; drops += game.drops; hits += game.hits; misses += game.total_misses;
        bonuses += 2u * (game.score / 1000u) + (game.score % 1000u >= 200u) + (game.score % 1000u >= 500u);
        ++games;
      }
    }
    assert(games == 120 && catches > 1000 && drops > 100 && hits > 0 && misses > 0 && best_run > 5);
    assert(totals.catches == catches && totals.drops == drops && totals.hits == hits);
    assert(totals.best_streak == best_run && totals.bonuses == bonuses);
    assert(totals.play_seconds == play_ms / 1000u && carry == play_ms % 1000u);
  }
}

static void test_glance(void) {
  HighScores scores;
  Settings settings;
  char text[64], small[10];
  scores_defaults(&scores); settings_defaults(&settings);
  glance_text(text, sizeof(text), &scores, &settings, true);
  assert(strcmp(text, "No scores yet") == 0);
  scores.best[0] = 214; scores.best[1] = 187;
  glance_text(text, sizeof(text), &scores, &settings, true);
  assert(strcmp(text, "Best A 214 / B 187") == 0); /* Alarm off adds nothing. */
  settings.alarm_on = true; settings.alarm_hour = 7; settings.alarm_minute = 5;
  glance_text(text, sizeof(text), &scores, &settings, true);
  assert(strcmp(text, "Best A 214 / B 187 - 07:05") == 0);
  glance_text(text, sizeof(text), &scores, &settings, false);
  assert(strcmp(text, "Best A 214 / B 187 - 7:05 AM") == 0);
  settings.alarm_hour = 0;
  glance_text(text, sizeof(text), &scores, &settings, false);
  assert(strstr(text, "12:05 AM") != NULL);
  settings.alarm_hour = 23; settings.alarm_minute = 59;
  glance_text(text, sizeof(text), &scores, &settings, false);
  assert(strstr(text, "11:59 PM") != NULL);
  scores.best[0] = scores.best[1] = UINT32_MAX;
  glance_text(text, sizeof(text), &scores, &settings, false);
  assert(strlen(text) < 150 && strchr(text, '{') == NULL);
  glance_text(small, sizeof(small), &scores, &settings, true); /* Truncates, stays terminated. */
  assert(strlen(small) == 9);
  glance_text(small, 0, &scores, &settings, true);
}

int main(void) {
  test_stats_records(); test_stats_accumulation(); test_glance();
  test_records(); test_storage(); test_clock(); test_alarm(); test_orientation();
  test_saved_orientation_preferences(); test_oriented_controls();
  puts("M4 tests passed: versioned storage, corrupt records, clock, calendar/DST recurrence and wakeup lifecycle");
  return 0;
}

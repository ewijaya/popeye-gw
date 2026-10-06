#include <pebble.h>

#include "alarm.h"
#include "clock.h"
#include "game.h"
#include "feedback_service.h"
#include "glance.h"
#include "online_net.h"
#include "orientation.h"
#include "replay.h"
#include "storage.h"
#include "tuning.h"
#include "view.h"

typedef enum { PAGE_CLOCK, PAGE_GAME, PAGE_MENU, PAGE_SCORES,
               PAGE_RESET, PAGE_SETTINGS, PAGE_ALARM, PAGE_HELP, PAGE_ABOUT,
               PAGE_ORIENTATION, PAGE_STATS, PAGE_STATS_RESET } Page;

typedef enum { SETTING_ORIENTATION, SETTING_BUTTONS, SETTING_SWAP,
               SETTING_VIBRATION, SETTING_SOUND, SETTING_GHOSTS, SETTING_DEMO, SETTING_ONLINE } SettingItem;

#define HELP_PAGES 7u
#define STATS_PAGES 3u
#define SCORES_PAGES 3u
#define MENU_ROWS 8u

/* What is being played. Sprint and Daily are Game B rules with a 60 s active-play limit;
 * Daily uses the date's seed and records a replay. */
typedef enum { ROUND_A, ROUND_B, ROUND_SPRINT, ROUND_DAILY } Round;

/* Clock page: Select held shows the best score of the mode that release starts. */
typedef enum { HOLD_NONE, HOLD_A, HOLD_B } Hold;

static Window *s_window;
static Game s_game;
static SaveData s_data;
static Page s_page;
static unsigned s_row;
static bool s_editing, s_focused = true;
static uint8_t s_edit_value;
static bool s_scores_dirty, s_scores_error, s_settings_error;
static bool s_stats_dirty, s_stats_error, s_game_counted;
static bool s_modes_dirty, s_modes_error, s_warned;
static bool s_online_dirty, s_online_error;
static Round s_round;
static Replay s_replay;
static uint32_t s_round_date, s_daily_baseline;
static uint32_t s_streak, s_play_carry_ms;
static AppTimer *s_timer, *s_ring_timer, *s_idle_timer;
static uint32_t s_timer_delay;
static uint64_t s_timer_start;
static TimeUnits s_tick_units;
static bool s_ringing, s_passive_ring;
static time_t s_ring_end;
static Hold s_hold;
static char s_glance_text[64];
static bool s_select_down, s_select_handled;

static void render(void);
static void sync_ticks(void);
static void ring_tick(void *data);
static void arm_idle(void);
static void log_heap(const char *moment);

static uint64_t now_ms(void) {
  time_t seconds;
  uint16_t millis = time_ms(&seconds, NULL);
  return (uint64_t)seconds * 1000u + millis;
}

static bool save_error(void) { return s_scores_error || s_settings_error || s_stats_error || s_modes_error || s_online_error; }

static bool timed_round(void) { return s_round >= ROUND_SPRINT; }
/* Only Daily rounds are recorded; a NULL recorder makes the replay calls plain engine calls. */
static Replay *recorder(void) { return s_round == ROUND_DAILY ? &s_replay : NULL; }

/* Lifetime stats ride along with the scores: written at pause, game over, focus loss and exit. */
static void flush_scores(void) {
  if (s_stats_dirty) {
    s_stats_error = !storage_save_stats(&s_data.stats);
    if (!s_stats_error) s_stats_dirty = false;
  }
  if (s_modes_dirty) {
    s_modes_error = !storage_save_modes(&s_data.modes);
    if (!s_modes_error) s_modes_dirty = false;
  }
  if (s_online_dirty) {
    s_online_error = !storage_save_online(&s_data.online);
    if (!s_online_error) s_online_dirty = false;
  }
  if (!s_scores_dirty) return;
  s_scores_error = !storage_save_scores(&s_data.scores);
  if (!s_scores_error) s_scores_dirty = false;
}

static void record_score(void) {
  time_t now = time(NULL);
  if (timed_round()) {
    if (modes_record(&s_data.modes, s_round == ROUND_DAILY, s_game.score, s_round_date)) s_modes_dirty = true;
  } else if (scores_record(&s_data.scores, s_game.mode, s_game.score, clock_date(localtime(&now))))
    s_scores_dirty = true;
}

/* A game counts once it has scored or ended; quitting before that does not. */
static void note_progress(void) {
  if (s_game_counted || (s_game.score == 0u && s_game.status != GAME_OVER)) return;
  s_game_counted = true;
  if (timed_round()) {
    modes_count_game(&s_data.modes, s_round == ROUND_DAILY);
    s_modes_dirty = true;
  } else {
    stats_count_game(&s_data.stats, s_game.mode);
    s_stats_dirty = true;
  }
}

static void note_play_ms(uint32_t ms) {
  stats_add_play_ms(&s_data.stats, &s_play_carry_ms, ms);
  s_stats_dirty = true;
}

static bool save_settings(const Settings *settings) {
  s_settings_error = !storage_save_settings(settings);
  if (s_settings_error) return false;
  s_data.settings = *settings;
  s_game.controls_swapped = settings->swap_buttons;
  if (!settings->vibration) vibes_cancel();
  if (!settings->sound) feedback_service_stop_sound();
  if (!settings->online) online_net_cancel();
  alarm_refresh(settings, time(NULL));
  sync_ticks();
  return true;
}

static void log_heap(const char *moment) {
  APP_LOG(APP_LOG_LEVEL_INFO, "heap %s game %c: free %d used %d", moment,
          s_game.mode == GAME_A ? 'A' : 'B', (int)heap_bytes_free(), (int)heap_bytes_used());
}

static void cancel_timer(void) {
  if (s_timer != NULL) app_timer_cancel(s_timer);
  s_timer = NULL;
}

static void timer_fired(void *data);

static void schedule_timer(void) {
  arm_idle();
  cancel_timer();
  if (!s_focused || s_page != PAGE_GAME) return;
  if (s_game.status != GAME_PLAYING && s_game.status != GAME_RECOVERING) return;
  s_timer_delay = game_next_boundary_ms(&s_game); /* Tick, end of recovery or the time limit. */
  s_timer_start = now_ms();
  s_timer = app_timer_register(s_timer_delay, timer_fired, NULL);
}

/* Only outside a round: a transfer never runs while the game is being played or paused. */
static bool round_in_progress(void) {
  return s_page == PAGE_GAME && s_game.status != GAME_OVER;
}

static void online_done(OnlineNetResult result, uint32_t rank, uint32_t total) {
  if (result == ONLINE_NET_OK) online_succeeded(&s_data.online, rank, total);
  else if (result != ONLINE_NET_UNAVAILABLE) online_failed(&s_data.online, result == ONLINE_NET_REJECTED);
  s_online_dirty = true;
  flush_scores();
  log_heap("online done");
  render();
}

/* Sends the kept replay to the phone if Online is on and it has not been accepted yet. */
static void online_try_send(void) {
  if (!s_data.settings.online || !online_should_send(&s_data.online) || online_net_busy() || round_in_progress()) return;
  if (storage_replay_length() == 0u) {
    online_clear(&s_data.online); /* The replay is gone: nothing to send. */
    s_online_dirty = true;
    flush_scores();
    return;
  }
  if (!online_net_start(online_done)) {
    online_failed(&s_data.online, false);
    s_online_dirty = true;
    flush_scores();
  }
  log_heap("online start");
  render();
}

/* The Daily replay worth keeping is the best of the day: sealed when a round ends above
 * the day's earlier best. Anything that cannot be stored verifiably is not kept. */
static void keep_daily_replay(void) {
  size_t length = replay_finish(&s_replay, &s_game);
  if (length == 0u || s_game.score <= s_daily_baseline) return;
  if (s_replay.overflow || !storage_save_replay(s_replay.bytes, length)) {
    storage_clear_replay();
    online_net_cancel();
    online_clear(&s_data.online);
  } else online_queue(&s_data.online, s_round_date, s_game.score);
  s_online_dirty = true;
  flush_scores();
  online_try_send();
}

static void timer_fired(void *data) {
  uint32_t events;
  s_timer = NULL;
  replay_step(recorder(), &s_game);
  events = game_take_events(&s_game);
  feedback_service_events(events, s_game.new_high_score, s_data.settings.vibration,
                          s_data.settings.sound);
  if (timed_round() && !s_warned && s_game.status == GAME_PLAYING &&
      s_game.time_left_ms <= PGW_SPRINT_WARNING_MS &&
      (events & (GAME_EVENT_CATCH | GAME_EVENT_DROP | GAME_EVENT_MISS | GAME_EVENT_BONUS)) == 0u) {
    s_warned = true; /* Ten seconds left: a cue on the first quiet tick. */
    feedback_service_warn(s_data.settings.vibration, s_data.settings.sound);
  }
  stats_note_events(&s_data.stats, events, &s_streak);
  note_play_ms(s_timer_delay);
  note_progress();
  record_score();
  if (s_game.status == GAME_OVER) {
    flush_scores();
    if (s_round == ROUND_DAILY) keep_daily_replay();
    log_heap("over");
  }
  schedule_timer();
  render();
}

static void start_game(Round round) {
  time_t now = time(NULL);
  uint32_t seed;
  online_net_cancel(); /* Never while a round is played. */
  feedback_service_reset();
  flush_scores();
  s_round = round;
  s_round_date = clock_date(localtime(&now));
  s_game.high_scores[0] = s_data.scores.best[0];
  s_game.high_scores[1] = round == ROUND_SPRINT ? s_data.modes.sprint_best :
                          round == ROUND_DAILY ? s_data.modes.daily_best : s_data.scores.best[1];
  s_game.controls_swapped = s_data.settings.swap_buttons;
  seed = round == ROUND_DAILY ? game_daily_seed(s_round_date) : (uint32_t)now_ms();
  game_start_timed(&s_game, round == ROUND_A ? GAME_A : GAME_B, seed, timed_round() ? PGW_SPRINT_MS : 0u);
  s_daily_baseline = modes_daily_today(&s_data.modes, s_round_date);
  s_warned = false;
  if (round == ROUND_DAILY) replay_begin(&s_replay, &s_game, seed, s_round_date);
  s_streak = 0u;
  s_game_counted = false;
  s_page = PAGE_GAME;
  feedback_service_set_running(s_focused && !(s_ringing && !s_passive_ring));
  sync_ticks();
  log_heap("start");
  schedule_timer();
  render();
}

static void pause_game(void) {
  feedback_service_set_running(false);
  vibes_cancel();
  if (s_timer != NULL) {
    uint64_t elapsed = now_ms() - s_timer_start;
    if (elapsed >= s_timer_delay) elapsed = s_timer_delay - 1u;
    cancel_timer();
    replay_advance(recorder(), &s_game, (uint32_t)elapsed);
    note_play_ms((uint32_t)elapsed);
  }
  replay_pause(recorder(), &s_game);
  note_progress();
  record_score();
  flush_scores();
  render();
}

static void cancel_hold(void) {
  if (s_hold == HOLD_NONE) return;
  s_hold = HOLD_NONE;
  s_select_handled = true; /* The release must not act on a different page or state. */
}

static void open_page(Page page) {
  cancel_hold();
  if (page != PAGE_GAME) {
    feedback_service_set_running(false);
    feedback_service_reset();
    vibes_cancel();
  }
  s_page = page;
  s_row = page == PAGE_ORIENTATION && s_data.settings.landscape ? 1u : 0u;
  s_editing = false;
  if (page == PAGE_CLOCK || page == PAGE_ALARM) alarm_refresh(&s_data.settings, time(NULL));
  sync_ticks();
  render();
}

/* After game over, return to the clock once nothing has been pressed for 5 minutes.
 * The only timer is armed on the game-over screen while focused. */
static void idle_fired(void *data) {
  s_idle_timer = NULL;
  if (s_page != PAGE_GAME || s_game.status != GAME_OVER) return;
  flush_scores();
  open_page(PAGE_CLOCK);
}

static void arm_idle(void) {
  if (s_idle_timer != NULL) app_timer_cancel(s_idle_timer);
  s_idle_timer = NULL;
  if (s_focused && s_page == PAGE_GAME && s_game.status == GAME_OVER)
    s_idle_timer = app_timer_register(PGW_GAME_OVER_IDLE_MS, idle_fired, NULL);
}

static void stop_ring(void) {
  if (s_ring_timer != NULL) app_timer_cancel(s_ring_timer);
  s_ring_timer = NULL;
  s_ringing = false;
  feedback_service_set_running(s_focused && s_page == PAGE_GAME && s_game.status != GAME_PAUSED);
  feedback_service_stop_sound();
  vibes_cancel();
  sync_ticks();
  render();
}

/* Idle alarms consume the dismissing press. An in-game alarm never steals a
 * movement or pause action; that press also clears the flashing notification. */
static bool dismiss_ring(void) {
  bool consume;
  if (!s_ringing) return false;
  consume = !s_passive_ring;
  stop_ring();
  return consume;
}

static void pulse(void) {
  if (s_data.settings.vibration && !quiet_time_is_active()) vibes_short_pulse();
  feedback_service_play(CUE_ALARM, s_data.settings.sound);
}

static void begin_ring(void) {
  if (s_ring_timer != NULL) app_timer_cancel(s_ring_timer);
  s_ringing = true;
  cancel_hold();
  s_passive_ring = s_page == PAGE_GAME &&
      (s_game.status == GAME_PLAYING || s_game.status == GAME_RECOVERING);
  s_ring_end = time(NULL) + 60;
  if (!s_passive_ring) feedback_service_set_running(false);
  APP_LOG(APP_LOG_LEVEL_INFO, "alarm ringing passive %d", s_passive_ring);
  if (s_focused) pulse();
  s_ring_timer = s_focused ? app_timer_register(1000, ring_tick, NULL) : NULL;
  sync_ticks();
  render();
}

static void ring_tick(void *data) {
  time_t now = time(NULL);
  s_ring_timer = NULL;
  if (!s_ringing) return;
  if (now >= s_ring_end) { stop_ring(); return; }
  if (!s_focused) return;
  if (!s_passive_ring && (s_ring_end - now) % 2 == 0) pulse();
  render();
  s_ring_timer = app_timer_register(1000, ring_tick, NULL);
}

static void clock_tick(struct tm *local, TimeUnits changed) {
  if (changed & MINUTE_UNIT) alarm_refresh(&s_data.settings, time(NULL));
  render();
}

static void sync_ticks(void) {
  TimeUnits wanted = 0;
  if (s_focused && s_page == PAGE_CLOCK && !s_ringing)
    wanted = s_data.settings.attract ? SECOND_UNIT : MINUTE_UNIT;
  if (wanted == s_tick_units) return;
  tick_timer_service_unsubscribe();
  s_tick_units = wanted;
  if (wanted != 0) tick_timer_service_subscribe(wanted, clock_tick);
}

static const char *on_off(bool value) { return value ? "On" : "Off"; }

static unsigned settings_row_count(void) {
  return s_data.settings.landscape ? 8u : 7u;
}

/* Button position has no visible row in Vertical mode. */
static SettingItem settings_item(unsigned row) {
  return (SettingItem)(row + (!s_data.settings.landscape && row > 0u ? 1u : 0u));
}

static void score_date(char *buffer, size_t size, uint32_t date) {
  if (date == 0) snprintf(buffer, size, "No score yet");
  else snprintf(buffer, size, "%04lu-%02lu-%02lu", (unsigned long)(date / 10000u),
                (unsigned long)(date / 100u % 100u), (unsigned long)(date % 100u));
}

static void play_time(char *buffer, size_t size, uint32_t seconds) {
  if (seconds >= 3600u)
    snprintf(buffer, size, "Time: %luh %02lum", (unsigned long)(seconds / 3600u), (unsigned long)(seconds / 60u % 60u));
  else
    snprintf(buffer, size, "Time: %lum %02lus", (unsigned long)(seconds / 60u), (unsigned long)(seconds % 60u));
}

static void render_panel(void) {
  ViewPanel panel = { .count = 4, .selected = (int)s_row };
  const Settings *settings = &s_data.settings;
  const AlarmStatus *alarm = alarm_status();
  snprintf(panel.footer, sizeof(panel.footer), "Select: open  Back: clock");
  switch (s_page) {
    case PAGE_MENU:
      panel.count = MENU_ROWS;
      panel.title = "Popeye G&W";
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Daily");
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Sprint");
      snprintf(panel.rows[2], sizeof(panel.rows[2]), "High scores");
      snprintf(panel.rows[3], sizeof(panel.rows[3]), "Stats");
      snprintf(panel.rows[4], sizeof(panel.rows[4]), "Alarm");
      snprintf(panel.rows[5], sizeof(panel.rows[5]), "Settings");
      snprintf(panel.rows[6], sizeof(panel.rows[6]), "Help");
      snprintf(panel.rows[7], sizeof(panel.rows[7]), "About");
      if (s_row < 2u) snprintf(panel.footer, sizeof(panel.footer), "60 s of Game B rules\nSelect: play  Back: clock");
      break;
    case PAGE_SETTINGS: {
      unsigned row = 0;
      panel.count = settings_row_count();
      panel.title = "Settings";
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Orientation");
      if (settings->landscape)
        snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Buttons: %s",
                 settings->buttons_bottom ? "Bottom" : "Top");
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Swap: %s", on_off(settings->swap_buttons));
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Vibrate: %s", on_off(settings->vibration));
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Sound: %s", on_off(settings->sound));
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Ghosts: %s", on_off(settings->ghosts));
      snprintf(panel.rows[row++], sizeof(panel.rows[0]), "Demo: %s", on_off(settings->attract));
      snprintf(panel.rows[row], sizeof(panel.rows[0]), "Online: %s", on_off(settings->online));
      snprintf(panel.footer, sizeof(panel.footer), "Select: change  Back: menu");
      if (settings_item(s_row) == SETTING_ORIENTATION)
        snprintf(panel.footer, sizeof(panel.footer), "%s\nSelect: open  Back: menu",
                 settings->landscape ? "Horizontal" : "Vertical");
      else if (settings_item(s_row) == SETTING_BUTTONS)
        snprintf(panel.footer, sizeof(panel.footer), "Buttons %s screen\nSelect: change  Back: menu",
                 settings->buttons_bottom ? "below" : "above");
      else if (settings_item(s_row) == SETTING_SOUND)
        snprintf(panel.footer, sizeof(panel.footer), "Muted by Quiet Time\nSelect: change  Back: menu");
      else if (settings_item(s_row) == SETTING_ONLINE)
        snprintf(panel.footer, sizeof(panel.footer), "Daily rank via phone\nSelect: change  Back: menu");
      break;
    }
    case PAGE_ORIENTATION:
      panel.title = "Orientation";
      panel.count = 2;
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Vertical");
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Horizontal");
      snprintf(panel.footer, sizeof(panel.footer), "%s\nSelect: save  Back: cancel",
               s_row == 0u ? "Upright on wrist" : "Hold watch sideways");
      break;
    case PAGE_SCORES:
      panel.title = s_row == 0u ? "High scores 1/3" : s_row == 1u ? "High scores 2/3" : "High scores 3/3";
      panel.selected = -1;
      if (s_row == 0u) {
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Game A: %lu", (unsigned long)s_data.scores.best[0]);
        score_date(panel.rows[1], sizeof(panel.rows[1]), s_data.scores.date[0]);
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Game B: %lu", (unsigned long)s_data.scores.best[1]);
        score_date(panel.rows[3], sizeof(panel.rows[3]), s_data.scores.date[1]);
        snprintf(panel.footer, sizeof(panel.footer), "%s: Sprint, Daily\nSelect: reset  Back: clock",
                 settings->landscape ? "Left/Right" : "Up/Down");
      } else {
        time_t now = time(NULL);
        uint32_t date = clock_date(localtime(&now));
        uint32_t today = modes_daily_today(&s_data.modes, date);
        if (s_row == 1u) {
          snprintf(panel.rows[0], sizeof(panel.rows[0]), "Sprint: %lu", (unsigned long)s_data.modes.sprint_best);
          score_date(panel.rows[1], sizeof(panel.rows[1]), s_data.modes.sprint_date);
          snprintf(panel.rows[2], sizeof(panel.rows[2]), "Daily: %lu", (unsigned long)s_data.modes.daily_best);
          score_date(panel.rows[3], sizeof(panel.rows[3]), s_data.modes.daily_best_date);
          snprintf(panel.footer, sizeof(panel.footer), "Daily today: %lu\nSelect: reset  Back: clock", (unsigned long)today);
        } else {
          snprintf(panel.rows[0], sizeof(panel.rows[0]), "Daily today");
          snprintf(panel.rows[1], sizeof(panel.rows[1]), "Score: %lu", (unsigned long)today);
          online_text(panel.rows[2], sizeof(panel.rows[2]), settings->online, online_net_busy(), &s_data.online, date);
          panel.count = 3;
          snprintf(panel.footer, sizeof(panel.footer), "Needs phone link\nSelect: reset  Back: clock");
        }
      }
      break;
    case PAGE_STATS: {
      const Stats *stats = &s_data.stats;
      panel.selected = -1;
      panel.title = s_row == 0u ? "Stats 1/3" : s_row == 1u ? "Stats 2/3" : "Stats 3/3";
      if (s_row == 0u) {
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Games A: %lu", (unsigned long)stats->games[0]);
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Games B: %lu", (unsigned long)stats->games[1]);
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Catches: %lu", (unsigned long)stats->catches);
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Drops: %lu", (unsigned long)stats->drops);
        snprintf(panel.footer, sizeof(panel.footer), "Select: next  Back: menu");
      } else if (s_row == 1u) {
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Brutus hits: %lu", (unsigned long)stats->hits);
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Best run: %lu", (unsigned long)stats->best_streak);
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Bonuses: %lu", (unsigned long)stats->bonuses);
        play_time(panel.rows[3], sizeof(panel.rows[3]), stats->play_seconds);
        snprintf(panel.footer, sizeof(panel.footer), "Select: next  Back: menu");
      } else {
        panel.count = 2;
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Sprints: %lu", (unsigned long)s_data.modes.sprint_games);
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Dailies: %lu", (unsigned long)s_data.modes.daily_games);
        snprintf(panel.footer, sizeof(panel.footer), "Select: reset stats\nBack: menu");
      }
      break;
    }
    case PAGE_STATS_RESET:
      panel.title = "Reset stats?";
      panel.count = 2;
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Keep stats");
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Reset stats");
      snprintf(panel.footer, sizeof(panel.footer), "Scores stay. Select: choose\nBack: cancel");
      break;
    case PAGE_RESET:
      panel.title = "Reset scores?";
      panel.count = 2;
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Keep scores");
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Reset all scores");
      snprintf(panel.footer, sizeof(panel.footer), "A, B, Sprint, Daily\nSelect: choose  Back: cancel");
      break;
    case PAGE_ALARM: {
      unsigned hour = settings->alarm_hour, minute = settings->alarm_minute;
      panel.title = s_editing ? "Set alarm" : "Daily alarm";
      if (s_editing && s_row == 1u) hour = s_edit_value;
      if (s_editing && s_row == 2u) minute = s_edit_value;
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Alarm: %s", on_off(settings->alarm_on));
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Hour (24h): %02u", hour);
      snprintf(panel.rows[2], sizeof(panel.rows[2]), "Minute: %02u", minute);
      snprintf(panel.rows[3], sizeof(panel.rows[3]), "%s", alarm->error ? "Retry alarm" : "Test alarm");
      if (s_editing) snprintf(panel.footer, sizeof(panel.footer), "%s: change\nSelect: save  Back: cancel",
                             settings->landscape ? "Left/Right" : "Up/Down");
      else if (alarm->error) {
        const char *reason = alarm->error == E_RANGE ? "Time unavailable" :
                             alarm->error == E_OUT_OF_STORAGE ? "Alarm save failed" : "Alarm not set";
        snprintf(panel.footer, sizeof(panel.footer), "%s\nSelect: retry", reason);
      }
      else if (alarm->next != 0) {
        char next[24];
        strftime(next, sizeof(next), "%a %H:%M", localtime(&alarm->next));
        snprintf(panel.footer, sizeof(panel.footer), "%s: %s\nSelect: edit  Back: menu", alarm->adjusted ? "+1m" : "Next", next);
      } else snprintf(panel.footer, sizeof(panel.footer), "Select: edit  Back: menu");
      break;
    }
    case PAGE_HELP:
      panel.selected = -1;
      snprintf(panel.footer, sizeof(panel.footer), "Select: next  Back: menu");
      if (s_row == 0u) {
        panel.title = "Help 1/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Tap Select: A");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Hold Select: B");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "%s: move",
                 settings->landscape ? "Left/Right" : "Up/Down");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Select: pause/play");
        snprintf(panel.footer, sizeof(panel.footer), "Holding shows best score\nSelect: next  Back: menu");
      } else if (s_row == 1u) {
        panel.title = "Help 2/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Catch food: +1");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Center can't catch");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "2 drops = 1 MISS");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Hit = 1 MISS");
        snprintf(panel.footer, sizeof(panel.footer), "3 MISS ends game\nSelect: next  Back: menu");
      } else if (s_row == 2u) {
        panel.title = "Help 3/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Quit play: 2x Back");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Clock %s: scores",
                 settings->landscape ? "Left" : "Up");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Clock %s: menu",
                 settings->landscape ? "Right" : "Down");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Settings: screen");
      } else if (s_row == 3u) {
        panel.title = "Help 4/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Settings: Orientation");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Vertical / Horizontal");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Horizontal: Buttons");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Bottom / Top");
        snprintf(panel.footer, sizeof(panel.footer), "Swap: reverse movement\nSelect: next  Back: menu");
      } else if (s_row == 4u) {
        panel.title = "Help 5/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Menu: Daily, Sprint");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "60 s of Game B play");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Pause stops the clock");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Daily: same food today");
        snprintf(panel.footer, sizeof(panel.footer), "Beep at 10 s left\nSelect: next  Back: menu");
      } else if (s_row == 5u) {
        panel.title = "Help 6/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Online: Settings");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Off by default");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Sends best Daily");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Needs phone + net");
        snprintf(panel.footer, sizeof(panel.footer), "Server not live yet\nSelect: next  Back: menu");
      } else {
        panel.title = "Help 7/7";
        snprintf(panel.rows[0], sizeof(panel.rows[0]), "Open Popeye fast:");
        snprintf(panel.rows[1], sizeof(panel.rows[1]), "Watch Settings >");
        snprintf(panel.rows[2], sizeof(panel.rows[2]), "Quick Launch");
        snprintf(panel.rows[3], sizeof(panel.rows[3]), "Pick button + app");
        snprintf(panel.footer, sizeof(panel.footer), "Hold it 2 s on watchface\nSelect: next  Back: menu");
      }
      break;
    case PAGE_ABOUT:
      panel.title = "About";
      panel.selected = -1;
      snprintf(panel.rows[0], sizeof(panel.rows[0]), "Popeye G&W");
      snprintf(panel.rows[1], sizeof(panel.rows[1]), "Fan-made game");
      snprintf(panel.rows[2], sizeof(panel.rows[2]), "Code: MIT");
      snprintf(panel.rows[3], sizeof(panel.rows[3]), "Edward Wijaya");
      snprintf(panel.footer, sizeof(panel.footer), "v" POPEYE_GW_VERSION " - Pebble Time 2\nBack: menu");
      break;
    default: return;
  }
  if (save_error()) snprintf(panel.footer, sizeof(panel.footer), "Save failed\nSelect: retry  Back: return");
  view_panel(&panel);
}

static void render(void) {
  Scene scene;
  ViewOverlay overlay = VIEW_OVERLAY_NONE;
  char note[24] = "";
  time_t now = time(NULL);
  struct tm local = *localtime(&now);
  bool full_alarm = s_ringing && !s_passive_ring;
  view_set_landscape(s_data.settings.landscape, s_data.settings.buttons_bottom);
  if (s_page != PAGE_CLOCK && s_page != PAGE_GAME && !full_alarm) { render_panel(); return; }
  if (s_page == PAGE_CLOCK || full_alarm) {
    clock_scene(&scene, &local, clock_is_24h_style(), s_data.settings.attract,
                s_data.settings.alarm_on, full_alarm);
    overlay = full_alarm ? VIEW_OVERLAY_ALARM : VIEW_OVERLAY_CLOCK;
    if (s_hold != HOLD_NONE && !full_alarm) {
      GameMode mode = s_hold == HOLD_B ? GAME_B : GAME_A;
      scene_best(&scene, mode, s_data.scores.best[mode]);
      overlay = s_hold == HOLD_B ? VIEW_OVERLAY_BEST_B : VIEW_OVERLAY_BEST_A;
    }
  } else {
    scene_game(&scene, &s_game);
    feedback_service_apply(&scene);
    if (s_data.settings.alarm_on && (!s_ringing || local.tm_sec % 2 == 0)) scene_light(&scene, SEG_BELL);
    if (s_game.status == GAME_PAUSED) {
      overlay = VIEW_OVERLAY_PAUSED;
      if (timed_round()) { /* Remaining active seconds, rounded up, in the overlay title. */
        unsigned left = (s_game.time_left_ms + 999u) / 1000u;
        snprintf(note, sizeof(note), "%s %u:%02u", s_round == ROUND_DAILY ? "Daily" : "Sprint", left / 60u, left % 60u);
      }
    }
    if (s_game.status == GAME_OVER) {
      overlay = VIEW_OVERLAY_GAME_OVER;
      if (s_game.time_up) snprintf(note, sizeof(note), "Time up!");
    }
  }
  view_show(&scene, overlay, note[0] != '\0' ? note : NULL, s_data.settings.ghosts, save_error());
}

static void move_down_handler(ClickRecognizerRef recognizer, void *context) {
  bool up = orientation_logical_up(click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP,
      s_data.settings.landscape && s_data.settings.buttons_bottom);
  if (dismiss_ring()) return;
  if (s_page == PAGE_GAME) {
    if (replay_input(recorder(), &s_game, up ? GAME_UP : GAME_DOWN, true)) render();
    arm_idle();
  } else if (s_page == PAGE_CLOCK) {
    open_page(up ? PAGE_SCORES : PAGE_MENU);
  } else if (s_editing) {
    unsigned limit = s_row == 1u ? 24u : 60u;
    s_edit_value = (uint8_t)((s_edit_value + (up ? 1u : limit - 1u)) % limit);
    render();
  } else if (s_page != PAGE_ABOUT) {
    unsigned count = (s_page == PAGE_RESET || s_page == PAGE_ORIENTATION || s_page == PAGE_STATS_RESET) ? 2u :
                     s_page == PAGE_HELP ? HELP_PAGES : s_page == PAGE_STATS ? STATS_PAGES :
                     s_page == PAGE_SCORES ? SCORES_PAGES : s_page == PAGE_SETTINGS ? settings_row_count() :
                     s_page == PAGE_MENU ? MENU_ROWS : 4u;
    s_row = (s_row + (up ? count - 1u : 1u)) % count;
    render();
  }
}

static void move_up_handler(ClickRecognizerRef recognizer, void *context) {
  bool up = orientation_logical_up(click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP,
      s_data.settings.landscape && s_data.settings.buttons_bottom);
  GameButton button = up ? GAME_UP : GAME_DOWN;
  if (s_page == PAGE_GAME) replay_input(recorder(), &s_game, button, false);
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  Settings settings = s_data.settings;
  if (dismiss_ring()) return;
  flush_scores();
  switch (s_page) {
    case PAGE_CLOCK: start_game(ROUND_A); return;
    case PAGE_GAME:
      if (s_game.status == GAME_PAUSED) {
        replay_resume(recorder(), &s_game);
        feedback_service_set_running(s_focused);
        schedule_timer();
      }
      else if (s_game.status == GAME_OVER) start_game(s_round);
      else pause_game();
      break;
    case PAGE_MENU: {
      static const Page destinations[] = { PAGE_SCORES, PAGE_STATS, PAGE_ALARM, PAGE_SETTINGS, PAGE_HELP, PAGE_ABOUT };
      if (s_row < 2u) start_game(s_row == 0u ? ROUND_DAILY : ROUND_SPRINT);
      else open_page(destinations[s_row - 2u]);
      return;
    }
    case PAGE_SCORES: open_page(PAGE_RESET); return;
    case PAGE_STATS:
      if (s_row + 1u < STATS_PAGES) { ++s_row; break; }
      open_page(PAGE_STATS_RESET);
      return;
    case PAGE_STATS_RESET:
      if (s_row == 1u) {
        Stats reset;
        stats_defaults(&reset);
        s_stats_error = !storage_save_stats(&reset);
        if (s_stats_error) break;
        s_data.stats = reset;
        s_stats_dirty = false;
      }
      open_page(PAGE_STATS);
      return;
    case PAGE_RESET:
      if (s_row == 1u) {
        HighScores reset;
        ModeScores modes = s_data.modes;
        scores_defaults(&reset);
        modes_reset_scores(&modes); /* Bests only; round counts stay with the stats. */
        s_scores_error = !(storage_save_scores(&reset) && storage_save_modes(&modes));
        if (s_scores_error) break;
        s_data.scores = reset;
        s_data.modes = modes;
        s_scores_dirty = s_modes_dirty = false;
        storage_clear_replay(); /* It belonged to today's cleared Daily best. */
        online_net_cancel();
        online_clear(&s_data.online);
        s_online_dirty = true;
      }
      open_page(PAGE_SCORES);
      return;
    case PAGE_SETTINGS:
      switch (settings_item(s_row)) {
        case SETTING_ORIENTATION: open_page(PAGE_ORIENTATION); return;
        case SETTING_BUTTONS: settings.buttons_bottom = !settings.buttons_bottom; break;
        case SETTING_SWAP: settings.swap_buttons = !settings.swap_buttons; break;
        case SETTING_VIBRATION: settings.vibration = !settings.vibration; break;
        case SETTING_SOUND: settings.sound = !settings.sound; break;
        case SETTING_GHOSTS: settings.ghosts = !settings.ghosts; break;
        case SETTING_DEMO: settings.attract = !settings.attract; break;
        case SETTING_ONLINE: settings.online = !settings.online; break;
      }
      if (save_settings(&settings)) {
        if (settings_item(s_row) == SETTING_SOUND) feedback_service_play(CUE_CATCH, settings.sound); /* Preview the beep. */
        else if (settings_item(s_row) == SETTING_ONLINE) online_try_send();
      }
      break;
    case PAGE_ORIENTATION:
      settings.landscape = s_row == 1u;
      if (save_settings(&settings)) { open_page(PAGE_SETTINGS); return; }
      break;
    case PAGE_ALARM:
      if (s_editing) {
        if (s_row == 1u) settings.alarm_hour = s_edit_value;
        else settings.alarm_minute = s_edit_value;
        if (save_settings(&settings)) s_editing = false;
      } else if (s_row == 0u) {
        settings.alarm_on = !settings.alarm_on;
        save_settings(&settings);
      } else if (s_row == 3u) {
        if (alarm_status()->error) alarm_refresh(&settings, time(NULL));
        else begin_ring();
      } else {
        s_editing = true;
        s_edit_value = s_row == 1u ? settings.alarm_hour : settings.alarm_minute;
      }
      break;
    case PAGE_HELP: s_row = (s_row + 1u) % HELP_PAGES; break;
    case PAGE_ABOUT: break;
  }
  render();
}

static void select_long_action(ClickRecognizerRef recognizer, void *context) {
  if (dismiss_ring()) return;
  if (s_page == PAGE_CLOCK) start_game(ROUND_B);
  else if (s_page == PAGE_GAME && s_game.status == GAME_OVER)
    start_game(s_round == ROUND_A ? ROUND_B : s_round == ROUND_B ? ROUND_A :
               s_round == ROUND_SPRINT ? ROUND_DAILY : ROUND_SPRINT);
  else if (s_page != PAGE_GAME) select_handler(recognizer, context);
}

/* Select is raw so the clock can preview the best score while it is held: down
 * shows A, passing the long-press delay shows B, release starts the shown mode.
 * Everywhere else release is the click and the long press acts as before. */
static void select_down_handler(ClickRecognizerRef recognizer, void *context) {
  s_select_down = true;
  s_select_handled = false;
  if (s_page == PAGE_CLOCK && !s_ringing) { s_hold = HOLD_A; render(); }
}

static void select_up_handler(ClickRecognizerRef recognizer, void *context) {
  Hold hold = s_hold;
  if (!s_select_down) return;
  s_select_down = false;
  s_hold = HOLD_NONE;
  if (hold != HOLD_NONE) start_game(hold == HOLD_B ? ROUND_B : ROUND_A);
  else if (!s_select_handled) select_handler(recognizer, context);
}

static void select_long_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_select_down || s_select_handled) return;
  s_select_handled = true;
  if (s_hold == HOLD_A) { s_hold = HOLD_B; render(); }
  else select_long_action(recognizer, context);
}

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  if (dismiss_ring()) return;
  if (s_editing) { s_editing = false; render(); return; }
  if (s_page == PAGE_CLOCK) { window_stack_pop(true); return; }
  if (s_page == PAGE_GAME) {
    if (s_game.status == GAME_PLAYING || s_game.status == GAME_RECOVERING) pause_game();
    else { flush_scores(); cancel_timer(); open_page(PAGE_CLOCK); }
  } else if (s_page == PAGE_MENU || s_page == PAGE_SCORES) open_page(PAGE_CLOCK);
  else if (s_page == PAGE_RESET) open_page(PAGE_SCORES);
  else if (s_page == PAGE_STATS_RESET) open_page(PAGE_STATS);
  else if (s_page == PAGE_ORIENTATION) open_page(PAGE_SETTINGS);
  else open_page(PAGE_MENU);
}

static void click_config(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, move_down_handler, move_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, move_down_handler, move_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, select_down_handler, select_up_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, select_long_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

static void will_focus(bool in_focus) {
  s_focused = in_focus;
  if (!in_focus) {
    cancel_hold();
    if (s_page == PAGE_GAME) pause_game();
    flush_scores();
    if (s_ring_timer != NULL) app_timer_cancel(s_ring_timer);
    s_ring_timer = NULL;
    feedback_service_stop_sound();
    vibes_cancel();
  } else {
    alarm_refresh(&s_data.settings, time(NULL));
    if (s_ringing && time(NULL) >= s_ring_end) stop_ring();
    else if (s_ringing && s_ring_timer == NULL) s_ring_timer = app_timer_register(1000, ring_tick, NULL);
    feedback_service_set_running(s_page == PAGE_GAME && s_game.status != GAME_PAUSED &&
                                 !(s_ringing && !s_passive_ring));
    render();
  }
  arm_idle();
  sync_ticks();
}

static void window_load(Window *window) { view_init(window_get_root_layer(window), s_data.settings.ghosts); render(); }

static void glance_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  AppGlanceSlice slice = {
    .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON, .subtitle_template_string = s_glance_text },
    .expiration_time = APP_GLANCE_SLICE_NO_EXPIRATION
  };
  AppGlanceResult result;
  if (limit == 0u) return;
  result = app_glance_add_slice(session, slice);
  if (result != APP_GLANCE_RESULT_SUCCESS) APP_LOG(APP_LOG_LEVEL_ERROR, "glance slice %d", (int)result);
}

static void window_unload(Window *window) {
  online_net_cancel();
  feedback_service_set_running(false);
  feedback_service_reset();
  cancel_timer();
  if (s_idle_timer != NULL) app_timer_cancel(s_idle_timer);
  s_idle_timer = NULL;
  if (s_ring_timer != NULL) app_timer_cancel(s_ring_timer);
  s_ring_timer = NULL;
  tick_timer_service_unsubscribe();
  s_tick_units = 0;
  flush_scores();
  /* One update as the app closes; the launcher shows it until the next launch. */
  feedback_service_stop_sound();
  {
    time_t now = time(NULL);
    glance_text(s_glance_text, sizeof(s_glance_text), &s_data.scores, &s_data.modes,
                clock_date(localtime(&now)), &s_data.settings, clock_is_24h_style());
  }
  app_glance_reload(glance_reload, NULL);
  vibes_cancel();
  view_deinit();
}

/* A pending replay gets one more try a few seconds after launch, so the phone link is up. */
static void launch_send(void *data) { online_try_send(); }

int main(void) {
  feedback_service_init(render);
  storage_load(&s_data);
  s_window = window_create();
  if (s_window == NULL) return 1;
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  alarm_init(&s_data.settings, begin_ring);
  if (s_data.settings.online && online_should_send(&s_data.online)) app_timer_register(4000, launch_send, NULL);
  app_focus_service_subscribe_handlers((AppFocusHandlers) { .will_focus = will_focus });
  sync_ticks();
  app_event_loop();
  app_focus_service_unsubscribe();
  alarm_deinit();
  window_destroy(s_window);
  return 0;
}

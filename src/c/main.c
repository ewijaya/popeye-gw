#include <pebble.h>

#include "game.h"
#include "scene.h"
#include "view.h"

static Window *s_window;
static Game s_game;
static bool s_in_game;
static AppTimer *s_timer;
static uint32_t s_timer_delay;
static uint64_t s_timer_start;

static uint64_t now_ms(void) {
  time_t seconds;
  uint16_t millis = time_ms(&seconds, NULL);
  return (uint64_t)seconds * 1000u + millis;
}

static void log_heap(const char *moment) {
  APP_LOG(APP_LOG_LEVEL_INFO, "heap %s game %c: free %d used %d", moment,
          s_game.mode == GAME_A ? 'A' : 'B', (int)heap_bytes_free(), (int)heap_bytes_used());
}

static void render(void) {
  Scene scene;
  ViewOverlay overlay = VIEW_OVERLAY_NONE;
  if (!s_in_game) {
    scene_idle(&scene);
    overlay = VIEW_OVERLAY_TITLE;
  } else {
    scene_game(&scene, &s_game);
    if (s_game.status == GAME_PAUSED) overlay = VIEW_OVERLAY_PAUSED;
    if (s_game.status == GAME_OVER) overlay = VIEW_OVERLAY_GAME_OVER;
  }
  view_show(&scene, overlay);
}

static void cancel_timer(void) {
  if (s_timer != NULL) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

static void timer_fired(void *data);

/* One AppTimer to the next step or the end of recovery; none otherwise. */
static void schedule_timer(void) {
  cancel_timer();
  if (!s_in_game) return;
  if (s_game.status == GAME_PLAYING) {
    s_timer_delay = s_game.step_ms_left;
  } else if (s_game.status == GAME_RECOVERING) {
    s_timer_delay = s_game.recovery_ms_left;
  } else {
    return;
  }
  s_timer_start = now_ms();
  s_timer = app_timer_register(s_timer_delay, timer_fired, NULL);
}

static void timer_fired(void *data) {
  s_timer = NULL;
  game_step(&s_game);
  game_take_events(&s_game); /* Vibration and effects arrive in M5. */
  if (s_game.status == GAME_OVER) log_heap("over");
  schedule_timer();
  render();
}

static void start_game(GameMode mode) {
  game_start(&s_game, mode, (uint32_t)now_ms());
  s_in_game = true;
  log_heap("start");
  schedule_timer();
  render();
}

/* Keeps the elapsed part of the current interval, short of its boundary. */
static void pause_game(void) {
  if (s_timer != NULL) {
    uint64_t now = now_ms();
    uint64_t elapsed = now > s_timer_start ? now - s_timer_start : 0u;
    if (elapsed >= s_timer_delay) elapsed = s_timer_delay - 1u;
    cancel_timer();
    game_advance(&s_game, (uint32_t)elapsed);
  }
  if (game_pause(&s_game)) render();
}

static void quit_to_idle(void) {
  cancel_timer();
  s_in_game = false;
  render();
}

static void move_down_handler(ClickRecognizerRef recognizer, void *context) {
  GameButton button = click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP ? GAME_UP : GAME_DOWN;
  if (s_in_game && game_input(&s_game, button, true)) render();
}

static void move_up_handler(ClickRecognizerRef recognizer, void *context) {
  GameButton button = click_recognizer_get_button_id(recognizer) == BUTTON_ID_UP ? GAME_UP : GAME_DOWN;
  if (s_in_game) game_input(&s_game, button, false);
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_in_game) {
    start_game(GAME_A);
    return;
  }
  switch (s_game.status) {
    case GAME_PLAYING:
    case GAME_RECOVERING:
      pause_game();
      break;
    case GAME_PAUSED:
      game_resume(&s_game);
      schedule_timer();
      render();
      break;
    case GAME_OVER:
      start_game(s_game.mode);
      break;
  }
}

static void select_long_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_in_game) {
    start_game(GAME_B);
  } else if (s_game.status == GAME_OVER) {
    start_game(s_game.mode == GAME_A ? GAME_B : GAME_A);
  }
}

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_in_game) {
    window_stack_pop(true);
    return;
  }
  if (s_game.status == GAME_PLAYING || s_game.status == GAME_RECOVERING) {
    pause_game();
  } else {
    quit_to_idle();
  }
}

static void click_config(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, move_down_handler, move_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, move_down_handler, move_up_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, select_long_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

/* PRD 5.11: losing focus (notification, call) pauses a running game. */
static void will_focus(bool in_focus) {
  if (!in_focus && s_in_game) pause_game();
}

static void window_load(Window *window) {
  view_init(window_get_root_layer(window));
  render();
}

static void window_unload(Window *window) {
  cancel_timer();
  view_deinit();
}

int main(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  app_focus_service_subscribe_handlers((AppFocusHandlers) { .will_focus = will_focus });
  window_stack_push(s_window, true);
  app_event_loop();
  app_focus_service_unsubscribe();
  window_destroy(s_window);
}

#include <pebble.h>

#include "face.h"
#include "segments.h"

/* The face is static between minutes. The only timers are the one-shot beats of
 * the food demonstration, which run for ~10 s after each minute change. */
typedef char CompleteSegmentArt[(SEGMENT_ART_COUNT == SEG_COUNT) ? 1 : -1];

static Window *s_window;
static Layer *s_layer;
static GBitmap *s_backdrop, *s_sheet;
static GBitmap *s_art[SEG_COUNT];
static Scene s_scene;
static AppTimer *s_timer;
static int s_frame = -1;
static bool s_focused = true, s_art_ready;

static void cancel_demo(void) {
  if (s_timer != NULL) app_timer_cancel(s_timer);
  s_timer = NULL;
  s_frame = -1;
}

static void render(void) {
  time_t now = time(NULL);
  face_scene(&s_scene, localtime(&now), clock_is_24h_style(), s_frame);
  if (s_layer != NULL) layer_mark_dirty(s_layer);
}

static void demo_beat(void *data) {
  s_timer = NULL;
  if (!s_focused) { cancel_demo(); return; }
  ++s_frame;
  if (s_frame > FACE_DEMO_LAST) s_frame = -1;
  else s_timer = app_timer_register(FACE_DEMO_BEAT_MS, demo_beat, NULL);
  render();
}

/* Skip the demo when it would only spend battery: covered face, Quiet Time or a
 * low battery that is not charging. The clock itself always updates. */
static bool demo_allowed(void) {
  BatteryChargeState battery = battery_state_service_peek();
  return s_focused && !quiet_time_is_active() &&
         (battery.is_charging || battery.is_plugged || battery.charge_percent > 20);
}

static void minute_tick(struct tm *local, TimeUnits changed) {
  cancel_demo();
  if (demo_allowed()) {
    s_frame = FACE_DEMO_FIRST;
    s_timer = app_timer_register(FACE_DEMO_BEAT_MS, demo_beat, NULL);
  }
  render();
}

static void will_focus(bool in_focus) {
  s_focused = in_focus;
  if (!in_focus) cancel_demo();
  render();
}

static void update_proc(Layer *layer, GContext *ctx) {
  unsigned seg;
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  if (!s_art_ready) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    return;
  }
  graphics_draw_bitmap_in_rect(ctx, s_backdrop, layer_get_bounds(layer));
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    const SegmentArt *art = &segment_art[seg];
    if (scene_lit(&s_scene, seg))
      graphics_draw_bitmap_in_rect(ctx, s_art[seg], GRect(art->x, art->y, art->w, art->h));
  }
}

static void unload_art(void) {
  unsigned seg;
  for (seg = 0u; seg < SEG_COUNT; ++seg) {
    if (s_art[seg] != NULL) gbitmap_destroy(s_art[seg]);
    s_art[seg] = NULL;
  }
  if (s_sheet != NULL) gbitmap_destroy(s_sheet);
  if (s_backdrop != NULL) gbitmap_destroy(s_backdrop);
  s_sheet = s_backdrop = NULL;
  s_art_ready = false;
}

static void load_art(void) {
  unsigned seg;
  s_backdrop = gbitmap_create_with_resource(RESOURCE_ID_BACKDROP_GHOSTS);
  s_sheet = gbitmap_create_with_resource(RESOURCE_ID_SEGMENTS);
  s_art_ready = s_backdrop != NULL && s_sheet != NULL;
  for (seg = 0u; s_art_ready && seg < SEG_COUNT; ++seg) {
    const SegmentArt *art = &segment_art[seg];
    s_art[seg] = gbitmap_create_as_sub_bitmap(s_sheet,
                    GRect(art->sheet_x, art->sheet_y, art->w, art->h));
    if (s_art[seg] == NULL) s_art_ready = false;
  }
  if (!s_art_ready) APP_LOG(APP_LOG_LEVEL_ERROR, "Unable to load complete segment art");
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  load_art();
  s_layer = layer_create(layer_get_bounds(root));
  if (s_layer != NULL) {
    layer_set_update_proc(s_layer, update_proc);
    layer_add_child(root, s_layer);
  }
  render();
}

static void window_unload(Window *window) {
  cancel_demo();
  if (s_layer != NULL) layer_destroy(s_layer);
  s_layer = NULL;
  unload_art();
}

int main(void) {
  s_window = window_create();
  if (s_window == NULL) return 1;
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .unload = window_unload });
  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, minute_tick);
  app_focus_service_subscribe_handlers((AppFocusHandlers) { .will_focus = will_focus });
  app_event_loop();
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
  return 0;
}

#include <pebble.h>

#include "data.h"
#include "face_view.h"

enum { PERSIST_SETTINGS = 300, PERSIST_CACHE = 301, PERSIST_GOAL_DAY = 302, PERSIST_EVENT_DAY = 303 };
enum { REQUEST_WEATHER = 1, REQUEST_WORLD = 2 };
typedef char SettingsFitStorage[(FACE_SETTINGS_STORAGE_SIZE <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];

typedef struct {
  int32_t version, temp, high, low, code, unit, status, weather_updated;
  int32_t world_offset, world_updated;
  char location[17];
  uint32_t checksum;
} CacheRecord;
typedef char CacheFitStorage[(sizeof(CacheRecord) <= PERSIST_DATA_MAX_LENGTH) ? 1 : -1];

static Window *s_window;
static Layer *s_layer;
static FaceSettings s_settings;
static FaceData s_data;
static FaceRuntime s_runtime;
static AppTimer *s_timer;
static uint64_t s_animation_due;
static bool s_focused = true, s_art_ready, s_messages_open;
#if defined(PBL_HEALTH)
static bool s_health_subscribed;
#endif
static bool s_phone_ready, s_tick_seconds;
static FaceRequestQueue s_requests;
static time_t s_weather_attempt, s_world_attempt, s_celebration_until;

static void minute_tick(struct tm *local, TimeUnits changed);
static void presentation_beat(void *context);

static uint32_t cache_checksum(const CacheRecord *record) {
  const uint8_t *bytes = (const uint8_t *)record;
  uint32_t hash = 2166136261u;
  size_t i;
  for (i = 0; i < offsetof(CacheRecord, checksum); ++i) { hash ^= bytes[i]; hash *= 16777619u; }
  return hash;
}

static void save_settings(void) {
  uint8_t buffer[FACE_SETTINGS_STORAGE_SIZE];
  size_t length = settings_encode(&s_settings, buffer, sizeof(buffer));
  if (persist_write_data(PERSIST_SETTINGS, buffer, length) != (int)length)
    APP_LOG(APP_LOG_LEVEL_ERROR, "Settings persistence failed");
}

static void save_cache(void) {
  CacheRecord record;
  memset(&record, 0, sizeof(record));
  record.version = 1;
  record.temp = s_data.weather_temp; record.high = s_data.weather_high;
  record.low = s_data.weather_low; record.code = s_data.weather_code;
  record.unit = s_data.weather_unit; record.status = s_data.weather_status;
  record.weather_updated = s_data.weather_updated;
  record.world_offset = s_data.world_offset; record.world_updated = s_data.world_updated;
  memcpy(record.location, s_data.weather_location, sizeof(record.location));
  record.checksum = cache_checksum(&record);
  if (persist_write_data(PERSIST_CACHE, &record, sizeof(record)) != (int)sizeof(record))
    APP_LOG(APP_LOG_LEVEL_ERROR, "Data cache persistence failed");
}

static void load_persistence(void) {
  uint8_t buffer[FACE_SETTINGS_STORAGE_SIZE];
  CacheRecord record;
  time_t now = time(NULL);
  settings_defaults(&s_settings);
  if (persist_read_data(PERSIST_SETTINGS, buffer, sizeof(buffer)) == (int)sizeof(buffer) &&
      !settings_decode(&s_settings, buffer, sizeof(buffer)))
    APP_LOG(APP_LOG_LEVEL_WARNING, "Ignoring invalid settings record");
  if (persist_read_data(PERSIST_CACHE, &record, sizeof(record)) != (int)sizeof(record) ||
      record.version != 1 || record.checksum != cache_checksum(&record)) return;
  if (record.unit >= 0 && record.unit <= 1 && record.status >= 0 && record.status <= 2 &&
      record.temp >= -200 && record.temp <= 250 && record.high >= -200 && record.high <= 250 &&
      record.low >= -200 && record.low <= 250 && record.code >= 0 && record.code <= 99 &&
      record.weather_updated > 0 && record.weather_updated <= now + 300) {
    s_data.weather_temp = record.temp; s_data.weather_high = record.high;
    s_data.weather_low = record.low; s_data.weather_code = record.code;
    s_data.weather_unit = record.unit; s_data.weather_status = record.status;
    s_data.weather_updated = record.weather_updated;
    memcpy(s_data.weather_location, record.location, sizeof(record.location));
    s_data.weather_location[sizeof(s_data.weather_location)-1] = '\0';
  }
  if (record.world_offset >= -840 && record.world_offset <= 840 &&
      record.world_updated > 0 && record.world_updated <= now + 300) {
    s_data.world_offset = record.world_offset; s_data.world_updated = record.world_updated;
  }
}

static void render(void) {
  if (s_data.celebration && time(NULL) >= s_celebration_until) {
    s_data.celebration = false;
    s_data.celebration_kind = FACE_CELEBRATION_NONE;
  }
  if (s_layer != NULL) layer_mark_dirty(s_layer);
}

static uint64_t milliseconds(void) {
  time_t seconds;
  uint16_t ms = time_ms(&seconds, NULL);
  return (uint64_t)seconds*1000 + ms;
}

static void cancel_timer(void) {
  if (s_timer != NULL) app_timer_cancel(s_timer);
  s_timer = NULL;
}

static void schedule_timer(void) {
  uint64_t now_ms = milliseconds();
  time_t now = (time_t)(now_ms/1000);
  struct tm local = *localtime(&now);
  uint32_t delay = data_rotation_delay(&s_settings, &local, s_focused);
  /* Anchor swaps to local wall time, including the fractional second. */
  if (delay != 0) delay -= (uint32_t)(now_ms % 1000);
  cancel_timer();
  if (s_runtime.timer_pending && s_animation_due != 0) {
    uint64_t remaining = s_animation_due > now_ms ? s_animation_due - now_ms : 1;
    if (remaining > 4000) {
      remaining = data_animation_interval(&s_settings);
      s_animation_due = now_ms + remaining;
    }
    if (delay == 0 || remaining < delay) delay = (uint32_t)remaining;
  }
  if (delay == 0 || !s_focused || s_layer == NULL) return;
  s_timer = app_timer_register(delay, presentation_beat, NULL);
  if (s_timer == NULL) {
    data_runtime_update(&s_runtime, &s_settings, &s_data, &local, s_focused,
      quiet_time_is_active(), FACE_RUNTIME_TIMER_FAILED);
    s_animation_due = 0;
    APP_LOG(APP_LOG_LEVEL_WARNING, "Presentation timer unavailable; static face");
  }
}

static void reconcile(enum FaceRuntimeEvent event) {
  time_t now = time(NULL);
  struct tm local = *localtime(&now);
  int days, day = local.tm_year*366 + local.tm_yday;
  render(); /* Expire any prior celebration before considering today's event. */
  if (!s_data.celebration && s_settings.celebrate &&
      data_animation_allowed(&s_settings, &s_data, &local, s_focused, quiet_time_is_active()) &&
      settings_event_days(&s_settings, &local, &days) && days == 0 &&
      (!persist_exists(PERSIST_EVENT_DAY) || persist_read_int(PERSIST_EVENT_DAY) != day)) {
    s_data.celebration = true;
    s_data.celebration_kind = FACE_CELEBRATION_EVENT;
    s_celebration_until = now + 12;
    if (persist_write_int(PERSIST_EVENT_DAY, day) < 0)
      APP_LOG(APP_LOG_LEVEL_WARNING, "Event celebration persistence failed");
    event = FACE_RUNTIME_RESTART;
  }
  unsigned effects = data_runtime_update(&s_runtime, &s_settings, &s_data, &local,
    s_focused, quiet_time_is_active(), event);
  if (effects & FACE_EFFECT_CANCEL) cancel_timer();
  if (!s_runtime.timer_pending) s_animation_due = 0;
  if (effects & FACE_EFFECT_START) s_animation_due = milliseconds() + data_animation_interval(&s_settings);
  schedule_timer();
  render();
}

static void presentation_beat(void *context) {
  uint64_t now_ms = milliseconds();
  (void)context;
  s_timer = NULL;
  reconcile(s_runtime.timer_pending && s_animation_due <= now_ms ?
    FACE_RUNTIME_BEAT : FACE_RUNTIME_CHECK);
}

static bool update_health(void) {
  bool triggered = false;
#if defined(PBL_HEALTH)
  time_t now = time(NULL);
  struct tm local = *localtime(&now);
  HealthServiceAccessibilityMask access = health_service_metric_accessible(
    HealthMetricStepCount, time_start_of_today(), now);
  s_data.health_available = (access & HealthServiceAccessibilityMaskAvailable) != 0;
  if (s_data.health_available) {
    HealthValue steps = health_service_sum_today(HealthMetricStepCount);
    int day = local.tm_year*366 + local.tm_yday;
    s_data.steps = steps > 0 ? steps : 0;
    if (!s_data.celebration && s_settings.celebrate && s_data.steps >= s_settings.step_goal &&
        data_animation_allowed(&s_settings, &s_data, &local, s_focused, quiet_time_is_active()) &&
        (!persist_exists(PERSIST_GOAL_DAY) || persist_read_int(PERSIST_GOAL_DAY) != day)) {
      s_data.celebration = true;
      s_data.celebration_kind = FACE_CELEBRATION_STEPS;
      triggered = true;
      s_celebration_until = now + 12;
      if (persist_write_int(PERSIST_GOAL_DAY, day) < 0)
        APP_LOG(APP_LOG_LEVEL_WARNING, "Goal celebration persistence failed");
    }
  } else {
    s_data.steps = 0;
    if (s_data.celebration_kind == FACE_CELEBRATION_STEPS) {
      s_data.celebration = false;
      s_data.celebration_kind = FACE_CELEBRATION_NONE;
    }
  }
#else
  s_data.health_available = false;
#endif
  return triggered;
}

#if defined(PBL_HEALTH)
static void health_changed(HealthEventType event, void *context) {
  (void)context;
  if (event == HealthEventSignificantUpdate || event == HealthEventMovementUpdate ||
      event == HealthEventMetricAlert) {
    bool triggered = update_health();
    reconcile(triggered ? FACE_RUNTIME_RESTART : FACE_RUNTIME_CHECK);
  }
}
#endif

static void request_data(bool force) {
  time_t now = time(NULL);
  bool world_visible = s_settings.row1 == FACE_ROW_WORLD || s_settings.row2 == FACE_ROW_WORLD;
  time_t weather_period = s_data.weather_status == 2 ? 5*60 :
    s_data.weather_updated == 0 ? 60 : s_settings.weather_refresh*60;
  if (s_settings.weather_enabled && (force ||
      (now - s_weather_attempt >= weather_period && data_weather_stale(&s_settings, &s_data, now))))
    s_requests.pending |= REQUEST_WEATHER;
  if (world_visible && (force || ((s_data.world_updated == 0 || now - s_data.world_updated >= 24*60*60) &&
      now - s_world_attempt >= 5*60))) s_requests.pending |= REQUEST_WORLD;
  if (!s_settings.weather_enabled) s_requests.pending &= ~REQUEST_WEATHER;
  if (!world_visible) s_requests.pending &= ~REQUEST_WORLD;
  unsigned requests = data_request_take(&s_requests, now,
    s_messages_open && s_phone_ready && s_data.connected);
  if (requests == 0) return;
  DictionaryIterator *iterator = NULL;
  if (app_message_outbox_begin(&iterator) != APP_MSG_OK || iterator == NULL ||
      dict_write_uint8(iterator, FACE_KEY_REQUEST_DATA, requests) != DICT_OK) {
    data_request_complete(&s_requests, false);
    return;
  }
  if (requests & REQUEST_WEATHER) s_weather_attempt = now;
  if (requests & REQUEST_WORLD) s_world_attempt = now;
  if (app_message_outbox_send() != APP_MSG_OK) {
    data_request_complete(&s_requests, false);
  }
}

static bool tuple_integer(const Tuple *tuple, int32_t *value) {
  if (tuple == NULL || (tuple->type != TUPLE_INT && tuple->type != TUPLE_UINT)) return false;
  if (tuple->type == TUPLE_INT) {
    if (tuple->length == 1) *value = tuple->value->int8;
    else if (tuple->length == 2) *value = tuple->value->int16;
    else if (tuple->length == 4) *value = tuple->value->int32;
    else return false;
  } else {
    uint32_t unsigned_value;
    if (tuple->length == 1) unsigned_value = tuple->value->uint8;
    else if (tuple->length == 2) unsigned_value = tuple->value->uint16;
    else if (tuple->length == 4) unsigned_value = tuple->value->uint32;
    else return false;
    *value = unsigned_value > INT32_MAX ? INT32_MAX : (int32_t)unsigned_value;
  }
  return true;
}

static bool read_integer(DictionaryIterator *iterator, uint32_t key, int32_t *value) {
  return tuple_integer(dict_find(iterator, key), value);
}

static void copy_tuple_text(char *dest, size_t length, const Tuple *tuple) {
  size_t i, count = 0;
  const uint8_t *source;
  size_t available;
  if (!tuple || tuple->type != TUPLE_CSTRING || length == 0) return;
  /* Pebble declares cstring as an interior zero-length array. Read the
   * validated tuple payload through its byte address, bounded by inbox size. */
  source = (const uint8_t *)(const void *)tuple + offsetof(Tuple, value);
  available = tuple->length < 1024 ? tuple->length : 1024;
  for (i = 0; i < available && count + 1 < length; ++i) {
    unsigned char value = source[i];
    if (value == 0) break;
    if (value >= 32 && value <= 126) dest[count++] = (char)value;
  }
  dest[count] = '\0';
}

static void apply_data(DictionaryIterator *iterator) {
  int32_t updated, offset = 0, world_updated;
  FaceData snapshot = s_data;
  time_t now = time(NULL);
  bool changed = false;
  if (read_integer(iterator, FACE_KEY_WEATHER_STATUS, &snapshot.weather_status)) {
    bool has_updated = read_integer(iterator, FACE_KEY_WEATHER_UPDATED, &updated);
    if (has_updated) snapshot.weather_updated = updated;
    bool complete = has_updated && read_integer(iterator, FACE_KEY_WEATHER_TEMP, &snapshot.weather_temp) &&
      read_integer(iterator, FACE_KEY_WEATHER_HIGH, &snapshot.weather_high) &&
      read_integer(iterator, FACE_KEY_WEATHER_LOW, &snapshot.weather_low) &&
      read_integer(iterator, FACE_KEY_WEATHER_CODE, &snapshot.weather_code) &&
      read_integer(iterator, FACE_KEY_WEATHER_UNIT, &snapshot.weather_unit);
    copy_tuple_text(snapshot.weather_location, sizeof(snapshot.weather_location),
      dict_find(iterator, FACE_KEY_WEATHER_LOCATION));
    changed = data_apply_weather(&s_data, &snapshot, complete, has_updated, now);
  }
  if (read_integer(iterator, FACE_KEY_WORLD_UPDATED, &world_updated)) {
    bool has_offset = read_integer(iterator, FACE_KEY_WORLD_OFFSET, &offset);
    changed |= data_apply_world(&s_data, offset, world_updated, has_offset, now);
  }
  if (changed) save_cache();
}

static void inbox_received(DictionaryIterator *iterator, void *context) {
  FaceSettings before = s_settings;
  int32_t version, preset, value;
  bool valid_version = dict_find(iterator, FACE_KEY_CONFIG_VERSION) == NULL ||
    (read_integer(iterator, FACE_KEY_CONFIG_VERSION, &version) && version == 1);
  (void)context;
  if (valid_version) {
    if (read_integer(iterator, FACE_KEY_PRESET, &preset) && preset >= 0 && preset <= 5)
      settings_apply_preset(&s_settings, preset);
    for (Tuple *tuple = dict_read_first(iterator); tuple != NULL; tuple = dict_read_next(iterator)) {
      if (tuple->key == FACE_KEY_PRESET) continue;
      if (tuple_integer(tuple, &value)) settings_apply_integer(&s_settings, tuple->key, value);
      else if (tuple->type == TUPLE_CSTRING &&
          (tuple->key == FACE_KEY_WORLD_LABEL || tuple->key == FACE_KEY_EVENT_LABEL || tuple->key == FACE_KEY_EVENT_DATE)) {
        char text[17] = "";
        copy_tuple_text(text, sizeof(text), tuple);
        settings_apply_text(&s_settings, tuple->key, text);
      }
    }
    settings_validate(&s_settings);
  }
  apply_data(iterator);
  if (memcmp(&before, &s_settings, sizeof(before)) != 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Settings preset=%ld theme=%ld rows=%ld/%ld mode=%ld",
      (long)s_settings.preset, (long)s_settings.theme, (long)s_settings.row1,
      (long)s_settings.row2, (long)s_settings.animation_mode);
    save_settings();
    if (s_art_ready) face_view_configure(&s_settings);
    if (s_tick_seconds != s_settings.blink_colon) {
      s_tick_seconds = s_settings.blink_colon;
      tick_timer_service_subscribe(s_tick_seconds ? SECOND_UNIT : MINUTE_UNIT, minute_tick);
    }
    update_health();
    s_weather_attempt = s_world_attempt = 0;
    reconcile(FACE_RUNTIME_RESTART);
    request_data(true);
  } else render();
  if (read_integer(iterator, FACE_KEY_PHONE_READY, &value) && value == 1) {
    s_phone_ready = true;
    s_requests.next_attempt = 0;
    request_data(true);
  }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  (void)context;
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
}

static void outbox_sent(DictionaryIterator *iterator, void *context) {
  (void)iterator; (void)context;
  data_request_complete(&s_requests, true);
}

static void outbox_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  (void)iterator; (void)context;
  data_request_complete(&s_requests, false);
  /* Retry from minute_tick; no rapid error or allocation retry loop. */
  APP_LOG(APP_LOG_LEVEL_WARNING, "Data request failed: %d", reason);
}

static void battery_changed(BatteryChargeState battery) {
  s_data.battery_percent = battery.charge_percent;
  s_data.charging = battery.is_charging || battery.is_plugged;
  bool triggered = update_health();
  reconcile(triggered ? FACE_RUNTIME_RESTART : FACE_RUNTIME_CHECK);
}

static void connection_changed(bool connected) {
  s_data.connected = connected;
  if (connected) { s_requests.next_attempt = 0; request_data(true); }
  render();
}

static void minute_tick(struct tm *local, TimeUnits changed) {
  (void)local;
  if (changed & MINUTE_UNIT) {
    update_health();
    request_data(false);
    reconcile(FACE_RUNTIME_MINUTE);
  } else reconcile(FACE_RUNTIME_CHECK);
}

static void will_focus(bool focused) {
  s_focused = focused;
  bool triggered = focused && update_health();
  reconcile(triggered ? FACE_RUNTIME_RESTART : FACE_RUNTIME_CHECK);
  if (focused) request_data(false);
}

static void update_proc(Layer *layer, GContext *context) {
  time_t now = time(NULL);
  struct tm local = *localtime(&now);
  face_view_draw(context, layer_get_bounds(layer), &s_settings, &s_data, &local,
    clock_is_24h_style(), s_runtime.frame, s_art_ready && s_runtime.animate);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_art_ready = face_view_init();
  if (s_art_ready) face_view_configure(&s_settings);
  s_layer = layer_create(layer_get_bounds(root));
  if (s_layer != NULL) {
    layer_set_update_proc(s_layer, update_proc);
    layer_add_child(root, s_layer);
  }
  reconcile(FACE_RUNTIME_RESTART);
  APP_LOG(APP_LOG_LEVEL_INFO, "Face ready=%d heap used=%d free=%d", s_art_ready,
    (int)heap_bytes_used(), (int)heap_bytes_free());
}

static void window_unload(Window *window) {
  (void)window;
  cancel_timer();
  s_animation_due = 0;
  s_runtime.timer_pending = s_runtime.animate = false;
  if (s_layer != NULL) layer_destroy(s_layer);
  s_layer = NULL;
  face_view_deinit();
  s_art_ready = false;
}

int main(void) {
  BatteryChargeState battery;
  load_persistence();
  battery = battery_state_service_peek();
  s_data.battery_percent = battery.charge_percent;
  s_data.charging = battery.is_charging || battery.is_plugged;
  s_data.connected = connection_service_peek_pebble_app_connection();
  update_health();
  s_window = window_create();
  if (s_window == NULL) return 1;
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_sent(outbox_sent);
  app_message_register_outbox_failed(outbox_failed);
  s_messages_open = app_message_open(1024, 64) == APP_MSG_OK;
  if (!s_messages_open) APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage unavailable");
  window_set_window_handlers(s_window, (WindowHandlers){.load = window_load, .unload = window_unload});
  window_stack_push(s_window, true);
  s_tick_seconds = s_settings.blink_colon;
  tick_timer_service_subscribe(s_tick_seconds ? SECOND_UNIT : MINUTE_UNIT, minute_tick);
  app_focus_service_subscribe_handlers((AppFocusHandlers){.will_focus = will_focus});
  battery_state_service_subscribe(battery_changed);
  connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler = connection_changed});
#if defined(PBL_HEALTH)
  s_health_subscribed = health_service_events_subscribe(health_changed, NULL);
#endif
  app_event_loop();
  cancel_timer();
  app_message_deregister_callbacks();
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  if (s_health_subscribed) health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
  return 0;
}

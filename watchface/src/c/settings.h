#ifndef POPEYE_FACE_SETTINGS_H
#define POPEYE_FACE_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef PBL_PLATFORM_EMERY
#include <pebble.h> /* SDK time.h omits struct tm; mirror the shared clock header. */
#else
#include <time.h>
#endif

/* Values and key numbers are shared with the Clay phone configuration. */
enum FaceRow { FACE_ROW_NONE, FACE_ROW_DATE, FACE_ROW_BATTERY,
  FACE_ROW_WEATHER, FACE_ROW_STEPS, FACE_ROW_WORLD, FACE_ROW_EVENT };
enum FacePreset { FACE_PRESET_CUSTOM, FACE_PRESET_CLASSIC,
  FACE_PRESET_EVERYDAY, /* Retired value 3 stays reserved for old preferences. */
  FACE_PRESET_TRAVELLER = 4, FACE_PRESET_LARGE = 5 };
/* Wire value 0 was Lively (now Arcade) and 3 was Once per minute (now Still); saved
 * or sent retired values load as their replacement and the others keep their numbers. */
enum FaceAnimation { FACE_ANIMATION_RETIRED_LIVELY, FACE_ANIMATION_ARCADE,
  FACE_ANIMATION_RELAXED, FACE_ANIMATION_RETIRED_CLASSIC, FACE_ANIMATION_STILL };
enum FaceCelebration { FACE_CELEBRATION_NONE, FACE_CELEBRATION_STEPS,
  FACE_CELEBRATION_EVENT };
enum FaceWireKey {
  FACE_KEY_PRESET = 1, FACE_KEY_ROW1, FACE_KEY_ROW2, FACE_KEY_ROTATE_SECONDS,
  FACE_KEY_TIME_FORMAT, FACE_KEY_LARGE_TIME, FACE_KEY_HIGH_CONTRAST,
  FACE_KEY_REDUCED_MOTION, FACE_KEY_BLINK_COLON, FACE_KEY_DATE_FORMAT,
  FACE_KEY_LANGUAGE, FACE_KEY_SHOW_YEAR, FACE_KEY_SHOW_WEEK,
  FACE_KEY_BATTERY_STYLE, FACE_KEY_BATTERY_THRESHOLD, FACE_KEY_DISCONNECT_ALERT,
  FACE_KEY_STEP_GOAL, FACE_KEY_STEP_STYLE, FACE_KEY_CELEBRATE,
  FACE_KEY_WEATHER_ENABLED, FACE_KEY_WEATHER_UNITS, FACE_KEY_WEATHER_DETAIL,
  FACE_KEY_WEATHER_REFRESH, FACE_KEY_WEATHER_EFFECTS, FACE_KEY_LOCATION_MODE,
  FACE_KEY_LOCATION_NAME, FACE_KEY_WORLD_ZONE, FACE_KEY_WORLD_LABEL,
  FACE_KEY_WORLD_FORMAT, FACE_KEY_EVENT_LABEL, FACE_KEY_EVENT_DATE,
  FACE_KEY_EVENT_REPEAT, FACE_KEY_EVENT_ELAPSED, FACE_KEY_ANIMATION_MODE,
  FACE_KEY_ANIMATION_SPEED, FACE_KEY_ANIMATION_PAUSE, FACE_KEY_CHARACTER_ACTIVITY,
  FACE_KEY_QUIET_START, FACE_KEY_QUIET_END, FACE_KEY_QUIET_HOURS,
  FACE_KEY_LOW_BATTERY_CUTOFF, FACE_KEY_THEME, FACE_KEY_BACKGROUND_COLOR,
  FACE_KEY_SEGMENT_COLOR, FACE_KEY_ACCENT_COLOR, FACE_KEY_CUSTOM_COLORS,
  FACE_KEY_GHOST_STRENGTH, FACE_KEY_COLOR_ARTWORK, FACE_KEY_ROW1_THEN, FACE_KEY_ROW2_THEN,
  FACE_KEY_REQUEST_DATA = 100, FACE_KEY_WEATHER_TEMP, FACE_KEY_WEATHER_HIGH,
  FACE_KEY_WEATHER_LOW, FACE_KEY_WEATHER_CODE, FACE_KEY_WEATHER_UPDATED,
  FACE_KEY_WEATHER_STATUS, FACE_KEY_WORLD_OFFSET, FACE_KEY_WORLD_UPDATED,
  FACE_KEY_CONFIG_VERSION, FACE_KEY_PHONE_READY, FACE_KEY_WEATHER_LOCATION,
  FACE_KEY_WEATHER_UNIT
};

typedef struct {
  int32_t preset, row1, row2, rotate_seconds, time_format;
  int32_t row1_then, row2_then; /* optional second item on a row; see settings_row_then */
  bool large_time, high_contrast, reduced_motion, blink_colon;
  int32_t date_format, language;
  bool show_year, show_week;
  int32_t battery_style, battery_threshold;
  bool disconnect_alert;
  int32_t step_goal, step_style;
  bool celebrate, weather_enabled;
  int32_t weather_units, weather_detail, weather_refresh;
  bool weather_effects;
  int32_t location_mode;
  char world_label[9];
  int32_t world_format;
  char event_label[11], event_date[11];
  bool event_repeat, event_elapsed;
  int32_t animation_mode, animation_speed, animation_pause, character_activity;
  int32_t quiet_start, quiet_end;
  bool quiet_hours;
  int32_t low_battery_cutoff, theme, background_color, segment_color, accent_color;
  bool custom_colors;
  int32_t ghost_strength;
  bool color_artwork;
} FaceSettings;

typedef struct {
  int32_t battery_percent;
  bool charging, connected, health_available;
  int32_t steps;
  int32_t weather_temp, weather_high, weather_low, weather_code, weather_status,
          weather_unit;
  time_t weather_updated;
  int32_t world_offset;
  time_t world_updated;
  bool celebration;
  int32_t celebration_kind;
  char weather_location[17];
} FaceData;

void settings_defaults(FaceSettings *settings);
void settings_validate(FaceSettings *settings);
void settings_apply_preset(FaceSettings *settings, int preset);
/* The item shown after a row's own, or FACE_ROW_NONE. Only Battery, Weather and Steps
 * are short enough to share a line, and a row never pairs with itself. slot is 0 or 1. */
int settings_row_then(const FaceSettings *settings, int slot);
bool settings_apply_integer(FaceSettings *settings, uint32_t key, int32_t value);
bool settings_apply_text(FaceSettings *settings, uint32_t key, const char *value);
bool settings_quiet_now(const FaceSettings *settings, const struct tm *local);
bool settings_event_days(const FaceSettings *settings, const struct tm *local,
                         int *days);

/* Stable, little-endian, checksummed schema; fits one Pebble persistent record. */
#define FACE_SETTINGS_STORAGE_SIZE 235
size_t settings_encode(const FaceSettings *settings, uint8_t *buffer, size_t length);
bool settings_decode(FaceSettings *settings, const uint8_t *buffer, size_t length);

#endif

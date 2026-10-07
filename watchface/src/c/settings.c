#include "settings.h"

#include <string.h>

#define INTEGER_FIELDS(X) \
  X(1, preset) X(2, row1) X(3, row2) X(4, rotate_seconds) X(5, time_format) \
  X(10, date_format) X(11, language) X(14, battery_style) X(15, battery_threshold) \
  X(17, step_goal) X(18, step_style) X(21, weather_units) X(22, weather_detail) \
  X(23, weather_refresh) X(25, location_mode) X(29, world_format) \
  X(34, animation_mode) X(35, animation_speed) X(36, animation_pause) \
  X(37, character_activity) X(38, quiet_start) X(39, quiet_end) \
  X(41, low_battery_cutoff) X(42, theme) X(43, background_color) \
  X(44, segment_color) X(45, accent_color) X(47, ghost_strength)
#define BOOL_FIELDS(X) \
  X(6, large_time) X(7, high_contrast) X(8, reduced_motion) X(9, blink_colon) \
  X(12, show_year) X(13, show_week) X(16, disconnect_alert) X(19, celebrate) \
  X(20, weather_enabled) X(24, weather_effects) X(32, event_repeat) \
  X(33, event_elapsed) X(40, quiet_hours) X(46, custom_colors) X(48, color_artwork)

static int32_t clamp(int32_t value, int32_t minimum, int32_t maximum) {
  return value < minimum ? minimum : value > maximum ? maximum : value;
}

static void ascii_copy(char *dest, size_t length, const char *source) {
  size_t count = 0;
  if (source != NULL) {
    while (*source != '\0' && count + 1 < length) {
      unsigned char value = (unsigned char)*source++;
      if (value >= 32 && value <= 126) dest[count++] = (char)value;
    }
  }
  dest[count] = '\0';
  while (++count < length) dest[count] = '\0';
}

static bool leap(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int month_length(int year, int month) {
  static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  return month < 1 || month > 12 ? 0 : lengths[month - 1] + (month == 2 && leap(year));
}

static bool date_parts(const char *date, int *year, int *month, int *day) {
  unsigned i;
  if (strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
  for (i = 0; i < 10; ++i) {
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) return false;
  }
  *year = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + date[3]-'0';
  *month = (date[5]-'0')*10 + date[6]-'0';
  *day = (date[8]-'0')*10 + date[9]-'0';
  return *year >= 1 && *year <= 9999 && *day >= 1 && *day <= month_length(*year, *month);
}

/* Gregorian days since 0001-01-01; avoids 32-bit time_t and DST arithmetic. */
static int civil_day(int year, int month, int day) {
  int previous = year - 1, result = previous*365 + previous/4 - previous/100 + previous/400;
  int index;
  for (index = 1; index < month; ++index) result += month_length(year, index);
  return result + day - 1;
}

void settings_defaults(FaceSettings *s) {
  memset(s, 0, sizeof(*s));
  s->preset = FACE_PRESET_EVERYDAY;
  s->row1 = FACE_ROW_DATE;
  s->row2 = FACE_ROW_BATTERY;
  s->battery_style = 2; /* Battery icon + percentage. */
  s->step_goal = 10000;
  s->celebrate = true;
  s->weather_refresh = 60;
  memcpy(s->world_label, "HOME", 5);
  memcpy(s->event_label, "EVENT", 6);
  s->animation_mode = FACE_ANIMATION_ARCADE;
  s->animation_pause = 1;
  s->character_activity = 7;
  s->quiet_start = 22;
  s->quiet_end = 7;
  s->low_battery_cutoff = 20;
  s->theme = 1; /* Ivory */
  s->background_color = 0xffffff;
  s->segment_color = 0;
  s->accent_color = 0xff5500;
  s->ghost_strength = 1;
  s->color_artwork = true;
}

void settings_validate(FaceSettings *s) {
  int y, m, d;
  /* Keep legacy record fields and wire keys, but retire their behavior. */
  s->disconnect_alert = false;
  s->weather_effects = false;
#define RANGE(field, lo, hi) s->field = clamp(s->field, lo, hi)
  RANGE(preset, 0, 5); RANGE(row1, 0, 6); RANGE(row2, 0, 6);
  if (s->preset == 3) s->preset = FACE_PRESET_CUSTOM;
  if (s->rotate_seconds != 0) RANGE(rotate_seconds, 15, 120);
  RANGE(time_format, 0, 2); RANGE(date_format, 0, 2); RANGE(language, 0, 4);
  RANGE(battery_style, 0, 3); RANGE(battery_threshold, 0, 100);
  RANGE(step_goal, 100, 100000); RANGE(step_style, 0, 2);
  RANGE(weather_units, 0, 1); RANGE(weather_detail, 0, 1);
  RANGE(weather_refresh, 15, 180); RANGE(location_mode, 0, 1);
  RANGE(world_format, 0, 2); RANGE(animation_mode, 0, 4);
  if (s->animation_mode == FACE_ANIMATION_RETIRED_LIVELY) s->animation_mode = FACE_ANIMATION_ARCADE;
  if (s->animation_mode == FACE_ANIMATION_RETIRED_CLASSIC) s->animation_mode = FACE_ANIMATION_STILL;
  if (s->animation_speed != 0) RANGE(animation_speed, 500, 4000);
  RANGE(animation_pause, 0, 10); RANGE(character_activity, 0, 7);
  RANGE(quiet_start, 0, 23); RANGE(quiet_end, 0, 23);
  RANGE(low_battery_cutoff, 0, 50);
  /* Retired Midnight preferences migrate to Ivory, retaining other choices. */
  if (s->theme < 0 || s->theme > 3) s->theme = 1;
  RANGE(background_color, 0, 0xffffff); RANGE(segment_color, 0, 0xffffff);
  RANGE(accent_color, 0, 0xffffff); RANGE(ghost_strength, 0, 2);
#undef RANGE
  s->world_label[sizeof(s->world_label)-1] = '\0';
  s->event_label[sizeof(s->event_label)-1] = '\0';
  s->event_date[sizeof(s->event_date)-1] = '\0';
  ascii_copy(s->world_label, sizeof(s->world_label), s->world_label);
  ascii_copy(s->event_label, sizeof(s->event_label), s->event_label);
  if (s->event_date[0] && !date_parts(s->event_date, &y, &m, &d)) s->event_date[0] = '\0';
}

bool settings_apply_integer(FaceSettings *s, uint32_t key, int32_t value) {
  switch (key) {
#define APPLY_INT(number, field) case number: s->field = value; break;
    INTEGER_FIELDS(APPLY_INT)
#undef APPLY_INT
#define APPLY_BOOL(number, field) case number: s->field = value != 0; break;
    BOOL_FIELDS(APPLY_BOOL)
#undef APPLY_BOOL
    default: return false;
  }
  return true;
}

static int32_t integer_value(const FaceSettings *s, uint32_t key) {
  switch (key) {
#define GET(number, field) case number: return s->field;
    INTEGER_FIELDS(GET)
    BOOL_FIELDS(GET)
#undef GET
    default: return 0;
  }
}

bool settings_apply_text(FaceSettings *s, uint32_t key, const char *value) {
  switch (key) {
    case FACE_KEY_WORLD_LABEL: ascii_copy(s->world_label, sizeof(s->world_label), value); return true;
    case FACE_KEY_EVENT_LABEL: ascii_copy(s->event_label, sizeof(s->event_label), value); return true;
    case FACE_KEY_EVENT_DATE: ascii_copy(s->event_date, sizeof(s->event_date), value); return true;
    default: return false;
  }
}

void settings_apply_preset(FaceSettings *s, int preset) {
  FaceSettings defaults;
  uint32_t key;
  if (preset < FACE_PRESET_CLASSIC || preset > FACE_PRESET_LARGE || preset == 3) {
    s->preset = FACE_PRESET_CUSTOM;
    return;
  }
  settings_defaults(&defaults);
  /* Reset layout/style only. Motion, service consent and personal values
   * stay independent, including the reduced-motion accessibility override. */
  for (key = 2; key <= 48; ++key) {
    if ((key <= 15 && key != FACE_KEY_REDUCED_MOTION) || key == 18 ||
        key == 22 || key == 29 || key >= 42)
      settings_apply_integer(s, key, integer_value(&defaults, key));
  }
  s->preset = preset;
  switch (preset) {
    case FACE_PRESET_CLASSIC:
      s->row1 = s->row2 = FACE_ROW_NONE;
      break;
    case FACE_PRESET_TRAVELLER:
      s->row1 = FACE_ROW_WORLD; s->row2 = FACE_ROW_DATE;
      break;
    case FACE_PRESET_LARGE:
      s->large_time = s->high_contrast = true;
      break;
    default: break;
  }
  settings_validate(s);
}

bool settings_quiet_now(const FaceSettings *s, const struct tm *local) {
  int hour;
  if (!s->quiet_hours || local == NULL || s->quiet_start == s->quiet_end) return false;
  hour = local->tm_hour;
  return s->quiet_start < s->quiet_end ? hour >= s->quiet_start && hour < s->quiet_end :
         hour >= s->quiet_start || hour < s->quiet_end;
}

bool settings_event_days(const FaceSettings *s, const struct tm *local, int *days) {
  int year, month, day, today, event, local_year;
  if (local == NULL || days == NULL || !date_parts(s->event_date, &year, &month, &day)) return false;
  local_year = local->tm_year + 1900;
  if (local_year < 1 || local_year > 9999 || local->tm_mon < 0 || local->tm_mon > 11 || local->tm_mday < 1 ||
      local->tm_mday > month_length(local_year, local->tm_mon + 1)) return false;
  today = civil_day(local_year, local->tm_mon + 1, local->tm_mday);
  if (s->event_repeat) {
    year = local_year;
    /* A Feb 29 anniversary uses Feb 28 in a common year. */
    if (month == 2 && day == 29 && !leap(year)) day = 28;
    event = civil_day(year, month, day);
    if (event < today && !s->event_elapsed) {
      ++year;
      day = (s->event_date[8]-'0')*10 + s->event_date[9]-'0';
      if (month == 2 && day == 29 && !leap(year)) day = 28;
      event = civil_day(year, month, day);
    } else if (event > today && s->event_elapsed) {
      --year;
      day = (s->event_date[8]-'0')*10 + s->event_date[9]-'0';
      if (month == 2 && day == 29 && !leap(year)) day = 28;
      event = civil_day(year, month, day);
    }
  } else {
    event = civil_day(year, month, day);
    if (event < today && !s->event_elapsed) return false;
  }
  *days = event - today;
  return true;
}

static void write32(uint8_t *buffer, uint32_t value) {
  unsigned i;
  for (i = 0; i < 4; ++i) buffer[i] = (uint8_t)(value >> (i*8));
}
static uint32_t read32(const uint8_t *buffer) {
  return (uint32_t)buffer[0] | (uint32_t)buffer[1]<<8 | (uint32_t)buffer[2]<<16 | (uint32_t)buffer[3]<<24;
}
static uint32_t checksum(const uint8_t *buffer, size_t length) {
  uint32_t hash = 2166136261u;
  size_t i;
  for (i = 0; i < length; ++i) { hash ^= buffer[i]; hash *= 16777619u; }
  return hash;
}

size_t settings_encode(const FaceSettings *settings, uint8_t *buffer, size_t length) {
  FaceSettings s = *settings;
  uint32_t key;
  if (buffer == NULL || length < FACE_SETTINGS_STORAGE_SIZE) return 0;
  settings_validate(&s);
  write32(buffer, 0x57464750u); /* PGFW */
  write32(buffer + 4, 1);
  for (key = 1; key <= 48; ++key) write32(buffer + 12 + (key-1)*4, (uint32_t)integer_value(&s, key));
  memcpy(buffer + 204, s.world_label, 9);
  memcpy(buffer + 213, s.event_label, 11);
  memcpy(buffer + 224, s.event_date, 11);
  write32(buffer + 8, checksum(buffer + 12, FACE_SETTINGS_STORAGE_SIZE-12));
  return FACE_SETTINGS_STORAGE_SIZE;
}

bool settings_decode(FaceSettings *settings, const uint8_t *buffer, size_t length) {
  FaceSettings result;
  uint32_t key;
  if (buffer == NULL || length != FACE_SETTINGS_STORAGE_SIZE || read32(buffer) != 0x57464750u ||
      read32(buffer + 4) != 1 || read32(buffer + 8) != checksum(buffer + 12, length-12)) return false;
  settings_defaults(&result);
  for (key = 1; key <= 48; ++key) settings_apply_integer(&result, key, (int32_t)read32(buffer + 12 + (key-1)*4));
  memcpy(result.world_label, buffer + 204, 9);
  memcpy(result.event_label, buffer + 213, 11);
  memcpy(result.event_date, buffer + 224, 11);
  settings_validate(&result);
  *settings = result;
  return true;
}

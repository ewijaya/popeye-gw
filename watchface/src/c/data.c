#include "data.h"

#include <stdio.h>
#include <string.h>

static bool leap(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int iso_weeks(int year) {
  int previous = year - 1;
  int jan1 = (previous*365 + previous/4 - previous/100 + previous/400) % 7;
  return jan1 == 3 || (jan1 == 2 && leap(year)) ? 53 : 52;
}

static int iso_week(const struct tm *local) {
  static const int cumulative[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int year = local->tm_year + 1900;
  int yday = cumulative[local->tm_mon] + local->tm_mday - 1 +
             (local->tm_mon > 1 && leap(year));
  int weekday = local->tm_wday == 0 ? 7 : local->tm_wday;
  int week = (yday + 11 - weekday) / 7;
  return week < 1 ? iso_weeks(year - 1) : week > iso_weeks(year) ? 1 : week;
}

void data_format_date(char *out, size_t length, const FaceSettings *s, const struct tm *local) {
  static const char * const weekdays[4][7] = {
    {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"},
    {"So","Mo","Di","Mi","Do","Fr","Sa"},
    {"Dim","Lun","Mar","Mer","Jeu","Ven","Sam"},
    {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"}};
  static const char * const months[4][12] = {
    {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"},
    {"Jan","Feb","Mar","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez"},
    {"Jan","Fev","Mar","Avr","Mai","Jun","Jul","Aou","Sep","Oct","Nov","Dec"},
    {"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"}};
  char date[32], year[8] = "", week[8] = "";
  unsigned calendar_year;
  int language = s->language >= 0 && s->language <= 3 ? s->language : 0;
  if (length == 0) return;
  out[0] = '\0';
  if (!local || local->tm_mon < 0 || local->tm_mon > 11 || local->tm_wday < 0 ||
      local->tm_wday > 6 || local->tm_mday < 1 || local->tm_mday > 31 ||
      local->tm_year < -1899 || local->tm_year > 8099) return;
  calendar_year = (unsigned)(local->tm_year + 1900);
  if (s->language == 4) {
    static const char * const days[] = {"日","月","火","水","木","金","土"};
    if (s->show_year)
      snprintf(date, sizeof(date), "%04u/%02d/%02d%s", calendar_year,
        local->tm_mon+1, local->tm_mday, days[local->tm_wday]);
    else if (s->date_format == 2)
      snprintf(date, sizeof(date), "%02d/%02d(%s)", local->tm_mon+1,
        local->tm_mday, days[local->tm_wday]);
    else snprintf(date, sizeof(date), "%d月%d日(%s)", local->tm_mon+1,
      local->tm_mday, days[local->tm_wday]);
    if (s->show_week) snprintf(week, sizeof(week), s->show_year ? "W%02d" : " W%02d", iso_week(local));
    snprintf(out, length, "%s%s", date, week);
    return;
  }
  if (s->show_year && s->show_week) {
    if (s->date_format == 2) snprintf(date, sizeof(date), "%.2s%02d/%02d/%04u",
      weekdays[language][local->tm_wday], local->tm_mday, local->tm_mon+1, calendar_year);
    else if (s->date_format == 1) snprintf(date, sizeof(date), "%.2s%s%02d%04u",
      weekdays[language][local->tm_wday], months[language][local->tm_mon], local->tm_mday, calendar_year);
    else snprintf(date, sizeof(date), "%.2s%02d%s%04u",
      weekdays[language][local->tm_wday], local->tm_mday, months[language][local->tm_mon], calendar_year);
  } else if (s->date_format == 2) {
    snprintf(date, sizeof(date), "%02d/%02d", local->tm_mday, local->tm_mon+1);
    if (s->show_year) snprintf(year, sizeof(year), "/%04u", calendar_year);
  } else if (s->date_format == 1) snprintf(date, sizeof(date), "%s %s %d", weekdays[language][local->tm_wday], months[language][local->tm_mon], local->tm_mday);
  else snprintf(date, sizeof(date), "%s %d %s", weekdays[language][local->tm_wday], local->tm_mday, months[language][local->tm_mon]);
  if (s->show_year && !s->show_week && s->date_format != 2) snprintf(year, sizeof(year), " %04u", calendar_year);
  if (s->show_week) snprintf(week, sizeof(week), " W%02d", iso_week(local));
  snprintf(out, length, "%s%s%s", date, year, week);
}

bool data_world_time(const FaceData *data, time_t now, struct tm *world) {
  time_t adjusted;
  struct tm *result;
  if (data->world_updated <= 0 || data->world_offset < -840 || data->world_offset > 840 || world == NULL) return false;
  adjusted = now + data->world_offset*60;
  result = gmtime(&adjusted);
  if (result == NULL) return false;
  *world = *result;
  return true;
}

void data_format_world(char *out, size_t length, const FaceSettings *s,
                       const FaceData *data, time_t now, bool watch_24h) {
  struct tm world;
  bool format_24h = s->world_format == 2 || (s->world_format == 0 &&
    (s->time_format == 2 || (s->time_format == 0 && watch_24h)));
  const char *marker = data_world_stale(data, now) ? "*" : "";
  if (length == 0) return;
  if (!data_world_time(data, now, &world)) {
    snprintf(out, length, "%s --:--", s->world_label);
  } else if (format_24h) {
    snprintf(out, length, "%s %02d:%02d%s", s->world_label, world.tm_hour, world.tm_min, marker);
  } else {
    int hour = world.tm_hour % 12;
    snprintf(out, length, "%s %d:%02d%s%s", s->world_label, hour ? hour : 12,
      world.tm_min, world.tm_hour < 12 ? "a" : "p", marker);
  }
}

bool data_weather_stale(const FaceSettings *s, const FaceData *d, time_t now) {
  return !s->weather_enabled || d->weather_unit != s->weather_units ||
    d->weather_status != 1 || d->weather_updated <= 0 || d->weather_updated > now + 300 ||
    now - d->weather_updated >= s->weather_refresh*60;
}

bool data_world_stale(const FaceData *d, time_t now) {
  return d->world_updated <= 0 || d->world_updated > now + 300 || now - d->world_updated >= 48*60*60;
}

int data_step_percent(const FaceSettings *s, const FaceData *d) {
  int64_t percent;
  if (!d->health_available || d->steps <= 0 || s->step_goal <= 0) return 0;
  percent = (int64_t)d->steps * 100 / s->step_goal;
  return percent > 100 ? 100 : (int)percent;
}

bool data_apply_weather(FaceData *d, const FaceData *snapshot, bool complete,
                        bool has_updated, time_t now) {
  int status = snapshot->weather_status;
  if ((status == 1 || status == 2) && complete && snapshot->weather_updated > 0 &&
      snapshot->weather_updated <= now + 300 && snapshot->weather_temp >= -200 &&
      snapshot->weather_temp <= 250 && snapshot->weather_high >= -200 && snapshot->weather_high <= 250 &&
      snapshot->weather_low >= -200 && snapshot->weather_low <= 250 &&
      snapshot->weather_low <= snapshot->weather_high && snapshot->weather_code >= 0 &&
      snapshot->weather_code <= 99 && snapshot->weather_unit >= 0 && snapshot->weather_unit <= 1) {
    d->weather_temp = snapshot->weather_temp; d->weather_high = snapshot->weather_high;
    d->weather_low = snapshot->weather_low; d->weather_code = snapshot->weather_code;
    d->weather_unit = snapshot->weather_unit; d->weather_status = status;
    d->weather_updated = snapshot->weather_updated;
    memcpy(d->weather_location, snapshot->weather_location, sizeof(d->weather_location));
    d->weather_location[sizeof(d->weather_location)-1] = '\0';
    return true;
  }
  if (status == 0 || status == 2) {
    d->weather_status = status;
    if (has_updated && snapshot->weather_updated == 0) {
      d->weather_updated = 0;
      d->weather_location[0] = '\0';
    }
    return true;
  }
  return false;
}

bool data_apply_world(FaceData *d, int32_t offset, time_t updated, bool has_offset, time_t now) {
  if (updated == 0) { d->world_updated = 0; return true; }
  if (!has_offset || updated < 0 || updated > now + 300 || offset < -840 || offset > 840) return false;
  d->world_offset = offset;
  d->world_updated = updated;
  return true;
}

int data_rotating_row(const FaceSettings *s, const struct tm *local) {
  if (s->row1 == FACE_ROW_NONE) return s->row2;
  if (s->row2 == FACE_ROW_NONE || s->rotate_seconds == 0 || local == NULL) return s->row1;
  return ((local->tm_hour*3600 + local->tm_min*60 + local->tm_sec) / s->rotate_seconds) % 2 ? s->row2 : s->row1;
}

uint32_t data_rotation_delay(const FaceSettings *s, const struct tm *local, bool focused) {
  int seconds;
  if (!focused || !local || s->rotate_seconds <= 0 || s->row1 == FACE_ROW_NONE ||
      s->row2 == FACE_ROW_NONE || s->row1 == s->row2) return 0;
  seconds = local->tm_hour*3600 + local->tm_min*60 + local->tm_sec;
  return (uint32_t)(s->rotate_seconds - seconds % s->rotate_seconds)*1000;
}

bool data_animation_allowed(const FaceSettings *s, const FaceData *d,
                            const struct tm *local, bool focused, bool system_quiet) {
  return focused && !system_quiet && !settings_quiet_now(s, local) &&
    !s->reduced_motion && s->animation_mode != FACE_ANIMATION_STILL &&
    s->character_activity != 0 &&
    (s->low_battery_cutoff == 0 || d->charging || d->battery_percent > s->low_battery_cutoff);
}

uint32_t data_animation_interval(const FaceSettings *s) {
  if (s->animation_speed != 0) return (uint32_t)s->animation_speed;
  switch (s->animation_mode) {
    case FACE_ANIMATION_ARCADE: return 600;
    case FACE_ANIMATION_RELAXED: return 2000;
    default: return 1000;
  }
}

unsigned data_runtime_update(FaceRuntime *r, const FaceSettings *s, const FaceData *d,
                             const struct tm *local, bool focused, bool system_quiet,
                             enum FaceRuntimeEvent event) {
  unsigned effects = FACE_EFFECT_NONE;
  bool allowed = data_animation_allowed(s, d, local, focused, system_quiet);
  bool begin = event == FACE_RUNTIME_RESTART || event == FACE_RUNTIME_MINUTE || !r->permitted;
  if (event == FACE_RUNTIME_TIMER_FAILED) {
    r->timer_pending = r->animate = false;
    r->timer_failed = true;
    return FACE_EFFECT_REDRAW;
  }
  if (event == FACE_RUNTIME_RESTART && r->timer_pending) {
    effects |= FACE_EFFECT_CANCEL;
    r->timer_pending = false;
  }
  if (begin) r->timer_failed = false;
  if (!allowed) {
    if (r->timer_pending && event != FACE_RUNTIME_BEAT) effects |= FACE_EFFECT_CANCEL;
    if (r->animate) effects |= FACE_EFFECT_REDRAW;
    r->animate = r->timer_pending = r->permitted = false;
    return effects;
  }
  r->permitted = true;
  if (event == FACE_RUNTIME_BEAT) {
    if (!r->timer_pending) return effects;
    r->timer_pending = false;
    ++r->frame;
    effects |= FACE_EFFECT_REDRAW;
  }
  if (!r->timer_pending && !r->timer_failed) {
    r->animate = r->timer_pending = true;
    effects |= FACE_EFFECT_START | FACE_EFFECT_REDRAW;
  }
  return effects;
}

unsigned data_request_take(FaceRequestQueue *queue, time_t now, bool ready) {
  if (!ready || queue->busy || queue->pending == 0 || now < queue->next_attempt) return 0;
  queue->inflight = queue->pending;
  queue->pending = 0;
  queue->busy = true;
  queue->next_attempt = now + 60;
  return queue->inflight;
}

void data_request_complete(FaceRequestQueue *queue, bool success) {
  if (!success) queue->pending |= queue->inflight;
  queue->inflight = 0;
  queue->busy = false;
}

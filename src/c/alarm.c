#include "alarm.h"
#include "clock.h"

enum { KEY_WAKEUP = 3 };
static WakeupId s_id = -1;
static Settings s_settings;
static AlarmStatus s_status;
static void (*s_ring)(void);

const AlarmStatus *alarm_status(void) { return &s_status; }

void alarm_refresh(const Settings *settings, time_t now) {
  time_t desired, existing, original;
  WakeupId scheduled;
  s_settings = *settings;
  s_status = (AlarmStatus){0};
  if (!settings->alarm_on) {
    /* All wakeups in this app belong to its single daily alarm. Also recovers
     * orphan events if the ID record was lost or corrupted. */
    wakeup_cancel_all();
    s_id = -1;
    persist_delete(KEY_WAKEUP);
    return;
  }
  desired = clock_next_alarm(now, settings->alarm_hour, settings->alarm_minute);
  if (desired == (time_t)-1) { s_status.error = E_INVALID_ARGUMENT; return; }
  if (s_id >= 0 && wakeup_query(s_id, &existing)) {
    /* Keep a due event until its callback, including the extra minute after
     * a scheduling clash. A minute tick must not cancel it before delivery. */
    original = clock_next_alarm(existing - 120, settings->alarm_hour, settings->alarm_minute);
    if (existing >= now - 60 && existing <= desired + 60 &&
        (existing == original || existing == original + 60)) {
      s_status.next = existing;
      s_status.adjusted = existing != original;
      return;
    }
  }
  wakeup_cancel_all();
  s_id = -1;
  persist_delete(KEY_WAKEUP);
  scheduled = wakeup_schedule(desired, ALARM_COOKIE, true);
  if (scheduled == E_RANGE) {
    desired += 60;
    scheduled = wakeup_schedule(desired, ALARM_COOKIE, true);
    s_status.adjusted = scheduled >= 0;
  }
  if (scheduled < 0) { s_status.error = scheduled; return; }
  if (persist_write_int(KEY_WAKEUP, scheduled) != (int)sizeof(int32_t)) {
    wakeup_cancel(scheduled);
    s_status.adjusted = false;
    s_status.error = E_OUT_OF_STORAGE;
    return;
  }
  s_id = scheduled;
  s_status.next = desired;
  APP_LOG(APP_LOG_LEVEL_INFO, "alarm scheduled id %ld at %ld adjusted %d",
          (long)s_id, (long)desired, s_status.adjusted);
}

static void wakeup_handler(WakeupId id, int32_t cookie) {
  if (id != s_id || cookie != ALARM_COOKIE || !s_settings.alarm_on) return;
  s_id = -1;
  persist_delete(KEY_WAKEUP);
  alarm_refresh(&s_settings, time(NULL));
  APP_LOG(APP_LOG_LEVEL_INFO, "alarm fired while open");
  if (s_ring != NULL) s_ring();
}

void alarm_init(const Settings *settings, void (*ring)(void)) {
  WakeupId launched_id;
  int32_t cookie;
  bool launched;
  s_ring = ring;
  s_id = persist_get_size(KEY_WAKEUP) == sizeof(int32_t) ? persist_read_int(KEY_WAKEUP) : -1;
  launched = wakeup_get_launch_event(&launched_id, &cookie) &&
             cookie == ALARM_COOKIE && settings->alarm_on;
  if (launched) s_id = -1;
  wakeup_service_subscribe(wakeup_handler);
  alarm_refresh(settings, time(NULL));
  if (launched && s_ring != NULL) {
    APP_LOG(APP_LOG_LEVEL_INFO, "alarm fired from closed app");
    s_ring();
  }
}

void alarm_deinit(void) {
  wakeup_service_subscribe(NULL);
  s_ring = NULL;
}

#ifndef POPEYE_GW_ALARM_H
#define POPEYE_GW_ALARM_H
#include <pebble.h>
#include "store.h"

#define ALARM_COOKIE INT32_C(0x50475701)

typedef struct {
  time_t next;
  int error;
  bool adjusted;
} AlarmStatus;

/* One owned wakeup; the persisted ID survives app exit and updates.
 * The callback is for actual alarms, including a wakeup launch. */
void alarm_init(const Settings *settings, void (*ring)(void));
void alarm_refresh(const Settings *settings, time_t now);
const AlarmStatus *alarm_status(void);
/* Leaves the scheduled wakeup intact on exit. */
void alarm_deinit(void);
#endif

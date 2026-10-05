#ifndef TEST_PEBBLE_H
#define TEST_PEBBLE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
typedef int32_t WakeupId;
typedef void (*WakeupHandler)(WakeupId, int32_t);
enum { E_INVALID_ARGUMENT = -4, E_OUT_OF_STORAGE = -6, E_RANGE = -8, E_DOES_NOT_EXIST = -9 };
#define APP_LOG_LEVEL_INFO 1
#define APP_LOG(...) test_log(__VA_ARGS__)
void test_log(int level, const char *format, ...);
int persist_get_size(uint32_t key);
int persist_read_data(uint32_t key, void *data, size_t size);
int persist_write_data(uint32_t key, const void *data, size_t size);
int32_t persist_read_int(uint32_t key);
int32_t persist_write_int(uint32_t key, int32_t value);
int32_t persist_delete(uint32_t key);
void wakeup_service_subscribe(WakeupHandler handler);
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify);
void wakeup_cancel(WakeupId id);
void wakeup_cancel_all(void);
bool wakeup_get_launch_event(WakeupId *id, int32_t *cookie);
bool wakeup_query(WakeupId id, time_t *timestamp);
/* Feedback adapter surface. */
typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data);
void app_timer_cancel(AppTimer *timer);
uint16_t time_ms(time_t *seconds, uint16_t *milliseconds);
typedef struct { const uint32_t *durations; uint32_t num_segments; } VibePattern;
bool quiet_time_is_active(void);
void vibes_cancel(void);
void vibes_short_pulse(void);
void vibes_long_pulse(void);
void vibes_enqueue_custom_pattern(VibePattern pattern);
#endif

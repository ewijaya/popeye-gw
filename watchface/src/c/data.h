#ifndef POPEYE_FACE_DATA_H
#define POPEYE_FACE_DATA_H

#include "settings.h"

void data_format_date(char *output, size_t length, const FaceSettings *settings,
                      const struct tm *local);
bool data_world_time(const FaceData *data, time_t now, struct tm *world);
void data_format_world(char *output, size_t length, const FaceSettings *settings,
                       const FaceData *data, time_t now, bool watch_24h);
bool data_weather_stale(const FaceSettings *settings, const FaceData *data, time_t now);
bool data_world_stale(const FaceData *data, time_t now);
int data_step_percent(const FaceSettings *settings, const FaceData *data);
bool data_apply_weather(FaceData *data, const FaceData *snapshot, bool complete,
                        bool has_updated, time_t now);
bool data_apply_world(FaceData *data, int32_t offset, time_t updated,
                      bool has_offset, time_t now);
int data_rotating_row(const FaceSettings *settings, const struct tm *local);
uint32_t data_rotation_delay(const FaceSettings *settings, const struct tm *local,
                              bool focused);

/* Timer effects are applied by main.c; the reducer allocates nothing and never
 * advances more than one frame per callback, even after a long interruption. */
typedef struct {
  uint32_t frame;
  bool animate, timer_pending, timer_failed, permitted;
  unsigned classic_remaining;
} FaceRuntime;
enum FaceRuntimeEvent { FACE_RUNTIME_CHECK, FACE_RUNTIME_RESTART,
  FACE_RUNTIME_MINUTE, FACE_RUNTIME_BEAT, FACE_RUNTIME_TIMER_FAILED };
enum FaceRuntimeEffect { FACE_EFFECT_NONE = 0, FACE_EFFECT_CANCEL = 1,
  FACE_EFFECT_START = 2, FACE_EFFECT_REDRAW = 4 };
bool data_animation_allowed(const FaceSettings *settings, const FaceData *data,
                            const struct tm *local, bool focused, bool system_quiet);
uint32_t data_animation_interval(const FaceSettings *settings);
unsigned data_runtime_update(FaceRuntime *runtime, const FaceSettings *settings,
                             const FaceData *data, const struct tm *local,
                             bool focused, bool system_quiet,
                             enum FaceRuntimeEvent event);

typedef struct {
  unsigned pending, inflight;
  bool busy;
  time_t next_attempt;
} FaceRequestQueue;
/* A failed begin/send/callback restores the same request mask. Attempts are
 * bounded to once a minute, including attempts rejected before transmission. */
unsigned data_request_take(FaceRequestQueue *queue, time_t now, bool ready);
void data_request_complete(FaceRequestQueue *queue, bool success);

#endif

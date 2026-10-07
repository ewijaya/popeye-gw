#ifndef POPEYE_GW_FACE_H
#define POPEYE_GW_FACE_H

#include "clock.h"

/* First and last demonstration frame. The app's attract cycle is six 2 s beats;
 * beat 0 is plain idle, so the face starts at the throw and ends on the catch. */
#define FACE_DEMO_FIRST 1
#define FACE_DEMO_LAST 5
#define FACE_DEMO_BEAT_MS 2000u

typedef struct {
  int starts[4], widths[4];
  int colon_x, meridiem_x, right;
} FaceClockLayout;

/* Large clock geometry on the 200px screen. Center the digits and optional
 * 11px AM/PM label together; meridiem_x is -1 for a 24-hour scene. */
void face_clock_layout(const Scene *scene, FaceClockLayout *layout);

/* frame < 0 is the static clock; FACE_DEMO_FIRST..FACE_DEMO_LAST show that beat
 * of the food demonstration. Pure: no platform calls, wall clock is the caller's. */
void face_scene(Scene *scene, const struct tm *local, bool style_24h, int frame);

/* Continuous, stepped LCD cycle. Activity bits select motion, never visibility:
 * Olive=1, Popeye=2, Brutus=4. Pause adds quiet beats after each food catch.
 * The caller advances frame at the selected cadence; no timer lives here. */
void face_scene_active(Scene *scene, const struct tm *local, bool style_24h,
                       uint32_t frame, unsigned activity, bool celebration,
                       unsigned pause);

#endif

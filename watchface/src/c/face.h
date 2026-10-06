#ifndef POPEYE_GW_FACE_H
#define POPEYE_GW_FACE_H

#include "clock.h"

/* First and last demonstration frame. The app's attract cycle is six 2 s beats;
 * beat 0 is plain idle, so the face starts at the throw and ends on the catch. */
#define FACE_DEMO_FIRST 1
#define FACE_DEMO_LAST 5
#define FACE_DEMO_BEAT_MS 2000u

/* frame < 0 is the static clock; FACE_DEMO_FIRST..FACE_DEMO_LAST show that beat
 * of the food demonstration. Pure: no platform calls, wall clock is the caller's. */
void face_scene(Scene *scene, const struct tm *local, bool style_24h, int frame);

#endif

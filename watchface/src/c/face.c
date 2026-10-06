#include "face.h"

void face_scene(Scene *scene, const struct tm *local, bool style_24h, int frame) {
  struct tm shown = *local;
  if (frame < FACE_DEMO_FIRST || frame > FACE_DEMO_LAST) {
    clock_scene(scene, &shown, style_24h, false, false, false);
    return;
  }
  /* clock_scene derives the demo beat from the seconds. An even second keeps the
   * colon lit and selects the requested beat; hour and minute stay real, so the
   * food lane still follows the minute exactly as in the app. */
  shown.tm_sec = frame * 2;
  clock_scene(scene, &shown, style_24h, true, false, false);
}

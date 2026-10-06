#include "glance.h"

#include <stdio.h>

void glance_text(char *out, size_t size, const HighScores *scores, const Settings *settings,
                 bool style_24h) {
  int used;
  if (size == 0u) return;
  if (scores->best[0] == 0u && scores->best[1] == 0u)
    used = snprintf(out, size, "No scores yet");
  else
    used = snprintf(out, size, "Best A %lu / B %lu", (unsigned long)scores->best[0],
                    (unsigned long)scores->best[1]);
  if (!settings->alarm_on || used < 0 || (size_t)used >= size) return;
  if (style_24h)
    snprintf(out + used, size - (size_t)used, " - %02u:%02u", settings->alarm_hour, settings->alarm_minute);
  else
    snprintf(out + used, size - (size_t)used, " - %u:%02u %s",
             settings->alarm_hour % 12u == 0u ? 12u : settings->alarm_hour % 12u,
             settings->alarm_minute, settings->alarm_hour < 12u ? "AM" : "PM");
}

#ifndef POPEYE_GW_GLANCE_H
#define POPEYE_GW_GLANCE_H

#include "store.h"

/* Launcher subtitle: "Best A 214 / B 187", then " / Daily 43" when Daily was scored on
 * `today` (YYYYMMDD), then " - 07:00" while the alarm is on. Plain ASCII without
 * braces, which App Glance would parse as a template. */
void glance_text(char *out, size_t size, const HighScores *scores, const ModeScores *modes,
                 uint32_t today, const Settings *settings, bool style_24h);

#endif

#ifndef POPEYE_FACE_VIEW_H
#define POPEYE_FACE_VIEW_H

#include <pebble.h>
#include "settings.h"

bool face_view_init(void);
void face_view_deinit(void);
/* Call after init and when settings change. Bitmap recoloring happens here,
 * outside drawing; draw performs no allocation or resource loading. */
void face_view_configure(const FaceSettings *settings);
void face_view_draw(GContext *ctx, GRect bounds, const FaceSettings *settings,
                    const FaceData *data, const struct tm *local,
                    bool watch_24h, uint32_t frame, bool animate);

#endif

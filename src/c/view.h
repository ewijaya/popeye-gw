#ifndef POPEYE_GW_VIEW_H
#define POPEYE_GW_VIEW_H

#include <pebble.h>

#include "scene.h"

typedef enum {
  VIEW_OVERLAY_NONE,
  VIEW_OVERLAY_TITLE,
  VIEW_OVERLAY_PAUSED,
  VIEW_OVERLAY_GAME_OVER
} ViewOverlay;

void view_init(Layer *parent);
void view_deinit(void);
/* Copies the lit segments and requests one redraw. */
void view_show(const Scene *scene, ViewOverlay overlay);

#endif

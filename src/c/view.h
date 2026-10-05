#ifndef POPEYE_GW_VIEW_H
#define POPEYE_GW_VIEW_H

#include <pebble.h>

#include "scene.h"

typedef enum {
  VIEW_OVERLAY_NONE,
  VIEW_OVERLAY_CLOCK,
  VIEW_OVERLAY_PAUSED,
  VIEW_OVERLAY_GAME_OVER,
  VIEW_OVERLAY_ALARM
} ViewOverlay;

typedef struct {
  const char *title;
  char rows[6][32];
  char footer[64];
  unsigned count;
  int selected; /* -1 for informational pages. */
} ViewPanel;

void view_init(Layer *parent, bool ghosts);
void view_deinit(void);
void view_set_landscape(bool landscape, bool buttons_bottom);
/* Copies the lit segments and requests one redraw. */
void view_show(const Scene *scene, ViewOverlay overlay, bool ghosts, bool save_error);
void view_panel(const ViewPanel *panel);

#endif

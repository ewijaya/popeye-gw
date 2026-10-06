#ifndef POPEYE_GW_VIEW_H
#define POPEYE_GW_VIEW_H

#include <pebble.h>

#include "scene.h"
#include "theme.h"

typedef enum {
  VIEW_OVERLAY_NONE,
  VIEW_OVERLAY_CLOCK,
  VIEW_OVERLAY_PAUSED,
  VIEW_OVERLAY_GAME_OVER,
  VIEW_OVERLAY_ALARM,
  VIEW_OVERLAY_BEST_A,
  VIEW_OVERLAY_BEST_B
} ViewOverlay;

typedef struct {
  const char *title;
  char rows[9][32];
  char footer[64];
  unsigned count;
  int selected; /* -1 for informational pages. */
} ViewPanel;

void view_init(Layer *parent, bool ghosts, Theme theme);
void view_deinit(void);
void view_set_landscape(bool landscape, bool buttons_bottom);
/* Copies the lit segments and requests one redraw. */
/* note, if not NULL, replaces the Paused / Game over overlay title (for example "Daily 0:42"). */
void view_show(const Scene *scene, ViewOverlay overlay, const char *note, bool ghosts,
               Theme theme, bool save_error);
void view_panel(const ViewPanel *panel);

#endif

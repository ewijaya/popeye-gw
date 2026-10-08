#ifndef POPEYE_SETTINGS_WIRE_H
#define POPEYE_SETTINGS_WIRE_H
#include "store.h"

/* Menu order: Orientation, Buttons (0 Top/1 Bottom), Swap, Vibrate,
 * Sound (0 Off/1 Low/2 Medium/3 High), Light, Ghosts, Theme, Demo, Online.
 * Snapshot: ten bytes. Patch: little-endian 10-bit mask, then ten values.
 * Only selected values are applied; alarm fields and remembered sound level stay intact. */
#define SETTINGS_WIRE_COUNT 10u
#define SETTINGS_PATCH_SIZE (SETTINGS_WIRE_COUNT + 2u)
void settings_wire_encode(const Settings *settings, uint8_t out[SETTINGS_WIRE_COUNT]);
bool settings_wire_apply(Settings *settings, const uint8_t *patch, size_t length);
#endif

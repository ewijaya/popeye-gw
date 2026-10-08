#include "settings_wire.h"

void settings_wire_encode(const Settings *s, uint8_t out[SETTINGS_WIRE_COUNT]) {
  out[0] = s->landscape;
  out[1] = s->buttons_bottom;
  out[2] = s->swap_buttons;
  out[3] = s->vibration;
  out[4] = s->sound ? s->sound_level + 1u : 0u;
  out[5] = s->light_play;
  out[6] = s->ghosts;
  out[7] = s->theme;
  out[8] = s->attract;
  out[9] = s->online;
}

bool settings_wire_apply(Settings *s, const uint8_t *patch, size_t length) {
  uint16_t mask;
  unsigned i;
  uint8_t values[SETTINGS_WIRE_COUNT];
  if (patch == NULL || length != SETTINGS_PATCH_SIZE) return false;
  mask = (uint16_t)(patch[0] | (uint16_t)patch[1] << 8);
  if ((mask >> SETTINGS_WIRE_COUNT) != 0u) return false;
  settings_wire_encode(s, values);
  for (i = 0; i < SETTINGS_WIRE_COUNT; ++i) {
    if ((mask & (1u << i)) == 0u) continue;
    if (patch[i + 2u] > (i == 4u ? 3u : 1u)) return false;
    values[i] = patch[i + 2u];
  }
  s->landscape = values[0] != 0u;
  s->buttons_bottom = values[1] != 0u;
  s->swap_buttons = values[2] != 0u;
  s->vibration = values[3] != 0u;
  s->sound = values[4] != 0u;
  if (s->sound) s->sound_level = values[4] - 1u;
  s->light_play = values[5] != 0u;
  s->ghosts = values[6] != 0u;
  s->theme = values[7];
  s->attract = values[8] != 0u;
  s->online = values[9] != 0u;
  return true;
}

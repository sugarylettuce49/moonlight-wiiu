#pragma once

#include "../config.h"
#include <xml.h>

enum {
  SETTINGS_ACTIVE,
  SETTINGS_SAVED,
  SETTINGS_CANCELLED,
};

// Opens the on-console settings editor with the current configuration.
// `apps` is the host's app list (may be NULL). The settings menu takes
// ownership of the list and frees it.
void settings_open(PCONFIGURATION config, PAPP_LIST apps);

// Draws the menu and handles input for one frame. When the user saves, the
// changes are written to moonlight.conf and applied to `config` before
// SETTINGS_SAVED is returned.
int settings_update(PCONFIGURATION config);

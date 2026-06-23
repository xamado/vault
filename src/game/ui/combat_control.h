#ifndef FALLOUT_GAME_UI_COMBAT_CONTROL_H_
#define FALLOUT_GAME_UI_COMBAT_CONTROL_H_

#include "game/object_types.h"

// Standalone combat-control / party-AI-customization screen: a self-contained
// bottom panel with its own window, buttons and art. Launched from a conversation
// today (and directly on a critter later). Eventually becomes the "NPC control" UI.
void combat_control_run(Object* critter);

#endif /* FALLOUT_GAME_UI_COMBAT_CONTROL_H_ */

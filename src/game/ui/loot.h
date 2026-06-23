#ifndef FALLOUT_GAME_UI_LOOT_H_
#define FALLOUT_GAME_UI_LOOT_H_

#include "game/object_types.h"

// Standalone loot/steal screen. Fully self-contained: owns its own window,
// item rendering, cursor handling, container navigation, item-action menu,
// quantity picker and screen state. Shares only pure inventory-data operations
// (item_*, skill_*, obj_*, inven_left/right/worn) with the rest of the game.
// Does NOT depend on the inventory screen (inventry.c) for any rendering, so the
// two screens can be changed independently without affecting one another.
//
// a1 - the looter (the player).
// a2 - the container or corpse being looted.
int loot_container(Object* a1, Object* a2);

// Same screen driven in "steal" mode (Steal skill against an active critter).
int inven_steal_container(Object* a1, Object* a2);

#endif /* FALLOUT_GAME_UI_LOOT_H_ */

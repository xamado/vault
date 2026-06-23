#ifndef FALLOUT_GAME_UI_TRADE_H_
#define FALLOUT_GAME_UI_TRADE_H_

#include "game/object_types.h"

// Standalone barter/trade screen. Fully self-contained: owns its own window,
// item rendering, input loop, offer-table scratch objects and barter mechanics.
// Does not depend on the inventory screen (inventry.c).
//
// partner       - the critter being traded with.
// barterMod     - script-supplied barter difficulty modifier.
// partnerIsParty- true when trading with a party member (weight-based instead of
//                 caps-based, no "offer not good enough" gating).
void trade_run(Object* partner, int barterMod, bool partnerIsParty);

#endif /* FALLOUT_GAME_UI_TRADE_H_ */

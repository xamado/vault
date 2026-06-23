#include "game/ui/trade.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "game/art.h"
#include "game/combatai.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/item.h"
#include "game/message.h"
#include "game/object.h"
#include "game/party.h"
#include "game/perk.h"
#include "game/proto.h"
#include "game/proto_types.h"
#include "game/reaction.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/ui.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"

// Native (unscaled) geometry of the barter panel; rendering scales by ui_get_scale().
#define TRADE_SLOT_WIDTH 64
#define TRADE_SLOT_HEIGHT 48

// The number of item rows shown per scroller column.
#define TRADE_CUR_DISP 3

// custom item-description message file (shared "Ok, that's a good trade." etc.)
// lives in inventry's message file in vanilla; trade owns its own load.
static MessageList trade_msg_file;

// ---- Trade state (fully owned here, no inventry.c dependency) ----

// The player (always obj_dude) and the critter being traded with.
static Object* trade_partner;

// The two offer tables: items the player has put up (trade_player_table) and
// items the partner has put up (trade_partner_table). Scratch container objects.
static Object* trade_player_table;
static Object* trade_partner_table;

// Scroll offsets for the four columns.
static int trade_player_inv_offset;  // player's own inventory list
static int trade_partner_inv_offset; // partner's inventory list
static int trade_player_table_offset;
static int trade_partner_table_offset;

static int trade_barter_mod;
static bool trade_is_party;

// Windows: the barter.frm background panel and the item-rendering window.
static int trade_back_win = -1;
static int trade_item_win = -1;

// Absolute screen origin of the item window (for drag drop-zone hit testing).
static int trade_item_x;
static int trade_item_y;

// ---- Barter mechanics (pure game logic, was barter_compute_value/transaction) ----

// Value the partner places on their current offer table (trade_partner_table).
// For party members it's weight-based; for merchants it's caps-adjusted cost
// scaled by the barter-skill ratio and Master Trader perk.
static int trade_compute_value(Object* buyer, Object* npc)
{
    if (trade_is_party) {
        return item_total_weight(trade_partner_table);
    }

    int cost = item_total_cost(trade_partner_table);
    int caps = item_caps_total(trade_partner_table);
    int delta = cost - caps;

    double bonus = 0.0;
    if (buyer == obj_dude) {
        if (perkHasRank(obj_dude, PERK_MASTER_TRADER)) {
            bonus = 25.0;
        }
    }

    int partyBarter = partyMemberHighestSkillLevel(SKILL_BARTER);
    int npcBarter = skill_level(npc, SKILL_BARTER);

    double v1 = (trade_barter_mod + 100.0 - bonus) * 0.01;
    double v2 = (160.0 + npcBarter) / (160.0 + partyBarter) * (delta * 2.0);
    if (v1 < 0) {
        v1 = 0.0099999998;
    }

    return (int)(v1 * v2 + caps);
}

// Returns 0 if the trade was accepted and executed, -1 otherwise.
// buyer        - player (obj_dude)
// playerOffer  - trade_player_table (what the player gives up)
// npc          - the partner
// partnerOffer - trade_partner_table (what the partner gives up)
static int trade_attempt_transaction(Object* buyer, Object* playerOffer, Object* npc, Object* partnerOffer)
{
    MessageListItem messageListItem;

    int freeCarry = critterGetStat(buyer, STAT_CARRY_WEIGHT) - item_total_weight(buyer);
    if (item_total_weight(partnerOffer) > freeCarry) {
        // Sorry, you cannot carry that much.
        messageListItem.num = 31;
        if (message_search(&trade_msg_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return -1;
    }

    if (trade_is_party) {
        int partnerFreeCarry = critterGetStat(npc, STAT_CARRY_WEIGHT) - item_total_weight(npc);
        if (item_total_weight(playerOffer) > partnerFreeCarry) {
            // Sorry, that's too much to carry.
            messageListItem.num = 32;
            if (message_search(&trade_msg_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
            return -1;
        }
    } else {
        bool reject = false;
        if (playerOffer->data.inventory.length == 0) {
            reject = true;
        } else if (item_queued(playerOffer)) {
            if (playerOffer->pid != PROTO_ID_GEIGER_COUNTER_I || item_m_turn_off(playerOffer) == -1) {
                reject = true;
            }
        }

        if (!reject) {
            int offered = item_total_cost(playerOffer);
            if (trade_compute_value(buyer, npc) > offered) {
                reject = true;
            }
        }

        if (reject) {
            // No, your offer is not good enough.
            messageListItem.num = 28;
            if (message_search(&trade_msg_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
            return -1;
        }
    }

    item_move_all(partnerOffer, buyer);
    item_move_all(playerOffer, npc);
    return 0;
}

// ---- Item-slot rendering ----

// Renders an item's quantity/ammo text onto a slot (was display_inventory_info).
static void trade_display_item_info(Object* item, int quantity, unsigned char* dest, int pitch, bool isMoving)
{
    int oldFont = text_curr();
    text_font(101);

    char formattedText[12];
    bool draw = false;

    if (item_get_type(item) == ITEM_TYPE_AMMO) {
        int ammoQuantity = item_w_max_ammo(item) * (quantity - 1);
        if (!isMoving) {
            ammoQuantity += item_w_curr_ammo(item);
        }
        if (ammoQuantity > 99999) {
            ammoQuantity = 99999;
        }
        sprintf(formattedText, "x%d", ammoQuantity);
        draw = true;
    } else if (quantity > 1) {
        int shown = quantity;
        if (isMoving) {
            shown -= 1;
        }
        if (shown > 99999) {
            shown = 99999;
        }
        sprintf(formattedText, "x%d", shown);
        draw = true;
    }

    if (draw) {
        text_to_buf(dest, formattedText, 80, pitch, colorTable[32767]);
    }

    text_font(oldFont);
}

// Renders the player's own inventory column (was display_inventory TRADE branch).
static void trade_display_player_inventory(int offset, int movingIndex)
{
    const int pitch = 480;
    unsigned char* windowBuffer = win_get_buf(trade_item_win);

    // Clear the scroll view from the background panel.
    buf_to_buf(win_get_buf(trade_back_win) + (35 * win_width(trade_back_win) + 100) * 4,
        64, 48 * TRADE_CUR_DISP, win_width(trade_back_win),
        windowBuffer + (pitch * 35 + 20) * 4, pitch);

    Inventory* inventory = &(obj_dude->data.inventory);
    int y = 0;
    for (int index = 0; index + offset < inventory->length && index < TRADE_CUR_DISP; index += 1) {
        InventoryItem* inventoryItem = &(inventory->items[inventory->length - (index + offset + 1)]);
        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + (pitch * (y + 39) + 26) * 4, 59, 40, pitch);
        trade_display_item_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + (pitch * (y + 39) + 28) * 4, pitch, index == movingIndex);
        y += 48;
    }

    win_draw(trade_item_win);
}

// Renders the partner's inventory column (was display_target_inventory TRADE branch).
static void trade_display_partner_inventory(int offset, int movingIndex)
{
    const int pitch = 480;
    unsigned char* windowBuffer = win_get_buf(trade_item_win);

    buf_to_buf(win_get_buf(trade_back_win) + (win_width(trade_back_win) * 35 + 475) * 4,
        64, 48 * TRADE_CUR_DISP, win_width(trade_back_win),
        windowBuffer + (pitch * 35 + 395) * 4, pitch);

    Inventory* inventory = &(trade_partner->data.inventory);
    int y = 0;
    for (int index = 0; index < TRADE_CUR_DISP; index += 1) {
        int slot = offset + index;
        if (slot >= inventory->length) {
            break;
        }
        InventoryItem* inventoryItem = &(inventory->items[inventory->length - (slot + 1)]);
        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + (pitch * (y + 39) + 397) * 4, 56, 40, pitch);
        trade_display_item_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + (pitch * (y + 39) + 397) * 4, pitch, index == movingIndex);
        y += 48;
    }
}

// Renders the two offer tables and their value totals (was display_table_inventories).
// Pass the table objects to redraw; NULL to skip a side.
static void trade_display_tables(Object* playerTable, Object* partnerTable, int movingIndex)
{
    const int pitch = 480;
    unsigned char* windowBuffer = win_get_buf(trade_item_win);

    int oldFont = text_curr();
    text_font(101);

    char formattedText[80];
    int columnHeight = text_height() + 48 * TRADE_CUR_DISP;

    if (playerTable != NULL) {
        unsigned char* src = win_get_buf(trade_back_win);
        buf_to_buf(src + (win_width(trade_back_win) * 20 + 249) * 4, 64, columnHeight + 1, win_width(trade_back_win),
            windowBuffer + (pitch * 20 + 169) * 4, pitch);

        unsigned char* dest = windowBuffer + (pitch * 24 + 169) * 4;
        Inventory* inventory = &(playerTable->data.inventory);
        for (int index = 0; index < TRADE_CUR_DISP && index + trade_player_table_offset < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[inventory->length - (index + trade_player_table_offset + 1)]);
            scale_art(item_inv_fid(inventoryItem->item), dest, 56, 40, pitch);
            trade_display_item_info(inventoryItem->item, inventoryItem->quantity, dest, pitch, index == movingIndex);
            dest += pitch * 48 * 4;
        }

        if (trade_is_party) {
            MessageListItem messageListItem;
            messageListItem.num = 30;
            if (message_search(&trade_msg_file, &messageListItem)) {
                sprintf(formattedText, "%s %d", messageListItem.text, item_total_weight(playerTable));
            }
        } else {
            sprintf(formattedText, "$%d", item_total_cost(playerTable));
        }
        text_to_buf(windowBuffer + (pitch * (48 * TRADE_CUR_DISP + 24) + 169) * 4, formattedText, 80, pitch, colorTable[32767]);

        Rect rect = { 169, 24, 223, 24 + columnHeight };
        win_draw_rect(trade_item_win, &rect);
    }

    if (partnerTable != NULL) {
        unsigned char* src = win_get_buf(trade_back_win);
        buf_to_buf(src + (win_width(trade_back_win) * 20 + 334) * 4, 64, columnHeight + 1, win_width(trade_back_win),
            windowBuffer + (pitch * 20 + 254) * 4, pitch);

        unsigned char* dest = windowBuffer + (pitch * 24 + 254) * 4;
        Inventory* inventory = &(partnerTable->data.inventory);
        for (int index = 0; index < TRADE_CUR_DISP && index + trade_partner_table_offset < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[inventory->length - (index + trade_partner_table_offset + 1)]);
            scale_art(item_inv_fid(inventoryItem->item), dest, 56, 40, pitch);
            trade_display_item_info(inventoryItem->item, inventoryItem->quantity, dest, pitch, index == movingIndex);
            dest += pitch * 48 * 4;
        }

        if (trade_is_party) {
            MessageListItem messageListItem;
            messageListItem.num = 30;
            if (message_search(&trade_msg_file, &messageListItem)) {
                sprintf(formattedText, "%s %d", messageListItem.text, trade_compute_value(obj_dude, trade_partner));
            }
        } else {
            sprintf(formattedText, "$%d", trade_compute_value(obj_dude, trade_partner));
        }
        text_to_buf(windowBuffer + (pitch * (48 * TRADE_CUR_DISP + 24) + 254) * 4, formattedText, 80, pitch, colorTable[32767]);

        Rect rect = { 254, 24, 318, 24 + columnHeight };
        win_draw_rect(trade_item_win, &rect);
    }

    text_font(oldFont);
}

// ---- Equipped-item queries (reimplemented to avoid an inventry.c dependency) ----

static Object* trade_find_flagged(Object* critter, int flag)
{
    Inventory* inventory = &(critter->data.inventory);
    for (int i = 0; i < inventory->length; i++) {
        Object* item = inventory->items[i].item;
        if (item->flags & flag) {
            return item;
        }
    }
    return NULL;
}

static Object* trade_find_weapon(Object* critter)
{
    Inventory* inventory = &(critter->data.inventory);
    for (int i = 0; i < inventory->length; i++) {
        Object* item = inventory->items[i].item;
        if (item_get_type(item) == ITEM_TYPE_WEAPON) {
            return item;
        }
    }
    return NULL;
}

// ---- Setup / teardown ----

static void trade_refresh_all(void)
{
    trade_display_partner_inventory(trade_partner_inv_offset, -1);
    trade_display_player_inventory(trade_player_inv_offset, -1);
    trade_display_tables(trade_player_table, trade_partner_table, -1);
}

// ---- Drag and drop ----

// A column's drop zone, in absolute screen coordinates. Item-window-relative
// origins: player inv x29, partner inv x395, player table x165, partner table x250.
// Inventory columns start at y35, offer tables at y20; both are TRADE_CUR_DISP rows tall.
static bool trade_dropped_in(int relX, int relY)
{
    return mouse_click_in(trade_item_x + relX, trade_item_y + relY,
        trade_item_x + relX + TRADE_SLOT_WIDTH, trade_item_y + relY + TRADE_SLOT_HEIGHT * TRADE_CUR_DISP);
}

// Picks `item` up onto the cursor, waits for the left button to be released,
// then restores the pointer. The caller checks the drop zone afterwards.
static void trade_pickup_drag(Object* item)
{
    CacheEntry* handle;
    Art* frm = art_ptr_lock(item_inv_fid(item), &handle);
    if (frm != NULL) {
        int w = art_frame_width(frm, 0, 0);
        int h = art_frame_length(frm, 0, 0);
        mouse_set_shape(art_frame_data(frm, 0, 0), w, h, w, w / 2, h / 2, 0);
        gsound_play_sfx_file("ipickup1");
    }

    do {
        get_input();
    } while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (frm != NULL) {
        art_ptr_unlock(handle);
        gsound_play_sfx_file("iputdown");
    }

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
}

// Creates the barter panel + item window + slot/offer buttons + offer tables.
// Returns 0 on success, -1 on failure.
static int trade_setup(void)
{
    int frmId = trade_is_party ? 420 : 111; // trade.frm / barter.frm

    CacheEntry* handle;
    Art* frm = art_ptr_lock(art_id(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0), &handle);
    if (frm == NULL) {
        return -1;
    }

    // The item-column offsets (100/475/249/334 within the panel) assume a fixed
    // 640-wide panel with the frm scaled to fill it — match that. Using the frm's
    // native width here would put the partner column's source read out of bounds.
    const int panelW = 640;
    int panelH = art_frame_length(frm, 0, 0);

    int screenW = scr_size.lrx - scr_size.ulx + 1;
    int screenH = scr_size.lry - scr_size.uly + 1;
    int panelX = (screenW - panelW) / 2;
    int panelY = screenH - panelH;

    // Background panel (native size for now; scaling is a follow-up).
    // WINDOW_FLAG_0x02 = "don't raise on show": the m/t buttons live on this
    // window, and without the flag clicking them would raise the panel above the
    // item window, hiding the items. This is how the original barter avoided it.
    trade_back_win = win_add(panelX, panelY, panelW, panelH, 256, WINDOW_FLAG_0x02);
    if (trade_back_win == -1) {
        art_ptr_unlock(handle);
        return -1;
    }
    ui_image_32(frm, trade_back_win, 0, 0, panelW, panelH);
    win_draw(trade_back_win);

    int itemH = panelH < 180 ? panelH : 180;

    // Item window sits 80px into the panel (matches the legacy trade layout).
    trade_item_x = panelX + 80;
    trade_item_y = panelY;
    trade_item_win = win_add(trade_item_x, trade_item_y, 480, itemH, 257, 0);
    if (trade_item_win == -1) {
        art_ptr_unlock(handle);
        return -1;
    }
    buf_to_buf(win_get_buf(trade_back_win) + 80 * 4, 480, itemH, panelW, win_get_buf(trade_item_win), 480);
    art_ptr_unlock(handle);

    // Invisible slot buttons (emit event codes consumed by the loop).
    for (int index = 0; index < TRADE_CUR_DISP; index++) {
        win_register_button(trade_item_win, 29, 35 + 48 * index, 64, 48, 1000 + index, -1, 1000 + index, -1, NULL, NULL, NULL, 0);
        win_register_button(trade_item_win, 395, 35 + 48 * index, 64, 48, 2000 + index, -1, 2000 + index, -1, NULL, NULL, NULL, 0);
        win_register_button(trade_item_win, 165, 20 + 48 * index, 64, 48, 2300 + index, -1, 2300 + index, -1, NULL, NULL, NULL, 0);
        win_register_button(trade_item_win, 250, 20 + 48 * index, 64, 48, 2400 + index, -1, 2400 + index, -1, NULL, NULL, NULL, 0);
    }

    // Invisible OFFER (m) / LEAVE (t) buttons over the panel's red-button spots.
    // TODO: render the red-button art; keyboard m/t also work.
    win_register_button(trade_back_win, 41, 163, 14, 14, -1, -1, -1, KEY_LOWERCASE_M, NULL, NULL, NULL, 0);
    win_register_button(trade_back_win, 584, 162, 14, 14, -1, -1, -1, KEY_LOWERCASE_T, NULL, NULL, NULL, 0);

    // Offer-table scratch containers.
    if (obj_new(&trade_player_table, -1, -1) == -1) {
        return -1;
    }
    trade_player_table->flags |= OBJECT_HIDDEN;
    if (obj_new(&trade_partner_table, -1, -1) == -1) {
        return -1;
    }
    trade_partner_table->flags |= OBJECT_HIDDEN;

    return 0;
}

static void trade_teardown(void)
{
    obj_erase_object(trade_partner_table, NULL);
    obj_erase_object(trade_player_table, NULL);

    if (trade_item_win != -1) {
        win_delete(trade_item_win);
        trade_item_win = -1;
    }
    if (trade_back_win != -1) {
        win_delete(trade_back_win);
        trade_back_win = -1;
    }

    cai_attempt_w_reload(trade_partner, 0);
}

// ---- Public entry point ----

void trade_run(Object* partner, int barterMod, bool partnerIsParty)
{
    trade_partner = partner;
    trade_barter_mod = barterMod;
    trade_is_party = partnerIsParty;
    trade_player_inv_offset = 0;
    trade_partner_inv_offset = 0;
    trade_player_table_offset = 0;
    trade_partner_table_offset = 0;

    if (!message_init(&trade_msg_file)) {
        return;
    }
    if (!message_load(&trade_msg_file, "game\\inventry.msg")) {
        message_exit(&trade_msg_file);
        return;
    }

    // Suppress map tile repaints for the whole trade. item moves call
    // tile_refresh_rect() to update on-map critters' sprites, which would paint
    // the map straight over our bottom-of-screen panel (vanilla never saw this:
    // its trade lived inside a full-screen opaque dialog that hid the map).
    tile_disable_refresh();

    // Strip the partner's equipped items so they can't be bartered away; restore
    // them on exit.
    Object* partnerArmor = trade_find_flagged(partner, OBJECT_WORN);
    if (partnerArmor != NULL) {
        item_remove_mult(partner, partnerArmor, 1);
    }

    Object* partnerWeapon = NULL;
    Object* partnerRightHand = trade_find_flagged(partner, OBJECT_IN_RIGHT_HAND);
    if (partnerRightHand != NULL) {
        item_remove_mult(partner, partnerRightHand, 1);
    } else if (!partnerIsParty) {
        partnerWeapon = trade_find_weapon(partner);
        if (partnerWeapon != NULL) {
            item_remove_mult(partner, partnerWeapon, 1);
        }
    }

    if (trade_setup() != 0) {
        trade_teardown();
        message_exit(&trade_msg_file);
        return;
    }

    trade_refresh_all();

    int modifier;
    switch (reaction_to_level(reaction_get(partner))) {
    case NPC_REACTION_BAD:
        modifier = 25;
        break;
    case NPC_REACTION_GOOD:
        modifier = -15;
        break;
    default:
        modifier = 0;
        break;
    }

    int keyCode = -1;
    for (;;) {
        if (keyCode == KEY_ESCAPE || game_user_wants_to_quit != 0) {
            break;
        }

        keyCode = get_input();
        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            game_quit_with_confirm();
        }
        if (game_user_wants_to_quit != 0) {
            break;
        }

        trade_barter_mod = barterMod + modifier;

        if (keyCode == KEY_LOWERCASE_T || modifier <= -30) {
            break;
        } else if (keyCode == KEY_LOWERCASE_M) {
            if (trade_player_table->data.inventory.length != 0 || trade_partner_table->data.inventory.length != 0) {
                if (trade_attempt_transaction(obj_dude, trade_player_table, partner, trade_partner_table) == 0) {
                    trade_refresh_all();
                    if (!partnerIsParty) {
                        // Ok, that's a good trade.
                        MessageListItem messageListItem;
                        messageListItem.num = 27;
                        if (message_search(&trade_msg_file, &messageListItem)) {
                            display_print(messageListItem.text);
                        }
                    }
                }
            }
        } else if (keyCode == KEY_ARROW_UP) {
            if (trade_player_inv_offset > 0) {
                trade_player_inv_offset -= 1;
                trade_display_player_inventory(trade_player_inv_offset, -1);
            }
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (trade_player_inv_offset + TRADE_CUR_DISP < obj_dude->data.inventory.length) {
                trade_player_inv_offset += 1;
                trade_display_player_inventory(trade_player_inv_offset, -1);
            }
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            if (trade_partner_inv_offset > 0) {
                trade_partner_inv_offset -= 1;
                trade_display_partner_inventory(trade_partner_inv_offset, -1);
                win_draw(trade_item_win);
            }
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            if (trade_partner_inv_offset + TRADE_CUR_DISP < partner->data.inventory.length) {
                trade_partner_inv_offset += 1;
                trade_display_partner_inventory(trade_partner_inv_offset, -1);
                win_draw(trade_item_win);
            }
        } else if (keyCode == KEY_PAGE_UP) {
            if (trade_player_table_offset > 0) {
                trade_player_table_offset -= 1;
                trade_display_tables(trade_player_table, trade_partner_table, -1);
            }
        } else if (keyCode == KEY_PAGE_DOWN) {
            if (trade_player_table_offset + TRADE_CUR_DISP < trade_player_table->data.inventory.length) {
                trade_player_table_offset += 1;
                trade_display_tables(trade_player_table, trade_partner_table, -1);
            }
        } else if (keyCode == KEY_CTRL_PAGE_UP) {
            if (trade_partner_table_offset > 0) {
                trade_partner_table_offset -= 1;
                trade_display_tables(trade_player_table, trade_partner_table, -1);
            }
        } else if (keyCode == KEY_CTRL_PAGE_DOWN) {
            if (trade_partner_table_offset + TRADE_CUR_DISP < trade_partner_table->data.inventory.length) {
                trade_partner_table_offset += 1;
                trade_display_tables(trade_player_table, trade_partner_table, -1);
            }
        } else if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
            // Slot buttons emit their event code on hover (mouse-enter) too, so
            // only start a drag when the left button is actually pressed. Drag the
            // item onto the cursor; on release, move it if dropped over the valid
            // target column.
            // TODO: stack quantity picker (do_move_timer); currently moves whole stacks.
            if (keyCode >= 1000 && keyCode < 1000 + TRADE_CUR_DISP) {
                // Player inventory -> drop on player offer table (rel x165,y20).
                Inventory* inv = &(obj_dude->data.inventory);
                int slot = (keyCode - 1000) + trade_player_inv_offset;
                if (slot < inv->length) {
                    Object* item = inv->items[inv->length - (slot + 1)].item;
                    int quantity = inv->items[inv->length - (slot + 1)].quantity;
                    trade_pickup_drag(item);
                    if (trade_dropped_in(165, 20)) {
                        item_move_force(obj_dude, trade_player_table, item, quantity);
                    }
                    trade_refresh_all();
                }
            } else if (keyCode >= 2000 && keyCode < 2000 + TRADE_CUR_DISP) {
                // Partner inventory -> drop on partner offer table (rel x250,y20).
                Inventory* inv = &(partner->data.inventory);
                int slot = (keyCode - 2000) + trade_partner_inv_offset;
                if (slot < inv->length) {
                    Object* item = inv->items[inv->length - (slot + 1)].item;
                    int quantity = inv->items[inv->length - (slot + 1)].quantity;
                    trade_pickup_drag(item);
                    if (trade_dropped_in(250, 20)) {
                        item_move_force(partner, trade_partner_table, item, quantity);
                    }
                    trade_refresh_all();
                }
            } else if (keyCode >= 2300 && keyCode < 2300 + TRADE_CUR_DISP) {
                // Player offer table -> drop on player inventory (rel x29,y35).
                Inventory* inv = &(trade_player_table->data.inventory);
                int slot = (keyCode - 2300) + trade_player_table_offset;
                if (slot < inv->length) {
                    Object* item = inv->items[inv->length - (slot + 1)].item;
                    int quantity = inv->items[inv->length - (slot + 1)].quantity;
                    trade_pickup_drag(item);
                    if (trade_dropped_in(29, 35)) {
                        item_move_force(trade_player_table, obj_dude, item, quantity);
                    }
                    trade_refresh_all();
                }
            } else if (keyCode >= 2400 && keyCode < 2400 + TRADE_CUR_DISP) {
                // Partner offer table -> drop on partner inventory (rel x395,y35).
                Inventory* inv = &(trade_partner_table->data.inventory);
                int slot = (keyCode - 2400) + trade_partner_table_offset;
                if (slot < inv->length) {
                    Object* item = inv->items[inv->length - (slot + 1)].item;
                    int quantity = inv->items[inv->length - (slot + 1)].quantity;
                    trade_pickup_drag(item);
                    if (trade_dropped_in(395, 35)) {
                        item_move_force(trade_partner_table, partner, item, quantity);
                    }
                    trade_refresh_all();
                }
            }
        }
    }

    // Return any items still on the offer tables to their owners.
    item_move_all(trade_partner_table, partner);
    item_move_all(trade_player_table, obj_dude);

    // Restore the partner's stripped equipment.
    if (partnerArmor != NULL) {
        partnerArmor->flags |= OBJECT_WORN;
        item_add_force(partner, partnerArmor, 1);
    }
    if (partnerRightHand != NULL) {
        partnerRightHand->flags |= OBJECT_IN_RIGHT_HAND;
        item_add_force(partner, partnerRightHand, 1);
    }
    if (partnerWeapon != NULL) {
        item_add_force(partner, partnerWeapon, 1);
    }

    trade_teardown();
    tile_enable_refresh();
    message_exit(&trade_msg_file);
}

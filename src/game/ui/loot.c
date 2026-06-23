#include "game/ui/loot.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "game/actions.h"
#include "game/anim.h"
#include "game/art.h"
#include "plib/color/color.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "plib/gnw/input.h"
#include "game/critter.h"
#include "game/bmpdlog.h"
#include "plib/gnw/button.h"
#include "plib/gnw/debug.h"
#include "int/dialog.h"
#include "game/display.h"
#include "plib/gnw/grbuf.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/inventry.h"
#include "game/item.h"
#include "game/light.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/proto.h"
#include "game/protinst.h"
#include "game/roll.h"
#include "game/reaction.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "plib/gnw/text.h"
#include "game/tile.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/svga.h"
#include "plib/math.h"

// Native (unscaled) loot panel geometry. Mirrors the loot-relevant subset of the
// inventory screen's layout; kept here so loot rendering is independent.
#define INVENTORY_SLOT_WIDTH 64
#define INVENTORY_SLOT_HEIGHT 48
#define INVENTORY_LARGE_SLOT_WIDTH 90
#define INVENTORY_LARGE_SLOT_HEIGHT 61

#define INVENTORY_LOOT_LEFT_SCROLLER_X 176
#define INVENTORY_LOOT_LEFT_SCROLLER_Y 37
#define INVENTORY_LOOT_LEFT_SCROLLER_MAX_X (INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_LOOT_RIGHT_SCROLLER_X 297
#define INVENTORY_LOOT_RIGHT_SCROLLER_Y 37
#define INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X (INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_BODY_VIEW_WIDTH 60
#define INVENTORY_BODY_VIEW_HEIGHT 100

#define INVENTORY_LOOT_RIGHT_BODY_VIEW_X 422
#define INVENTORY_LOOT_RIGHT_BODY_VIEW_Y 35
#define INVENTORY_LOOT_LEFT_BODY_VIEW_X 44
#define INVENTORY_LOOT_LEFT_BODY_VIEW_Y 35

// Absolute (screen) variants for drag-drop hit testing.
#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_X (i_wid_x + INVENTORY_LOOT_LEFT_SCROLLER_X)
#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_Y (i_wid_y + INVENTORY_LOOT_LEFT_SCROLLER_Y)
#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_MAX_X (i_wid_x + INVENTORY_LOOT_LEFT_SCROLLER_MAX_X)

#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_X (i_wid_x + INVENTORY_LOOT_RIGHT_SCROLLER_X)
#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_Y (i_wid_y + INVENTORY_LOOT_RIGHT_SCROLLER_Y)
#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_MAX_X (i_wid_x + INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X)

#define INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY (1000U / ROTATION_COUNT)

// Number of cursor-handle / button-art cache slots owned by this screen.
#define LOOT_CACHE_ENTRY_COUNT 12

typedef void(InventoryPrintItemDescriptionHandler)(char* string);

typedef enum InventoryArrowFrm {
    INVENTORY_ARROW_FRM_LEFT_ARROW_UP,
    INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_UP,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_COUNT,
} InventoryArrowFrm;

typedef struct InventoryWindowConfiguration {
    int field_0; // artId
    int width;
    int height;
    int x;
    int y;
} InventoryWindowDescription;

typedef struct InventoryCursorData {
    Art* frm;
    unsigned char* frmData;
    int width;
    int height;
    int offsetX;
    int offsetY;
    CacheEntry* frmHandle;
} InventoryCursorData;

static int inventry_msg_load();
static int inventry_msg_unload();
static void display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool a5);
static int do_move_timer(int inventoryWindowType, Object* item, int a3);
static int setup_move_timer_win(int inventoryWindowType, Object* item);
static int exit_move_timer_win(int inventoryWindowType);

static bool loot_setup_inventory(int inventoryWindowType);
static void loot_exit_inventory(bool shouldEnableIso);
static void loot_display_inventory(int a1, int a2, int inventoryWindowType);
static void loot_display_target_inventory(int a1, int a2, Inventory* inventory, int inventoryWindowType);
static void loot_display_body(int fid, int inventoryWindowType);
static int loot_init();
static void loot_uninit();
static void loot_set_mouse(int cursor);
static void loot_hover_on(int btn, int keyCode);
static void loot_hover_off(int btn, int keyCode);
static void loot_adjust_fid();
static int loot_from_button(int keyCode, Object** a2, Object*** a3, Object** a4);
static void loot_action_cursor(int keyCode, int inventoryWindowType);
static int loot_move_inventory(Object* a1, int a2, Object* a3, bool a4);
static void loot_container_enter(int keyCode, int inventoryWindowType);
static void loot_container_exit(int keyCode, int inventoryWindowType);
static void loot_draw_amount(int value, int inventoryWindowType);

// The number of items to show in scroller.
static int inven_cur_disp = 6;

// The critter whose inventory is shown on the left side (the looter, or a nested
// container the player has descended into).
static Object* inven_dude = NULL;

// Armor pid for the rotating body fid.
static int inven_pid = -1;

static bool inven_is_initialized = false;

// Window descriptions indexed by InventoryWindowType. Only LOOT (the screen
// itself) and MOVE_ITEMS (the quantity picker popup) are used by this module.
static InventoryWindowDescription iscr_data[INVENTORY_WINDOW_TYPE_COUNT] = {
    { 48, 499, 377, 80, 0 },
    { 113, 292, 376, 80, 0 },
    { 114, 537, 376, 80, 0 },
    { 305, 259, 162, 140, 80 },
    { 305, 259, 162, 140, 80 },
};

static bool dropped_explosive = false;

static int inven_scroll_up_bid = -1;
static int inven_scroll_dn_bid = -1;
static int loot_scroll_up_bid = -1;
static int loot_scroll_dn_bid = -1;

// Quantity-picker popup button-art handles.
static CacheEntry* mt_key[8];

// Loot window button-art handles (done/scroll-arrow/take-all).
static CacheEntry* loot_ikey[LOOT_CACHE_ENTRY_COUNT];

static int target_stack_offset[10];

// inventry.msg
static MessageList loot_message_file;

static Object* target_stack[10];
static int stack_offset[10];
static Object* stack[10];

static int mt_wid;

static InventoryCursorData imdata[INVENTORY_WINDOW_CURSOR_COUNT];

static InventoryPrintItemDescriptionHandler* display_msg;

static int im_value;
static int immode;
static int target_curr_stack;
static bool inven_ui_was_disabled;

static Object* i_worn;
static Object* i_lhand;

// Rotating character's fid.
static int i_fid;

static Inventory* pud;
static int i_wid;
static Object* i_rhand;
static int curr_stack;
static int i_wid_max_y;
static int i_wid_max_x;

// Runtime loot window position (for absolute hit testing).
static int i_wid_x;
static int i_wid_y;

static Inventory* target_pud;

static int inventry_msg_load()
{
    char path[MAX_PATH];

    if (!message_init(&loot_message_file))
        return -1;

    sprintf(path, "%s%s", msg_path, "inventry.msg");
    if (!message_load(&loot_message_file, path))
        return -1;

    return 0;
}

static int inventry_msg_unload()
{
    message_exit(&loot_message_file);
    return 0;
}

static bool loot_setup_inventory(int inventoryWindowType)
{
    dropped_explosive = 0;
    curr_stack = 0;
    stack_offset[0] = 0;
    inven_cur_disp = 6;
    pud = &(inven_dude->data.inventory);
    stack[0] = inven_dude;

    InventoryWindowDescription* windowDescription = &(iscr_data[inventoryWindowType]);
    const Size invScreenSize = screen_get_size();
    int inventoryWindowX = (invScreenSize.width - windowDescription->width) / 2;
    int inventoryWindowY = (invScreenSize.height - windowDescription->height) / 2;
    i_wid = win_add(inventoryWindowX,
        inventoryWindowY,
        windowDescription->width,
        windowDescription->height,
        257,
        WINDOW_FLAG_MODAL | WINDOW_FLAG_ALWAYS_ON_TOP);
    i_wid_max_x = windowDescription->width + inventoryWindowX;
    i_wid_max_y = windowDescription->height + inventoryWindowY;
    i_wid_x = inventoryWindowX;
    i_wid_y = inventoryWindowY;

    windowDescription->x = inventoryWindowX;
    windowDescription->y = inventoryWindowY;

    unsigned char* dest = win_get_buf(i_wid);
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, windowDescription->field_0, 0, 0, 0);

    CacheEntry* backgroundFrmHandle;
    unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData != NULL) {
        buf_to_buf(backgroundFrmData, windowDescription->width, windowDescription->height, windowDescription->width, dest, windowDescription->width);
        art_ptr_unlock(backgroundFrmHandle);
    }

    display_msg = display_print;

    // Invisible buttons representing the looter's inventory item slots (left).
    for (int index = 0; index < inven_cur_disp; index++) {
        int btn = win_register_button(i_wid, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_SLOT_HEIGHT * (inven_cur_disp - index - 1) + INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, 999 + inven_cur_disp - index, -1, 999 + inven_cur_disp - index, -1, NULL, NULL, NULL, 0);
        if (btn != -1) {
            win_register_button_func(btn, loot_hover_on, loot_hover_off, NULL, NULL);
        }
    }

    int eventCode = 2005;
    int y = INVENTORY_SLOT_HEIGHT * 5 + INVENTORY_LOOT_LEFT_SCROLLER_Y;

    // Invisible buttons representing the container's inventory item slots (right).
    for (int index = 0; index < 6; index++) {
        int btn = win_register_button(i_wid, INVENTORY_LOOT_RIGHT_SCROLLER_X, y, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, eventCode, -1, eventCode, -1, NULL, NULL, NULL, 0);
        if (btn != -1) {
            win_register_button_func(btn, loot_hover_on, loot_hover_off, NULL, NULL);
        }

        eventCode -= 1;
        y -= INVENTORY_SLOT_HEIGHT;
    }

    memset(loot_ikey, 0, sizeof(loot_ikey));

    int fid;
    int btn;
    unsigned char* buttonUpData;
    unsigned char* buttonDownData;
    unsigned char* buttonDisabledData;

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    buttonUpData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[0]));

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    buttonDownData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[1]));

    if (buttonUpData != NULL && buttonDownData != NULL) {
        // Done button
        btn = win_register_button(i_wid, 476, 331, 15, 16, -1, -1, -1, KEY_ESCAPE, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    {
        // Large up arrow (normal).
        fid = art_id(OBJ_TYPE_INTERFACE, 49, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[2]));

        // Large up arrow (pressed).
        fid = art_id(OBJ_TYPE_INTERFACE, 50, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[3]));

        // Large up arrow (disabled).
        fid = art_id(OBJ_TYPE_INTERFACE, 53, 0, 0, 0);
        buttonDisabledData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[4]));

        if (buttonUpData != NULL && buttonDownData != NULL && buttonDisabledData != NULL) {
            // Left inventory up button.
            inven_scroll_up_bid = win_register_button(i_wid, 128, 39, 22, 23, -1, -1, KEY_ARROW_UP, -1, buttonUpData, buttonDownData, NULL, 0);
            if (inven_scroll_up_bid != -1) {
                win_register_button_disable(inven_scroll_up_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
                win_register_button_sound_func(inven_scroll_up_bid, gsound_red_butt_press, gsound_red_butt_release);
                win_disable_button(inven_scroll_up_bid);
            }

            // Right inventory up button.
            loot_scroll_up_bid = win_register_button(i_wid, 379, 39, 22, 23, -1, -1, KEY_CTRL_ARROW_UP, -1, buttonUpData, buttonDownData, NULL, 0);
            if (loot_scroll_up_bid != -1) {
                win_register_button_disable(loot_scroll_up_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
                win_register_button_sound_func(loot_scroll_up_bid, gsound_red_butt_press, gsound_red_butt_release);
                win_disable_button(loot_scroll_up_bid);
            }
        }
    }

    {
        // Large arrow down (normal).
        fid = art_id(OBJ_TYPE_INTERFACE, 51, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[5]));

        // Large arrow down (pressed).
        fid = art_id(OBJ_TYPE_INTERFACE, 52, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[6]));

        // Large arrow down (disabled).
        fid = art_id(OBJ_TYPE_INTERFACE, 54, 0, 0, 0);
        buttonDisabledData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[7]));

        if (buttonUpData != NULL && buttonDownData != NULL && buttonDisabledData != NULL) {
            // Left inventory down button.
            inven_scroll_dn_bid = win_register_button(i_wid, 128, 62, 22, 23, -1, -1, KEY_ARROW_DOWN, -1, buttonUpData, buttonDownData, NULL, 0);
            win_register_button_sound_func(inven_scroll_dn_bid, gsound_red_butt_press, gsound_red_butt_release);
            win_register_button_disable(inven_scroll_dn_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
            win_disable_button(inven_scroll_dn_bid);

            // Invisible button representing left character.
            win_register_button(i_wid, INVENTORY_LOOT_LEFT_BODY_VIEW_X, INVENTORY_LOOT_LEFT_BODY_VIEW_Y, INVENTORY_BODY_VIEW_WIDTH, INVENTORY_BODY_VIEW_HEIGHT, -1, -1, 2500, -1, NULL, NULL, NULL, 0);

            // Right inventory down button.
            loot_scroll_dn_bid = win_register_button(i_wid, 379, 62, 22, 23, -1, -1, KEY_CTRL_ARROW_DOWN, -1, buttonUpData, buttonDownData, 0, 0);
            if (loot_scroll_dn_bid != -1) {
                win_register_button_sound_func(loot_scroll_dn_bid, gsound_red_butt_press, gsound_red_butt_release);
                win_register_button_disable(loot_scroll_dn_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
                win_disable_button(loot_scroll_dn_bid);
            }

            // Invisible button representing right character.
            win_register_button(i_wid, INVENTORY_LOOT_RIGHT_BODY_VIEW_X, INVENTORY_LOOT_RIGHT_BODY_VIEW_Y, INVENTORY_BODY_VIEW_WIDTH, INVENTORY_BODY_VIEW_HEIGHT, -1, -1, 2501, -1, NULL, NULL, NULL, 0);
        }
    }

    if (!gIsSteal) {
        // Take all button (normal)
        fid = art_id(OBJ_TYPE_INTERFACE, 436, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[8]));

        // Take all button (pressed)
        fid = art_id(OBJ_TYPE_INTERFACE, 437, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(loot_ikey[9]));

        if (buttonUpData != NULL && buttonDownData != NULL) {
            // Take all button.
            btn = win_register_button(i_wid, 432, 204, 39, 41, -1, -1, KEY_UPPERCASE_A, -1, buttonUpData, buttonDownData, NULL, 0);
            if (btn != -1) {
                win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
            }
        }
    }

    i_rhand = NULL;
    i_worn = NULL;
    i_lhand = NULL;

    for (int index = 0; index < pud->length; index++) {
        InventoryItem* inventoryItem = &(pud->items[index]);
        Object* item = inventoryItem->item;
        if ((item->flags & OBJECT_IN_LEFT_HAND) != 0) {
            if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
                i_rhand = item;
            }
            i_lhand = item;
        } else if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
            i_rhand = item;
        } else if ((item->flags & OBJECT_WORN) != 0) {
            i_worn = item;
        }
    }

    if (i_lhand != NULL) {
        item_remove_mult(inven_dude, i_lhand, 1);
    }

    if (i_rhand != NULL && i_rhand != i_lhand) {
        item_remove_mult(inven_dude, i_rhand, 1);
    }

    if (i_worn != NULL) {
        item_remove_mult(inven_dude, i_worn, 1);
    }

    loot_adjust_fid();

    bool isoWasEnabled = map_disable_bk_processes();

    gmouse_disable(0);

    return isoWasEnabled;
}

static void loot_exit_inventory(bool shouldEnableIso)
{
    inven_dude = stack[0];

    if (i_lhand != NULL) {
        i_lhand->flags |= OBJECT_IN_LEFT_HAND;
        if (i_lhand == i_rhand) {
            i_lhand->flags |= OBJECT_IN_RIGHT_HAND;
        }

        item_add_force(inven_dude, i_lhand, 1);
    }

    if (i_rhand != NULL && i_rhand != i_lhand) {
        i_rhand->flags |= OBJECT_IN_RIGHT_HAND;
        item_add_force(inven_dude, i_rhand, 1);
    }

    if (i_worn != NULL) {
        i_worn->flags |= OBJECT_WORN;
        item_add_force(inven_dude, i_worn, 1);
    }

    i_rhand = NULL;
    i_worn = NULL;
    i_lhand = NULL;

    for (int index = 0; index < LOOT_CACHE_ENTRY_COUNT; index++) {
        art_ptr_unlock(loot_ikey[index]);
    }

    if (shouldEnableIso) {
        map_enable_bk_processes();
    }

    win_delete(i_wid);

    gmouse_enable();

    if (dropped_explosive) {
        Attack v1;
        combat_ctd_init(&v1, obj_dude, NULL, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
        v1.attackerFlags = DAM_HIT;
        v1.tile = obj_dude->tile;
        compute_explosion_on_extras(&v1, 0, 0, 1);

        Object* v2 = NULL;
        for (int index = 0; index < v1.extrasLength; index++) {
            Object* critter = v1.extras[index];
            if (critter != obj_dude
                && critter->data.critter.combat.team != obj_dude->data.critter.combat.team
                && stat_result(critter, STAT_PERCEPTION, 0, NULL) >= ROLL_SUCCESS) {
                critter_set_who_hit_me(critter, obj_dude);

                if (v2 == NULL) {
                    v2 = critter;
                }
            }
        }

        if (v2 != NULL) {
            if (!isInCombat()) {
                STRUCT_664980 v3;
                v3.attacker = v2;
                v3.defender = obj_dude;
                v3.actionPointsBonus = 0;
                v3.accuracyBonus = 0;
                v3.damageBonus = 0;
                v3.minDamage = 0;
                v3.maxDamage = INT_MAX;
                v3.field_1C = 0;
                scripts_request_combat(&v3);
            }
        }

        dropped_explosive = false;
    }
}

static void loot_display_inventory(int a1, int a2, int inventoryWindowType)
{
    unsigned char* windowBuffer = win_get_buf(i_wid);
    int pitch = 537;

    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);

    CacheEntry* backgroundFrmHandle;
    unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
    if (backgroundFrmData != NULL) {
        // Clear scroll view background.
        buf_to_buf(backgroundFrmData + (pitch * 37 + 176) * 4, 64, inven_cur_disp * 48, pitch, windowBuffer + (pitch * 37 + 176) * 4, pitch);
        art_ptr_unlock(backgroundFrmHandle);
    }

    if (inven_scroll_up_bid != -1) {
        if (a1 <= 0) {
            win_disable_button(inven_scroll_up_bid);
        } else {
            win_enable_button(inven_scroll_up_bid);
        }
    }

    if (inven_scroll_dn_bid != -1) {
        if (pud->length - a1 <= inven_cur_disp) {
            win_disable_button(inven_scroll_dn_bid);
        } else {
            win_enable_button(inven_scroll_dn_bid);
        }
    }

    int y = 0;
    for (int v19 = 0; v19 + a1 < pud->length && v19 < inven_cur_disp; v19 += 1) {
        int v21 = v19 + a1 + 1;
        int width = 56;
        int offset = pitch * (y + 41) + 180;

        InventoryItem* inventoryItem = &(pud->items[pud->length - v21]);

        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + offset * 4, width, 40, pitch);

        offset = pitch * (y + 41) + 180;
        display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset * 4, pitch, v19 == a2);

        y += 48;
    }

    win_draw(i_wid);
}

static void loot_display_target_inventory(int a1, int a2, Inventory* inventory, int inventoryWindowType)
{
    unsigned char* windowBuffer = win_get_buf(i_wid);

    int pitch = 537;

    int fid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);

    CacheEntry* handle;
    unsigned char* data = art_ptr_lock_data(fid, 0, 0, &handle);
    if (data != NULL) {
        buf_to_buf(data + (537 * 37 + 297) * 4, 64, 48 * inven_cur_disp, 537, windowBuffer + (537 * 37 + 297) * 4, 537);
        art_ptr_unlock(handle);
    }

    int y = 0;
    for (int index = 0; index < inven_cur_disp; index++) {
        int v27 = a1 + index;
        if (v27 >= inventory->length) {
            break;
        }

        int offset = pitch * (y + 41) + 301;

        InventoryItem* inventoryItem = &(inventory->items[inventory->length - (v27 + 1)]);
        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + offset * 4, 56, 40, pitch);
        display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset * 4, pitch, index == a2);

        y += 48;
    }

    if (loot_scroll_up_bid != -1) {
        if (a1 <= 0) {
            win_disable_button(loot_scroll_up_bid);
        } else {
            win_enable_button(loot_scroll_up_bid);
        }
    }

    if (loot_scroll_dn_bid != -1) {
        if (inventory->length - a1 <= inven_cur_disp) {
            win_disable_button(loot_scroll_dn_bid);
        } else {
            win_enable_button(loot_scroll_dn_bid);
        }
    }
}

static void display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool a5)
{
    int oldFont = text_curr();
    text_font(101);

    char formattedText[12];

    bool draw = false;

    if (item_get_type(item) == ITEM_TYPE_AMMO) {
        int ammoQuantity = item_w_max_ammo(item) * (quantity - 1);

        if (!a5) {
            ammoQuantity += item_w_curr_ammo(item);
        }

        if (ammoQuantity > 99999) {
            ammoQuantity = 99999;
        }

        sprintf(formattedText, "x%d", ammoQuantity);
        draw = true;
    } else {
        if (quantity > 1) {
            int v9 = quantity;
            if (a5) {
                v9 -= 1;
            }

            if (quantity > 1) {
                if (v9 > 99999) {
                    v9 = 99999;
                }

                sprintf(formattedText, "x%d", v9);
                draw = true;
            }
        }
    }

    if (draw) {
        text_to_buf(dest, formattedText, 80, pitch, colorTable[32767]);
    }

    text_font(oldFont);
}

static void loot_display_body(int fid, int inventoryWindowType)
{
    static unsigned int ticker = 0;
    static int curr_rot = 0;

    if (elapsed_time(ticker) < INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY) {
        return;
    }

    curr_rot += 1;

    if (curr_rot == ROTATION_COUNT) {
        curr_rot = 0;
    }

    int rotations[2];
    if (fid == -1) {
        rotations[0] = curr_rot;
        rotations[1] = ROTATION_SE;
    } else {
        rotations[0] = ROTATION_SW;
        rotations[1] = target_stack[target_curr_stack]->rotation;
    }

    int fids[2] = {
        i_fid,
        fid,
    };

    for (int index = 0; index < 2; index += 1) {
        int fid = fids[index];
        if (fid == -1) {
            continue;
        }

        CacheEntry* handle;
        Art* art = art_ptr_lock(fid, &handle);
        if (art == NULL) {
            continue;
        }

        int frame = 0;
        if (index == 1) {
            frame = art_frame_max_frame(art) - 1;
        }

        int rotation = rotations[index];

        unsigned char* frameData = art_frame_data(art, frame, rotation);

        int framePitch = art_frame_width(art, frame, rotation);
        int frameWidth = min(framePitch, INVENTORY_BODY_VIEW_WIDTH);

        int frameHeight = art_frame_length(art, frame, rotation);
        if (frameHeight > INVENTORY_BODY_VIEW_HEIGHT) {
            frameHeight = INVENTORY_BODY_VIEW_HEIGHT;
        }

        int win;
        Rect rect;
        CacheEntry* backrgroundFrmHandle;
        {
            unsigned char* windowBuffer = win_get_buf(i_wid);
            int windowPitch = win_width(i_wid);

            if (index == 1) {
                rect.ulx = 426;
                rect.uly = 39;
            } else {
                rect.ulx = 48;
                rect.uly = 39;
            }

            rect.lrx = rect.ulx + INVENTORY_BODY_VIEW_WIDTH - 1;
            rect.lry = rect.uly + INVENTORY_BODY_VIEW_HEIGHT - 1;

            int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
            unsigned char* src = art_ptr_lock_data(backgroundFid, 0, 0, &backrgroundFrmHandle);
            if (src != NULL) {
                buf_to_buf(src + (537 * rect.uly + rect.ulx) * 4,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    537,
                    windowBuffer + (windowPitch * rect.uly + rect.ulx) * 4,
                    windowPitch);
            }

            trans_buf_to_buf(frameData, frameWidth, frameHeight, framePitch,
                windowBuffer + (windowPitch * (rect.uly + (INVENTORY_BODY_VIEW_HEIGHT - frameHeight) / 2) + (INVENTORY_BODY_VIEW_WIDTH - frameWidth) / 2 + rect.ulx) * 4,
                windowPitch);

            win = i_wid;
        }
        win_draw_rect(win, &rect);

        art_ptr_unlock(backrgroundFrmHandle);
        art_ptr_unlock(handle);
    }

    ticker = get_time();
}

static int loot_init()
{
    static int num[INVENTORY_WINDOW_CURSOR_COUNT] = {
        286, // pointing hand
        250, // action arrow
        282, // action pick
        283, // action menu
        266, // blank
    };

    if (inventry_msg_load() == -1) {
        return -1;
    }

    inven_ui_was_disabled = game_ui_is_disabled();

    if (inven_ui_was_disabled) {
        game_ui_enable();
    }

    gmouse_3d_off();

    gmouse_set_cursor(MOUSE_CURSOR_ARROW);

    int index;
    for (index = 0; index < INVENTORY_WINDOW_CURSOR_COUNT; index++) {
        InventoryCursorData* cursorData = &(imdata[index]);

        int fid = art_id(OBJ_TYPE_INTERFACE, num[index], 0, 0, 0);
        Art* frm = art_ptr_lock(fid, &(cursorData->frmHandle));
        if (frm == NULL) {
            break;
        }

        cursorData->frm = frm;
        cursorData->frmData = art_frame_data(frm, 0, 0);
        cursorData->width = art_frame_width(frm, 0, 0);
        cursorData->height = art_frame_length(frm, 0, 0);
        art_frame_hot(frm, 0, 0, &(cursorData->offsetX), &(cursorData->offsetY));
    }

    if (index != INVENTORY_WINDOW_CURSOR_COUNT) {
        for (; index >= 0; index--) {
            art_ptr_unlock(imdata[index].frmHandle);
        }

        if (inven_ui_was_disabled) {
            game_ui_disable(0);
        }

        message_exit(&loot_message_file);

        return -1;
    }

    inven_is_initialized = true;
    im_value = -1;

    return 0;
}

static void loot_uninit()
{
    for (int index = 0; index < INVENTORY_WINDOW_CURSOR_COUNT; index++) {
        art_ptr_unlock(imdata[index].frmHandle);
    }

    if (inven_ui_was_disabled) {
        game_ui_disable(0);
    }

    inventry_msg_unload();

    inven_is_initialized = 0;
}

static void loot_set_mouse(int cursor)
{
    immode = cursor;

    if (cursor != INVENTORY_WINDOW_CURSOR_ARROW || im_value == -1) {
        InventoryCursorData* cursorData = &(imdata[cursor]);
        mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    } else {
        loot_hover_on(-1, im_value);
    }
}

static void loot_hover_on(int btn, int keyCode)
{
    static Object* last_target = NULL;

    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
        int x;
        int y;
        mouse_get_position(&x, &y);

        Object* a2a = NULL;
        if (loot_from_button(keyCode, &a2a, NULL, NULL) != 0) {
            gmouse_3d_build_pick_frame(x, y, 3, i_wid_max_x, i_wid_max_y);

            int v5 = 0;
            int v6 = 0;
            gmouse_3d_pick_frame_hot(&v5, &v6);

            InventoryCursorData* cursorData = &(imdata[INVENTORY_WINDOW_CURSOR_PICK]);
            mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, v5, v6, 0);

            if (a2a != last_target) {
                obj_look_at_func(stack[0], a2a, display_msg);
            }
        } else {
            InventoryCursorData* cursorData = &(imdata[INVENTORY_WINDOW_CURSOR_ARROW]);
            mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
        }

        last_target = a2a;
    }

    im_value = keyCode;
}

static void loot_hover_off(int btn, int keyCode)
{
    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
        InventoryCursorData* cursorData = &(imdata[INVENTORY_WINDOW_CURSOR_ARROW]);
        mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    }

    im_value = -1;
}

static void loot_adjust_fid()
{
    int fid;
    if (FID_TYPE(inven_dude->fid) == OBJ_TYPE_CRITTER) {
        Proto* proto;

        int v0 = art_vault_guy_num;

        if (proto_ptr(inven_pid, &proto) == -1) {
            v0 = proto->fid & 0xFFF;
        }

        if (i_worn != NULL) {
            proto_ptr(i_worn->pid, &proto);
            if (critterGetStat(inven_dude, STAT_GENDER) == GENDER_FEMALE) {
                v0 = proto->item.data.armor.femaleFid;
            } else {
                v0 = proto->item.data.armor.maleFid;
            }

            if (v0 == -1) {
                v0 = art_vault_guy_num;
            }
        }

        int animationCode = 0;
        if (intface_is_item_right_hand()) {
            if (i_rhand != NULL) {
                proto_ptr(i_rhand->pid, &proto);
                if (proto->item.type == ITEM_TYPE_WEAPON) {
                    animationCode = proto->item.data.weapon.animationCode;
                }
            }
        } else {
            if (i_lhand != NULL) {
                proto_ptr(i_lhand->pid, &proto);
                if (proto->item.type == ITEM_TYPE_WEAPON) {
                    animationCode = proto->item.data.weapon.animationCode;
                }
            }
        }

        fid = art_id(OBJ_TYPE_CRITTER, v0, 0, animationCode, 0);
    } else {
        fid = inven_dude->fid;
    }

    i_fid = fid;
}

static int loot_from_button(int keyCode, Object** a2, Object*** a3, Object** a4)
{
    Object** v6;
    Object* v7;
    Object* v8;
    int quantity = 0;

    switch (keyCode) {
    case 1006:
        v6 = &i_rhand;
        v7 = stack[0];
        v8 = i_rhand;
        break;
    case 1007:
        v6 = &i_lhand;
        v7 = stack[0];
        v8 = i_lhand;
        break;
    case 1008:
        v6 = &i_worn;
        v7 = stack[0];
        v8 = i_worn;
        break;
    default:
        v6 = NULL;
        v7 = NULL;
        v8 = NULL;

        InventoryItem* inventoryItem = NULL;
        if (keyCode < 2000) {
            int index = stack_offset[curr_stack] + keyCode - 1000;
            if (index >= pud->length) {
                break;
            }

            inventoryItem = &(pud->items[pud->length - (index + 1)]);
            v8 = inventoryItem->item;
            v7 = stack[curr_stack];
        } else if (keyCode < 2300) {
            int index = target_stack_offset[target_curr_stack] + keyCode - 2000;
            if (index >= target_pud->length) {
                break;
            }

            inventoryItem = &(target_pud->items[target_pud->length - (index + 1)]);
            v8 = inventoryItem->item;
            v7 = target_stack[target_curr_stack];
        }

        if (inventoryItem != NULL) {
            quantity = inventoryItem->quantity;
        }
    }

    if (a3 != NULL) {
        *a3 = v6;
    }

    if (a2 != NULL) {
        *a2 = v8;
    }

    if (a4 != NULL) {
        *a4 = v7;
    }

    if (quantity == 0 && v8 != NULL) {
        quantity = 1;
    }

    return quantity;
}

static void loot_action_cursor(int keyCode, int inventoryWindowType)
{
    static int act_use[4] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_USE,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    static int act_no_use[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    static int act_just_use[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_USE,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    static int act_nothing[2] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    static int act_weap[4] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    static int act_weap2[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    Object* item;
    Object** v43;
    Object* v41;

    int v56 = loot_from_button(keyCode, &item, &v43, &v41);
    if (v56 == 0) {
        return;
    }

    int itemType = item_get_type(item);

    int mouseState;
    do {
        get_input();

        mouseState = mouse_get_buttons();
        if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
            obj_look_at_func(stack[0], item, display_msg);
            win_draw(i_wid);
            return;
        }
    } while ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT);

    loot_set_mouse(INVENTORY_WINDOW_CURSOR_BLANK);

    unsigned char* windowBuffer = win_get_buf(i_wid);

    int x;
    int y;
    mouse_get_position(&x, &y);

    int actionMenuItemsLength;
    const int* actionMenuItems;
    if (itemType == ITEM_TYPE_WEAPON && item_w_can_unload(item)) {
        if (obj_top_environment(item) != obj_dude) {
            actionMenuItemsLength = 3;
            actionMenuItems = act_weap2;
        } else {
            actionMenuItemsLength = 4;
            actionMenuItems = act_weap;
        }
    } else {
        if (obj_top_environment(item) != obj_dude) {
            if (itemType == ITEM_TYPE_CONTAINER) {
                actionMenuItemsLength = 3;
                actionMenuItems = act_just_use;
            } else {
                actionMenuItemsLength = 2;
                actionMenuItems = act_nothing;
            }
        } else {
            if (itemType == ITEM_TYPE_CONTAINER) {
                actionMenuItemsLength = 4;
                actionMenuItems = act_use;
            } else {
                actionMenuItemsLength = 3;
                actionMenuItems = act_no_use;
            }
        }
    }

    InventoryWindowDescription* windowDescription = &(iscr_data[inventoryWindowType]);
    gmouse_3d_build_menu_frame(x, y, actionMenuItems, actionMenuItemsLength,
        windowDescription->width + windowDescription->x,
        windowDescription->height + windowDescription->y);

    InventoryCursorData* cursorData = &(imdata[INVENTORY_WINDOW_CURSOR_MENU]);

    int offsetX;
    int offsetY;
    art_frame_offset(cursorData->frm, 0, &offsetX, &offsetY);

    Rect rect;
    rect.ulx = x - windowDescription->x - cursorData->width / 2 + offsetX;
    rect.uly = y - windowDescription->y - cursorData->height + 1 + offsetY;
    rect.lrx = rect.ulx + cursorData->width - 1;
    rect.lry = rect.uly + cursorData->height - 1;

    int menuButtonHeight = cursorData->height;
    if (rect.uly + menuButtonHeight > windowDescription->height) {
        menuButtonHeight = windowDescription->height - rect.uly;
    }

    int btn = win_register_button(i_wid,
        rect.ulx,
        rect.uly,
        cursorData->width,
        menuButtonHeight,
        -1,
        -1,
        -1,
        -1,
        cursorData->frmData,
        cursorData->frmData,
        0,
        BUTTON_FLAG_TRANSPARENT);
    win_draw_rect(i_wid, &rect);

    int menuItemIndex = 0;
    int previousMouseY = y;
    while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        get_input();

        int x;
        int y;
        mouse_get_position(&x, &y);
        if (y - previousMouseY > 10 || previousMouseY - y > 10) {
            if (y >= previousMouseY || menuItemIndex <= 0) {
                if (previousMouseY < y && menuItemIndex < actionMenuItemsLength - 1) {
                    menuItemIndex++;
                }
            } else {
                menuItemIndex--;
            }
            gmouse_3d_highlight_menu_frame(menuItemIndex);
            win_draw_rect(i_wid, &rect);
            previousMouseY = y;
        }
    }

    win_delete_button(btn);

    {
        int backgroundFid = art_id(OBJ_TYPE_INTERFACE, windowDescription->field_0, 0, 0, 0);
        CacheEntry* backgroundFrmHandle;
        unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
        buf_to_buf(backgroundFrmData + (windowDescription->width * rect.uly + rect.ulx) * 4,
            cursorData->width,
            menuButtonHeight,
            windowDescription->width,
            windowBuffer + (windowDescription->width * rect.uly + rect.ulx) * 4,
            windowDescription->width);
        art_ptr_unlock(backgroundFrmHandle);
    }

    mouse_set_position(x, y);

    loot_display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);

    int actionMenuItem = actionMenuItems[menuItemIndex];
    switch (actionMenuItem) {
    case GAME_MOUSE_ACTION_MENU_ITEM_DROP:
        if (v43 != NULL) {
            if (v43 == &i_worn) {
                adjust_ac(stack[0], item, NULL);
            }
            item_add_force(v41, item, 1);
            v56 = 1;
            *v43 = NULL;
        }

        if (item->pid == PROTO_ID_MONEY) {
            if (v56 > 1) {
                v56 = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, v56);
            } else {
                v56 = 1;
            }

            if (v56 > 0) {
                if (v56 == 1) {
                    item_caps_set_amount(item, 1);
                    obj_drop(v41, item);
                } else {
                    if (item_remove_mult(v41, item, v56 - 1) == 0) {
                        Object* a2;
                        if (loot_from_button(keyCode, &a2, &v43, &v41) != 0) {
                            item_caps_set_amount(a2, v56);
                            obj_drop(v41, a2);
                        } else {
                            item_add_force(v41, item, v56 - 1);
                        }
                    }
                }
            }
        } else if (item->pid == PROTO_ID_DYNAMITE_II || item->pid == PROTO_ID_PLASTIC_EXPLOSIVES_II) {
            dropped_explosive = 1;
            obj_drop(v41, item);
        } else {
            if (v56 > 1) {
                v56 = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, v56);

                for (int index = 0; index < v56; index++) {
                    if (loot_from_button(keyCode, &item, &v43, &v41) != 0) {
                        obj_drop(v41, item);
                    }
                }
            } else {
                obj_drop(v41, item);
            }
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
        obj_examine_func(stack[0], item, display_msg);
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
        switch (itemType) {
        case ITEM_TYPE_CONTAINER:
            loot_container_enter(keyCode, inventoryWindowType);
            break;
        case ITEM_TYPE_DRUG:
            if (item_d_take_drug(stack[0], item)) {
                if (v43 != NULL) {
                    *v43 = NULL;
                } else {
                    item_remove_mult(v41, item, 1);
                }

                obj_connect(item, obj_dude->tile, obj_dude->elevation, NULL);
                obj_destroy(item);
            }
            intface_update_hit_points(true);
            break;
        case ITEM_TYPE_WEAPON:
        case ITEM_TYPE_MISC:
            if (v43 == NULL) {
                item_remove_mult(v41, item, 1);
            }

            int v21;
            if (obj_action_can_use(item)) {
                v21 = protinst_use_item(stack[0], item);
            } else {
                v21 = protinst_use_item_on(stack[0], stack[0], item);
            }

            if (v21 == 1) {
                if (v43 != NULL) {
                    *v43 = NULL;
                }

                obj_connect(item, obj_dude->tile, obj_dude->elevation, NULL);
                obj_destroy(item);
            } else {
                if (v43 == NULL) {
                    item_add_force(v41, item, 1);
                }
            }
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD:
        if (v43 == NULL) {
            item_remove_mult(v41, item, 1);
        }

        for (;;) {
            Object* ammo = item_w_unload(item);
            if (ammo == NULL) {
                break;
            }

            Rect rect;
            obj_disconnect(ammo, &rect);
            item_add_force(v41, ammo, 1);
        }

        if (v43 == NULL) {
            item_add_force(v41, item, 1);
        }
        break;
    default:
        break;
    }

    loot_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);

    loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);

    loot_display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);

    loot_adjust_fid();
}

static void loot_container_enter(int keyCode, int inventoryWindowType)
{
    if (keyCode >= 2000) {
        int index = target_pud->length - (target_stack_offset[target_curr_stack] + keyCode - 2000 + 1);
        if (index < target_pud->length && target_curr_stack < 9) {
            InventoryItem* inventoryItem = &(target_pud->items[index]);
            Object* item = inventoryItem->item;
            if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
                target_curr_stack += 1;
                target_stack[target_curr_stack] = item;
                target_stack_offset[target_curr_stack] = 0;

                target_pud = &(item->data.inventory);

                loot_display_body(item->fid, inventoryWindowType);
                loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);
                win_draw(i_wid);
            }
        }
    } else {
        int index = pud->length - (stack_offset[curr_stack] + keyCode - 1000 + 1);
        if (index < pud->length && curr_stack < 9) {
            InventoryItem* inventoryItem = &(pud->items[index]);
            Object* item = inventoryItem->item;
            if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
                curr_stack += 1;

                stack[curr_stack] = item;
                stack_offset[curr_stack] = 0;

                inven_dude = stack[curr_stack];
                pud = &(item->data.inventory);

                loot_adjust_fid();
                loot_display_body(-1, inventoryWindowType);
                loot_display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);
            }
        }
    }
}

static void loot_container_exit(int keyCode, int inventoryWindowType)
{
    if (keyCode == 2500) {
        if (curr_stack > 0) {
            curr_stack -= 1;
            inven_dude = stack[curr_stack];
            pud = &inven_dude->data.inventory;
            loot_adjust_fid();
            loot_display_body(-1, inventoryWindowType);
            loot_display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);
        }
    } else if (keyCode == 2501) {
        if (target_curr_stack > 0) {
            target_curr_stack -= 1;
            Object* v5 = target_stack[target_curr_stack];
            target_pud = &(v5->data.inventory);
            loot_display_body(v5->fid, inventoryWindowType);
            loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);
            win_draw(i_wid);
        }
    }
}

static int loot_move_inventory(Object* a1, int a2, Object* a3, bool a4)
{
    bool v38 = true;

    Rect rect;

    int quantity;
    if (a4) {
        rect.ulx = INVENTORY_LOOT_LEFT_SCROLLER_X;
        rect.uly = INVENTORY_SLOT_HEIGHT * a2 + INVENTORY_LOOT_LEFT_SCROLLER_Y;

        InventoryItem* inventoryItem = &(pud->items[pud->length - (a2 + stack_offset[curr_stack] + 1)]);
        quantity = inventoryItem->quantity;
        if (quantity > 1) {
            loot_display_inventory(stack_offset[curr_stack], a2, INVENTORY_WINDOW_TYPE_LOOT);
            v38 = false;
        }
    } else {
        rect.ulx = INVENTORY_LOOT_RIGHT_SCROLLER_X;
        rect.uly = INVENTORY_SLOT_HEIGHT * a2 + INVENTORY_LOOT_RIGHT_SCROLLER_Y;

        InventoryItem* inventoryItem = &(target_pud->items[target_pud->length - (a2 + target_stack_offset[target_curr_stack] + 1)]);
        quantity = inventoryItem->quantity;
        if (quantity > 1) {
            loot_display_target_inventory(target_stack_offset[target_curr_stack], a2, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
            win_draw(i_wid);
            v38 = false;
        }
    }

    if (v38) {
        unsigned char* windowBuffer = win_get_buf(i_wid);

        CacheEntry* handle;
        int fid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        unsigned char* data = art_ptr_lock_data(fid, 0, 0, &handle);
        if (data != NULL) {
            buf_to_buf(data + (537 * rect.uly + rect.ulx) * 4, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, 537, windowBuffer + (537 * rect.uly + rect.ulx) * 4, 537);
            art_ptr_unlock(handle);
        }

        rect.lrx = rect.ulx + INVENTORY_SLOT_WIDTH - 1;
        rect.lry = rect.uly + INVENTORY_SLOT_HEIGHT - 1;
        win_draw_rect(i_wid, &rect);
    }

    CacheEntry* inventoryFrmHandle;
    int inventoryFid = item_inv_fid(a1);
    Art* inventoryFrm = art_ptr_lock(inventoryFid, &inventoryFrmHandle);
    if (inventoryFrm != NULL) {
        int width = art_frame_width(inventoryFrm, 0, 0);
        int height = art_frame_length(inventoryFrm, 0, 0);
        unsigned char* data = art_frame_data(inventoryFrm, 0, 0);
        mouse_set_shape(data, width, height, width, width / 2, height / 2, 0);
        gsound_play_sfx_file("ipickup1");
    }

    do {
        get_input();
    } while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (inventoryFrm != NULL) {
        art_ptr_unlock(inventoryFrmHandle);
        gsound_play_sfx_file("iputdown");
    }

    int rc = 0;
    MessageListItem messageListItem;

    if (a4) {
        if (mouse_click_in(INVENTORY_LOOT_RIGHT_SCROLLER_ABS_X, INVENTORY_LOOT_RIGHT_SCROLLER_ABS_Y, INVENTORY_LOOT_RIGHT_SCROLLER_ABS_MAX_X, INVENTORY_SLOT_HEIGHT * inven_cur_disp + INVENTORY_LOOT_RIGHT_SCROLLER_ABS_Y)) {
            int quantityToMove;
            if (quantity > 1) {
                quantityToMove = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, a1, quantity);
            } else {
                quantityToMove = 1;
            }

            if (quantityToMove != -1) {
                if (gIsSteal) {
                    if (skill_check_stealing(inven_dude, a3, a1, true) == 0) {
                        rc = 1;
                    }
                }

                if (rc != 1) {
                    if (item_move(inven_dude, a3, a1, quantityToMove) != -1) {
                        rc = 2;
                    } else {
                        // There is no space left for that item.
                        messageListItem.num = 26;
                        if (message_search(&loot_message_file, &messageListItem)) {
                            display_print(messageListItem.text);
                        }
                    }
                }
            }
        }
    } else {
        if (mouse_click_in(INVENTORY_LOOT_LEFT_SCROLLER_ABS_X, INVENTORY_LOOT_LEFT_SCROLLER_ABS_Y, INVENTORY_LOOT_LEFT_SCROLLER_ABS_MAX_X, INVENTORY_SLOT_HEIGHT * inven_cur_disp + INVENTORY_LOOT_LEFT_SCROLLER_ABS_Y)) {
            int quantityToMove;
            if (quantity > 1) {
                quantityToMove = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, a1, quantity);
            } else {
                quantityToMove = 1;
            }

            if (quantityToMove != -1) {
                if (gIsSteal) {
                    if (skill_check_stealing(inven_dude, a3, a1, false) == 0) {
                        rc = 1;
                    }
                }

                if (rc != 1) {
                    if (item_move(a3, inven_dude, a1, quantityToMove) == 0) {
                        if ((a1->flags & OBJECT_IN_RIGHT_HAND) != 0) {
                            a3->fid = art_id(FID_TYPE(a3->fid), a3->fid & 0xFFF, FID_ANIM_TYPE(a3->fid), 0, a3->rotation + 1);
                        }

                        a3->flags &= ~OBJECT_EQUIPPED;

                        rc = 2;
                    } else {
                        // You cannot pick that up. You are at your maximum weight capacity.
                        messageListItem.num = 25;
                        if (message_search(&loot_message_file, &messageListItem)) {
                            display_print(messageListItem.text);
                        }
                    }
                }
            }
        }
    }

    loot_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);

    return rc;
}

static void loot_draw_amount(int value, int inventoryWindowType)
{
    // BIGNUM.frm
    CacheEntry* handle;
    int fid = art_id(OBJ_TYPE_INTERFACE, 170, 0, 0, 0);
    unsigned char* data = art_ptr_lock_data(fid, 0, 0, &handle);
    if (data == NULL) {
        return;
    }

    Rect rect;

    int windowWidth = win_width(mt_wid);
    unsigned char* windowBuffer = win_get_buf(mt_wid);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        rect.ulx = 125;
        rect.uly = 45;
        rect.lrx = 195;
        rect.lry = 69;

        int ranks[5];
        ranks[4] = value % 10;
        ranks[3] = value / 10 % 10;
        ranks[2] = value / 100 % 10;
        ranks[1] = value / 1000 % 10;
        ranks[0] = value / 10000 % 10;

        windowBuffer += (rect.uly * windowWidth + rect.ulx) * 4;

        for (int index = 0; index < 5; index++) {
            unsigned char* src = data + 14 * ranks[index] * 4;
            buf_to_buf(src, 14, 24, 336, windowBuffer, windowWidth);
            windowBuffer += 14 * 4;
        }
    } else {
        rect.ulx = 133;
        rect.uly = 64;
        rect.lrx = 189;
        rect.lry = 88;

        windowBuffer += (windowWidth * rect.uly + rect.ulx) * 4;
        buf_to_buf(data + (14 * (value / 60)) * 4, 14, 24, 336, windowBuffer, windowWidth);
        buf_to_buf(data + (14 * (value % 60 / 10)) * 4, 14, 24, 336, windowBuffer + 14 * 2 * 4, windowWidth);
        buf_to_buf(data + (14 * (value % 10)) * 4, 14, 24, 336, windowBuffer + 14 * 3 * 4, windowWidth);
    }

    art_ptr_unlock(handle);
    win_draw_rect(mt_wid, &rect);
}

static int do_move_timer(int inventoryWindowType, Object* item, int max)
{
    setup_move_timer_win(inventoryWindowType, item);

    int value;
    int min;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        value = 1;
        if (max > 99999) {
            max = 99999;
        }
        min = 1;
    } else {
        value = 60;
        min = 10;
    }

    loot_draw_amount(value, inventoryWindowType);

    bool v5 = false;
    for (;;) {
        int keyCode = get_input();
        if (keyCode == KEY_ESCAPE) {
            exit_move_timer_win(inventoryWindowType);
            return -1;
        }

        if (keyCode == KEY_RETURN) {
            if (value >= min && value <= max) {
                if (inventoryWindowType != INVENTORY_WINDOW_TYPE_SET_TIMER || value % 10 == 0) {
                    gsound_play_sfx_file("ib1p1xx1");
                    break;
                }
            }

            gsound_play_sfx_file("iisxxxx1");
        } else if (keyCode == 5000) {
            v5 = false;
            value = max;
            loot_draw_amount(value, inventoryWindowType);
        } else if (keyCode == 6000) {
            v5 = false;
            if (value < max) {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
                    if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                        get_time();

                        unsigned int delay = 100;
                        while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                            if (value < max) {
                                value++;
                            }

                            loot_draw_amount(value, inventoryWindowType);
                            get_input();

                            if (delay > 1) {
                                delay--;
                                pause_for_tocks(delay);
                            }
                        }
                    } else {
                        if (value < max) {
                            value++;
                        }
                    }
                } else {
                    value += 10;
                }

                loot_draw_amount(value, inventoryWindowType);
                continue;
            }
        } else if (keyCode == 7000) {
            v5 = false;
            if (value > min) {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
                    if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                        get_time();

                        unsigned int delay = 100;
                        while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                            if (value > min) {
                                value--;
                            }

                            loot_draw_amount(value, inventoryWindowType);
                            get_input();

                            if (delay > 1) {
                                delay--;
                                pause_for_tocks(delay);
                            }
                        }
                    } else {
                        if (value > min) {
                            value--;
                        }
                    }
                } else {
                    value -= 10;
                }

                loot_draw_amount(value, inventoryWindowType);
                continue;
            }
        }

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
            if (keyCode >= KEY_0 && keyCode <= KEY_9) {
                int number = keyCode - KEY_0;
                if (!v5) {
                    value = 0;
                }

                value = 10 * value % 100000 + number;
                v5 = true;

                loot_draw_amount(value, inventoryWindowType);
                continue;
            } else if (keyCode == KEY_BACKSPACE) {
                if (!v5) {
                    value = 0;
                }

                value /= 10;
                v5 = true;

                loot_draw_amount(value, inventoryWindowType);
                continue;
            }
        }
    }

    exit_move_timer_win(inventoryWindowType);

    return value;
}

static int setup_move_timer_win(int inventoryWindowType, Object* item)
{
    const int oldFont = text_curr();
    text_font(103);

    for (int index = 0; index < 8; index++) {
        mt_key[index] = NULL;
    }

    InventoryWindowDescription* windowDescription = &(iscr_data[inventoryWindowType]);

    int quantityWindowX = windowDescription->x;
    int quantityWindowY = windowDescription->y;
    mt_wid = win_add(quantityWindowX, quantityWindowY, windowDescription->width, windowDescription->height, 257, WINDOW_FLAG_MODAL | WINDOW_FLAG_ALWAYS_ON_TOP);
    unsigned char* windowBuffer = win_get_buf(mt_wid);

    CacheEntry* backgroundHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, windowDescription->field_0, 0, 0, 0);
    unsigned char* backgroundData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundHandle);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData, windowDescription->width, windowDescription->height, windowDescription->width, windowBuffer, windowDescription->width);
        art_ptr_unlock(backgroundHandle);
    }

    MessageListItem messageListItem;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        // MOVE ITEMS
        messageListItem.num = 21;
        if (message_search(&loot_message_file, &messageListItem)) {
            int length = text_width(messageListItem.text);
            text_to_buf(windowBuffer + (windowDescription->width * 9 + (windowDescription->width - length) / 2) * 4, messageListItem.text, 200, windowDescription->width, colorTable[21091]);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_SET_TIMER) {
        // SET TIMER
        messageListItem.num = 23;
        if (message_search(&loot_message_file, &messageListItem)) {
            int length = text_width(messageListItem.text);
            text_to_buf(windowBuffer + (windowDescription->width * 9 + (windowDescription->width - length) / 2) * 4, messageListItem.text, 200, windowDescription->width, colorTable[21091]);
        }

        // Timer overlay
        CacheEntry* overlayFrmHandle;
        int overlayFid = art_id(OBJ_TYPE_INTERFACE, 306, 0, 0, 0);
        unsigned char* overlayFrmData = art_ptr_lock_data(overlayFid, 0, 0, &overlayFrmHandle);
        if (overlayFrmData != NULL) {
            buf_to_buf(overlayFrmData, 105, 81, 105, windowBuffer + (34 * windowDescription->width + 113) * 4, windowDescription->width);
            art_ptr_unlock(overlayFrmHandle);
        }
    }

    int inventoryFid = item_inv_fid(item);
    scale_art(inventoryFid, windowBuffer + (windowDescription->width * 46 + 16) * 4, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, windowDescription->width);

    int x;
    int y;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        x = 200;
        y = 46;
    } else {
        x = 194;
        y = 64;
    }

    int fid;
    unsigned char* buttonUpData;
    unsigned char* buttonDownData;
    int btn;

    // Plus button
    fid = art_id(OBJ_TYPE_INTERFACE, 193, 0, 0, 0);
    buttonUpData = art_ptr_lock_data(fid, 0, 0, &(mt_key[0]));

    fid = art_id(OBJ_TYPE_INTERFACE, 194, 0, 0, 0);
    buttonDownData = art_ptr_lock_data(fid, 0, 0, &(mt_key[1]));

    if (buttonUpData != NULL && buttonDownData != NULL) {
        btn = win_register_button(mt_wid, x, y, 16, 12, -1, -1, 6000, -1, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    // Minus button
    fid = art_id(OBJ_TYPE_INTERFACE, 191, 0, 0, 0);
    buttonUpData = art_ptr_lock_data(fid, 0, 0, &(mt_key[2]));

    fid = art_id(OBJ_TYPE_INTERFACE, 192, 0, 0, 0);
    buttonDownData = art_ptr_lock_data(fid, 0, 0, &(mt_key[3]));

    if (buttonUpData != NULL && buttonDownData != NULL) {
        btn = win_register_button(mt_wid, x, y + 12, 17, 12, -1, -1, 7000, -1, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    buttonUpData = art_ptr_lock_data(fid, 0, 0, &(mt_key[4]));

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    buttonDownData = art_ptr_lock_data(fid, 0, 0, &(mt_key[5]));

    if (buttonUpData != NULL && buttonDownData != NULL) {
        // Done
        btn = win_register_button(mt_wid, 98, 128, 15, 16, -1, -1, -1, KEY_RETURN, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }

        // Cancel
        btn = win_register_button(mt_wid, 148, 128, 15, 16, -1, -1, -1, KEY_ESCAPE, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        fid = art_id(OBJ_TYPE_INTERFACE, 307, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(mt_key[6]));

        fid = art_id(OBJ_TYPE_INTERFACE, 308, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(mt_key[7]));

        if (buttonUpData != NULL && buttonDownData != NULL) {
            // ALL
            messageListItem.num = 22;
            if (message_search(&loot_message_file, &messageListItem)) {
                int length = text_width(messageListItem.text);

                text_to_buf(buttonUpData + (94 - length) / 2 + 376, messageListItem.text, 200, 94, colorTable[21091]);
                text_to_buf(buttonDownData + (94 - length) / 2 + 376, messageListItem.text, 200, 94, colorTable[18977]);

                btn = win_register_button(mt_wid, 120, 80, 94, 33, -1, -1, -1, 5000, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
                if (btn != -1) {
                    win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
                }
            }
        }
    }

    win_draw(mt_wid);
    loot_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);
    text_font(oldFont);

    return 0;
}

static int exit_move_timer_win(int inventoryWindowType)
{
    int count = inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS ? 8 : 6;

    for (int index = 0; index < count; index++) {
        art_ptr_unlock(mt_key[index]);
    }

    win_delete(mt_wid);

    return 0;
}

int loot_container(Object* a1, Object* a2)
{
    static const int arrowFrmIds[INVENTORY_ARROW_FRM_COUNT] = {
        122, // left arrow up
        123, // left arrow down
        124, // right arrow up
        125, // right arrow down
    };

    CacheEntry* arrowFrmHandles[INVENTORY_ARROW_FRM_COUNT];
    MessageListItem messageListItem;

    // Only the player drives the loot screen. Make the looter the active left-hand
    // critter (mirrors the original inven_dude == player invariant).
    if (a1 != obj_dude) {
        return 0;
    }
    inven_dude = a1;
    inven_pid = 0x1000000;

    if (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER) {
        if (critter_flag_check(a2->pid, CRITTER_NO_STEAL)) {
            // You can't find anything to take from that.
            messageListItem.num = 50;
            if (message_search(&loot_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
            return 0;
        }
    }

    if (FID_TYPE(a2->fid) == OBJ_TYPE_ITEM) {
        if (item_get_type(a2) == ITEM_TYPE_CONTAINER) {
            if (a2->frame == 0) {
                CacheEntry* handle;
                Art* frm = art_ptr_lock(a2->fid, &handle);
                if (frm != NULL) {
                    int frameCount = art_frame_max_frame(frm);
                    art_ptr_unlock(handle);
                    if (frameCount > 1) {
                        return 0;
                    }
                }
            }
        }
    }

    int sid = -1;
    if (!gIsSteal) {
        if (obj_sid(a2, &sid) != -1) {
            scr_set_objs(sid, a1, NULL);
            exec_script_proc(sid, SCRIPT_PROC_PICKUP);

            Script* script;
            if (scr_ptr(sid, &script) != -1) {
                if (script->scriptOverrides) {
                    return 0;
                }
            }
        }
    }

    if (loot_init() == -1) {
        return 0;
    }

    target_pud = &(a2->data.inventory);
    target_curr_stack = 0;
    target_stack_offset[0] = 0;
    target_stack[0] = a2;

    Object* a1a = NULL;
    if (obj_new(&a1a, 0, 467) == -1) {
        return 0;
    }

    item_move_all_hidden(a2, a1a);

    Object* item1 = NULL;
    Object* item2 = NULL;
    Object* armor = NULL;

    if (gIsSteal) {
        item1 = inven_left_hand(a2);
        if (item1 != NULL) {
            item_remove_mult(a2, item1, 1);
        }

        item2 = inven_right_hand(a2);
        if (item2 != NULL) {
            item_remove_mult(a2, item2, 1);
        }

        armor = inven_worn(a2);
        if (armor != NULL) {
            item_remove_mult(a2, armor, 1);
        }
    }

    bool isoWasEnabled = loot_setup_inventory(INVENTORY_WINDOW_TYPE_LOOT);

    Object** critters = NULL;
    int critterCount = 0;
    int critterIndex = 0;
    if (!gIsSteal) {
        if (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER) {
            critterCount = obj_create_list(a2->tile, a2->elevation, OBJ_TYPE_CRITTER, &critters);
            int endIndex = critterCount - 1;
            for (int index = 0; index < critterCount; index++) {
                Object* critter = critters[index];
                if ((critter->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) == 0) {
                    critters[index] = critters[endIndex];
                    critters[endIndex] = critter;
                    critterCount--;
                    index--;
                    endIndex--;
                } else {
                    critterIndex++;
                }
            }

            if (critterCount == 1) {
                obj_delete_list(critters);
                critterCount = 0;
            }

            if (critterCount > 1) {
                int fid;
                unsigned char* buttonUpData;
                unsigned char* buttonDownData;
                int btn;

                for (int index = 0; index < INVENTORY_ARROW_FRM_COUNT; index++) {
                    arrowFrmHandles[index] = INVALID_CACHE_ENTRY;
                }

                // Setup left arrow button.
                fid = art_id(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_LEFT_ARROW_UP], 0, 0, 0);
                buttonUpData = art_ptr_lock_data(fid, 0, 0, &(arrowFrmHandles[INVENTORY_ARROW_FRM_LEFT_ARROW_UP]));

                fid = art_id(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN], 0, 0, 0);
                buttonDownData = art_ptr_lock_data(fid, 0, 0, &(arrowFrmHandles[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN]));

                if (buttonUpData != NULL && buttonDownData != NULL) {
                    btn = win_register_button(i_wid, 436, 162, 20, 18, -1, -1, KEY_PAGE_UP, -1, buttonUpData, buttonDownData, NULL, 0);
                    if (btn != -1) {
                        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
                    }
                }

                // Setup right arrow button.
                fid = art_id(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP], 0, 0, 0);
                buttonUpData = art_ptr_lock_data(fid, 0, 0, &(arrowFrmHandles[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP]));

                fid = art_id(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN], 0, 0, 0);
                buttonDownData = art_ptr_lock_data(fid, 0, 0, &(arrowFrmHandles[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN]));

                if (buttonUpData != NULL && buttonDownData != NULL) {
                    btn = win_register_button(i_wid, 456, 162, 20, 18, -1, -1, KEY_PAGE_DOWN, -1, buttonUpData, buttonDownData, NULL, 0);
                    if (btn != -1) {
                        win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
                    }
                }

                for (int index = 0; index < critterCount; index++) {
                    if (a2 == critters[index]) {
                        critterIndex = index;
                    }
                }
            }
        }
    }

    loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
    loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
    loot_display_body(a2->fid, INVENTORY_WINDOW_TYPE_LOOT);
    loot_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);

    bool isCaughtStealing = false;
    int stealingXp = 0;
    int stealingXpBonus = 10;
    for (;;) {
        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (isCaughtStealing) {
            break;
        }

        int keyCode = get_input();

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            game_quit_with_confirm();
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (keyCode == KEY_UPPERCASE_A) {
            if (!gIsSteal) {
                int maxCarryWeight = critterGetStat(a1, STAT_CARRY_WEIGHT);
                int currentWeight = item_total_weight(a1);
                int newInventoryWeight = item_total_weight(a2);
                if (newInventoryWeight <= maxCarryWeight - currentWeight) {
                    item_move_all(a2, a1);
                    loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                    loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                } else {
                    // Sorry, you cannot carry that much.
                    messageListItem.num = 31;
                    if (message_search(&loot_message_file, &messageListItem)) {
                        dialog_out(messageListItem.text, NULL, 0, 169, 117, colorTable[32328], NULL, colorTable[32328], 0);
                    }
                }
            }
        } else if (keyCode == KEY_ARROW_UP) {
            if (stack_offset[curr_stack] > 0) {
                stack_offset[curr_stack] -= 1;
                loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_PAGE_UP) {
            if (critterCount != 0) {
                if (critterIndex > 0) {
                    critterIndex -= 1;
                } else {
                    critterIndex = critterCount - 1;
                }

                a2 = critters[critterIndex];
                target_pud = &(a2->data.inventory);
                target_stack[0] = a2;
                target_curr_stack = 0;
                target_stack_offset[0] = 0;
                loot_display_target_inventory(0, -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                loot_display_body(a2->fid, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (stack_offset[curr_stack] + inven_cur_disp < pud->length) {
                stack_offset[curr_stack] += 1;
                loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_PAGE_DOWN) {
            if (critterCount != 0) {
                if (critterIndex < critterCount - 1) {
                    critterIndex += 1;
                } else {
                    critterIndex = 0;
                }

                a2 = critters[critterIndex];
                target_pud = &(a2->data.inventory);
                target_stack[0] = a2;
                target_curr_stack = 0;
                target_stack_offset[0] = 0;
                loot_display_target_inventory(0, -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                loot_display_body(a2->fid, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            if (target_stack_offset[target_curr_stack] > 0) {
                target_stack_offset[target_curr_stack] -= 1;
                loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                win_draw(i_wid);
            }
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            if (target_stack_offset[target_curr_stack] + inven_cur_disp < target_pud->length) {
                target_stack_offset[target_curr_stack] += 1;
                loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                win_draw(i_wid);
            }
        } else if (keyCode >= 2500 && keyCode <= 2501) {
            loot_container_exit(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
        } else {
            if ((mouse_get_buttons() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (immode == INVENTORY_WINDOW_CURSOR_HAND) {
                    loot_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);
                } else {
                    loot_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);
                }
            } else if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode <= 1000 + inven_cur_disp) {
                    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
                        loot_action_cursor(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
                    } else {
                        int v40 = keyCode - 1000;
                        if (v40 + stack_offset[curr_stack] < pud->length) {
                            gStealCount += 1;
                            gStealSize += item_size(stack[curr_stack]);

                            InventoryItem* inventoryItem = &(pud->items[pud->length - (v40 + stack_offset[curr_stack] + 1)]);
                            int rc = loot_move_inventory(inventoryItem->item, v40, target_stack[target_curr_stack], true);
                            if (rc == 1) {
                                isCaughtStealing = true;
                            } else if (rc == 2) {
                                stealingXp += stealingXpBonus;
                                stealingXpBonus += 10;
                            }

                            loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }

                        keyCode = -1;
                    }
                } else if (keyCode >= 2000 && keyCode <= 2000 + inven_cur_disp) {
                    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
                        loot_action_cursor(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
                    } else {
                        int v46 = keyCode - 2000;
                        if (v46 + target_stack_offset[target_curr_stack] < target_pud->length) {
                            gStealCount += 1;
                            gStealSize += item_size(stack[curr_stack]);

                            InventoryItem* inventoryItem = &(target_pud->items[target_pud->length - (v46 + target_stack_offset[target_curr_stack] + 1)]);
                            int rc = loot_move_inventory(inventoryItem->item, v46, target_stack[target_curr_stack], false);
                            if (rc == 1) {
                                isCaughtStealing = true;
                            } else if (rc == 2) {
                                stealingXp += stealingXpBonus;
                                stealingXpBonus += 10;
                            }

                            loot_display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            loot_display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }
                    }
                }
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }
    }

    if (critterCount != 0) {
        obj_delete_list(critters);

        for (int index = 0; index < INVENTORY_ARROW_FRM_COUNT; index++) {
            art_ptr_unlock(arrowFrmHandles[index]);
        }
    }

    if (gIsSteal) {
        if (item1 != NULL) {
            item1->flags |= OBJECT_IN_LEFT_HAND;
            item_add_force(a2, item1, 1);
        }

        if (item2 != NULL) {
            item2->flags |= OBJECT_IN_RIGHT_HAND;
            item_add_force(a2, item2, 1);
        }

        if (armor != NULL) {
            armor->flags |= OBJECT_WORN;
            item_add_force(a2, armor, 1);
        }
    }

    item_move_all(a1a, a2);
    obj_erase_object(a1a, NULL);

    if (gIsSteal) {
        if (!isCaughtStealing) {
            if (stealingXp > 0) {
                if (!isPartyMember(a2)) {
                    stealingXp = min(300 - skill_level(a1, SKILL_STEAL), stealingXp);
                    debug_printf("\n[[[%d]]]", 300 - skill_level(a1, SKILL_STEAL));

                    // You gain %d experience points for successfully using your Steal skill.
                    messageListItem.num = 29;
                    if (message_search(&loot_message_file, &messageListItem)) {
                        char formattedText[200];
                        sprintf(formattedText, messageListItem.text, stealingXp);
                        display_print(formattedText);
                    }

                    stat_pc_add_experience(stealingXp);
                }
            }
        }
    }

    loot_exit_inventory(isoWasEnabled);

    loot_uninit();

    if (gIsSteal) {
        if (isCaughtStealing) {
            if (gStealCount > 0) {
                if (obj_sid(a2, &sid) != -1) {
                    scr_set_objs(sid, a1, NULL);
                    exec_script_proc(sid, SCRIPT_PROC_PICKUP);

                    Script* script;
                    scr_ptr(sid, &script);
                }
            }
        }
    }

    return 0;
}

int inven_steal_container(Object* a1, Object* a2)
{
    if (a1 == a2) {
        return -1;
    }

    gIsSteal = PID_TYPE(a1->pid) == OBJ_TYPE_CRITTER && critter_is_active(a2);
    gStealCount = 0;
    gStealSize = 0;

    int rc = loot_container(a1, a2);

    gIsSteal = 0;
    gStealCount = 0;
    gStealSize = 0;

    return rc;
}

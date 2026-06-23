#include "game/inventry.h"

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
#include "game/gdialog.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "game/intface.h"
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

#define INVENTORY_WINDOW_X 80
#define INVENTORY_WINDOW_Y 0


#define INVENTORY_LARGE_SLOT_WIDTH 90
#define INVENTORY_LARGE_SLOT_HEIGHT 61

#define INVENTORY_SLOT_WIDTH 64
#define INVENTORY_SLOT_HEIGHT 48

#define INVENTORY_LEFT_HAND_SLOT_X 154
#define INVENTORY_LEFT_HAND_SLOT_Y 286
#define INVENTORY_LEFT_HAND_SLOT_MAX_X (INVENTORY_LEFT_HAND_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_LEFT_HAND_SLOT_MAX_Y (INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)

#define INVENTORY_RIGHT_HAND_SLOT_X 245
#define INVENTORY_RIGHT_HAND_SLOT_Y 286
#define INVENTORY_RIGHT_HAND_SLOT_MAX_X (INVENTORY_RIGHT_HAND_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_RIGHT_HAND_SLOT_MAX_Y (INVENTORY_RIGHT_HAND_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)

#define INVENTORY_ARMOR_SLOT_X 154
#define INVENTORY_ARMOR_SLOT_Y 183
#define INVENTORY_ARMOR_SLOT_MAX_X (INVENTORY_ARMOR_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_ARMOR_SLOT_MAX_Y (INVENTORY_ARMOR_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)








#define INVENTORY_LOOT_LEFT_SCROLLER_X 176
#define INVENTORY_LOOT_LEFT_SCROLLER_Y 37
#define INVENTORY_LOOT_LEFT_SCROLLER_MAX_X (INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_LOOT_RIGHT_SCROLLER_X 297
#define INVENTORY_LOOT_RIGHT_SCROLLER_Y 37
#define INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X (INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_SCROLLER_X 44
#define INVENTORY_SCROLLER_Y 35
#define INVENTORY_SCROLLER_MAX_X (INVENTORY_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_BODY_VIEW_WIDTH 60
#define INVENTORY_BODY_VIEW_HEIGHT 100

#define INVENTORY_PC_BODY_VIEW_X 176
#define INVENTORY_PC_BODY_VIEW_Y 37
#define INVENTORY_PC_BODY_VIEW_MAX_X (INVENTORY_PC_BODY_VIEW_X + INVENTORY_BODY_VIEW_WIDTH)
#define INVENTORY_PC_BODY_VIEW_MAX_Y (INVENTORY_PC_BODY_VIEW_Y + INVENTORY_BODY_VIEW_HEIGHT)

#define INVENTORY_LOOT_RIGHT_BODY_VIEW_X 422
#define INVENTORY_LOOT_RIGHT_BODY_VIEW_Y 35

#define INVENTORY_LOOT_LEFT_BODY_VIEW_X 44
#define INVENTORY_LOOT_LEFT_BODY_VIEW_Y 35

// NOTE: CE uses relative coordinates for hit testing for which coordinates
// above is enough. However RE requires separate sets of coordinates as it
// performs hit tests in screen coordinates.

#define INVENTORY_LEFT_HAND_SLOT_ABS_X (i_wid_x + INVENTORY_LEFT_HAND_SLOT_X)
#define INVENTORY_LEFT_HAND_SLOT_ABS_Y (i_wid_y + INVENTORY_LEFT_HAND_SLOT_Y)
#define INVENTORY_LEFT_HAND_SLOT_ABS_MAX_X (i_wid_x + INVENTORY_LEFT_HAND_SLOT_MAX_X)
#define INVENTORY_LEFT_HAND_SLOT_ABS_MAX_Y (i_wid_y + INVENTORY_LEFT_HAND_SLOT_MAX_Y)

#define INVENTORY_RIGHT_HAND_SLOT_ABS_X (i_wid_x + INVENTORY_RIGHT_HAND_SLOT_X)
#define INVENTORY_RIGHT_HAND_SLOT_ABS_Y (i_wid_y + INVENTORY_RIGHT_HAND_SLOT_Y)
#define INVENTORY_RIGHT_HAND_SLOT_ABS_MAX_X (i_wid_x + INVENTORY_RIGHT_HAND_SLOT_MAX_X)
#define INVENTORY_RIGHT_HAND_SLOT_ABS_MAX_Y (i_wid_y + INVENTORY_RIGHT_HAND_SLOT_MAX_Y)

#define INVENTORY_ARMOR_SLOT_ABS_X (i_wid_x + INVENTORY_ARMOR_SLOT_X)
#define INVENTORY_ARMOR_SLOT_ABS_Y (i_wid_y + INVENTORY_ARMOR_SLOT_Y)
#define INVENTORY_ARMOR_SLOT_ABS_MAX_X (i_wid_x + INVENTORY_ARMOR_SLOT_MAX_X)
#define INVENTORY_ARMOR_SLOT_ABS_MAX_Y (i_wid_y + INVENTORY_ARMOR_SLOT_MAX_Y)





#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_X (i_wid_x + INVENTORY_LOOT_LEFT_SCROLLER_X)
#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_Y (i_wid_y + INVENTORY_LOOT_LEFT_SCROLLER_Y)
#define INVENTORY_LOOT_LEFT_SCROLLER_ABS_MAX_X (i_wid_x + INVENTORY_LOOT_LEFT_SCROLLER_MAX_X)

#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_X (i_wid_x + INVENTORY_LOOT_RIGHT_SCROLLER_X)
#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_Y (i_wid_y + INVENTORY_LOOT_RIGHT_SCROLLER_Y)
#define INVENTORY_LOOT_RIGHT_SCROLLER_ABS_MAX_X (i_wid_x + INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X)

#define INVENTORY_SCROLLER_ABS_X (i_wid_x + INVENTORY_SCROLLER_X)
#define INVENTORY_SCROLLER_ABS_Y (i_wid_y + INVENTORY_SCROLLER_Y)
#define INVENTORY_SCROLLER_ABS_MAX_X (i_wid_x + INVENTORY_SCROLLER_MAX_X)

#define INVENTORY_PC_BODY_VIEW_ABS_X (i_wid_x + INVENTORY_PC_BODY_VIEW_X)
#define INVENTORY_PC_BODY_VIEW_ABS_Y (i_wid_y + INVENTORY_PC_BODY_VIEW_Y)
#define INVENTORY_PC_BODY_VIEW_ABS_MAX_X (i_wid_x + INVENTORY_PC_BODY_VIEW_MAX_X)
#define INVENTORY_PC_BODY_VIEW_ABS_MAX_Y (i_wid_y + INVENTORY_PC_BODY_VIEW_MAX_Y)

#define INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY (1000U / ROTATION_COUNT)

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
static void inven_update_lighting(Object* a1);
static int do_move_timer(int inventoryWindowType, Object* item, int a3);
static int setup_move_timer_win(int inventoryWindowType, Object* item);
static int exit_move_timer_win(int inventoryWindowType);

// The number of items to show in scroller.
//
// 0x519054
static int inven_cur_disp = 6;

// 0x519058
static Object* inven_dude = NULL;

// Probably fid of armor to display in inventory dialog.
//
// 0x51905C
static int inven_pid = -1;

// 0x519060
static bool inven_is_initialized = false;

// 0x519064
static int inven_display_msg_line = 1;

// 0x519068
static InventoryWindowDescription iscr_data[INVENTORY_WINDOW_TYPE_COUNT] = {
    { 48, 499, 377, 80, 0 },
    { 113, 292, 376, 80, 0 },
    { 114, 537, 376, 80, 0 },
    { 305, 259, 162, 140, 80 },
    { 305, 259, 162, 140, 80 },
};

// 0x5190E0
static bool dropped_explosive = false;

// 0x5190E4
static int inven_scroll_up_bid = -1;

// 0x5190E8
static int inven_scroll_dn_bid = -1;

// 0x5190EC
static int loot_scroll_up_bid = -1;

// 0x5190F0
static int loot_scroll_dn_bid = -1;

// 0x59E79C
static CacheEntry* mt_key[8];

// 0x59E7BC
CacheEntry* ikey[OFF_59E7BC_COUNT];

// 0x59E7EC
static int target_stack_offset[10];

// inventory.msg
//
// 0x59E814
static MessageList inventry_message_file;

// 0x59E81C
static Object* target_stack[10];

// 0x59E844
static int stack_offset[10];

// 0x59E86C
static Object* stack[10];

// 0x59E894
static int mt_wid;

// 0x59E8A8
static InventoryCursorData imdata[INVENTORY_WINDOW_CURSOR_COUNT];

// 0x59E938
static InventoryPrintItemDescriptionHandler* display_msg;

// 0x59E93C
static int im_value;

// 0x59E940
static int immode;

// 0x59E948
static int target_curr_stack;

// 0x59E950
static bool inven_ui_was_disabled;

// 0x59E954
static Object* i_worn;

// 0x59E958
static Object* i_lhand;

// Rotating character's fid.
//
// 0x59E95C
static int i_fid;

// 0x59E960
static Inventory* pud;

// 0x59E964
static int i_wid;

// item2
// 0x59E968
static Object* i_rhand;

// 0x59E96C
static int curr_stack;

// 0x59E970
static int i_wid_max_y;

// 0x59E974
static int i_wid_max_x;

// Runtime inventory window position (for absolute hit testing).
static int i_wid_x;
static int i_wid_y;

// 0x59E978
static Inventory* target_pud;

// NOTE: Unused.
//
// 0x46E718
void inven_set_dude(Object* obj, int pid)
{
    inven_dude = obj;
    inven_pid = pid;
}

// 0x46E724
void inven_reset_dude()
{
    inven_dude = obj_dude;
    inven_pid = 0x1000000;
}

// 0x46E73C
static int inventry_msg_load()
{
    char path[MAX_PATH];

    if (!message_init(&inventry_message_file))
        return -1;

    sprintf(path, "%s%s", msg_path, "inventry.msg");
    if (!message_load(&inventry_message_file, path))
        return -1;

    return 0;
}

// 0x46E7A0
static int inventry_msg_unload()
{
    message_exit(&inventry_message_file);
    return 0;
}

// 0x46E7B0
void handle_inventory()
{
    if (isInCombat()) {
        if (combat_whose_turn() != inven_dude) {
            return;
        }
    }

    if (inven_init() == -1) {
        return;
    }

    if (isInCombat()) {
        if (inven_dude == obj_dude) {
            int actionPointsRequired = 4 - 2 * perk_level(inven_dude, PERK_QUICK_POCKETS);
            if (actionPointsRequired > 0 && actionPointsRequired > obj_dude->data.critter.combat.ap) {
                // You don't have enough action points to use inventory
                MessageListItem messageListItem;
                messageListItem.num = 19;
                if (message_search(&inventry_message_file, &messageListItem)) {
                    display_print(messageListItem.text);
                }

                // NOTE: Uninline.
                inven_exit();

                return;
            }

            if (actionPointsRequired > 0) {
                if (actionPointsRequired > obj_dude->data.critter.combat.ap) {
                    obj_dude->data.critter.combat.ap = 0;
                } else {
                    obj_dude->data.critter.combat.ap -= actionPointsRequired;
                }
                intface_update_move_points(obj_dude->data.critter.combat.ap, combat_free_move);
            }
        }
    }

    Object* oldArmor = inven_worn(inven_dude);
    bool isoWasEnabled = setup_inventory(INVENTORY_WINDOW_TYPE_NORMAL);
    register_clear(inven_dude);
    display_stats();
    display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
    inven_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);

    for (;;) {
        int keyCode = get_input();

        if (keyCode == KEY_ESCAPE) {
            break;
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);

        if (game_state() == GAME_STATE_5) {
            break;
        }

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X) {
            game_quit_with_confirm();
        } else if (keyCode == KEY_HOME) {
            stack_offset[curr_stack] = 0;
            display_inventory(0, -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_ARROW_UP) {
            if (stack_offset[curr_stack] > 0) {
                stack_offset[curr_stack] -= 1;
                display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
            }
        } else if (keyCode == KEY_PAGE_UP) {
            stack_offset[curr_stack] -= inven_cur_disp;
            if (stack_offset[curr_stack] < 0) {
                stack_offset[curr_stack] = 0;
            }
            display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_END) {
            stack_offset[curr_stack] = pud->length - inven_cur_disp;
            if (stack_offset[curr_stack] < 0) {
                stack_offset[curr_stack] = 0;
            }
            display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (inven_cur_disp + stack_offset[curr_stack] < pud->length) {
                stack_offset[curr_stack] += 1;
                display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
            }
        } else if (keyCode == KEY_PAGE_DOWN) {
            int v12 = inven_cur_disp + stack_offset[curr_stack];
            int v13 = v12 + inven_cur_disp;
            stack_offset[curr_stack] = v12;
            int v14 = pud->length;
            if (v13 >= pud->length) {
                int v15 = v14 - inven_cur_disp;
                stack_offset[curr_stack] = v14 - inven_cur_disp;
                if (v15 < 0) {
                    stack_offset[curr_stack] = 0;
                }
            }
            display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == 2500) {
            container_exit(keyCode, INVENTORY_WINDOW_TYPE_NORMAL);
        } else {
            if ((mouse_get_buttons() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (immode == INVENTORY_WINDOW_CURSOR_HAND) {
                    inven_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);
                } else if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
                    inven_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);
                    display_stats();
                    win_draw(i_wid);
                }
            } else if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode <= 1008) {
                    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inven_action_cursor(keyCode, INVENTORY_WINDOW_TYPE_NORMAL);
                    } else {
                        inven_pickup(keyCode, stack_offset[curr_stack]);
                    }
                }
            }
        }
    }

    inven_dude = stack[0];
    adjust_fid();

    if (inven_dude == obj_dude) {
        Rect rect;
        obj_change_fid(inven_dude, i_fid, &rect);
        tile_refresh_rect(&rect, inven_dude->elevation);
    }

    Object* newArmor = inven_worn(inven_dude);
    if (inven_dude == obj_dude) {
        if (oldArmor != newArmor) {
            intface_update_ac(true);
        }
    }

    exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inven_exit();

    if (inven_dude == obj_dude) {
        intface_update_items(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
}

// 0x46EC90
bool setup_inventory(int inventoryWindowType)
{
    dropped_explosive = 0;
    curr_stack = 0;
    stack_offset[0] = 0;
    inven_cur_disp = 6;
    pud = &(inven_dude->data.inventory);
    stack[0] = inven_dude;

    if (inventoryWindowType <= INVENTORY_WINDOW_TYPE_LOOT) {
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

        // Update the window description with actual position for coordinate conversion.
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
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        // Create invsibile buttons representing character's inventory item
        // slots.
        for (int index = 0; index < inven_cur_disp; index++) {
            int btn = win_register_button(i_wid, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_SLOT_HEIGHT * (inven_cur_disp - index - 1) + INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, 999 + inven_cur_disp - index, -1, 999 + inven_cur_disp - index, -1, NULL, NULL, NULL, 0);
            if (btn != -1) {
                win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
            }
        }

        int eventCode = 2005;
        int y = INVENTORY_SLOT_HEIGHT * 5 + INVENTORY_LOOT_LEFT_SCROLLER_Y;

        // Create invisible buttons representing container's inventory item
        // slots. For unknown reason it loops backwards and it's size is
        // hardcoded at 6 items.
        //
        // Original code is slightly different. It loops until y reaches -11,
        // which is a bit awkward for a loop. Probably result of some
        // optimization.
        for (int index = 0; index < 6; index++) {
            int btn = win_register_button(i_wid, INVENTORY_LOOT_RIGHT_SCROLLER_X, y, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, eventCode, -1, eventCode, -1, NULL, NULL, NULL, 0);
            if (btn != -1) {
                win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
            }

            eventCode -= 1;
            y -= INVENTORY_SLOT_HEIGHT;
        }
    } else {
        // Create invisible buttons representing item slots.
        for (int index = 0; index < inven_cur_disp; index++) {
            int btn = win_register_button(i_wid,
                INVENTORY_SCROLLER_X,
                INVENTORY_SLOT_HEIGHT * (inven_cur_disp - index - 1) + INVENTORY_SCROLLER_Y,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                999 + inven_cur_disp - index,
                -1,
                999 + inven_cur_disp - index,
                -1,
                NULL,
                NULL,
                NULL,
                0);
            if (btn != -1) {
                win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
            }
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        int btn;

        // Item2 slot
        btn = win_register_button(i_wid, INVENTORY_RIGHT_HAND_SLOT_X, INVENTORY_RIGHT_HAND_SLOT_Y, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, 1006, -1, 1006, -1, NULL, NULL, NULL, 0);
        if (btn != -1) {
            win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
        }

        // Item1 slot
        btn = win_register_button(i_wid, INVENTORY_LEFT_HAND_SLOT_X, INVENTORY_LEFT_HAND_SLOT_Y, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, 1007, -1, 1007, -1, NULL, NULL, NULL, 0);
        if (btn != -1) {
            win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
        }

        // Armor slot
        btn = win_register_button(i_wid, INVENTORY_ARMOR_SLOT_X, INVENTORY_ARMOR_SLOT_Y, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, 1008, -1, 1008, -1, NULL, NULL, NULL, 0);
        if (btn != -1) {
            win_register_button_func(btn, inven_hover_on, inven_hover_off, NULL, NULL);
        }
    }

    memset(ikey, 0, sizeof(ikey));

    int fid;
    int btn;
    unsigned char* buttonUpData;
    unsigned char* buttonDownData;
    unsigned char* buttonDisabledData;

    fid = art_id(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    buttonUpData = art_ptr_lock_data(fid, 0, 0, &(ikey[0]));

    fid = art_id(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    buttonDownData = art_ptr_lock_data(fid, 0, 0, &(ikey[1]));

    if (buttonUpData != NULL && buttonDownData != NULL) {
        btn = -1;
        switch (inventoryWindowType) {
        case INVENTORY_WINDOW_TYPE_NORMAL:
            // Done button
            btn = win_register_button(i_wid, 437, 329, 15, 16, -1, -1, -1, KEY_ESCAPE, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
            break;
        case INVENTORY_WINDOW_TYPE_USE_ITEM_ON:
            // Cancel button
            btn = win_register_button(i_wid, 233, 328, 15, 16, -1, -1, -1, KEY_ESCAPE, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
            break;
        case INVENTORY_WINDOW_TYPE_LOOT:
            // Done button
            btn = win_register_button(i_wid, 476, 331, 15, 16, -1, -1, -1, KEY_ESCAPE, buttonUpData, buttonDownData, NULL, BUTTON_FLAG_TRANSPARENT);
            break;
        }

        if (btn != -1) {
            win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
        }
    }

    {
        // Large up arrow (normal).
        fid = art_id(OBJ_TYPE_INTERFACE, 49, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(ikey[2]));

        // Large up arrow (pressed).
        fid = art_id(OBJ_TYPE_INTERFACE, 50, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(ikey[3]));

        // Large up arrow (disabled).
        fid = art_id(OBJ_TYPE_INTERFACE, 53, 0, 0, 0);
        buttonDisabledData = art_ptr_lock_data(fid, 0, 0, &(ikey[4]));

        if (buttonUpData != NULL && buttonDownData != NULL && buttonDisabledData != NULL) {
            // Left inventory up button.
            inven_scroll_up_bid = win_register_button(i_wid, 128, 39, 22, 23, -1, -1, KEY_ARROW_UP, -1, buttonUpData, buttonDownData, NULL, 0);
            if (inven_scroll_up_bid != -1) {
                win_register_button_disable(inven_scroll_up_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
                win_register_button_sound_func(inven_scroll_up_bid, gsound_red_butt_press, gsound_red_butt_release);
                win_disable_button(inven_scroll_up_bid);
            }

            if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                // Right inventory up button.
                loot_scroll_up_bid = win_register_button(i_wid, 379, 39, 22, 23, -1, -1, KEY_CTRL_ARROW_UP, -1, buttonUpData, buttonDownData, NULL, 0);
                if (loot_scroll_up_bid != -1) {
                    win_register_button_disable(loot_scroll_up_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
                    win_register_button_sound_func(loot_scroll_up_bid, gsound_red_butt_press, gsound_red_butt_release);
                    win_disable_button(loot_scroll_up_bid);
                }
            }
        }
    }

    {
        // Large arrow down (normal).
        fid = art_id(OBJ_TYPE_INTERFACE, 51, 0, 0, 0);
        buttonUpData = art_ptr_lock_data(fid, 0, 0, &(ikey[5]));

        // Large arrow down (pressed).
        fid = art_id(OBJ_TYPE_INTERFACE, 52, 0, 0, 0);
        buttonDownData = art_ptr_lock_data(fid, 0, 0, &(ikey[6]));

        // Large arrow down (disabled).
        fid = art_id(OBJ_TYPE_INTERFACE, 54, 0, 0, 0);
        buttonDisabledData = art_ptr_lock_data(fid, 0, 0, &(ikey[7]));

        if (buttonUpData != NULL && buttonDownData != NULL && buttonDisabledData != NULL) {
            // Left inventory down button.
            inven_scroll_dn_bid = win_register_button(i_wid, 128, 62, 22, 23, -1, -1, KEY_ARROW_DOWN, -1, buttonUpData, buttonDownData, NULL, 0);
            win_register_button_sound_func(inven_scroll_dn_bid, gsound_red_butt_press, gsound_red_butt_release);
            win_register_button_disable(inven_scroll_dn_bid, buttonDisabledData, buttonDisabledData, buttonDisabledData);
            win_disable_button(inven_scroll_dn_bid);

            if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
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
            } else {
                // Invisible button representing character (in inventory and use on dialogs).
                win_register_button(i_wid, INVENTORY_PC_BODY_VIEW_X, INVENTORY_PC_BODY_VIEW_Y, INVENTORY_BODY_VIEW_WIDTH, INVENTORY_BODY_VIEW_HEIGHT, -1, -1, 2500, -1, NULL, NULL, NULL, 0);
            }
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        if (!gIsSteal) {
            // Take all button (normal)
            fid = art_id(OBJ_TYPE_INTERFACE, 436, 0, 0, 0);
            buttonUpData = art_ptr_lock_data(fid, 0, 0, &(ikey[8]));

            // Take all button (pressed)
            fid = art_id(OBJ_TYPE_INTERFACE, 437, 0, 0, 0);
            buttonDownData = art_ptr_lock_data(fid, 0, 0, &(ikey[9]));

            if (buttonUpData != NULL && buttonDownData != NULL) {
                // Take all button.
                btn = win_register_button(i_wid, 432, 204, 39, 41, -1, -1, KEY_UPPERCASE_A, -1, buttonUpData, buttonDownData, NULL, 0);
                if (btn != -1) {
                    win_register_button_sound_func(btn, gsound_red_butt_press, gsound_red_butt_release);
                }
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

    adjust_fid();

    bool isoWasEnabled = map_disable_bk_processes();

    gmouse_disable(0);

    return isoWasEnabled;
}

// 0x46FBD8
void exit_inventory(bool shouldEnableIso)
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

    for (int index = 0; index < OFF_59E7BC_COUNT; index++) {
        art_ptr_unlock(ikey[index]);
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

// 0x46FDF4
void display_inventory(int a1, int a2, int inventoryWindowType)
{
    unsigned char* windowBuffer = win_get_buf(i_wid);
    int pitch;

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        pitch = 499;

        int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);

        CacheEntry* backgroundFrmHandle;
        unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
        if (backgroundFrmData != NULL) {
            // Clear scroll view background.
            buf_to_buf(backgroundFrmData + (pitch * 35 + 44) * 4, INVENTORY_SLOT_WIDTH, inven_cur_disp * INVENTORY_SLOT_HEIGHT, pitch, windowBuffer + (pitch * 35 + 44) * 4, pitch);

            // Clear armor button background.
            buf_to_buf(backgroundFrmData + (pitch * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X) * 4, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, pitch, windowBuffer + (pitch * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X) * 4, pitch);

            if (i_lhand != NULL && i_lhand == i_rhand) {
                // Clear item1.
                int itemBackgroundFid = art_id(OBJ_TYPE_INTERFACE, 32, 0, 0, 0);

                CacheEntry* itemBackgroundFrmHandle;
                Art* itemBackgroundFrm = art_ptr_lock(itemBackgroundFid, &itemBackgroundFrmHandle);
                if (itemBackgroundFrm != NULL) {
                    unsigned char* data = art_frame_data(itemBackgroundFrm, 0, 0);
                    int width = art_frame_width(itemBackgroundFrm, 0, 0);
                    int height = art_frame_length(itemBackgroundFrm, 0, 0);
                    buf_to_buf(data, width, height, width, windowBuffer + (pitch * 284 + 152) * 4, pitch);
                    art_ptr_unlock(itemBackgroundFrmHandle);
                }
            } else {
                // Clear both items in one go.
                buf_to_buf(backgroundFrmData + (pitch * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X) * 4, INVENTORY_LARGE_SLOT_WIDTH * 2, INVENTORY_LARGE_SLOT_HEIGHT, pitch, windowBuffer + (pitch * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X) * 4, pitch);
            }

            art_ptr_unlock(backgroundFrmHandle);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_USE_ITEM_ON) {
        pitch = 292;

        int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 113, 0, 0, 0);

        CacheEntry* backgroundFrmHandle;
        unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
        if (backgroundFrmData != NULL) {
            // Clear scroll view background.
            buf_to_buf(backgroundFrmData + (pitch * 35 + 44) * 4, 64, inven_cur_disp * 48, pitch, windowBuffer + (pitch * 35 + 44) * 4, pitch);
            art_ptr_unlock(backgroundFrmHandle);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        pitch = 537;

        int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);

        CacheEntry* backgroundFrmHandle;
        unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
        if (backgroundFrmData != NULL) {
            // Clear scroll view background.
            buf_to_buf(backgroundFrmData + (pitch * 37 + 176) * 4, 64, inven_cur_disp * 48, pitch, windowBuffer + (pitch * 37 + 176) * 4, pitch);
            art_ptr_unlock(backgroundFrmHandle);
        }
    } else {
        assert(false && "Should be unreachable");
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL
        || inventoryWindowType == INVENTORY_WINDOW_TYPE_USE_ITEM_ON
        || inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
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
    }

    int y = 0;
    for (int v19 = 0; v19 + a1 < pud->length && v19 < inven_cur_disp; v19 += 1) {
        int v21 = v19 + a1 + 1;

        int width = 56;
        int offset;
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            offset = pitch * (y + 41) + 180;
        } else {
            offset = pitch * (y + 39) + 48;
        }

        InventoryItem* inventoryItem = &(pud->items[pud->length - v21]);

        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + offset * 4, width, 40, pitch);

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            offset = pitch * (y + 41) + 180;
        } else {
            offset = pitch * (y + 39) + 48;
        }

        display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset * 4, pitch, v19 == a2);

        y += 48;
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        if (i_rhand != NULL) {
            int width = i_rhand == i_lhand ? INVENTORY_LARGE_SLOT_WIDTH * 2 : INVENTORY_LARGE_SLOT_WIDTH;
            int inventoryFid = item_inv_fid(i_rhand);
            scale_art(inventoryFid, windowBuffer + (499 * INVENTORY_RIGHT_HAND_SLOT_Y + INVENTORY_RIGHT_HAND_SLOT_X) * 4, width, INVENTORY_LARGE_SLOT_HEIGHT, 499);
        }

        if (i_lhand != NULL && i_lhand != i_rhand) {
            int inventoryFid = item_inv_fid(i_lhand);
            scale_art(inventoryFid, windowBuffer + (499 * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X) * 4, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, 499);
        }

        if (i_worn != NULL) {
            int inventoryFid = item_inv_fid(i_worn);
            scale_art(inventoryFid, windowBuffer + (499 * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X) * 4, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, 499);
        }
    }

    win_draw(i_wid);
}

// Render inventory item.
//
// [a1] is likely an index of the first visible item in the scrolling view.
// [a2] is likely an index of selected item or moving item (it decreases displayed number of items in inner functions).
//
// 0x47036C
void display_target_inventory(int a1, int a2, Inventory* inventory, int inventoryWindowType)
{
    unsigned char* windowBuffer = win_get_buf(i_wid);

    int pitch;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        pitch = 537;

        int fid = art_id(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);

        CacheEntry* handle;
        unsigned char* data = art_ptr_lock_data(fid, 0, 0, &handle);
        if (data != NULL) {
            buf_to_buf(data + (537 * 37 + 297) * 4, 64, 48 * inven_cur_disp, 537, windowBuffer + (537 * 37 + 297) * 4, 537);
            art_ptr_unlock(handle);
        }
    } else {
        assert(false && "Should be unreachable");
    }

    int y = 0;
    for (int index = 0; index < inven_cur_disp; index++) {
        int v27 = a1 + index;
        if (v27 >= inventory->length) {
            break;
        }

        int offset;
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            offset = pitch * (y + 41) + 301;
        } else {
            assert(false && "Should be unreachable");
        }

        InventoryItem* inventoryItem = &(inventory->items[inventory->length - (v27 + 1)]);
        int inventoryFid = item_inv_fid(inventoryItem->item);
        scale_art(inventoryFid, windowBuffer + offset * 4, 56, 40, pitch);
        display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset * 4, pitch, index == a2);

        y += 48;
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
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
}

// Renders inventory item quantity.
//
// 0x4705A0
static void display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool a5)
{
    int oldFont = text_curr();
    text_font(101);

    char formattedText[12];

    // NOTE: Original code is slightly different and probably used goto.
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

            // NOTE: Checking for quantity twice probably means inlined function
            // or some macro expansion.
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

// 0x470650
void display_body(int fid, int inventoryWindowType)
{
    // 0x5190F4
    static unsigned int ticker = 0;

    // 0x5190F8
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
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                    rect.ulx = 426;
                    rect.uly = 39;
                } else {
                    rect.ulx = 297;
                    rect.uly = 37;
                }
            } else {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                    rect.ulx = 48;
                    rect.uly = 39;
                } else {
                    rect.ulx = 176;
                    rect.uly = 37;
                }
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

// 0x470A2C
int inven_init()
{
    // 0x5190FC
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

        message_exit(&inventry_message_file);

        return -1;
    }

    inven_is_initialized = true;
    im_value = -1;

    return 0;
}

// NOTE: Inlined.
//
// 0x470B8C
void inven_exit()
{
    for (int index = 0; index < INVENTORY_WINDOW_CURSOR_COUNT; index++) {
        art_ptr_unlock(imdata[index].frmHandle);
    }

    if (inven_ui_was_disabled) {
        game_ui_disable(0);
    }

    // NOTE: Uninline.
    inventry_msg_unload();

    inven_is_initialized = 0;
}

// 0x470BCC
void inven_set_mouse(int cursor)
{
    immode = cursor;

    if (cursor != INVENTORY_WINDOW_CURSOR_ARROW || im_value == -1) {
        InventoryCursorData* cursorData = &(imdata[cursor]);
        mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    } else {
        inven_hover_on(-1, im_value);
    }
}

// 0x470C2C
void inven_hover_on(int btn, int keyCode)
{
    // 0x519110
    static Object* last_target = NULL;

    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
        int x;
        int y;
        mouse_get_position(&x, &y);

        Object* a2a = NULL;
        if (inven_from_button(keyCode, &a2a, NULL, NULL) != 0) {
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

// 0x470D1C
void inven_hover_off(int btn, int keyCode)
{
    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
        InventoryCursorData* cursorData = &(imdata[INVENTORY_WINDOW_CURSOR_ARROW]);
        mouse_set_shape(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    }

    im_value = -1;
}

// 0x470D5C
static void inven_update_lighting(Object* a1)
{
    if (obj_dude == inven_dude) {
        int lightDistance;
        if (a1 != NULL && a1->lightDistance > 4) {
            lightDistance = a1->lightDistance;
        } else {
            lightDistance = 4;
        }

        Rect rect;
        obj_set_light(inven_dude, lightDistance, 0x10000, &rect);
        tile_refresh_rect(&rect, map_elevation);
    }
}

// 0x470DB8
void inven_pickup(int keyCode, int a2)
{
    Object* a1a;
    Object** v29 = NULL;
    int count = inven_from_button(keyCode, &a1a, &v29, NULL);
    if (count == 0) {
        return;
    }

    int v3 = -1;
    Object* v39 = NULL;
    Rect rect;

    switch (keyCode) {
    case 1006:
        rect.ulx = 245;
        rect.uly = 286;
        if (inven_dude == obj_dude && intface_is_item_right_hand() != HAND_LEFT) {
            v39 = a1a;
        }
        break;
    case 1007:
        rect.ulx = 154;
        rect.uly = 286;
        if (inven_dude == obj_dude && intface_is_item_right_hand() == HAND_LEFT) {
            v39 = a1a;
        }
        break;
    case 1008:
        rect.ulx = 154;
        rect.uly = 183;
        break;
    default:
        // NOTE: Original code a little bit different, this code path
        // is only for key codes below 1006.
        v3 = keyCode - 1000;
        rect.ulx = 44;
        rect.uly = 48 * v3 + 35;
        break;
    }

    if (v3 == -1 || pud->items[a2 + v3].quantity <= 1) {
        unsigned char* windowBuffer = win_get_buf(i_wid);
        if (i_rhand != i_lhand || a1a != i_lhand) {
            int height;
            int width;
            if (v3 == -1) {
                height = INVENTORY_LARGE_SLOT_HEIGHT;
                width = INVENTORY_LARGE_SLOT_WIDTH;
            } else {
                height = INVENTORY_SLOT_HEIGHT;
                width = INVENTORY_SLOT_WIDTH;
            }

            CacheEntry* backgroundFrmHandle;
            int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
            unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
            if (backgroundFrmData != NULL) {
                buf_to_buf(backgroundFrmData + (499 * rect.uly + rect.ulx) * 4, width, height, 499, windowBuffer + (499 * rect.uly + rect.ulx) * 4, 499);
                art_ptr_unlock(backgroundFrmHandle);
            }

            rect.lrx = rect.ulx + width - 1;
            rect.lry = rect.uly + height - 1;
        } else {
            CacheEntry* backgroundFrmHandle;
            int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
            unsigned char* backgroundFrmData = art_ptr_lock_data(backgroundFid, 0, 0, &backgroundFrmHandle);
            if (backgroundFrmData != NULL) {
                buf_to_buf(backgroundFrmData + (499 * 286 + 154) * 4, 180, 61, 499, windowBuffer + (499 * 286 + 154) * 4, 499);
                art_ptr_unlock(backgroundFrmHandle);
            }

            rect.ulx = 154;
            rect.uly = 286;
            rect.lrx = rect.ulx + 180 - 1;
            rect.lry = rect.uly + 61 - 1;
        }
        win_draw_rect(i_wid, &rect);
    } else {
        display_inventory(a2, v3, INVENTORY_WINDOW_TYPE_NORMAL);
    }

    CacheEntry* itemInventoryFrmHandle;
    int itemInventoryFid = item_inv_fid(a1a);
    Art* itemInventoryFrm = art_ptr_lock(itemInventoryFid, &itemInventoryFrmHandle);
    if (itemInventoryFrm != NULL) {
        int width = art_frame_width(itemInventoryFrm, 0, 0);
        int height = art_frame_length(itemInventoryFrm, 0, 0);
        unsigned char* itemInventoryFrmData = art_frame_data(itemInventoryFrm, 0, 0);
        mouse_set_shape(itemInventoryFrmData, width, height, width, width / 2, height / 2, 0);
        gsound_play_sfx_file("ipickup1");
    }

    if (v39 != NULL) {
        inven_update_lighting(NULL);
    }

    do {
        get_input();
        display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);
    } while ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (itemInventoryFrm != NULL) {
        art_ptr_unlock(itemInventoryFrmHandle);
        gsound_play_sfx_file("iputdown");
    }

    if (mouse_click_in(INVENTORY_SCROLLER_ABS_X, INVENTORY_SCROLLER_ABS_Y, INVENTORY_SCROLLER_ABS_MAX_X, INVENTORY_SLOT_HEIGHT * inven_cur_disp + INVENTORY_SCROLLER_ABS_Y)) {
        int x;
        int y;
        mouse_get_position(&x, &y);

        int v18 = (y - (i_wid_y + 39)) / 48 + a2;
        if (v18 < pud->length) {
            Object* v19 = pud->items[v18].item;
            if (v19 != a1a) {
                // TODO: Needs checking usage of v19
                if (item_get_type(v19) == ITEM_TYPE_CONTAINER) {
                    if (drop_into_container(v19, a1a, v3, v29, count) == 0) {
                        v3 = 0;
                    }
                } else {
                    if (drop_ammo_into_weapon(v19, a1a, v29, count, keyCode) == 0) {
                        v3 = 0;
                    }
                }
            }
        }

        if (v3 == -1) {
            // TODO: Holy shit, needs refactoring.
            *v29 = NULL;
            if (item_add_force(inven_dude, a1a, 1)) {
                *v29 = a1a;
            } else if (v29 == &i_worn) {
                adjust_ac(stack[0], a1a, NULL);
            } else if (i_rhand == i_lhand) {
                i_lhand = NULL;
                i_rhand = NULL;
            }
        }
    } else if (mouse_click_in(INVENTORY_LEFT_HAND_SLOT_ABS_X, INVENTORY_LEFT_HAND_SLOT_ABS_Y, INVENTORY_LEFT_HAND_SLOT_ABS_MAX_X, INVENTORY_LEFT_HAND_SLOT_ABS_MAX_Y)) {
        if (i_lhand != NULL && item_get_type(i_lhand) == ITEM_TYPE_CONTAINER && i_lhand != a1a) {
            drop_into_container(i_lhand, a1a, v3, v29, count);
        } else if (i_lhand == NULL || drop_ammo_into_weapon(i_lhand, a1a, v29, count, keyCode)) {
            switch_hand(a1a, &i_lhand, v29, keyCode);
        }
    } else if (mouse_click_in(INVENTORY_RIGHT_HAND_SLOT_ABS_X, INVENTORY_RIGHT_HAND_SLOT_ABS_Y, INVENTORY_RIGHT_HAND_SLOT_ABS_MAX_X, INVENTORY_RIGHT_HAND_SLOT_ABS_MAX_Y)) {
        if (i_rhand != NULL && item_get_type(i_rhand) == ITEM_TYPE_CONTAINER && i_rhand != a1a) {
            drop_into_container(i_rhand, a1a, v3, v29, count);
        } else if (i_rhand == NULL || drop_ammo_into_weapon(i_rhand, a1a, v29, count, keyCode)) {
            switch_hand(a1a, &i_rhand, v29, v3);
        }
    } else if (mouse_click_in(INVENTORY_ARMOR_SLOT_ABS_X, INVENTORY_ARMOR_SLOT_ABS_Y, INVENTORY_ARMOR_SLOT_ABS_MAX_X, INVENTORY_ARMOR_SLOT_ABS_MAX_Y)) {
        if (item_get_type(a1a) == ITEM_TYPE_ARMOR) {
            Object* v21 = i_worn;
            int v22 = 0;
            if (v3 != -1) {
                item_remove_mult(inven_dude, a1a, 1);
            }

            if (i_worn != NULL) {
                if (v29 != NULL) {
                    *v29 = i_worn;
                } else {
                    i_worn = NULL;
                    v22 = item_add_force(inven_dude, v21, 1);
                }
            } else {
                if (v29 != NULL) {
                    *v29 = i_worn;
                }
            }

            if (v22 != 0) {
                i_worn = v21;
                if (v3 != -1) {
                    item_add_force(inven_dude, a1a, 1);
                }
            } else {
                adjust_ac(stack[0], v21, a1a);
                i_worn = a1a;
            }
        }
    } else if (mouse_click_in(INVENTORY_PC_BODY_VIEW_ABS_X, INVENTORY_PC_BODY_VIEW_ABS_Y, INVENTORY_PC_BODY_VIEW_ABS_MAX_X, INVENTORY_PC_BODY_VIEW_ABS_MAX_Y)) {
        if (curr_stack != 0) {
            // TODO: Check this curr_stack - 1, not sure.
            drop_into_container(stack[curr_stack - 1], a1a, v3, v29, count);
        }
    }

    adjust_fid();
    display_stats();
    display_inventory(a2, -1, INVENTORY_WINDOW_TYPE_NORMAL);
    inven_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);
    if (inven_dude == obj_dude) {
        Object* item;
        if (intface_is_item_right_hand() == HAND_LEFT) {
            item = inven_left_hand(inven_dude);
        } else {
            item = inven_right_hand(inven_dude);
        }

        if (item != NULL) {
            inven_update_lighting(item);
        }
    }
}

// 0x4714E0
void switch_hand(Object* a1, Object** a2, Object** a3, int a4)
{
    if (*a2 != NULL) {
        if (item_get_type(*a2) == ITEM_TYPE_WEAPON && item_get_type(a1) == ITEM_TYPE_AMMO) {
            return;
        }

        if (a3 != NULL && (a3 != &i_worn || item_get_type(*a2) == ITEM_TYPE_ARMOR)) {
            if (a3 == &i_worn) {
                adjust_ac(stack[0], i_worn, *a2);
            }
            *a3 = *a2;
        } else {
            if (a4 != -1) {
                item_remove_mult(inven_dude, a1, 1);
            }

            Object* itemToAdd = *a2;
            *a2 = NULL;
            if (item_add_force(inven_dude, itemToAdd, 1) != 0) {
                item_add_force(inven_dude, a1, 1);
                return;
            }

            a4 = -1;

            if (a3 != NULL) {
                if (a3 == &i_worn) {
                    adjust_ac(stack[0], i_worn, NULL);
                }
                *a3 = NULL;
            }
        }
    } else {
        if (a3 != NULL) {
            if (a3 == &i_worn) {
                adjust_ac(stack[0], i_worn, NULL);
            }
            *a3 = NULL;
        }
    }

    *a2 = a1;

    if (a4 != -1) {
        item_remove_mult(inven_dude, a1, 1);
    }
}

// This function removes armor bonuses and effects granted by [oldArmor] and
// adds appropriate bonuses and effects granted by [newArmor]. Both [oldArmor]
// and [newArmor] can be NULL.
//
// 0x4715F8
void adjust_ac(Object* critter, Object* oldArmor, Object* newArmor)
{
    int armorClassBonus = stat_get_bonus(critter, STAT_ARMOR_CLASS);
    int oldArmorClass = item_ar_ac(oldArmor);
    int newArmorClass = item_ar_ac(newArmor);
    stat_set_bonus(critter, STAT_ARMOR_CLASS, armorClassBonus - oldArmorClass + newArmorClass);

    int damageResistanceStat = STAT_DAMAGE_RESISTANCE;
    int damageThresholdStat = STAT_DAMAGE_THRESHOLD;
    for (int damageType = 0; damageType < DAMAGE_TYPE_COUNT; damageType += 1) {
        int damageResistanceBonus = stat_get_bonus(critter, damageResistanceStat);
        int oldArmorDamageResistance = item_ar_dr(oldArmor, damageType);
        int newArmorDamageResistance = item_ar_dr(newArmor, damageType);
        stat_set_bonus(critter, damageResistanceStat, damageResistanceBonus - oldArmorDamageResistance + newArmorDamageResistance);

        int damageThresholdBonus = stat_get_bonus(critter, damageThresholdStat);
        int oldArmorDamageThreshold = item_ar_dt(oldArmor, damageType);
        int newArmorDamageThreshold = item_ar_dt(newArmor, damageType);
        stat_set_bonus(critter, damageThresholdStat, damageThresholdBonus - oldArmorDamageThreshold + newArmorDamageThreshold);

        damageResistanceStat += 1;
        damageThresholdStat += 1;
    }

    if (isPartyMember(critter)) {
        if (oldArmor != NULL) {
            int perk = item_ar_perk(oldArmor);
            perk_remove_effect(critter, perk);
        }

        if (newArmor != NULL) {
            int perk = item_ar_perk(newArmor);
            perk_add_effect(critter, perk);
        }
    }
}

// 0x4716E8
void adjust_fid()
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

// 0x4717E4
void use_inventory_on(Object* a1)
{
    if (inven_init() == -1) {
        return;
    }

    bool isoWasEnabled = setup_inventory(INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
    display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
    inven_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);
    for (;;) {
        if (game_user_wants_to_quit != 0) {
            break;
        }

        display_body(-1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);

        int keyCode = get_input();
        switch (keyCode) {
        case KEY_HOME:
            stack_offset[curr_stack] = 0;
            display_inventory(0, -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        case KEY_ARROW_UP:
            if (stack_offset[curr_stack] > 0) {
                stack_offset[curr_stack] -= 1;
                display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            }
            break;
        case KEY_PAGE_UP:
            stack_offset[curr_stack] -= inven_cur_disp;
            if (stack_offset[curr_stack] < 0) {
                stack_offset[curr_stack] = 0;
                display_inventory(stack_offset[curr_stack], -1, 1);
            }
            break;
        case KEY_END:
            stack_offset[curr_stack] = pud->length - inven_cur_disp;
            if (stack_offset[curr_stack] < 0) {
                stack_offset[curr_stack] = 0;
            }
            display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        case KEY_ARROW_DOWN:
            if (stack_offset[curr_stack] + inven_cur_disp < pud->length) {
                stack_offset[curr_stack] += 1;
                display_inventory(stack_offset[curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            }
            break;
        case KEY_PAGE_DOWN:
            stack_offset[curr_stack] += inven_cur_disp;
            if (stack_offset[curr_stack] + inven_cur_disp >= pud->length) {
                stack_offset[curr_stack] = pud->length - inven_cur_disp;
                if (stack_offset[curr_stack] < 0) {
                    stack_offset[curr_stack] = 0;
                }
            }
            display_inventory(stack_offset[curr_stack], -1, 1);
            break;
        case 2500:
            container_exit(keyCode, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        default:
            if ((mouse_get_buttons() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (immode == INVENTORY_WINDOW_CURSOR_HAND) {
                    inven_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);
                } else {
                    inven_set_mouse(INVENTORY_WINDOW_CURSOR_HAND);
                }
            } else if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode < 1000 + inven_cur_disp) {
                    if (immode == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inven_action_cursor(keyCode, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
                    } else {
                        int inventoryItemIndex = pud->length - (stack_offset[curr_stack] + keyCode - 1000 + 1);
                        if (inventoryItemIndex < pud->length) {
                            InventoryItem* inventoryItem = &(pud->items[inventoryItemIndex]);
                            if (isInCombat()) {
                                if (obj_dude->data.critter.combat.ap >= 2) {
                                    if (action_use_an_item_on_object(obj_dude, a1, inventoryItem->item) != -1) {
                                        int actionPoints = obj_dude->data.critter.combat.ap;
                                        if (actionPoints < 2) {
                                            obj_dude->data.critter.combat.ap = 0;
                                        } else {
                                            obj_dude->data.critter.combat.ap = actionPoints - 2;
                                        }
                                        intface_update_move_points(obj_dude->data.critter.combat.ap, combat_free_move);
                                    }
                                }
                            } else {
                                action_use_an_item_on_object(obj_dude, a1, inventoryItem->item);
                            }
                            keyCode = KEY_ESCAPE;
                        } else {
                            keyCode = -1;
                        }
                    }
                }
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }
    }

    exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inven_exit();
}

// 0x471B70
Object* inven_right_hand(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (i_rhand != NULL && critter == inven_dude) {
        return i_rhand;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_IN_RIGHT_HAND) {
            return item;
        }
    }

    return NULL;
}

// 0x471BBC
Object* inven_left_hand(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (i_lhand != NULL && critter == inven_dude) {
        return i_lhand;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_IN_LEFT_HAND) {
            return item;
        }
    }

    return NULL;
}

// 0x471C08
Object* inven_worn(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (i_worn != NULL && critter == inven_dude) {
        return i_worn;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_WORN) {
            return item;
        }
    }

    return NULL;
}

// 0x471CA0
Object* inven_pid_is_carried(Object* obj, int pid)
{
    Inventory* inventory = &(obj->data.inventory);

    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            return inventoryItem->item;
        }

        Object* found = inven_pid_is_carried(inventoryItem->item, pid);
        if (found != NULL) {
            return found;
        }
    }

    return NULL;
}

// 0x471CDC
int inven_pid_quantity_carried(Object* object, int pid)
{
    int quantity = 0;

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            quantity += inventoryItem->quantity;
        }

        quantity += inven_pid_quantity_carried(inventoryItem->item, pid);
    }

    return quantity;
}

// Renders character's summary of SPECIAL stats, equipped armor bonuses,
// and weapon's damage/range.
//
// 0x471D5C
void display_stats()
{
    // 0x46E6D0
    static const int v56[7] = {
        STAT_CURRENT_HIT_POINTS,
        STAT_ARMOR_CLASS,
        STAT_DAMAGE_THRESHOLD,
        STAT_DAMAGE_THRESHOLD_LASER,
        STAT_DAMAGE_THRESHOLD_FIRE,
        STAT_DAMAGE_THRESHOLD_PLASMA,
        STAT_DAMAGE_THRESHOLD_EXPLOSION,
    };

    // 0x46E6EC
    static const int v57[7] = {
        STAT_MAXIMUM_HIT_POINTS,
        -1,
        STAT_DAMAGE_RESISTANCE,
        STAT_DAMAGE_RESISTANCE_LASER,
        STAT_DAMAGE_RESISTANCE_FIRE,
        STAT_DAMAGE_RESISTANCE_PLASMA,
        STAT_DAMAGE_RESISTANCE_EXPLOSION,
    };

    char formattedText[80];

    int oldFont = text_curr();
    text_font(101);

    unsigned char* windowBuffer = win_get_buf(i_wid);

    int fid = art_id(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);

    CacheEntry* backgroundHandle;
    unsigned char* backgroundData = art_ptr_lock_data(fid, 0, 0, &backgroundHandle);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData + (499 * 44 + 297) * 4, 152, 188, 499, windowBuffer + (499 * 44 + 297) * 4, 499);
    }
    art_ptr_unlock(backgroundHandle);

    // Render character name.
    const char* critterName = critter_name(stack[0]);
    text_to_buf(windowBuffer + (499 * 44 + 297) * 4, critterName, 80, 499, colorTable[992]);

    draw_line(windowBuffer,
        499,
        297,
        3 * text_height() / 2 + 44,
        440,
        3 * text_height() / 2 + 44,
        paletteIndexToRGBA(colorTable[992]));

    MessageListItem messageListItem;

    int offset = 499 * 2 * text_height() + 499 * 44 + 297;
    for (int stat = 0; stat < 7; stat++) {
        messageListItem.num = stat;
        if (message_search(&inventry_message_file, &messageListItem)) {
            text_to_buf(windowBuffer + offset * 4, messageListItem.text, 80, 499, colorTable[992]);
        }

        int value = critterGetStat(stack[0], stat);
        sprintf(formattedText, "%d", value);
        text_to_buf(windowBuffer + offset * 4 + 24 * 4, formattedText, 80, 499, colorTable[992]);

        offset += 499 * text_height();
    }

    offset -= 499 * 7 * text_height();

    for (int index = 0; index < 7; index += 1) {
        messageListItem.num = 7 + index;
        if (message_search(&inventry_message_file, &messageListItem)) {
            text_to_buf(windowBuffer + offset * 4 + 40 * 4, messageListItem.text, 80, 499, colorTable[992]);
        }

        if (v57[index] == -1) {
            int value = critterGetStat(stack[0], v56[index]);
            sprintf(formattedText, "   %d", value);
        } else {
            int value1 = critterGetStat(stack[0], v56[index]);
            int value2 = critterGetStat(stack[0], v57[index]);
            const char* format = index != 0 ? "%d/%d%%" : "%d/%d";
            sprintf(formattedText, format, value1, value2);
        }

        text_to_buf(windowBuffer + offset * 4 + 104 * 4, formattedText, 80, 499, colorTable[992]);

        offset += 499 * text_height();
    }

    draw_line(windowBuffer, 499, 297, 18 * text_height() / 2 + 48, 440, 18 * text_height() / 2 + 48, paletteIndexToRGBA(colorTable[992]));
    draw_line(windowBuffer, 499, 297, 26 * text_height() / 2 + 48, 440, 26 * text_height() / 2 + 48, paletteIndexToRGBA(colorTable[992]));

    Object* itemsInHands[2] = {
        i_lhand,
        i_rhand,
    };

    const int hitModes[2] = {
        HIT_MODE_LEFT_WEAPON_PRIMARY,
        HIT_MODE_RIGHT_WEAPON_PRIMARY,
    };

    offset += 499 * text_height();

    for (int index = 0; index < 2; index += 1) {
        Object* item = itemsInHands[index];
        if (item == NULL) {
            formattedText[0] = '\0';

            // No item
            messageListItem.num = 14;
            if (message_search(&inventry_message_file, &messageListItem)) {
                text_to_buf(windowBuffer + offset * 4, messageListItem.text, 120, 499, colorTable[992]);
            }

            offset += 499 * text_height();

            // Unarmed dmg:
            messageListItem.num = 24;
            if (message_search(&inventry_message_file, &messageListItem)) {
                // TODO: Figure out why it uses STAT_MELEE_DAMAGE instead of
                // STAT_UNARMED_DAMAGE.
                int damage = critterGetStat(stack[0], STAT_MELEE_DAMAGE) + 2;
                sprintf(formattedText, "%s 1-%d", messageListItem.text, damage);
            }

            text_to_buf(windowBuffer + offset * 4, formattedText, 120, 499, colorTable[992]);

            offset += 3 * 499 * text_height();
            continue;
        }

        const char* itemName = item_name(item);
        text_to_buf(windowBuffer + offset * 4, itemName, 140, 499, colorTable[992]);

        offset += 499 * text_height();

        int itemType = item_get_type(item);
        if (itemType != ITEM_TYPE_WEAPON) {
            if (itemType == ITEM_TYPE_ARMOR) {
                // (Not worn)
                messageListItem.num = 18;
                if (message_search(&inventry_message_file, &messageListItem)) {
                    text_to_buf(windowBuffer + offset * 4, messageListItem.text, 120, 499, colorTable[992]);
                }
            }

            offset += 3 * 499 * text_height();
            continue;
        }

        int range = item_w_range(stack[0], hitModes[index]);

        int damageMin;
        int damageMax;
        item_w_damage_min_max(item, &damageMin, &damageMax);

        int attackType = item_w_subtype(item, hitModes[index]);

        formattedText[0] = '\0';

        int meleeDamage;
        if (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) {
            meleeDamage = critterGetStat(stack[0], STAT_MELEE_DAMAGE);
        } else {
            meleeDamage = 0;
        }

        messageListItem.num = 15; // Dmg:
        if (message_search(&inventry_message_file, &messageListItem)) {
            if (attackType != 4 && range <= 1) {
                sprintf(formattedText, "%s %d-%d", messageListItem.text, damageMin, damageMax + meleeDamage);
            } else {
                MessageListItem rangeMessageListItem;
                rangeMessageListItem.num = 16; // Rng:
                if (message_search(&inventry_message_file, &rangeMessageListItem)) {
                    sprintf(formattedText, "%s %d-%d   %s %d", messageListItem.text, damageMin, damageMax + meleeDamage, rangeMessageListItem.text, range);
                }
            }

            text_to_buf(windowBuffer + offset * 4, formattedText, 140, 499, colorTable[992]);
        }

        offset += 499 * text_height();

        if (item_w_max_ammo(item) > 0) {
            int ammoTypePid = item_w_ammo_pid(item);

            formattedText[0] = '\0';

            messageListItem.num = 17; // Ammo:
            if (message_search(&inventry_message_file, &messageListItem)) {
                if (ammoTypePid != -1) {
                    if (item_w_curr_ammo(item) != 0) {
                        const char* ammoName = proto_name(ammoTypePid);
                        int capacity = item_w_max_ammo(item);
                        int quantity = item_w_curr_ammo(item);
                        sprintf(formattedText, "%s %d/%d %s", messageListItem.text, quantity, capacity, ammoName);
                    } else {
                        int capacity = item_w_max_ammo(item);
                        int quantity = item_w_curr_ammo(item);
                        sprintf(formattedText, "%s %d/%d", messageListItem.text, quantity, capacity);
                    }
                }
            } else {
                int capacity = item_w_max_ammo(item);
                int quantity = item_w_curr_ammo(item);
                sprintf(formattedText, "%s %d/%d", messageListItem.text, quantity, capacity);
            }

            text_to_buf(windowBuffer + offset * 4, formattedText, 140, 499, colorTable[992]);
        }

        offset += 2 * 499 * text_height();
    }

    // Total wt:
    messageListItem.num = 20;
    if (message_search(&inventry_message_file, &messageListItem)) {
        if (PID_TYPE(stack[0]->pid) == OBJ_TYPE_CRITTER) {
            int carryWeight = critterGetStat(stack[0], STAT_CARRY_WEIGHT);
            int inventoryWeight = item_total_weight(stack[0]);
            sprintf(formattedText, "%s %d/%d", messageListItem.text, inventoryWeight, carryWeight);

            int color = colorTable[992];
            if (critterIsOverloaded(stack[0])) {
                color = colorTable[31744];
            }

            text_to_buf(windowBuffer + offset * 4 + 15 * 4, formattedText, 120, 499, color);
        } else {
            int inventoryWeight = item_total_weight(stack[0]);
            sprintf(formattedText, "%s %d", messageListItem.text, inventoryWeight);

            text_to_buf(windowBuffer + offset * 4 + 30 * 4, formattedText, 80, 499, colorTable[992]);
        }
    }

    text_font(oldFont);
}

// Finds next item of given [itemType] (can be -1 which means any type of
// item).
//
// The [index] is used to control where to continue the search from, -1 - from
// the beginning.
//
// 0x472698
Object* inven_find_type(Object* obj, int itemType, int* indexPtr)
{
    int dummy = -1;
    if (indexPtr == NULL) {
        indexPtr = &dummy;
    }

    *indexPtr += 1;

    Inventory* inventory = &(obj->data.inventory);

    // TODO: Refactor with for loop.
    if (*indexPtr >= inventory->length) {
        return NULL;
    }

    while (itemType != -1 && item_get_type(inventory->items[*indexPtr].item) != itemType) {
        *indexPtr += 1;

        if (*indexPtr >= inventory->length) {
            return NULL;
        }
    }

    return inventory->items[*indexPtr].item;
}

// 0x4726EC
Object* inven_find_id(Object* obj, int id)
{
    if (obj->id == id) {
        return obj;
    }

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item->id == id) {
            return item;
        }

        if (item_get_type(item) == ITEM_TYPE_CONTAINER) {
            item = inven_find_id(item, id);
            if (item != NULL) {
                return item;
            }
        }
    }

    return NULL;
}

// 0x472740
Object* inven_index_ptr(Object* obj, int a2)
{
    Inventory* inventory;

    inventory = &(obj->data.inventory);

    if (a2 < 0 || a2 >= inventory->length) {
        return NULL;
    }

    return inventory->items[a2].item;
}

// inven_wield
// 0x472758
int inven_wield(Object* a1, Object* a2, int a3)
{
    return invenWieldFunc(a1, a2, a3, true);
}

// 0x472768
int invenWieldFunc(Object* critter, Object* item, int a3, bool a4)
{
    if (a4) {
        if (!map_bk_processes_are_disabled()) {
            register_begin(ANIMATION_REQUEST_RESERVED);
        }
    }

    int itemType = item_get_type(item);
    if (itemType == ITEM_TYPE_ARMOR) {
        Object* armor = inven_worn(critter);
        if (armor != NULL) {
            armor->flags &= ~OBJECT_WORN;
        }

        item->flags |= OBJECT_WORN;

        int baseFrmId;
        if (critterGetStat(critter, STAT_GENDER) == GENDER_FEMALE) {
            baseFrmId = item_ar_female_fid(item);
        } else {
            baseFrmId = item_ar_male_fid(item);
        }

        if (baseFrmId == -1) {
            baseFrmId = 1;
        }

        if (critter == obj_dude) {
            if (!map_bk_processes_are_disabled()) {
                int fid = art_id(OBJ_TYPE_CRITTER, baseFrmId, 0, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                register_object_change_fid(critter, fid, 0);
            }
        } else {
            adjust_ac(critter, armor, item);
        }
    } else {
        int hand;
        if (critter == obj_dude) {
            hand = intface_is_item_right_hand();
        } else {
            hand = HAND_RIGHT;
        }

        int weaponAnimationCode = item_w_anim_code(item);
        int hitModeAnimationCode = item_w_anim_weap(item, HIT_MODE_RIGHT_WEAPON_PRIMARY);
        int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, hitModeAnimationCode, weaponAnimationCode, critter->rotation + 1);
        if (!art_exists(fid)) {
            debug_printf("\ninven_wield failed!  ERROR ERROR ERROR!");
            return -1;
        }

        Object* v17;
        if (a3) {
            v17 = inven_right_hand(critter);
            item->flags |= OBJECT_IN_RIGHT_HAND;
        } else {
            v17 = inven_left_hand(critter);
            item->flags |= OBJECT_IN_LEFT_HAND;
        }

        Rect rect;
        if (v17 != NULL) {
            v17->flags &= ~OBJECT_IN_ANY_HAND;

            if (v17->pid == PROTO_ID_LIT_FLARE) {
                int lightIntensity;
                int lightDistance;
                if (critter == obj_dude) {
                    lightIntensity = LIGHT_LEVEL_MAX;
                    lightDistance = 4;
                } else {
                    Proto* proto;
                    if (proto_ptr(critter->pid, &proto) == -1) {
                        return -1;
                    }

                    lightDistance = proto->lightDistance;
                    lightIntensity = proto->lightIntensity;
                }

                obj_set_light(critter, lightDistance, lightIntensity, &rect);
            }
        }

        if (item->pid == PROTO_ID_LIT_FLARE) {
            int lightDistance = item->lightDistance;
            if (lightDistance < critter->lightDistance) {
                lightDistance = critter->lightDistance;
            }

            int lightIntensity = item->lightIntensity;
            if (lightIntensity < critter->lightIntensity) {
                lightIntensity = critter->lightIntensity;
            }

            obj_set_light(critter, lightDistance, lightIntensity, &rect);
            tile_refresh_rect(&rect, map_elevation);
        }

        if (item_get_type(item) == ITEM_TYPE_WEAPON) {
            weaponAnimationCode = item_w_anim_code(item);
        } else {
            weaponAnimationCode = 0;
        }

        if (hand == a3) {
            if ((critter->fid & 0xF000) >> 12 != 0) {
                if (a4) {
                    if (!map_bk_processes_are_disabled()) {
                        const char* soundEffectName = gsnd_build_character_sfx_name(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
                        register_object_play_sfx(critter, soundEffectName, 0);
                        register_object_animate(critter, ANIM_PUT_AWAY, 0);
                    }
                }
            }

            if (a4 && !map_bk_processes_are_disabled()) {
                if (weaponAnimationCode != 0) {
                    register_object_take_out(critter, weaponAnimationCode, -1);
                } else {
                    int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, 0, critter->rotation + 1);
                    register_object_change_fid(critter, fid, -1);
                }
            } else {
                int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, weaponAnimationCode, critter->rotation + 1);
                dude_stand(critter, critter->rotation, fid);
            }
        }
    }

    if (a4) {
        if (!map_bk_processes_are_disabled()) {
            return register_end();
        }
    }

    return 0;
}

// inven_unwield
// 0x472A54
int inven_unwield(Object* critter_obj, int a2)
{
    return invenUnwieldFunc(critter_obj, a2, 1);
}

// 0x472A64
int invenUnwieldFunc(Object* obj, int a2, int a3)
{
    int v6;
    Object* item_obj;
    int fid;

    if (obj == obj_dude) {
        v6 = intface_is_item_right_hand();
    } else {
        v6 = 1;
    }

    if (a2) {
        item_obj = inven_right_hand(obj);
    } else {
        item_obj = inven_left_hand(obj);
    }

    if (item_obj) {
        item_obj->flags &= ~OBJECT_IN_ANY_HAND;
    }

    if (v6 == a2 && ((obj->fid & 0xF000) >> 12) != 0) {
        if (a3 && !map_bk_processes_are_disabled()) {
            register_begin(ANIMATION_REQUEST_RESERVED);

            const char* sfx = gsnd_build_character_sfx_name(obj, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            register_object_play_sfx(obj, sfx, 0);

            register_object_animate(obj, ANIM_PUT_AWAY, 0);

            fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, 0, 0, obj->rotation + 1);
            register_object_change_fid(obj, fid, -1);

            return register_end();
        }

        fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, 0, 0, obj->rotation + 1);
        dude_stand(obj, obj->rotation, fid);
    }

    return 0;
}

// 0x472B54
int inven_from_button(int keyCode, Object** a2, Object*** a3, Object** a4)
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

// Displays item description.
//
// The [string] is mutated in the process replacing spaces back and forth
// for word wrapping purposes.
//
// inven_display_msg
// 0x472D24
void inven_display_msg(char* string)
{
    int oldFont = text_curr();
    text_font(101);

    unsigned char* windowBuffer = win_get_buf(i_wid);
    windowBuffer += (499 * 44 + 297) * 4;

    char* c = string;
    while (c != NULL && *c != '\0') {
        inven_display_msg_line += 1;
        if (inven_display_msg_line > 17) {
            debug_printf("\nError: inven_display_msg: out of bounds!");
            return;
        }

        char* space = NULL;
        if (text_width(c) > 152) {
            // Look for next space.
            space = c + 1;
            while (*space != '\0' && *space != ' ') {
                space += 1;
            }

            if (*space == '\0') {
                // This was the last line containing very long word. Text
                // drawing routine will silently truncate it after reaching
                // desired length.
                text_to_buf(windowBuffer + (499 * inven_display_msg_line * text_height()) * 4, c, 152, 499, colorTable[992]);
                return;
            }

            char* nextSpace = space + 1;
            while (true) {
                while (*nextSpace != '\0' && *nextSpace != ' ') {
                    nextSpace += 1;
                }

                if (*nextSpace == '\0') {
                    break;
                }

                // Break string and measure it.
                *nextSpace = '\0';
                if (text_width(c) >= 152) {
                    // Next space is too far to fit in one line. Restore next
                    // space's character and stop.
                    *nextSpace = ' ';
                    break;
                }

                space = nextSpace;

                // Restore next space's character and continue looping from the
                // next character.
                *nextSpace = ' ';
                nextSpace += 1;
            }

            if (*space == ' ') {
                *space = '\0';
            }
        }

        if (text_width(c) > 152) {
            debug_printf("\nError: inven_display_msg: word too long!");
            return;
        }

        text_to_buf(windowBuffer + (499 * inven_display_msg_line * text_height()) * 4, c, 152, 499, colorTable[992]);

        if (space != NULL) {
            c = space + 1;
            if (*space == '\0') {
                *space = ' ';
            }
        } else {
            c = NULL;
        }
    }

    text_font(oldFont);
}

// Examines inventory item.
//
// 0x472EB8
void inven_obj_examine_func(Object* critter, Object* item)
{
    int oldFont = text_curr();
    text_font(101);

    unsigned char* windowBuffer = win_get_buf(i_wid);

    // Clear item description area.
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);

    CacheEntry* handle;
    unsigned char* backgroundData = art_ptr_lock_data(backgroundFid, 0, 0, &handle);
    if (backgroundData != NULL) {
        buf_to_buf(backgroundData + (499 * 44 + 297) * 4, 152, 188, 499, windowBuffer + (499 * 44 + 297) * 4, 499);
    }
    art_ptr_unlock(handle);

    // Reset item description lines counter.
    inven_display_msg_line = 0;

    // Render item's name.
    char* itemName = object_name(item);
    inven_display_msg(itemName);

    // Increment line counter to accomodate separator below.
    inven_display_msg_line += 1;

    int lineHeight = text_height();

    // Draw separator.
    draw_line(windowBuffer,
        499,
        297,
        3 * lineHeight / 2 + 49,
        440,
        3 * lineHeight / 2 + 49,
        paletteIndexToRGBA(colorTable[992]));

    // Examine item.
    obj_examine_func(critter, item, inven_display_msg);

    // Add weight if neccessary.
    int weight = item_weight(item);
    if (weight != 0) {
        MessageListItem messageListItem;
        messageListItem.num = 540;

        if (weight == 1) {
            messageListItem.num = 541;
        }

        if (!message_search(&proto_main_msg_file, &messageListItem)) {
            debug_printf("\nError: Couldn't find message!");
        }

        char formattedText[40];
        sprintf(formattedText, messageListItem.text, weight);
        inven_display_msg(formattedText);
    }

    text_font(oldFont);
}

// 0x47304C
void inven_action_cursor(int keyCode, int inventoryWindowType)
{
    // 0x519114
    static int act_use[4] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_USE,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    // 0x519124
    static int act_no_use[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    // 0x519130
    static int act_just_use[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_USE,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    // 0x51913C
    static int act_nothing[2] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    // 0x519144
    static int act_weap[4] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
        GAME_MOUSE_ACTION_MENU_ITEM_DROP,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    // 0x519154
    static int act_weap2[3] = {
        GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
        GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
        GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
    };

    Object* item;
    Object** v43;
    Object* v41;

    int v56 = inven_from_button(keyCode, &item, &v43, &v41);
    if (v56 == 0) {
        return;
    }

    int itemType = item_get_type(item);

    int mouseState;
    do {
        get_input();

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
            display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);
        }

        mouseState = mouse_get_buttons();
        if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
            if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
                obj_look_at_func(stack[0], item, display_msg);
            } else {
                inven_obj_examine_func(stack[0], item);
            }
            win_draw(i_wid);
            return;
        }
    } while ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT);

    inven_set_mouse(INVENTORY_WINDOW_CURSOR_BLANK);

    unsigned char* windowBuffer = win_get_buf(i_wid);

    int x;
    int y;
    mouse_get_position(&x, &y);

    int actionMenuItemsLength;
    const int* actionMenuItems;
    if (itemType == ITEM_TYPE_WEAPON && item_w_can_unload(item)) {
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL && obj_top_environment(item) != obj_dude) {
            actionMenuItemsLength = 3;
            actionMenuItems = act_weap2;
        } else {
            actionMenuItemsLength = 4;
            actionMenuItems = act_weap;
        }
    } else {
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
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
        } else {
            if (itemType == ITEM_TYPE_CONTAINER && v43 != NULL) {
                actionMenuItemsLength = 3;
                actionMenuItems = act_no_use;
            } else {
                if (obj_action_can_use(item) || proto_action_can_use_on(item->pid)) {
                    actionMenuItemsLength = 4;
                    actionMenuItems = act_use;
                } else {
                    actionMenuItemsLength = 3;
                    actionMenuItems = act_no_use;
                }
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

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
            display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);
        }

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

    display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);

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
                        if (inven_from_button(keyCode, &a2, &v43, &v41) != 0) {
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
                    if (inven_from_button(keyCode, &item, &v43, &v41) != 0) {
                        obj_drop(v41, item);
                    }
                }
            } else {
                obj_drop(v41, item);
            }
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
            obj_examine_func(stack[0], item, display_msg);
        } else {
            inven_obj_examine_func(stack[0], item);
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
        switch (itemType) {
        case ITEM_TYPE_CONTAINER:
            container_enter(keyCode, inventoryWindowType);
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

    inven_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL && actionMenuItem != GAME_MOUSE_ACTION_MENU_ITEM_LOOK) {
        display_stats();
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);
    }

    display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);

    adjust_fid();
}

// 0x47620C
void container_enter(int keyCode, int inventoryWindowType)
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

                display_body(item->fid, inventoryWindowType);
                display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);
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

                adjust_fid();
                display_body(-1, inventoryWindowType);
                display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);
            }
        }
    }
}

// 0x476394
void container_exit(int keyCode, int inventoryWindowType)
{
    if (keyCode == 2500) {
        if (curr_stack > 0) {
            curr_stack -= 1;
            inven_dude = stack[curr_stack];
            pud = &inven_dude->data.inventory;
            adjust_fid();
            display_body(-1, inventoryWindowType);
            display_inventory(stack_offset[curr_stack], -1, inventoryWindowType);
        }
    } else if (keyCode == 2501) {
        if (target_curr_stack > 0) {
            target_curr_stack -= 1;
            Object* v5 = target_stack[target_curr_stack];
            target_pud = &(v5->data.inventory);
            display_body(v5->fid, inventoryWindowType);
            display_target_inventory(target_stack_offset[target_curr_stack], -1, target_pud, inventoryWindowType);
            win_draw(i_wid);
        }
    }
}

// 0x476464
int drop_into_container(Object* a1, Object* a2, int a3, Object** a4, int quantity)
{
    int quantityToMove;
    if (quantity > 1) {
        quantityToMove = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, a2, quantity);
    } else {
        quantityToMove = 1;
    }

    if (quantityToMove == -1) {
        return -1;
    }

    if (a3 != -1) {
        if (item_remove_mult(inven_dude, a2, quantityToMove) == -1) {
            return -1;
        }
    }

    int rc = item_add_mult(a1, a2, quantityToMove);
    if (rc != 0) {
        if (a3 != -1) {
            item_add_mult(inven_dude, a2, quantityToMove);
        }
    } else {
        if (a4 != NULL) {
            if (a4 == &i_worn) {
                adjust_ac(stack[0], i_worn, NULL);
            }
            *a4 = NULL;
        }
    }

    return rc;
}

// 0x47650C
int drop_ammo_into_weapon(Object* weapon, Object* ammo, Object** a3, int quantity, int keyCode)
{
    if (item_get_type(weapon) != ITEM_TYPE_WEAPON) {
        return -1;
    }

    if (item_get_type(ammo) != ITEM_TYPE_AMMO) {
        return -1;
    }

    if (!item_w_can_reload(weapon, ammo)) {
        return -1;
    }

    int quantityToMove;
    if (quantity > 1) {
        quantityToMove = do_move_timer(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, ammo, quantity);
    } else {
        quantityToMove = 1;
    }

    if (quantityToMove == -1) {
        return -1;
    }

    Object* v14 = ammo;
    bool v17 = false;
    int rc = item_remove_mult(inven_dude, weapon, 1);
    for (int index = 0; index < quantityToMove; index++) {
        int v11 = item_w_reload(weapon, v14);
        if (v11 == 0) {
            if (a3 != NULL) {
                *a3 = NULL;
            }

            obj_destroy(v14);

            v17 = true;
            if (inven_from_button(keyCode, &v14, NULL, NULL) == 0) {
                break;
            }
        }
        if (v11 != -1) {
            v17 = true;
        }
        if (v11 != 0) {
            break;
        }
    }

    if (rc != -1) {
        item_add_force(inven_dude, weapon, 1);
    }

    if (!v17) {
        return -1;
    }

    const char* sfx = gsnd_build_weapon_sfx_name(WEAPON_SOUND_EFFECT_READY, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, NULL);
    gsound_play_sfx_file(sfx);

    return 0;
}

// 0x47664C
void draw_amount(int value, int inventoryWindowType)
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

// 0x47688C
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

    draw_amount(value, inventoryWindowType);

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
            draw_amount(value, inventoryWindowType);
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

                            draw_amount(value, inventoryWindowType);
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

                draw_amount(value, inventoryWindowType);
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

                            draw_amount(value, inventoryWindowType);
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

                draw_amount(value, inventoryWindowType);
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

                draw_amount(value, inventoryWindowType);
                continue;
            } else if (keyCode == KEY_BACKSPACE) {
                if (!v5) {
                    value = 0;
                }

                value /= 10;
                v5 = true;

                draw_amount(value, inventoryWindowType);
                continue;
            }
        }
    }

    exit_move_timer_win(inventoryWindowType);

    return value;
}

// Creates move items/set timer interface.
//
// 0x476AB8
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
        if (message_search(&inventry_message_file, &messageListItem)) {
            int length = text_width(messageListItem.text);
            text_to_buf(windowBuffer + (windowDescription->width * 9 + (windowDescription->width - length) / 2) * 4, messageListItem.text, 200, windowDescription->width, colorTable[21091]);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_SET_TIMER) {
        // SET TIMER
        messageListItem.num = 23;
        if (message_search(&inventry_message_file, &messageListItem)) {
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
            if (message_search(&inventry_message_file, &messageListItem)) {
                int length = text_width(messageListItem.text);

                // TODO: Where is y? Is it hardcoded in to 376?
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
    inven_set_mouse(INVENTORY_WINDOW_CURSOR_ARROW);
    text_font(oldFont);

    return 0;
}

// 0x477030
static int exit_move_timer_win(int inventoryWindowType)
{
    int count = inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS ? 8 : 6;

    for (int index = 0; index < count; index++) {
        art_ptr_unlock(mt_key[index]);
    }

    win_delete(mt_wid);

    return 0;
}

// 0x477074
int inven_set_timer(Object* a1)
{
    bool v1 = inven_is_initialized;

    if (!v1) {
        if (inven_init() == -1) {
            return -1;
        }
    }

    int seconds = do_move_timer(INVENTORY_WINDOW_TYPE_SET_TIMER, a1, 180);

    if (!v1) {
        // NOTE: Uninline.
        inven_exit();
    }

    return seconds;
}

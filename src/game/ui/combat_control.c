#include "game/ui/combat_control.h"

#include "game/ui.h"
#include <stdio.h>
#include "game/actions.h"
#include "game/art.h"
#include "plib/color/color.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "plib/gnw/input.h"
#include "game/critter.h"
#include "plib/gnw/debug.h"
#include "game/display.h"
#include "plib/gnw/grbuf.h"
#include "game/game.h"
#include "game/gsound.h"
#include "plib/gnw/rect.h"
#include "game/item.h"
#include "game/message.h"
#include "game/object.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/skill.h"
#include "game/stat.h"
#include "plib/gnw/text.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/svga.h"

// Own window geometry — a bottom-centered panel, independent of the conversation.
#define CC_WINDOW_WIDTH (640 * ui_get_scale())
#define CC_WINDOW_HEIGHT (190 * ui_get_scale())
#define SCREEN_WIDTH (scr_size.lrx - scr_size.ulx + 1)
#define SCREEN_HEIGHT (scr_size.lry - scr_size.uly + 1)
#define CC_WINDOW_X ((SCREEN_WIDTH - CC_WINDOW_WIDTH) / 2)
#define CC_WINDOW_Y ((SCREEN_HEIGHT - CC_WINDOW_HEIGHT) / 2)

static Object* cc_target;
static int cc_window = -1;
static int cc_buttons[9];
static int cc_subwin_len = 0;

static CacheEntry* cc_assets_keys[2];
static Art* cc_assets[2];

enum CombatControlAssets
{
    CC_BUTTON_RED_UP = 0,
    CC_BUTTON_RED_DOWN,
    CC_MAX_ASSETS
};

const char* ASSET_NAMES[] = {
    "di_rdbt2.frm", "di_rdbt1.frm"
};

typedef struct GameDialogButtonData {
    int x;
    int y;
    int upFrmId;
    int downFrmId;
    int disabledFrmId;
    CacheEntry* upFrmHandle;
    CacheEntry* downFrmHandle;
    CacheEntry* disabledFrmHandle;
    int keyCode;
    int value;
} GameDialogButtonData;

typedef struct STRUCT_5189E4 {
    int messageId;
    int value;
} STRUCT_5189E4;

typedef enum PartyMemberCustomizationOption {
    PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE,
    PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT,
} PartyMemberCustomizationOption;

typedef enum {
    GD_CONTROL_TALK,   // close combat control, return to the conversation
    GD_CONTROL_CUSTOM, // open the per-follower AI-settings sub-screen
} GdControlResult;

static GameDialogButtonData control_button_info[5] = {
    { 438, 37, 397, 395, 396, NULL, NULL, NULL, 2098, 4 },
    { 438, 67, 394, 392, 393, NULL, NULL, NULL, 2103, 3 },
    { 438, 96, 406, 404, 405, NULL, NULL, NULL, 2102, 2 },
    { 438, 126, 400, 398, 399, NULL, NULL, NULL, 2111, 1 },
    { 438, 156, 403, 401, 402, NULL, NULL, NULL, 2099, 0 },
};

static STRUCT_5189E4 custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT][6] = {
    {
        { 100, AREA_ATTACK_MODE_ALWAYS }, // Always!
        { 101, AREA_ATTACK_MODE_SOMETIMES }, // Sometimes, don't worry about hitting me
        { 102, AREA_ATTACK_MODE_BE_SURE }, // Be sure you won't hit me
        { 103, AREA_ATTACK_MODE_BE_CAREFUL }, // Be careful not to hit me
        { 104, AREA_ATTACK_MODE_BE_ABSOLUTELY_SURE }, // Be absolutely sure you won't hit me
        { -1, 0 },
    },
    {
        { 200, RUN_AWAY_MODE_COWARD - 1 }, // Abject coward
        { 201, RUN_AWAY_MODE_FINGER_HURTS - 1 }, // Your finger hurts
        { 202, RUN_AWAY_MODE_BLEEDING - 1 }, // You're bleeding a bit
        { 203, RUN_AWAY_MODE_NOT_FEELING_GOOD - 1 }, // Not feeling good
        { 204, RUN_AWAY_MODE_TOURNIQUET - 1 }, // You need a tourniquet
        { 205, RUN_AWAY_MODE_NEVER - 1 }, // Never!
    },
    {
        { 300, BEST_WEAPON_NO_PREF }, // None
        { 301, BEST_WEAPON_MELEE }, // Melee
        { 302, BEST_WEAPON_MELEE_OVER_RANGED }, // Melee then ranged
        { 303, BEST_WEAPON_RANGED_OVER_MELEE }, // Ranged then melee
        { 304, BEST_WEAPON_RANGED }, // Ranged
        { 305, BEST_WEAPON_UNARMED }, // Unarmed
    },
    {
        { 400, DISTANCE_STAY_CLOSE }, // Stay close to me
        { 401, DISTANCE_CHARGE }, // Charge!
        { 402, DISTANCE_SNIPE }, // Snipe the enemy
        { 403, DISTANCE_ON_YOUR_OWN }, // On your own
        { 404, DISTANCE_STAY }, // Say where you are
        { -1, 0 },
    },
    {
        { 500, ATTACK_WHO_WHOMEVER_ATTACKING_ME }, // Whomever is attacking me
        { 501, ATTACK_WHO_STRONGEST }, // The strongest
        { 502, ATTACK_WHO_WEAKEST }, // The weakest
        { 503, ATTACK_WHO_WHOMEVER }, // Whomever you want
        { 504, ATTACK_WHO_CLOSEST }, // Whoever is closest
        { -1, 0 },
    },
    {
        { 600, CHEM_USE_CLEAN }, // I'm clean
        { 601, CHEM_USE_STIMS_WHEN_HURT_LITTLE }, // Stimpacks when hurt a bit
        { 602, CHEM_USE_STIMS_WHEN_HURT_LOTS }, // Stimpacks when hurt a lot
        { 603, CHEM_USE_SOMETIMES }, // Any drug some of the time
        { 604, CHEM_USE_ANYTIME }, // Any drug any time
        { -1, 0 },
    },
};

static GameDialogButtonData custom_button_info[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT] = {
    { 95, 9, 410, 409, -1, NULL, NULL, NULL, 0, 0 },
    { 96, 38, 416, 415, -1, NULL, NULL, NULL, 1, 0 },
    { 96, 68, 418, 417, -1, NULL, NULL, NULL, 2, 0 },
    { 96, 98, 414, 413, -1, NULL, NULL, NULL, 3, 0 },
    { 96, 127, 408, 407, -1, NULL, NULL, NULL, 4, 0 },
    { 96, 157, 412, 411, -1, NULL, NULL, NULL, 5, 0 },
};

static int custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT];
static MessageList custom_msg_file;
static int custom_buttons_start;
static int control_buttons_start;

// ---- moved from gdialog.c (functions) ----

static int combat_control_create();
static void combat_control_destroy();
static void combat_control_update_info();
static GdControlResult gdControl();
static int gdCustomCreateWin();
static void gdCustomDestroyWin();
static void gdCustom();
static void gdCustomUpdateInfo();
static void gdCustomSelectRedraw(unsigned char* dest, int pitch, int type, int selectedIndex);
static int gdCustomSelect(int a1);
static void gdCustomUpdateSetting(int option, int value);

static int combat_control_load_assets()
{
    for (int i = 0; i < CC_MAX_ASSETS; ++i)
    {
        int fid = art_fid_by_name(OBJ_TYPE_INTERFACE, ASSET_NAMES[i]);
        cc_assets[i] = art_ptr_lock(fid, &cc_assets_keys[i]);
    }
}

static void combat_control_unload_assets()
{
    for (int i = 0; i < CC_MAX_ASSETS; ++i) {
        art_ptr_unlock(cc_assets_keys[i]);

        cc_assets[i] = nullptr;
        cc_assets_keys[i] = nullptr;
    }
}

static int combat_control_create()
{
    // Create window
    cc_window = win_add(
        CC_WINDOW_X,
        CC_WINDOW_Y,
        CC_WINDOW_WIDTH,
        CC_WINDOW_HEIGHT,
        256,
        WINDOW_FLAG_0x02);
    if (cc_window == -1) {
        combat_control_destroy();
        return -1;
    }

    // Draw the background
    CacheEntry* backgroundFrmHandle = nullptr;
    Art* backgroundFrm = art_ptr_lock(art_id(OBJ_TYPE_INTERFACE, 390, 0, 0, 0), &backgroundFrmHandle);
    if (backgroundFrm != NULL) {
        ui_image_32(backgroundFrm, cc_window, 0, 0, CC_WINDOW_WIDTH, CC_WINDOW_HEIGHT);
        art_ptr_unlock(backgroundFrmHandle);
    }

    // TALK
    cc_buttons[0] = ui_register_button(cc_window, 593 * ui_get_scale(), 41 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, KEY_ESCAPE, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (cc_buttons[0] == -1) {
        combat_control_destroy();
        return -1;
    }
    win_register_button_sound_func(cc_buttons[0], gsound_med_butt_press, gsound_med_butt_release);

    // USE BEST WEAPON
    cc_buttons[2] = ui_register_button(cc_window, 236 * ui_get_scale(), 15 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, KEY_LOWERCASE_W, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (cc_buttons[2] == -1) {
        combat_control_destroy();
        return -1;
    }
    win_register_button_sound_func(cc_buttons[1], gsound_med_butt_press, gsound_med_butt_release);

    // USE BEST ARMOR
    cc_buttons[3] = ui_register_button(cc_window, 235 * ui_get_scale(), 46 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, KEY_LOWERCASE_A, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (cc_buttons[3] == -1) {
        combat_control_destroy();
        return -1;
    }
    win_register_button_sound_func(cc_buttons[2], gsound_med_butt_press, gsound_med_butt_release);

    control_buttons_start = 4;

    int v21 = 3;

    for (int index = 0; index < 5; index++) {
        GameDialogButtonData* buttonData = &(control_button_info[index]);
        int fid;

        fid = art_id(OBJ_TYPE_INTERFACE, buttonData->upFrmId, 0, 0, 0);
        Art* upButtonFrm = art_ptr_lock(fid, &(buttonData->upFrmHandle));
        if (upButtonFrm == NULL) {
            combat_control_destroy();
            return -1;
        }

        int width = art_frame_width(upButtonFrm, 0, 0);
        int height = art_frame_length(upButtonFrm, 0, 0);

        fid = art_id(OBJ_TYPE_INTERFACE, buttonData->downFrmId, 0, 0, 0);
        Art* downButtonFrm = art_ptr_lock(fid, &(buttonData->downFrmHandle));
        if (downButtonFrm == NULL) {
            combat_control_destroy();
            return -1;
        }


        fid = art_id(OBJ_TYPE_INTERFACE, buttonData->disabledFrmId, 0, 0, 0);
        Art* disabledButtonFrm = art_ptr_lock(fid, &(buttonData->disabledFrmHandle));
        if (disabledButtonFrm == NULL) {
            combat_control_destroy();
            return -1;
        }

        unsigned char* disabledButtonFrmData = art_frame_data(disabledButtonFrm, 0, 0);

        v21++;

        int ui_scale = ui_get_scale();

        cc_buttons[v21] = ui_register_button(cc_window,
            buttonData->x * ui_scale,
            buttonData->y * ui_scale,
            width * ui_scale,
            height * ui_scale,
            -1,
            -1,
            buttonData->keyCode,
            -1,
            upButtonFrm,
            downButtonFrm,
            NULL,
            BUTTON_FLAG_TRANSPARENT | BUTTON_FLAG_0x04 | BUTTON_FLAG_0x01);
        if (cc_buttons[v21] == -1) {
            combat_control_destroy();
            return -1;
        }

        win_register_button_disable(cc_buttons[v21], disabledButtonFrmData, disabledButtonFrmData, disabledButtonFrmData);
        win_register_button_sound_func(cc_buttons[v21], gsound_med_butt_press, gsound_med_butt_release);

        if (!partyMemberHasAIDisposition(cc_target, buttonData->value)) {
            win_disable_button(cc_buttons[v21]);
        }
    }

    win_group_radio_buttons(5, &(cc_buttons[control_buttons_start]));

    int disposition = ai_get_disposition(cc_target);
    win_set_button_rest_state(cc_buttons[control_buttons_start + 4 - disposition], 1, 0);

    combat_control_update_info();

    win_draw(cc_window);

    return 0;
}

// 0x448C10
static void combat_control_destroy()
{
    if (cc_window == -1) {
        return;
    }

    for (int index = 0; index < 9; index++) {
        win_delete_button(cc_buttons[index]);
        cc_buttons[index] = -1;
    }

    for (int index = 0; index < 5; index++) {
        GameDialogButtonData* buttonData = &(control_button_info[index]);

        if (buttonData->upFrmHandle) {
            art_ptr_unlock(buttonData->upFrmHandle);
            buttonData->upFrmHandle = NULL;
        }

        if (buttonData->downFrmHandle) {
            art_ptr_unlock(buttonData->downFrmHandle);
            buttonData->downFrmHandle = NULL;
        }

        if (buttonData->disabledFrmHandle) {
            art_ptr_unlock(buttonData->disabledFrmHandle);
            buttonData->disabledFrmHandle = NULL;
        }
    }

    // Tear down our window and repaint whatever was behind it (head/map).
    Size scr_size = screen_get_size();
    Rect rect;
    rect.ulx = (scr_size.width - CC_WINDOW_WIDTH) / 2;
    rect.uly = (scr_size.height - CC_WINDOW_HEIGHT) / 2 + CC_WINDOW_HEIGHT - cc_subwin_len;
    rect.lrx = rect.ulx + CC_WINDOW_WIDTH - 1;
    rect.lry = rect.uly + cc_subwin_len - 1;

    win_delete(cc_window);
    cc_window = -1;
    win_refresh_all(&rect);
}

static void combat_control_update_info()
{
    int ui_scale = ui_get_scale();

    int oldFont = text_curr();
    text_font(105);

    unsigned char* windowBuffer = win_get_buf(cc_window);
    int windowWidth = win_width(cc_window);

    CacheEntry* backgroundHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 390, 0, 0, 0);
    Art* background = art_ptr_lock(backgroundFid, &backgroundHandle);
    if (background != NULL) {
        int width = art_frame_width(background, 0, 0);
        unsigned char* buffer = art_frame_data(background, 0, 0);

        // Clear "Weapon Used:". Scale the native bg region up to the scaled window.
        cscale(buffer + (width * 20 + 112) * 4, 110, text_height(), width,
            windowBuffer + (windowWidth * (20 * ui_scale) + (112 * ui_scale)) * 4, 110 * ui_scale, text_height() * ui_scale, windowWidth);

        // Clear "Armor Used:".
        cscale(buffer + (width * 49 + 112) * 4, 110, text_height(), width,
            windowBuffer + (windowWidth * (49 * ui_scale) + (112 * ui_scale)) * 4, 110 * ui_scale, text_height() * ui_scale, windowWidth);

        // Clear character preview.
        // cscale(buffer + (width * 84 + 8) * 4, 70, 98, width, windowBuffer + (windowWidth * (84 * ui_scale) + (8 * ui_scale)) * 4, 70 * ui_scale, 98 * ui_scale, windowWidth);

        // Clear ?
        cscale(buffer + (width * 80 + 232) * 4, 132, 106, width,
            windowBuffer + (windowWidth * (80 * ui_scale) + (232 * ui_scale)) * 4, 132 * ui_scale, 106 * ui_scale, windowWidth);

        art_ptr_unlock(backgroundHandle);
    }

    MessageListItem messageListItem;
    char* text;
    char formattedText[256];

    // Render item in right hand.
    Object* item2 = inven_right_hand(cc_target);
    text = item2 != NULL ? item_name(item2) : getmsg(&proto_main_msg_file, &messageListItem, 10);
    sprintf(formattedText, "%s", text);
    ui_text(cc_window, formattedText, 112 * ui_scale, 20 * ui_scale,110 * ui_scale, -1, 105, colorTable[992]);

    // Render armor.
    Object* armor = inven_worn(cc_target);
    text = armor != NULL ? item_name(armor) : getmsg(&proto_main_msg_file, &messageListItem, 10);
    sprintf(formattedText, "%s", text);
    ui_text(cc_window, formattedText, 112 * ui_scale, 49 * ui_scale,110 * ui_scale, -1, 105, colorTable[992]);

    // Render hit points.
    int maximumHitPoints = critterGetStat(cc_target, STAT_MAXIMUM_HIT_POINTS);
    int hitPoints = critterGetStat(cc_target, STAT_CURRENT_HIT_POINTS);
    sprintf(formattedText, "%d/%d", hitPoints, maximumHitPoints);
    ui_text(cc_window, formattedText, 240 * ui_scale, 96 * ui_scale,115 * ui_scale, -1, 105, colorTable[992]);

    // Render best skill.
    int bestSkill = partyMemberSkill(cc_target);
    text = skill_name(bestSkill);
    sprintf(formattedText, "%s", text);
    ui_text(cc_window, formattedText, 240 * ui_scale, 113 * ui_scale,115 * ui_scale, -1, 105, colorTable[992]);

    // Render weight summary.
    int inventoryWeight = item_total_weight(cc_target);
    int carryWeight = critterGetStat(cc_target, STAT_CARRY_WEIGHT);
    sprintf(formattedText, "%d/%d ", inventoryWeight, carryWeight);
    ui_text(cc_window, formattedText, 240 * ui_scale, 131 * ui_scale,115 * ui_scale, -1, 105, colorTable[992]);

    // Render melee damage.
    int meleeDamage = critterGetStat(cc_target, STAT_MELEE_DAMAGE);
    sprintf(formattedText, "%d", meleeDamage);
    ui_text(cc_window, formattedText, 240 * ui_scale, 148 * ui_scale,115 * ui_scale, -1, 105, colorTable[992]);

    // AP
    int actionPoints = isInCombat() ? cc_target->data.critter.combat.ap : critterGetStat(cc_target, STAT_MAXIMUM_ACTION_POINTS);
    int maximumActionPoints = critterGetStat(cc_target, STAT_MAXIMUM_ACTION_POINTS);
    sprintf(formattedText, "%d/%d ", actionPoints, maximumActionPoints);
    ui_text(cc_window, formattedText, 240 * ui_scale, 167 * ui_scale,115 * ui_scale, -1, 105, colorTable[992]);
}

// Runs the combat-control screen (and its custom AI sub-screen) as a single
// self-contained interaction, returning what the dialog should do next instead
// of driving the old dialogue_switch_mode state machine. The caller has already
// hidden the reply/options windows and the conversation can be torn down here.
void combat_control_run(Object* critter)
{
    cc_target = critter;

    combat_control_load_assets();

    combat_control_create();

    for (;;) {
        if (gdControl() != GD_CONTROL_CUSTOM) {
            combat_control_destroy();
            break;
        }
        combat_control_destroy();

        // "Custom" disposition: swap in the AI-settings sub-screen, then return
        // to the control screen when it closes.
        gdCustomCreateWin();
        gdCustom();
        gdCustomDestroyWin();
        combat_control_create();
    }

    combat_control_unload_assets();
}

// 0x449330
// 0x4493B8
static GdControlResult gdControl()
{
    MessageListItem messageListItem;

    bool done = false;
    while (!done) {
        int keyCode = get_input();
        if (keyCode != -1) {
            if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
                game_quit_with_confirm();
            }

            if (game_user_wants_to_quit != 0) {
                break;
            }

            if (keyCode == KEY_LOWERCASE_W) {
                inven_unwield(cc_target, 1);

                Object* weapon = ai_search_inven_weap(cc_target, 0, NULL);
                if (weapon != NULL) {
                    inven_wield(cc_target, weapon, 1);
                    cai_attempt_w_reload(cc_target, 0);

                    combat_control_update_info();
                }
            } else if (keyCode == 2098) {
                ai_set_disposition(cc_target, 4);
            } else if (keyCode == 2099) {
                ai_set_disposition(cc_target, 0);
                return GD_CONTROL_CUSTOM;
            } else if (keyCode == 2102) {
                ai_set_disposition(cc_target, 2);
            } else if (keyCode == 2103) {
                ai_set_disposition(cc_target, 3);
            } else if (keyCode == 2111) {
                ai_set_disposition(cc_target, 1);
            } else if (keyCode == KEY_ESCAPE) {
                return GD_CONTROL_TALK;
            } else if (keyCode == KEY_LOWERCASE_A) {
                if (cc_target->pid != 0x10000A1) {
                    Object* armor = ai_search_inven_armor(cc_target);
                    if (armor != NULL) {
                        inven_wield(cc_target, armor, 0);
                    }
                }

                combat_control_update_info();
            } else if (keyCode == -2) {
                if (mouse_click_in(441, 451, 540, 470)) {
                    ai_set_disposition(cc_target, 0);
                    return GD_CONTROL_CUSTOM;
                }
            }
        }
    }

    // Reached only when the player quits the game from this screen.
    return GD_CONTROL_TALK;
}

// 0x4496A0
static int gdCustomCreateWin()
{
    if (!message_init(&custom_msg_file)) {
        return -1;
    }

    if (!message_load(&custom_msg_file, "game\\custom.msg")) {
        return -1;
    }

    CacheEntry* backgroundFrmHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 391, 0, 0, 0);
    Art* backgroundFrm = art_ptr_lock(backgroundFid, &backgroundFrmHandle);
    if (backgroundFrm == NULL) {
        return -1;
    }

    unsigned char* backgroundFrmData = art_frame_data(backgroundFrm, 0, 0);
    if (backgroundFrmData == NULL) {
        // FIXME: Leaking background.
        gdCustomDestroyWin();
        return -1;
    }

    cc_subwin_len = art_frame_length(backgroundFrm, 0, 0) * ui_get_scale();

    int customizationWindowX = CC_WINDOW_X;
    int customizationWindowY = CC_WINDOW_Y + CC_WINDOW_HEIGHT - cc_subwin_len;
    cc_window = win_add(customizationWindowX,
        customizationWindowY,
        CC_WINDOW_WIDTH,
        cc_subwin_len,
        256,
        WINDOW_FLAG_0x02);
    if (cc_window == -1) {
        gdCustomDestroyWin();
        return -1;
    }

    // Draw the panel art straight into our (transparent) window — no head
    // composite, no slide animation.
    ui_image_32(backgroundFrm, cc_window, 0, 0, CC_WINDOW_WIDTH, cc_subwin_len);
    art_ptr_unlock(backgroundFrmHandle);
    win_draw(cc_window);

    cc_buttons[0] = ui_register_button(cc_window, 593 * ui_get_scale(), 101 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, 13, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], 0, BUTTON_FLAG_TRANSPARENT);
    if (cc_buttons[0] == -1) {
        gdCustomDestroyWin();
        return -1;
    }

    win_register_button_sound_func(cc_buttons[0], gsound_med_butt_press, gsound_med_butt_release);

    int optionButton = 0;
    custom_buttons_start = 1;

    for (int index = 0; index < PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT; index++) {
        GameDialogButtonData* buttonData = &(custom_button_info[index]);

        int upButtonFid = art_id(OBJ_TYPE_INTERFACE, buttonData->upFrmId, 0, 0, 0);
        Art* upButtonFrm = art_ptr_lock(upButtonFid, &(buttonData->upFrmHandle));
        if (upButtonFrm == NULL) {
            gdCustomDestroyWin();
            return -1;
        }

        int width = art_frame_width(upButtonFrm, 0, 0);
        int height = art_frame_length(upButtonFrm, 0, 0);

        int downButtonFid = art_id(OBJ_TYPE_INTERFACE, buttonData->downFrmId, 0, 0, 0);
        Art* downButtonFrm = art_ptr_lock(downButtonFid, &(buttonData->downFrmHandle));
        if (downButtonFrm == NULL) {
            gdCustomDestroyWin();
            return -1;
        }


        unsigned char* upButtonFrmData = art_frame_data(upButtonFrm, 0, 0);
        unsigned char* downButtonFrmData = art_frame_data(downButtonFrm, 0, 0);

        optionButton++;
        cc_buttons[optionButton] = win_register_button(cc_window,
            buttonData->x,
            buttonData->y,
            width,
            height,
            -1,
            -1,
            -1,
            buttonData->keyCode,
            upButtonFrmData,
            downButtonFrmData,
            NULL,
            BUTTON_FLAG_TRANSPARENT);
        if (cc_buttons[optionButton] == -1) {
            gdCustomDestroyWin();
            return -1;
        }

        win_register_button_sound_func(cc_buttons[index], gsound_med_butt_press, gsound_med_butt_release);
    }

    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE] = ai_get_burst_value(cc_target);
    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE] = ai_get_run_away_value(cc_target);
    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON] = ai_get_weapon_pref_value(cc_target);
    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE] = ai_get_distance_pref_value(cc_target);
    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO] = ai_get_attack_who_value(cc_target);
    custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE] = ai_get_chem_use_value(cc_target);


    gdCustomUpdateInfo();

    return 0;
}

// 0x449A10
static void gdCustomDestroyWin()
{
    if (cc_window == -1) {
        return;
    }

    for (int index = 0; index < 9; index++) {
        win_delete_button(cc_buttons[index]);
        cc_buttons[index] = -1;
    }

    for (int index = 0; index < PARTY_MEMBER_CUSTOMIZATION_OPTION_COUNT; index++) {
        GameDialogButtonData* buttonData = &(custom_button_info[index]);

        if (buttonData->upFrmHandle != NULL) {
            art_ptr_unlock(buttonData->upFrmHandle);
            buttonData->upFrmHandle = NULL;
        }

        if (buttonData->downFrmHandle != NULL) {
            art_ptr_unlock(buttonData->downFrmHandle);
            buttonData->downFrmHandle = NULL;
        }

        if (buttonData->disabledFrmHandle != NULL) {
            art_ptr_unlock(buttonData->disabledFrmHandle);
            buttonData->disabledFrmHandle = NULL;
        }
    }

    // Tear down our window and repaint whatever was behind it (head/map).
    Rect rect;
    rect.ulx = CC_WINDOW_X;
    rect.uly = CC_WINDOW_Y + CC_WINDOW_HEIGHT - cc_subwin_len;
    rect.lrx = rect.ulx + CC_WINDOW_WIDTH - 1;
    rect.lry = rect.uly + cc_subwin_len - 1;

    win_delete(cc_window);
    cc_window = -1;
    win_refresh_all(&rect);

    message_exit(&custom_msg_file);
}

// 0x449B3C
static void gdCustom()
{
    bool done = false;
    while (!done) {
        unsigned int keyCode = get_input();
        if (keyCode != -1) {
            if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
                game_quit_with_confirm();
            }

            if (game_user_wants_to_quit != 0) {
                break;
            }

            if (keyCode <= 5) {
                gdCustomSelect(keyCode);
                gdCustomUpdateInfo();
            } else if (keyCode == KEY_RETURN || keyCode == KEY_ESCAPE) {
                done = true;
            }
        }
    }
}

// 0x449BB4
static void gdCustomUpdateInfo()
{
    int oldFont = text_curr();
    text_font(105);

    unsigned char* windowBuffer = win_get_buf(cc_window);
    int windowWidth = win_width(cc_window);

    CacheEntry* backgroundHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 391, 0, 0, 0);
    Art* background = art_ptr_lock(backgroundFid, &backgroundHandle);
    if (background == NULL) {
        return;
    }

    int backgroundWidth = art_frame_width(background, 0, 0);
    int backgroundHeight = art_frame_length(background, 0, 0);
    unsigned char* backgroundData = art_frame_data(background, 0, 0);
    buf_to_buf(backgroundData, backgroundWidth, backgroundHeight, backgroundWidth, windowBuffer, CC_WINDOW_WIDTH);

    art_ptr_unlock(backgroundHandle);

    MessageListItem messageListItem;
    int num;
    char* msg;

    // BURST
    if (custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE] == -1) {
        // Not Applicable
        num = 99;
    } else {
        debug_printf("\nburst: %d", custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE]);
        num = custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE]].messageId;
    }

    msg = getmsg(&custom_msg_file, &messageListItem, num);
    text_to_buf(windowBuffer + (windowWidth * 20 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    // RUN AWAY
    msg = getmsg(&custom_msg_file, &messageListItem, custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE]].messageId);
    text_to_buf(windowBuffer + (windowWidth * 48 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    // WEAPON PREF
    msg = getmsg(&custom_msg_file, &messageListItem, custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON]].messageId);
    text_to_buf(windowBuffer + (windowWidth * 78 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    // DISTANCE
    msg = getmsg(&custom_msg_file, &messageListItem, custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE]].messageId);
    text_to_buf(windowBuffer + (windowWidth * 108 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    // ATTACK WHO
    msg = getmsg(&custom_msg_file, &messageListItem, custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO]].messageId);
    text_to_buf(windowBuffer + (windowWidth * 137 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    // CHEM USE
    msg = getmsg(&custom_msg_file, &messageListItem, custom_settings[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE][custom_current_selected[PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE]].messageId);
    text_to_buf(windowBuffer + (windowWidth * 166 + 232) * 4, msg, 248, windowWidth, colorTable[992]);

    
    text_font(oldFont);
}

// 0x449E64
static void gdCustomSelectRedraw(unsigned char* dest, int pitch, int type, int selectedIndex)
{
    MessageListItem messageListItem;

    text_font(105);

    for (int index = 0; index < 6; index++) {
        STRUCT_5189E4* ptr = &(custom_settings[type][index]);
        if (ptr->messageId != -1) {
            bool enabled = false;
            switch (type) {
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
                enabled = partyMemberHasAIBurstValue(cc_target, ptr->value);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
                enabled = partyMemberHasAIRunAwayValue(cc_target, ptr->value);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
                enabled = partyMemberHasAIWeaponPrefValue(cc_target, ptr->value);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
                enabled = partyMemberHasAIDistancePrefValue(cc_target, ptr->value);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
                enabled = partyMemberHasAIAttackWhoValue(cc_target, ptr->value);
                break;
            case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
                enabled = partyMemberHasAIChemUseValue(cc_target, ptr->value);
                break;
            }

            int color;
            if (enabled) {
                if (index == selectedIndex) {
                    color = colorTable[32747];
                } else {
                    color = colorTable[992];
                }
            } else {
                color = colorTable[15855];
            }

            const char* msg = getmsg(&custom_msg_file, &messageListItem, ptr->messageId);
            text_to_buf(dest + (pitch * (text_height() * index + 42) + 42) * 4, msg, pitch - 84, pitch, color);
        }
    }
}

// 0x449FC0
static int gdCustomSelect(int a1)
{
    int oldFont = text_curr();

    CacheEntry* backgroundFrmHandle;
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, 419, 0, 0, 0);
    Art* backgroundFrm = art_ptr_lock(backgroundFid, &backgroundFrmHandle);
    if (backgroundFrm == NULL) {
        return -1;
    }

    int backgroundFrmWidth = art_frame_width(backgroundFrm, 0, 0);
    int backgroundFrmHeight = art_frame_length(backgroundFrm, 0, 0);

    int selectWindowX = CC_WINDOW_X + (640 - backgroundFrmWidth) / 2;
    int selectWindowY = CC_WINDOW_Y + (480 - backgroundFrmHeight) / 2;
    int win = win_add(selectWindowX, selectWindowY, backgroundFrmWidth, backgroundFrmHeight, 256, WINDOW_FLAG_MODAL | WINDOW_FLAG_ALWAYS_ON_TOP);
    if (win == -1) {
        art_ptr_unlock(backgroundFrmHandle);
        return -1;
    }

    unsigned char* windowBuffer = win_get_buf(win);
    unsigned char* backgroundFrmData = art_frame_data(backgroundFrm, 0, 0);
    buf_to_buf(backgroundFrmData,
        backgroundFrmWidth,
        backgroundFrmHeight,
        backgroundFrmWidth,
        windowBuffer,
        backgroundFrmWidth);

    art_ptr_unlock(backgroundFrmHandle);

    int btn1 = ui_register_button(win, 70 * ui_get_scale(), 164 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, KEY_RETURN, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (btn1 == -1) {
        win_delete(win);
        return -1;
    }

    int btn2 = ui_register_button(win, 176 * ui_get_scale(), 163 * ui_get_scale(), 14 * ui_get_scale(), 14 * ui_get_scale(), -1, -1, -1, KEY_ESCAPE, cc_assets[CC_BUTTON_RED_UP], cc_assets[CC_BUTTON_RED_DOWN], NULL, BUTTON_FLAG_TRANSPARENT);
    if (btn2 == -1) {
        win_delete(win);
        return -1;
    }

    text_font(103);

    MessageListItem messageListItem;
    const char* msg;

    msg = getmsg(&custom_msg_file, &messageListItem, a1);
    text_to_buf(windowBuffer + (backgroundFrmWidth * 15 + 40) * 4, msg, backgroundFrmWidth, backgroundFrmWidth, colorTable[18979]);

    msg = getmsg(&custom_msg_file, &messageListItem, 10);
    text_to_buf(windowBuffer + (backgroundFrmWidth * 163 + 88) * 4, msg, backgroundFrmWidth, backgroundFrmWidth, colorTable[18979]);

    msg = getmsg(&custom_msg_file, &messageListItem, 11);
    text_to_buf(windowBuffer + (backgroundFrmWidth * 162 + 193) * 4, msg, backgroundFrmWidth, backgroundFrmWidth, colorTable[18979]);

    int value = custom_current_selected[a1];
    gdCustomSelectRedraw(windowBuffer, backgroundFrmWidth, a1, value);
    win_draw(win);

    int minX = selectWindowX + 42;
    int minY = selectWindowY + 42;
    int maxX = selectWindowX + backgroundFrmWidth - 42;
    int maxY = selectWindowY + backgroundFrmHeight - 42;

    bool done = false;
    unsigned int v53 = 0;
    while (!done) {
        int keyCode = get_input();
        if (keyCode == -1) {
            continue;
        }

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            game_quit_with_confirm();
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (keyCode == KEY_RETURN) {
            STRUCT_5189E4* ptr = &(custom_settings[a1][value]);
            custom_current_selected[a1] = value;
            gdCustomUpdateSetting(a1, ptr->value);
            done = true;
        } else if (keyCode == KEY_ESCAPE) {
            done = true;
        } else if (keyCode == -2) {
            if ((mouse_get_buttons() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
                continue;
            }

            if (!mouse_click_in(minX, minY, maxX, maxY)) {
                continue;
            }

            int mouseX;
            int mouseY;
            mouse_get_position(&mouseX, &mouseY);

            int lineHeight = text_height();
            int newValue = (mouseY - minY) / lineHeight;
            if (newValue >= 6) {
                continue;
            }

            unsigned int timestamp = get_time();
            if (newValue == value) {
                if (elapsed_tocks(timestamp, v53) < 250) {
                    custom_current_selected[a1] = newValue;
                    gdCustomUpdateSetting(a1, newValue);
                    done = true;
                }
            } else {
                STRUCT_5189E4* ptr = &(custom_settings[a1][newValue]);
                if (ptr->messageId != -1) {
                    bool enabled = false;
                    switch (a1) {
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
                        enabled = partyMemberHasAIBurstValue(cc_target, ptr->value);
                        break;
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
                        enabled = partyMemberHasAIRunAwayValue(cc_target, ptr->value);
                        break;
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
                        enabled = partyMemberHasAIWeaponPrefValue(cc_target, ptr->value);
                        break;
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
                        enabled = partyMemberHasAIDistancePrefValue(cc_target, ptr->value);
                        break;
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
                        enabled = partyMemberHasAIAttackWhoValue(cc_target, ptr->value);
                        break;
                    case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
                        enabled = partyMemberHasAIChemUseValue(cc_target, ptr->value);
                        break;
                    }

                    if (enabled) {
                        value = newValue;
                        gdCustomSelectRedraw(windowBuffer, backgroundFrmWidth, a1, newValue);
                        win_draw(win);
                    }
                }
            }
            v53 = timestamp;
        }
    }

    win_delete(win);
    text_font(oldFont);
    return 0;
}

// 0x44A4E0
static void gdCustomUpdateSetting(int option, int value)
{
    switch (option) {
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_AREA_ATTACK_MODE:
        ai_set_burst_value(cc_target, value);
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_RUN_AWAY_MODE:
        ai_set_run_away_value(cc_target, value);
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_BEST_WEAPON:
        ai_set_weapon_pref_value(cc_target, value);
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_DISTANCE:
        ai_set_distance_pref_value(cc_target, value);
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_ATTACK_WHO:
        ai_set_attack_who_value(cc_target, value);
        break;
    case PARTY_MEMBER_CUSTOMIZATION_OPTION_CHEM_USE:
        ai_set_chem_use_value(cc_target, value);
        break;
    }
}

#include "game/gdialog.h"
#include "game/ui/combat_control.h"
#include "game/ui/trade.h"
#include "game/ui.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "int/window.h"
#include "game/actions.h"
#include "game/art.h"
#include "plib/db/db.h"
#include "plib/color/color.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "plib/gnw/input.h"
#include "game/critter.h"
#include "game/cycle.h"
#include "plib/gnw/debug.h"
#include "int/dialog.h"
#include "game/display.h"
#include "plib/gnw/grbuf.h"
#include "game/game.h"
#include "game/gmouse.h"
#include "game/gsound.h"
#include "plib/gnw/rect.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/lip_sync.h"
#include "plib/gnw/memory.h"
#include "game/message.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "plib/gnw/text.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/svga.h"
#include "plib/os/os_window.h"

#define GAME_DIALOG_WINDOW_WIDTH (640 * ui_get_scale())
#define GAME_DIALOG_WINDOW_HEIGHT (480 * ui_get_scale())
#define GAME_DIALOG_NATIVE_WIDTH 640
#define GAME_DIALOG_NATIVE_HEIGHT 480
#define GAME_DIALOG_SCREEN_WIDTH (scr_size.lrx - scr_size.ulx + 1)
#define GAME_DIALOG_SCREEN_HEIGHT (scr_size.lry - scr_size.uly + 1)
#define GAME_DIALOG_WINDOW_X ((GAME_DIALOG_SCREEN_WIDTH - GAME_DIALOG_WINDOW_WIDTH) / 2)
#define GAME_DIALOG_WINDOW_Y ((GAME_DIALOG_SCREEN_HEIGHT - GAME_DIALOG_WINDOW_HEIGHT) / 2)

#define GAME_DIALOG_REPLY_WINDOW_X ((135 * ui_get_scale()) + GAME_DIALOG_WINDOW_X)
#define GAME_DIALOG_REPLY_WINDOW_Y ((225 * ui_get_scale()) + GAME_DIALOG_WINDOW_Y)
#define GAME_DIALOG_REPLY_WINDOW_WIDTH (379 * ui_get_scale())
#define GAME_DIALOG_REPLY_WINDOW_HEIGHT (58 * ui_get_scale())

#define GAME_DIALOG_OPTIONS_WINDOW_X ((127 * ui_get_scale()) + GAME_DIALOG_WINDOW_X)
#define GAME_DIALOG_OPTIONS_WINDOW_Y ((335 * ui_get_scale()) + GAME_DIALOG_WINDOW_Y)
#define GAME_DIALOG_OPTIONS_WINDOW_WIDTH (393 * ui_get_scale())
#define GAME_DIALOG_OPTIONS_WINDOW_HEIGHT (117 * ui_get_scale())

#define DIALOG_REVIEW_ENTRIES_CAPACITY 80

#define DIALOG_OPTION_ENTRIES_CAPACITY 30

// KeyCodes for engine-injected dialog options.
#define KEYCODE_BARTER 99
#define KEYCODE_COMBAT_CONTROL 100

typedef enum GameDialogReaction {
    GAME_DIALOG_REACTION_GOOD = 49,
    GAME_DIALOG_REACTION_NEUTRAL = 50,
    GAME_DIALOG_REACTION_BAD = 51,
} GameDialogReaction;

typedef struct GameDialogOptionEntry {
    int messageListId;
    int messageId;
    int reaction;
    int proc;
    int btn;
    int field_14;
    char text[900];
    int field_39C;
} GameDialogOptionEntry;

typedef struct GameDialogBlock {
    Program* program;
    int replyMessageListId;
    int replyMessageId;
    int offset;

    // NOTE: The is something odd about next two members. There are 2700 bytes,
    // which is 3 x 900, but anywhere in the app only 900 characters is used.
    // The length of text in [DialogOptionEntry] is definitely 900 bytes. There
    // are two possible explanations:
    // - it's an array of 3 elements.
    // - there are three separate elements, two of which are not used, therefore
    // they are not referenced anywhere, but they take up their space.
    //
    // See `gdProcessChoice` for more info how this unreferenced range plays
    // important role.
    char replyText[900];
    char field_394[1800];
    GameDialogOptionEntry options[DIALOG_OPTION_ENTRIES_CAPACITY];
} GameDialogBlock;

static int gdHide();
static int gdUnhide();
static int gdUnhideReply();
static int gdAddOption(int messageListId, int messageId, int reaction);
static int gdAddOptionStr(int a1, const char* a2, int a3);
static int gdProcessInit();
static void gdProcessCleanup();
static int gdProcessExit();
static int gdProcess();
static int gdProcessChoice(int a1);
static void gdProcessHighlight(int index);
static void gdProcessUnHighlight(int index);
static void gdProcessReply();
static void gdProcessUpdate();
static int gdCreateHeadWindow();
static void gdDestroyHeadWindow();
static void gdSetupFidget(int headFrmId, int reaction);
static void gdWaitForFidget();
static void gdPlayTransition(int anim);
static void reply_arrow_up(int btn, int keyCode);
static void reply_arrow_down(int btn, int keyCode);
static void reply_arrow_restore(int btn, int keyCode);
static void demo_copy_title(int win);
static void demo_copy_options(int win);
static void gDialogRefreshOptionsRect(int win, Rect* drawRect);
static void gdialog_bk();
static int text_num_lines(const char* a1, int a2);
static int text_to_rect_wrapped(unsigned char* buffer, Rect* rect, char* string, int* a4, int height, int pitch, int color);
static int text_to_rect_func(unsigned char* buffer, Rect* rect, char* string, int* a4, int height, int pitch, int color, int a7);
static void gdialog_barter_pressed(int btn, int keyCode);
static int gdialog_window_create();
static void gdialog_scroll_subwin(int win, int a2, unsigned char* a3, unsigned char* a4, unsigned char* a5, int a6, int a7, int backPitch);
static void gdialog_window_destroy();
static int talk_to_refresh_background_window();
static int talkToRefreshDialogWindowRect(Rect* rect);
static void gdDisplayFrame(Art* headFrm, int frame);
static void gdBlendTableInit();
static void gdBlendTableExit();

// 0x5186D4
static int dialog_state_fix = 0;

// 0x5186D8
static int gdNumOptions = 0;

// 0x5186E0
static unsigned char* headWindowBuffer = NULL;

// 0x5186E4
static int gReplyWin = -1;

// 0x5186E8
static int gOptionWin = -1;

// 0x5186EC
static bool gdialog_window_created = false;

// 0x5186F0
static int boxesWereDisabled = 0;

// 0x5186F4
static int fidgetFID = 0;

// 0x5186F8
static CacheEntry* fidgetKey = NULL;

// 0x5186FC
static Art* fidgetFp = NULL;

// 0x518700
static int backgroundIndex = 2;

// 0x518704
static int lipsFID = 0;

// 0x518708
static CacheEntry* lipsKey = NULL;

// 0x51870C
static Art* lipsFp = NULL;

// 0x518710
static bool gdialog_speech_playing = false;

// 0x518714
static int dialogue_state = 0;

// 0x518718
static int dialogue_switch_mode = 0;

// Set by any barter trigger (the TRADE button, the combat-control "trade" exit, or
// the gdialog_barter script opcode) and consumed once per gdProcess loop by
// trade_run(). Replaces the old dialogue_switch_mode 2/3 barter handoff.
static bool barter_requested = false;

// Set when the combat-control button ends the conversation; the combat-control
// screen is launched once the dialog has fully torn down (see gdialogEnter).
static Object* combat_control_pending = NULL;

// 0x51871C
static int gdialog_state = -1;

// 0x518720
static bool gdDialogWentOff = false;

// 0x518724
static bool gdDialogTurnMouseOff = false;

// 0x518728
static int gdReenterLevel = 0;

// 0x51872C
static bool gdReplyTooBig = false;

// 0x51873C
static int gdBarterMod = 0;

// 0x518740
static int dialogueBackWindow = -1;

// 0x518744
static int dialogueWindow = -1;

// 0x518748
static Rect backgrndRects[8] = {
    { 126, 14, 152, 40 },
    { 488, 14, 514, 40 },
    { 126, 188, 152, 214 },
    { 488, 188, 514, 214 },
    { 152, 14, 488, 24 },
    { 152, 204, 488, 214 },
    { 126, 40, 136, 188 },
    { 504, 40, 514, 188 },
};

// 0x5187C8
static int talk_need_to_center = 1;

// 0x5187CC
static bool can_start_new_fidget = false;

// 0x5187D0
static int gd_replyWin = -1;

// 0x5187D4
static int gd_optionsWin = -1;

// 0x5187D8
static int gDialogMusicVol = -1;

// 0x5187DC
static int gdCenterTile = -1;

// 0x5187E0
static int gdPlayerTile = -1;

// 0x5187EC
static int dialogue_just_started = 0;

// 0x5187F0
static int dialogue_seconds_since_last_input = 0;

// 0x518848
Object* dialog_target = NULL;

// 0x51884C
bool dialog_target_is_party = false;

// 0x518850
int dialogue_head = 0;

// 0x518854
int dialogue_scr_id = -1;

// Maps phoneme to talking head frame.
//
// 0x518858
static int head_phoneme_lookup[PHONEME_COUNT] = {
    0,
    3,
    1,
    1,
    3,
    1,
    1,
    1,
    7,
    8,
    7,
    3,
    1,
    8,
    1,
    7,
    7,
    6,
    6,
    2,
    2,
    2,
    2,
    4,
    4,
    5,
    5,
    2,
    2,
    2,
    2,
    2,
    6,
    2,
    2,
    5,
    8,
    2,
    2,
    2,
    2,
    8,
};

// 0x51890C
static const char* react_strs[3] = {
    "Said Good",
    "Said Neutral",
    "Said Bad",
};

// 0x518918
static int dialogue_subwin_len = 0;





// 0x58ECA0
static unsigned char* backgrndBufs[8];

// 0x58ECC0
static Rect optionRect;

// 0x58ECD0
static Rect replyRect;



// 0x58F46C
static CacheEntry* dialog_red_button_up_key;

// 0x58F470
static int gdialog_buttons[9];

// 0x58F494
static CacheEntry* upper_hi_key;

// 0x58F49C
static int lower_hi_len;

// 0x58F4A4
static Art* dialog_red_button_down_art;

// 0x58F4A8
static int lower_hi_wid;

// 0x58F4AC
static Art* dialog_red_button_up_art;

// 0x58F4B0
static int upper_hi_wid;

// Yellow highlight blick effect.
//
// 0x58F4B4
static Art* lower_hi_fp;

// 0x58F4B8
static int upper_hi_len;

// 0x58F4BC
static CacheEntry* dialog_red_button_down_key;

// 0x58F4C0
static CacheEntry* lower_hi_key;

// White highlight blick effect.
//
// This effect appears at the top-right corner on dialog display. Together with
// [gDialogLowerHighlight] it gives an effect of depth of the monitor.
//
// 0x58F4C4
static Art* upper_hi_fp;

// Raw 8-bit intensity data for the head-highlight FRMs. The normal art pipeline
// converts FRMs to RGBA at load, which destroys these masks (their pixel values
// are blend weights, not colors), so - like the wall-see-through egg mask - we
// load the raw bytes ourselves. Plus the precomputed 8-bit tint colors.
static unsigned char* upper_hi_mask = NULL;
static unsigned char* lower_hi_mask = NULL;
static int light_tint_r, light_tint_g, light_tint_b;
static int dark_tint_r, dark_tint_g, dark_tint_b;

// 0x58F4C8
static int oldFont;

// 0x58F4CC
static unsigned int fidgetLastTime;

// 0x58F4D0
static int fidgetAnim;

// 0x58F4D4
static GameDialogBlock dialogBlock;

// 0x596C30
static int talkOldFont;

// 0x596C34
static unsigned int fidgetTocksPerFrame;

// 0x596C38
static int fidgetFrameCounter;

// 0x444D1C
int gdialogInit()
{
    return 0;
}

// 0x444D20
int gdialogReset()
{
    gdialogFreeSpeech();
    return 0;
}

// 0x444D20.
int gdialogExit()
{
    gdialogFreeSpeech();
    return 0;
}

// 0x444D2C
bool gdialogActive()
{
    return dialog_state_fix != 0;
}

// gdialogEnter
// 0x444D3C
void gdialogEnter(Object* a1, int a2)
{
    if (a1 == NULL) {
        debug_printf("\nError: gdialogEnter: target was NULL!");
        return;
    }

    gdDialogWentOff = false;

    if (isInCombat()) {
        return;
    }

    if (a1->sid == -1) {
        return;
    }

    if (PID_TYPE(a1->pid) != OBJ_TYPE_ITEM && SID_TYPE(a1->sid) != SCRIPT_TYPE_SPATIAL) {
        MessageListItem messageListItem;

        int rc = action_can_talk_to(obj_dude, a1);
        if (rc == -1) {
            // You can't see there.
            messageListItem.num = 660;
            if (message_search(&proto_main_msg_file, &messageListItem)) {
                if (a2) {
                    display_print(messageListItem.text);
                } else {
                    debug_printf(messageListItem.text);
                }
            } else {
                debug_printf("\nError: gdialog: Can't find message!");
            }
            return;
        }

        if (rc == -2) {
            // Too far away.
            messageListItem.num = 661;
            if (message_search(&proto_main_msg_file, &messageListItem)) {
                if (a2) {
                    display_print(messageListItem.text);
                } else {
                    debug_printf(messageListItem.text);
                }
            } else {
                debug_printf("\nError: gdialog: Can't find message!");
            }
            return;
        }
    }

    gdCenterTile = tile_center_tile;
    gdBarterMod = 0;
    gdPlayerTile = obj_dude->tile;
    map_disable_bk_processes();

    dialog_state_fix = 1;
    dialog_target = a1;
    dialog_target_is_party = isPartyMember(a1);

    dialogue_just_started = 1;

    if (a1->sid != -1) {
        exec_script_proc(a1->sid, SCRIPT_PROC_TALK);
    }

    // The conversation has fully torn down here. If it ended via the combat-control
    // button, run that screen now as its own state, over the map.
    if (combat_control_pending != NULL) {
        Object* follower = combat_control_pending;
        combat_control_pending = NULL;
        combat_control_run(follower);
    }

    Script* script;
    if (scr_ptr(a1->sid, &script) == -1) {
        gmouse_3d_on();
        map_enable_bk_processes();
        scr_exec_map_update_scripts();
        dialog_state_fix = 0;
        return;
    }

    if (script->scriptOverrides || dialogue_state != 4) {
        dialogue_just_started = 0;
        map_enable_bk_processes();
        scr_exec_map_update_scripts();
        dialog_state_fix = 0;
        return;
    }

    gdialogFreeSpeech();

    if (gdialog_state == 1) {
        // Barter/control/custom are standalone screens now; the conversation only
        // ever owns the chat subwindow here (dialogue_state 1), so just tear that
        // down. (The old barter sub-window cleanup branches were dead — barter no
        // longer uses dialogueWindow.)
        if (dialogue_state == gdialog_state) {
            gdialog_window_destroy();
        }
        gdialogExitFromScript();
    }

    gdialog_state = 0;
    dialogue_state = 0;

    int tile = obj_dude->tile;
    if (gdPlayerTile != tile) {
        gdCenterTile = tile;
    }

    if (gdDialogWentOff) {
        tile_scroll_to(gdCenterTile, 2);
    }

    map_enable_bk_processes();
    scr_exec_map_update_scripts();

    dialog_state_fix = 0;
}

// 0x444FE4
void gdialogSystemEnter()
{
    game_state_update();

    gdDialogTurnMouseOff = true;

    soundContinueAll();
    gdialogEnter(dialog_target, 0);
    soundContinueAll();

    if (gdPlayerTile != obj_dude->tile) {
        gdCenterTile = obj_dude->tile;
    }

    if (gdDialogWentOff) {
        tile_scroll_to(gdCenterTile, 2);
    }

    game_state_request(GAME_STATE_2);

    game_state_update();
}

// 0x445050
void gdialogSetupSpeech(const char* audioFileName)
{
    if (audioFileName == NULL) {
        debug_printf("\nGDialog: Bleep!");
        gsound_play_sfx_file("censor");
        return;
    }

    char name[16];
    if (art_get_base_name(OBJ_TYPE_HEAD, dialogue_head & 0xFFF, name) == -1) {
        return;
    }

    if (lips_load_file(audioFileName, name) == -1) {
        return;
    }

    gdialog_speech_playing = true;

    lips_play_speech();

    debug_printf("Starting lipsynch speech");
}

// 0x4450C4
void gdialogFreeSpeech()
{
    if (gdialog_speech_playing) {
        debug_printf("Ending lipsynch system");
        gdialog_speech_playing = false;

        lips_free_speech();
    }
}

// 0x4450EC
int gdialogEnableBK()
{
    add_bk_process(gdialog_bk);
    return 0;
}

// 0x4450FC
int gdialogDisableBK()
{
    remove_bk_process(gdialog_bk);
    return 0;
}

// 0x44510C
int gdialogInitFromScript(int headFid, int reaction)
{
    if (dialogue_state == 1) {
        return -1;
    }

    if (gdialog_state == 1) {
        return 0;
    }

    anim_stop();

    boxesWereDisabled = disable_box_bar_win();
    dialog_target_is_party = isPartyMember(dialog_target);
    oldFont = text_curr();
    text_font(105);
    dialogSetReplyWindow(135, 225, 379, 58, NULL);
    dialogSetReplyColor(0.3f, 0.3f, 0.3f);
    dialogSetOptionWindow(127, 335, 393, 117, NULL);
    dialogSetOptionColor(0.2f, 0.2f, 0.2f);
    dialogTitle(NULL);
    dialogRegisterWinDrawCallbacks(demo_copy_title, demo_copy_options);
    gdBlendTableInit();
    cycle_disable();
    if (gdDialogTurnMouseOff) {
        gmouse_disable(0);
    }
    gmouse_3d_off();
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
    text_object_reset();

    if (PID_TYPE(dialog_target->pid) != OBJ_TYPE_ITEM) {
        tile_scroll_to(dialog_target->tile, 2);
    }

    talk_need_to_center = 1;

    gdCreateHeadWindow();
    add_bk_process(gdialog_bk);
    gdSetupFidget(headFid, reaction);
    gdialog_state = 1;
    gmouse_disable_scrolling();

    if (headFid == -1) {
        gDialogMusicVol = gsound_background_volume_get_set(gDialogMusicVol / 2);
    } else {
        gDialogMusicVol = -1;
        gsound_background_stop();
    }

    gdDialogWentOff = true;

    return 0;
}

// 0x445298
int gdialogExitFromScript()
{
    if (gdialog_state == 0) {
        return 0;
    }

    gdialogFreeSpeech();

    remove_bk_process(gdialog_bk);

    if (PID_TYPE(dialog_target->pid) != OBJ_TYPE_ITEM) {
        if (gdPlayerTile != obj_dude->tile) {
            gdCenterTile = obj_dude->tile;
        }
        tile_scroll_to(gdCenterTile, 2);
    }

    gdDestroyHeadWindow();

    text_font(oldFont);

    if (fidgetFp != NULL) {
        art_ptr_unlock(fidgetKey);
        fidgetFp = NULL;
    }

    if (lipsKey != NULL) {
        if (art_ptr_unlock(lipsKey) == -1) {
            debug_printf("Failure unlocking lips frame!\n");
        }
        lipsKey = NULL;
        lipsFp = NULL;
        lipsFID = 0;
    }

    // NOTE: Uninline.
    gdBlendTableExit();

    gdialog_state = 0;
    dialogue_state = 0;

    cycle_enable();

    if (!game_ui_is_disabled()) {
        gmouse_enable_scrolling();
    }

    if (gDialogMusicVol == -1) {
        gsound_background_restart_last(11);
    } else {
        gsound_background_volume_set(gDialogMusicVol);
    }

    if (boxesWereDisabled) {
        enable_box_bar_win();
    }

    boxesWereDisabled = 0;

    if (gdDialogTurnMouseOff) {
        if (!game_ui_is_disabled()) {
            gmouse_enable();
        }

        gdDialogTurnMouseOff = 0;
    }

    if (!game_ui_is_disabled()) {
        gmouse_3d_on();
    }

    gdDialogWentOff = true;

    return 0;
}

// 0x445438
void gdialogSetBackground(int a1)
{
    if (a1 != -1) {
        backgroundIndex = a1;
    }
}

// Renders supplementary message in reply area of the dialog.
//
// 0x445448
void gdialogDisplayMsg(char* msg)
{
    if (gd_replyWin == -1) {
        debug_printf("\nError: Reply window doesn't exist!");
        return;
    }

    replyRect.ulx = 5;
    replyRect.uly = 10;
    replyRect.lrx = 374*2;
    replyRect.lry = 58*2;
    demo_copy_title(gReplyWin);

    unsigned char* windowBuffer = win_get_buf(gReplyWin);
    int lineHeight = text_height();

    int a4 = 0;

    // NOTE: Uninline.
    text_to_rect_wrapped(windowBuffer,
        &replyRect,
        msg,
        &a4,
        lineHeight,
        379*2,
        colorTable[992] | 0x2000000);

    win_show(gd_replyWin);
    win_draw(gReplyWin);
}

// 0x4454FC
int gdialogStart()
{
    gdNumOptions = 0;
    return 0;
}

// 0x445510
int gdialogSayMessage()
{
    mouse_show();
    gdialogGo();

    gdNumOptions = 0;
    dialogBlock.replyMessageListId = -1;

    return 0;
}

// NOTE: If you look at the scripts handlers, my best guess that their intention
// was to allow scripters to specify proc names instead of proc addresses. They
// dropped this idea, probably because they've updated their own compiler, or
// maybe there was not enough time to complete it. Any way, [procedure] is the
// identifier of the procedure in the script, but it is silently ignored.
//
// 0x445538
int gdialogOption(int messageListId, int messageId, const char* proc, int reaction)
{
    dialogBlock.options[gdNumOptions].proc = 0;

    return gdAddOption(messageListId, messageId, reaction);
}

// NOTE: If you look at the script handlers, my best guess that their intention
// was to allow scripters to specify proc names instead of proc addresses. They
// dropped this idea, probably because they've updated their own compiler, or
// maybe there was not enough time to complete it. Any way, [procedure] is the
// identifier of the procedure in the script, but it is silently ignored.
//
// 0x445578
int gdialogOptionStr(int messageListId, const char* text, const char* proc, int reaction)
{
    dialogBlock.options[gdNumOptions].proc = 0;

    return gdAddOptionStr(messageListId, text, reaction);
}

// 0x4455B8
int gdialogOptionProc(int messageListId, int messageId, int proc, int reaction)
{
    dialogBlock.options[gdNumOptions].proc = proc;

    return gdAddOption(messageListId, messageId, reaction);
}

// 0x4455FC
int gdialogOptionProcStr(int messageListId, const char* text, int proc, int reaction)
{
    dialogBlock.options[gdNumOptions].proc = proc;

    return gdAddOptionStr(messageListId, text, reaction);
}

// 0x445640
int gdialogReply(Program* program, int messageListId, int messageId)
{
    dialogBlock.program = program;
    dialogBlock.replyMessageListId = messageListId;
    dialogBlock.replyMessageId = messageId;
    dialogBlock.offset = 0;
    dialogBlock.replyText[0] = '\0';
    gdNumOptions = 0;

    return 0;
}

// 0x44567C
int gdialogReplyStr(Program* program, int messageListId, const char* text)
{
    dialogBlock.program = program;
    dialogBlock.offset = 0;
    dialogBlock.replyMessageListId = -4;
    dialogBlock.replyMessageId = -4;

    strcpy(dialogBlock.replyText, text);

    gdNumOptions = 0;

    return 0;
}

// 0x4456D8
int gdialogGo()
{
    if (dialogBlock.replyMessageListId == -1) {
        return 0;
    }

    int rc = 0;

    if (gdNumOptions < 1) {
        dialogBlock.options[gdNumOptions].proc = 0;

        if (gdAddOption(-1, -1, 50) == -1) {
            interpretError("Error setting option.");
            rc = -1;
        }
    }

    if (rc != -1) {
        rc = gdProcess();
    }

    gdNumOptions = 0;

    return rc;
}

// 0x445764
void gdialogUpdatePartyStatus()
{
    if (dialogue_state != 1) {
        return;
    }

    bool is_party = isPartyMember(dialog_target);
    if (is_party == dialog_target_is_party) {
        return;
    }

    // NOTE: Uninline.
    gdHide();

    gdialog_window_destroy();

    dialog_target_is_party = is_party;

    gdialog_window_create();

    // NOTE: Uninline.
    gdUnhide();
}

// NOTE: Inlined.
//
// 0x4457EC
static int gdHide()
{
    if (gd_replyWin != -1) {
        win_hide(gd_replyWin);
    }

    if (gd_optionsWin != -1) {
        win_hide(gd_optionsWin);
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x445818
static int gdUnhide()
{
    if (gd_replyWin != -1) {
        win_show(gd_replyWin);
    }

    if (gd_optionsWin != -1) {
        win_show(gd_optionsWin);
    }

    return 0;
}

// 0x44585C
static int gdAddOption(int messageListId, int messageId, int reaction)
{
    if (gdNumOptions >= DIALOG_OPTION_ENTRIES_CAPACITY) {
        debug_printf("\nError: dialog: Ran out of options!");
        return -1;
    }

    GameDialogOptionEntry* optionEntry = &(dialogBlock.options[gdNumOptions]);
    optionEntry->messageListId = messageListId;
    optionEntry->messageId = messageId;
    optionEntry->reaction = reaction;
    optionEntry->btn = -1;
    optionEntry->text[0] = '\0';

    gdNumOptions++;

    return 0;
}

// 0x4458BC
static int gdAddOptionStr(int messageListId, const char* text, int reaction)
{
    if (gdNumOptions >= DIALOG_OPTION_ENTRIES_CAPACITY) {
        debug_printf("\nError: dialog: Ran out of options!");
        return -1;
    }

    GameDialogOptionEntry* optionEntry = &(dialogBlock.options[gdNumOptions]);
    optionEntry->messageListId = -4;
    optionEntry->messageId = -4;
    optionEntry->reaction = reaction;
    optionEntry->btn = -1;
    sprintf(optionEntry->text, "%c %s", '\x95', text);

    gdNumOptions++;

    return 0;
}

// Creates dialog interface.
//
// 0x446288
static int gdProcessInit()
{
    int upBtn;
    int downBtn;
    int optionsWindowX;
    int optionsWindowY;
    int fid;

    int replyWindowX = GAME_DIALOG_REPLY_WINDOW_X;
    int replyWindowY = GAME_DIALOG_REPLY_WINDOW_Y;
    gReplyWin = win_add(replyWindowX,
        replyWindowY,
        GAME_DIALOG_REPLY_WINDOW_WIDTH,
        GAME_DIALOG_REPLY_WINDOW_HEIGHT,
        0,
        WINDOW_FLAG_ALWAYS_ON_TOP | WINDOW_FLAG_0x20);
    if (gReplyWin == -1) {
        goto err;
    }

    // Top part of the reply window - scroll up.
    upBtn = win_register_button(gReplyWin, 1 * ui_get_scale(), 1 * ui_get_scale(), 377 * ui_get_scale(), 28 * ui_get_scale(), -1, -1, KEY_ARROW_UP, -1, NULL, NULL, NULL, 32);
    if (upBtn == -1) {
        goto err_1;
    }

    win_register_button_sound_func(upBtn, gsound_red_butt_press, gsound_red_butt_release);
    win_register_button_func(upBtn, reply_arrow_up, reply_arrow_restore, 0, 0);

    // Bottom part of the reply window - scroll down.
    downBtn = win_register_button(gReplyWin, 1 * ui_get_scale(), 29 * ui_get_scale(), 377 * ui_get_scale(), 28 * ui_get_scale(), -1, -1, KEY_ARROW_DOWN, -1, NULL, NULL, NULL, 32);
    if (downBtn == -1) {
        goto err_1;
    }

    win_register_button_sound_func(downBtn, gsound_red_butt_press, gsound_red_butt_release);
    win_register_button_func(downBtn, reply_arrow_down, reply_arrow_restore, 0, 0);

    optionsWindowX = GAME_DIALOG_OPTIONS_WINDOW_X;
    optionsWindowY = GAME_DIALOG_OPTIONS_WINDOW_Y;
    gOptionWin = win_add(optionsWindowX, optionsWindowY, GAME_DIALOG_OPTIONS_WINDOW_WIDTH, GAME_DIALOG_OPTIONS_WINDOW_HEIGHT, 0, WINDOW_FLAG_ALWAYS_ON_TOP | WINDOW_FLAG_0x20);
    if (gOptionWin == -1) {
        goto err_2;
    }

    // di_rdbt2.frm - dialog red button down
    fid = art_id(OBJ_TYPE_INTERFACE, 96, 0, 0, 0);
    dialog_red_button_up_art = art_ptr_lock(fid, &dialog_red_button_up_key);
    if (dialog_red_button_up_art == NULL) {
        goto err_3;
    }

    // di_rdbt1.frm - dialog red button up
    fid = art_id(OBJ_TYPE_INTERFACE, 95, 0, 0, 0);
    dialog_red_button_down_art = art_ptr_lock(fid, &dialog_red_button_down_key);
    if (dialog_red_button_down_art == NULL) {
        goto err_3;
    }

    talkOldFont = text_curr();
    text_font(105);

    return 0;

err_3:

    art_ptr_unlock(dialog_red_button_up_key);
    dialog_red_button_up_key = NULL;

err_2:

    win_delete(gOptionWin);
    gOptionWin = -1;

err_1:

    win_delete(gReplyWin);
    gReplyWin = -1;

err:

    return -1;
}

// RELASE: Rename/comment.
// free dialog option buttons
// 0x446454
static void gdProcessCleanup()
{
    for (int index = 0; index < gdNumOptions; index++) {
        GameDialogOptionEntry* optionEntry = &(dialogBlock.options[index]);

        if (optionEntry->btn != -1) {
            win_delete_button(optionEntry->btn);
            optionEntry->btn = -1;
        }
    }
}

// RELASE: Rename/comment.
// free dialog interface
// 0x446498
static int gdProcessExit()
{
    gdProcessCleanup();

    art_ptr_unlock(dialog_red_button_down_key);
    dialog_red_button_down_key = NULL;
    dialog_red_button_down_art = NULL;

    art_ptr_unlock(dialog_red_button_up_key);
    dialog_red_button_up_key = NULL;
    dialog_red_button_up_art = NULL;

    win_delete(gReplyWin);
    gReplyWin = -1;

    win_delete(gOptionWin);
    gOptionWin = -1;

    text_font(talkOldFont);

    return 0;
}

// 0x4465C0
static int gdProcess()
{
    if (gdReenterLevel == 0) {
        if (gdProcessInit() == -1) {
            return -1;
        }
    }

    gdReenterLevel += 1;

    gdProcessUpdate();

    int v18 = 0;
    if (dialogBlock.offset != 0) {
        v18 = 1;
        gdReplyTooBig = 1;
    }

    unsigned int tick = get_time();
    int pageCount = 0;
    int pageIndex = 0;
    int pageOffsets[10];
    pageOffsets[0] = 0;
    for (;;) {
        int keyCode = get_input();

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            game_quit_with_confirm();
        }

        if (game_user_wants_to_quit != 0) {
            break;
        }

        if (keyCode == KEY_CTRL_B && !mouse_click_in(135, 225, 514, 283)) {
            if (gmouse_get_cursor() != MOUSE_CURSOR_ARROW) {
                gmouse_set_cursor(MOUSE_CURSOR_ARROW);
            }
        } else {
            if (barter_requested) {
                barter_requested = false;

                // Hide the whole conversation (reply/options, chat subwindow and
                // the talking head) so the standalone trade screen shows on its
                // own, then restore it when trade returns.
                gdHide();
                gdialog_window_destroy();
                win_hide(dialogueBackWindow);
                win_refresh_all(&scr_size);

                // The conversation is suspended while the trade screen owns the
                // display, so stop its background process (head fidget/speech).
                gdialogDisableBK();
                trade_run(dialog_target, gdBarterMod, dialog_target_is_party);
                gdialogEnableBK();

                win_show(dialogueBackWindow);
                gdialog_window_create();
                gdUnhide();

                // trade's exit_inventory()/gmouse_enable() re-enables map
                // edge-scrolling; our centered transparent dialog would let a
                // stray edge-scroll repaint the map over it. Keep it disabled.
                gmouse_disable_scrolling();
                continue;
            }
        }

        if (gdReplyTooBig) {
            unsigned int v6 = get_bk_time();
            if (v18) {
                if (elapsed_tocks(v6, tick) >= 10000 || keyCode == KEY_SPACE) {
                    pageCount++;
                    pageIndex++;
                    pageOffsets[pageCount] = dialogBlock.offset;
                    gdProcessReply();
                    tick = v6;
                    if (!dialogBlock.offset) {
                        v18 = 0;
                    }
                }
            }

            if (keyCode == KEY_ARROW_UP) {
                if (pageIndex > 0) {
                    pageIndex--;
                    dialogBlock.offset = pageOffsets[pageIndex];
                    v18 = 0;
                    gdProcessReply();
                }
            } else if (keyCode == KEY_ARROW_DOWN) {
                if (pageIndex < pageCount) {
                    pageIndex++;
                    dialogBlock.offset = pageOffsets[pageIndex];
                    v18 = 0;
                    gdProcessReply();
                } else {
                    if (dialogBlock.offset != 0) {
                        tick = v6;
                        pageIndex++;
                        pageCount++;
                        pageOffsets[pageCount] = dialogBlock.offset;
                        v18 = 0;
                        gdProcessReply();
                    }
                }
            }
        }

        if (keyCode != -1) {
            if (keyCode >= 1200 && keyCode <= 1250) {
                gdProcessHighlight(keyCode - 1200);
            } else if (keyCode >= 1300 && keyCode <= 1330) {
                gdProcessUnHighlight(keyCode - 1300);
            } else if (keyCode == KEYCODE_BARTER) {
                gdialog_barter_pressed(-1, -1);
            } else if (keyCode == KEYCODE_COMBAT_CONTROL) {
                // Combat control is its own state, not a dialog sub-panel: end
                // the conversation (break out exactly like a goodbye option), and
                // launch combat control after the dialog has fully torn down — see
                // gdialogEnter, just past exec_script_proc(TALK).
                combat_control_pending = dialog_target;
                break;
            } else if (keyCode >= 48 && keyCode <= 57) {
                int v11 = keyCode - 49;
                if (v11 < gdNumOptions) {
                    pageCount = 0;
                    pageIndex = 0;
                    pageOffsets[0] = 0;
                    gdReplyTooBig = 0;

                    if (gdProcessChoice(v11) == -1) {
                        break;
                    }

                    tick = get_time();

                    if (dialogBlock.offset) {
                        v18 = 1;
                        gdReplyTooBig = 1;
                    } else {
                        v18 = 0;
                    }
                }
            }
        }
    }

    gdReenterLevel -= 1;

    if (gdReenterLevel == 0) {
        if (gdProcessExit() == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4468DC
static int gdProcessChoice(int a1)
{
    // FIXME: There is a buffer underread bug when `a1` is -1 (pressing 0 on the
    // keyboard, see `gdProcess`). When it happens the game looks into unused
    // continuation of `dialogBlock.replyText` (within 0x58F868-0x58FF70 range) which
    // is initialized to 0 according to C spec. I was not able to replicate the
    // same behaviour by extending dialogBlock.replyText to 2700 bytes or introduce
    // new 1800 bytes buffer in between, at least not in debug builds. In order
    // to preserve original behaviour this dummy dialog option entry is used.
    //
    // TODO: Recheck behaviour after introducing |GameDialogBlock|.
    GameDialogOptionEntry dummy;
    memset(&dummy, 0, sizeof(dummy));

    mouse_hide();
    gdProcessCleanup();

    GameDialogOptionEntry* dialogOptionEntry = a1 != -1 ? &(dialogBlock.options[a1]) : &dummy;

    can_start_new_fidget = false;

    gdialogFreeSpeech();

    int v1 = GAME_DIALOG_REACTION_NEUTRAL;
    switch (dialogOptionEntry->reaction) {
    case GAME_DIALOG_REACTION_GOOD:
        v1 = -1;
        break;
    case GAME_DIALOG_REACTION_NEUTRAL:
        v1 = 0;
        break;
    case GAME_DIALOG_REACTION_BAD:
        v1 = 1;
        break;
    default:
        // See 0x446907 in ecx but this branch should be unreachable. Due to the
        // bug described above, this code is reachable.
        v1 = GAME_DIALOG_REACTION_NEUTRAL;
        debug_printf("\nError: dialog: Empathy Perk: invalid reaction!");
        break;
    }

    demo_copy_title(gReplyWin);
    demo_copy_options(gOptionWin);
    win_draw(gReplyWin);
    win_draw(gOptionWin);

    gdProcessHighlight(a1);
    talk_to_critter_reacts(v1);

    gdNumOptions = 0;

    if (gdReenterLevel < 2) {
        if (dialogOptionEntry->proc != 0) {
            executeProcedure(dialogBlock.program, dialogOptionEntry->proc);
        }
    }

    mouse_show();

    if (gdNumOptions == 0) {
        return -1;
    }

    gdProcessUpdate();

    return 0;
}

// 0x446A18
static void gdProcessHighlight(int index)
{
    int ui_scale = ui_get_scale();
    // FIXME: See explanation in `gdProcessChoice`.
    GameDialogOptionEntry dummy;
    memset(&dummy, 0, sizeof(dummy));

    GameDialogOptionEntry* dialogOptionEntry = index != -1 ? &(dialogBlock.options[index]) : &dummy;
    if (dialogOptionEntry->btn == 0) {
        return;
    }

    optionRect.ulx = 0;
    optionRect.uly = dialogOptionEntry->field_14;
    optionRect.lrx = 391 * ui_scale;
    optionRect.lry = dialogOptionEntry->field_39C;
    gDialogRefreshOptionsRect(gOptionWin, &optionRect);

    optionRect.ulx = 5 * ui_scale;
    optionRect.lrx = 388 * ui_scale;

    int color = colorTable[32747] | 0x2000000;
    if (perkHasRank(obj_dude, PERK_EMPATHY)) {
        color = colorTable[32747] | 0x2000000;
        switch (dialogOptionEntry->reaction) {
        case GAME_DIALOG_REACTION_GOOD:
            color = colorTable[31775] | 0x2000000;
            break;
        case GAME_DIALOG_REACTION_NEUTRAL:
            break;
        case GAME_DIALOG_REACTION_BAD:
            color = colorTable[32074] | 0x2000000;
            break;
        default:
            debug_printf("\nError: dialog: Empathy Perk: invalid reaction!");
            break;
        }
    }

    optionRect.ulx = 5 * ui_scale;
    optionRect.lrx = 388 * ui_scale;

    optionRect.lry = GAME_DIALOG_OPTIONS_WINDOW_HEIGHT;
    // NOTE: Uninline.
    text_to_rect_wrapped(win_get_buf(gOptionWin),
        &optionRect,
        dialogOptionEntry->text,
        NULL,
        text_height(),
        GAME_DIALOG_OPTIONS_WINDOW_WIDTH,
        color);

    optionRect.ulx = 0;
    optionRect.lrx = 391 * ui_scale;
    optionRect.uly = dialogOptionEntry->field_14;
    optionRect.lry = dialogOptionEntry->field_39C;
    win_draw_rect(gOptionWin, &optionRect);
}

// 0x446B5C
static void gdProcessUnHighlight(int index)
{
    int ui_scale = ui_get_scale();
    GameDialogOptionEntry* dialogOptionEntry = &(dialogBlock.options[index]);

    optionRect.ulx = 0;
    optionRect.uly = dialogOptionEntry->field_14;
    optionRect.lrx = 391 * ui_scale;
    optionRect.lry = dialogOptionEntry->field_39C;
    gDialogRefreshOptionsRect(gOptionWin, &optionRect);

    int color = colorTable[992] | 0x2000000;
    if (perk_level(obj_dude, PERK_EMPATHY) != 0) {
        color = colorTable[32747] | 0x2000000;
        switch (dialogOptionEntry->reaction) {
        case GAME_DIALOG_REACTION_GOOD:
            color = colorTable[31] | 0x2000000;
            break;
        case GAME_DIALOG_REACTION_NEUTRAL:
            color = colorTable[992] | 0x2000000;
            break;
        case GAME_DIALOG_REACTION_BAD:
            color = colorTable[31744] | 0x2000000;
            break;
        default:
            debug_printf("\nError: dialog: Empathy Perk: invalid reaction!");
            break;
        }
    }

    optionRect.ulx = 5 * ui_scale;
    optionRect.lrx = 388 * ui_scale;

    optionRect.lry = GAME_DIALOG_OPTIONS_WINDOW_HEIGHT;
    // NOTE: Uninline.
    text_to_rect_wrapped(win_get_buf(gOptionWin),
        &optionRect,
        dialogOptionEntry->text,
        NULL,
        text_height(),
        GAME_DIALOG_OPTIONS_WINDOW_WIDTH,
        color);

    optionRect.lrx = 391 * ui_scale;
    optionRect.uly = dialogOptionEntry->field_14;
    optionRect.ulx = 0;
    optionRect.lry = dialogOptionEntry->field_39C;
    win_draw_rect(gOptionWin, &optionRect);
}

// 0x446C94
static void gdProcessReply()
{
    int ui_scale = ui_get_scale();

    replyRect.ulx = 5 * ui_scale;
    replyRect.uly = 10 * ui_scale;
    replyRect.lrx = 374 * ui_scale;
    replyRect.lry = 58 * ui_scale;

    // NOTE: There is an unused if condition.
    perk_level(obj_dude, PERK_EMPATHY);

    demo_copy_title(gReplyWin);

    // NOTE: Uninline.
    text_to_rect_wrapped(win_get_buf(gReplyWin),
        &replyRect,
        dialogBlock.replyText,
        &(dialogBlock.offset),
        text_height(),
        GAME_DIALOG_REPLY_WINDOW_WIDTH,
        colorTable[992] | 0x2000000);
    win_draw(gReplyWin);
}

// 0x446D30
static void gdProcessUpdate()
{
    int ui_scale = ui_get_scale();

    replyRect.ulx = 5 * ui_scale;
    replyRect.uly = 10 * ui_scale;
    replyRect.lrx = 374 * ui_scale;
    replyRect.lry = 58 * ui_scale;

    optionRect.ulx = 5 * ui_scale;
    optionRect.uly = 5 * ui_scale;
    optionRect.lrx = 388 * ui_scale;
    optionRect.lry = 112 * ui_scale;

    demo_copy_title(gReplyWin);
    demo_copy_options(gOptionWin);

    if (dialogBlock.replyMessageListId > 0) {
        char* s = scr_get_msg_str_speech(dialogBlock.replyMessageListId, dialogBlock.replyMessageId, 1);
        if (s == NULL) {
            GNWSystemError("\n'GDialog::Error Grabbing text message!");
            exit(1);
        }

        strncpy(dialogBlock.replyText, s, sizeof(dialogBlock.replyText) - 1);
        *(dialogBlock.replyText + sizeof(dialogBlock.replyText) - 1) = '\0';
    }

    gdProcessReply();

    // Remember where script options end, so we can assign different keyCodes
    // to engine-injected options during button registration.
    int scriptOptionCount = gdNumOptions;

    // Inject engine-level options after script options.
    // [Barter] - if the NPC has the CRITTER_BARTER flag.
    if (PID_TYPE(dialog_target->pid) == OBJ_TYPE_CRITTER && gdNumOptions < DIALOG_OPTION_ENTRIES_CAPACITY) {
        Proto* proto;
        proto_ptr(dialog_target->pid, &proto);
        if (proto->critter.data.flags & CRITTER_BARTER) {
            GameDialogOptionEntry* barterOption = &(dialogBlock.options[gdNumOptions]);
            barterOption->messageListId = -4;
            barterOption->messageId = -4;
            barterOption->reaction = GAME_DIALOG_REACTION_NEUTRAL;
            barterOption->proc = 0;
            barterOption->btn = -1;
            sprintf(barterOption->text, "%c %s", '\x95', "[TRADE]");
            gdNumOptions++;
        }
    }

    // [Combat Control] - if the target is a party member.
    if (dialog_target_is_party && gdNumOptions < DIALOG_OPTION_ENTRIES_CAPACITY) {
        GameDialogOptionEntry* controlOption = &(dialogBlock.options[gdNumOptions]);
        controlOption->messageListId = -4;
        controlOption->messageId = -4;
        controlOption->reaction = GAME_DIALOG_REACTION_NEUTRAL;
        controlOption->proc = 0;
        controlOption->btn = -1;
        sprintf(controlOption->text, "%c %s", '\x95', "[COMBAT CONTROL]");
        gdNumOptions++;
    }

    int color = colorTable[992] | 0x2000000;

    bool hasEmpathy = perk_level(obj_dude, PERK_EMPATHY) != 0;

    int width = optionRect.lrx - optionRect.ulx - 4 * ui_scale;

    MessageListItem messageListItem;

    int v21 = 0;

    for (int index = 0; index < gdNumOptions; index++) {
        GameDialogOptionEntry* dialogOptionEntry = &(dialogBlock.options[index]);

        // Add visual separator before engine-injected options.
        if (index == scriptOptionCount) {
            optionRect.uly += text_height();
        }

        if (hasEmpathy) {
            switch (dialogOptionEntry->reaction) {
            case GAME_DIALOG_REACTION_GOOD:
                color = colorTable[31] | 0x2000000;
                break;
            case GAME_DIALOG_REACTION_NEUTRAL:
                color = colorTable[992] | 0x2000000;
                break;
            case GAME_DIALOG_REACTION_BAD:
                color = colorTable[31744] | 0x2000000;
                break;
            default:
                debug_printf("\nError: dialog: Empathy Perk: invalid reaction!");
                break;
            }
        }

        if (dialogOptionEntry->messageListId >= 0) {
            char* text = scr_get_msg_str_speech(dialogOptionEntry->messageListId, dialogOptionEntry->messageId, 0);
            if (text == NULL) {
                GNWSystemError("\nGDialog::Error Grabbing text message!");
                exit(1);
            }

            sprintf(dialogOptionEntry->text, "%c ", '\x95');
            strncat(dialogOptionEntry->text, text, 897);
        } else if (dialogOptionEntry->messageListId == -1) {
            if (index == 0) {
                // Go on
                messageListItem.num = 655;
                if (critterGetStat(obj_dude, STAT_INTELLIGENCE) < 4) {
                    if (message_search(&proto_main_msg_file, &messageListItem)) {
                        strcpy(dialogOptionEntry->text, messageListItem.text);
                    } else {
                        debug_printf("\nError...can't find message!");
                        return;
                    }
                }
            } else {
                // TODO: Why only space?
                strcpy(dialogOptionEntry->text, " ");
            }
        } else if (dialogOptionEntry->messageListId == -2) {
            // [Done]
            messageListItem.num = 650;
            if (message_search(&proto_main_msg_file, &messageListItem)) {
                sprintf(dialogOptionEntry->text, "%c %s", '\x95', messageListItem.text);
            } else {
                debug_printf("\nError...can't find message!");
                return;
            }
        }

        int v11 = text_num_lines(dialogOptionEntry->text, optionRect.lrx - optionRect.ulx) * text_height() + optionRect.uly + 2 * ui_scale;
        if (v11 < optionRect.lry) {
            int y = optionRect.uly;

            dialogOptionEntry->field_39C = v11;
            dialogOptionEntry->field_14 = y;

            if (index == 0) {
                y = 0;
            }

            // NOTE: Uninline.
            text_to_rect_wrapped(win_get_buf(gOptionWin),
                &optionRect,
                dialogOptionEntry->text,
                NULL,
                text_height(),
                GAME_DIALOG_OPTIONS_WINDOW_WIDTH,
                color);

            optionRect.uly += 2 * ui_scale;

            int btnKeyCode;
            if (index >= scriptOptionCount) {
                // Engine-injected options get unique keyCodes.
                btnKeyCode = KEYCODE_BARTER + (index - scriptOptionCount);
            } else {
                btnKeyCode = 49 + index;
            }
            dialogOptionEntry->btn = win_register_button(gOptionWin, 2 * ui_get_scale(), y, width, optionRect.uly - y - 4 * ui_get_scale(), 1200 + index, 1300 + index, -1, btnKeyCode, NULL, NULL, NULL, 0);
            if (dialogOptionEntry->btn != -1) {
                win_register_button_sound_func(dialogOptionEntry->btn, gsound_red_butt_press, gsound_red_butt_release);
            } else {
                debug_printf("\nError: Can't create button!");
            }
        } else {
            if (!v21) {
                v21 = 1;
            } else {
                debug_printf("Error: couldn't make button because it went below the window.\n");
            }
        }
    }

    win_draw(gReplyWin);
    win_draw(gOptionWin);
}

// 0x44715C
static int gdCreateHeadWindow()
{
    dialogue_state = 1;

    int windowWidth = GAME_DIALOG_WINDOW_WIDTH;
    int screenWidth = GAME_DIALOG_WINDOW_WIDTH;

    dialogueBackWindow = win_add(
        GAME_DIALOG_WINDOW_X,
        GAME_DIALOG_WINDOW_Y,
        GAME_DIALOG_WINDOW_WIDTH,
        GAME_DIALOG_WINDOW_HEIGHT,
        0,
        WINDOW_FLAG_0x02 | WINDOW_FLAG_0x20
    );

    // XA: This draws the top half of the conversation window, around the talking head / screenshot
    // talk_to_refresh_background_window();

    // The back window is now 640x480 centered; content starts at (0,0) in its buffer.
    unsigned char* buf = win_get_buf(dialogueBackWindow);
    unsigned char* contentBuf = buf;

    for (int index = 0; index < 8; index++) {
        soundContinueAll();

        Rect* rect = &(backgrndRects[index]);
        int width = rect->lrx - rect->ulx;
        int height = rect->lry - rect->uly;
        backgrndBufs[index] = (unsigned char*)mem_malloc(width * height * 4);
        if (backgrndBufs[index] == NULL) {
            return -1;
        }

        unsigned char* src = contentBuf;
        src += (screenWidth * rect->uly + rect->ulx) * 4;

        buf_to_buf(src, width, height, screenWidth, backgrndBufs[index], width);
    }

    gdialog_window_create();

    // Top-left of the talking-head area, scaled into the ui-scaled back window.
    int scale = ui_get_scale();
    headWindowBuffer = contentBuf + (GAME_DIALOG_WINDOW_WIDTH * (14 * scale) + 126 * scale) * 4;

    return 0;
}

// 0x447294
static void gdDestroyHeadWindow()
{
    if (dialogueWindow != -1) {
        headWindowBuffer = NULL;
    }

    if (dialogue_state == 1) {
        gdialog_window_destroy();
    }

    if (dialogueBackWindow != -1) {
        win_delete(dialogueBackWindow);
        dialogueBackWindow = -1;
    }

    for (int index = 0; index < 8; index++) {
        mem_free(backgrndBufs[index]);
    }
}

// 0x447300
static void gdSetupFidget(int headFrmId, int reaction)
{
    // 0x518900
    static int phone_anim = 0;

    fidgetFrameCounter = 0;

    if (headFrmId == -1) {
        fidgetFID = -1;
        fidgetFp = NULL;
        fidgetKey = INVALID_CACHE_ENTRY;
        fidgetAnim = -1;
        fidgetTocksPerFrame = 0;
        fidgetLastTime = 0;
        gdDisplayFrame(NULL, 0);
        lipsFID = 0;
        lipsKey = NULL;
        lipsFp = 0;
        return;
    }

    int anim = HEAD_ANIMATION_NEUTRAL_PHONEMES;
    switch (reaction) {
    case FIDGET_GOOD:
        anim = HEAD_ANIMATION_GOOD_PHONEMES;
        break;
    case FIDGET_BAD:
        anim = HEAD_ANIMATION_BAD_PHONEMES;
        break;
    }

    if (lipsFID != 0) {
        if (anim != phone_anim) {
            if (art_ptr_unlock(lipsKey) == -1) {
                debug_printf("failure unlocking lips frame!\n");
            }
            lipsKey = NULL;
            lipsFp = NULL;
            lipsFID = 0;
        }
    }

    if (lipsFID == 0) {
        phone_anim = anim;
        lipsFID = art_id(OBJ_TYPE_HEAD, headFrmId, anim, 0, 0);
        lipsFp = art_ptr_lock(lipsFID, &lipsKey);
        if (lipsFp == NULL) {
            debug_printf("failure!\n");

            char stats[200];
            cache_stats(&art_cache, stats);
            debug_printf("%s", stats);
        }
    }

    int fid = art_id(OBJ_TYPE_HEAD, headFrmId, reaction, 0, 0);
    int fidgetCount = art_head_fidgets(fid);
    if (fidgetCount == -1) {
        debug_printf("\tError - No available fidgets for given frame id\n");
        return;
    }

    int chance = roll_random(1, 100) + dialogue_seconds_since_last_input / 2;

    int fidget = fidgetCount;
    switch (fidgetCount) {
    case 1:
        fidget = 1;
        break;
    case 2:
        if (chance < 68) {
            fidget = 1;
        } else {
            fidget = 2;
        }
        break;
    case 3:
        dialogue_seconds_since_last_input = 0;
        if (chance < 52) {
            fidget = 1;
        } else if (chance < 77) {
            fidget = 2;
        } else {
            fidget = 3;
        }
        break;
    }

    debug_printf("Choosing fidget %d out of %d\n", fidget, fidgetCount);

    if (fidgetFp != NULL) {
        if (art_ptr_unlock(fidgetKey) == -1) {
            debug_printf("failure!\n");
        }
    }

    fidgetFID = art_id(OBJ_TYPE_HEAD, headFrmId, reaction, fidget, 0);
    fidgetFrameCounter = 0;
    fidgetFp = art_ptr_lock(fidgetFID, &fidgetKey);
    if (fidgetFp == NULL) {
        debug_printf("failure!\n");

        char stats[200];
        cache_stats(&art_cache, stats);
        debug_printf("%s", stats);
    }

    fidgetLastTime = 0;
    fidgetAnim = reaction;
    fidgetTocksPerFrame = 1000 / art_frame_fps(fidgetFp);
}

// 0x447598
static void gdWaitForFidget()
{
    if (fidgetFp == NULL) {
        return;
    }

    if (dialogueWindow == -1) {
        return;
    }

    debug_printf("Waiting for fidget to complete...\n");

    while (art_frame_max_frame(fidgetFp) > fidgetFrameCounter) {
        if (elapsed_time(fidgetLastTime) >= fidgetTocksPerFrame) {
            gdDisplayFrame(fidgetFp, fidgetFrameCounter);
            fidgetLastTime = get_time();
            fidgetFrameCounter++;
        }
    }

    fidgetFrameCounter = 0;
}

// 0x447614
static void gdPlayTransition(int anim)
{
    if (fidgetFp == NULL) {
        return;
    }

    if (dialogueWindow == -1) {
        return;
    }

    mouse_hide();

    debug_printf("Starting transition...\n");

    gdWaitForFidget();

    if (fidgetFp != NULL) {
        if (art_ptr_unlock(fidgetKey) == -1) {
            debug_printf("\tError unlocking fidget in transition func...");
        }
        fidgetFp = NULL;
    }

    CacheEntry* headFrmHandle;
    int headFid = art_id(OBJ_TYPE_HEAD, dialogue_head, anim, 0, 0);
    Art* headFrm = art_ptr_lock(headFid, &headFrmHandle);
    if (headFrm == NULL) {
        debug_printf("\tError locking transition...\n");
    }

    unsigned int delay = 1000 / art_frame_fps(headFrm);

    int frame = 0;
    unsigned int time = 0;
    while (frame < art_frame_max_frame(headFrm)) {
        if (elapsed_time(time) >= delay) {
            gdDisplayFrame(headFrm, frame);
            time = get_time();
            frame++;
        }
    }

    if (art_ptr_unlock(headFrmHandle) == -1) {
        debug_printf("\tError unlocking transition...\n");
    }

    debug_printf("Finished transition...\n");
    mouse_show();
}

// 0x447724
static void reply_arrow_up(int btn, int keyCode)
{
    if (gdReplyTooBig) {
        gmouse_set_cursor(MOUSE_CURSOR_SMALL_ARROW_UP);
    }
}

// 0x447738
static void reply_arrow_down(int btn, int keyCode)
{
    if (gdReplyTooBig) {
        gmouse_set_cursor(MOUSE_CURSOR_SMALL_ARROW_DOWN);
    }
}

// 0x44774C
static void reply_arrow_restore(int btn, int keyCode)
{
    gmouse_set_cursor(MOUSE_CURSOR_ARROW);
}

// demo_copy_title
// 0x447758
static void demo_copy_title(int win)
{
    gd_replyWin = win;

    if (win == -1) {
        debug_printf("\nError: demo_copy_title: win invalid!");
        return;
    }

    int width = win_width(win);
    if (width < 1) {
        debug_printf("\nError: demo_copy_title: width invalid!");
        return;
    }

    int height = win_height(win);
    if (height < 1) {
        debug_printf("\nError: demo_copy_title: length invalid!");
        return;
    }

    unsigned char* dest = win_get_buf(win);
    if (dest == NULL) {
        return;
    }

    // Clear transparent window buffer
    memset(dest, 0, width * height * 4);

    Rect screenRect;
    win_get_rect(win, &screenRect);
    win_refresh_all(&screenRect);

    
    
}

// demo_copy_options
// 0x447818
static void demo_copy_options(int win)
{
    gd_optionsWin = win;

    if (win == -1) {
        debug_printf("\nError: demo_copy_options: win invalid!");
        return;
    }

    int width = win_width(win);
    if (width < 1) {
        debug_printf("\nError: demo_copy_options: width invalid!");
        return;
    }

    int height = win_height(win);
    if (height < 1) {
        debug_printf("\nError: demo_copy_options: length invalid!");
        return;
    }

    unsigned char* dest = win_get_buf(win);
    if (dest == NULL) {
        return;
    }

    // Clear transparent window buffer
    memset(dest, 0, width * height * 4);

    Rect screenRect;
    win_get_rect(win, &screenRect);
    win_refresh_all(&screenRect);

    
    
}

// gDialogRefreshOptionsRect
// 0x447914
static void gDialogRefreshOptionsRect(int win, Rect* drawRect)
{
    if (drawRect == NULL) {
        debug_printf("\nError: gDialogRefreshOptionsRect: drawRect NULL!");
        return;
    }

    if (win == -1) {
        debug_printf("\nError: gDialogRefreshOptionsRect: win invalid!");
        return;
    }

    if (dialogueBackWindow == -1) {
        debug_printf("\nError: gDialogRefreshOptionsRect: dialogueBackWindow wasn't created!");
        return;
    }

    int destWidth = win_width(win);
    unsigned char* dest = win_get_buf(win);

    int rectWidth = drawRect->lrx - drawRect->ulx;
    int rectHeight = drawRect->lry - drawRect->uly;

    for (int y = 0; y < rectHeight; y++) {
        memset(dest + ((drawRect->uly + y) * destWidth + drawRect->ulx) * 4, 0, rectWidth * 4);
    }

    Rect screenRect;
    win_get_rect(win, &screenRect);
    screenRect.lrx = screenRect.ulx + drawRect->lrx;
    screenRect.lry = screenRect.uly + drawRect->lry;
    screenRect.ulx += drawRect->ulx;
    screenRect.uly += drawRect->uly;
    win_refresh_all(&screenRect);


    

    
}

// 0x447A58
static void gdialog_bk()
{
    // 0x518908
    static unsigned int tocksWaiting = 10000;

    if (fidgetFp == NULL) {
        return;
    }

    if (gdialog_speech_playing) {
        lips_bkg_proc();

        if (lips_draw_head) {
            gdDisplayFrame(lipsFp, head_phoneme_lookup[head_phoneme_current]);
            lips_draw_head = false;
        }

        if (!soundPlaying(lip_info.sound)) {
            gdialogFreeSpeech();
            gdDisplayFrame(lipsFp, 0);
            can_start_new_fidget = true;
            dialogue_seconds_since_last_input = 3;
            fidgetFrameCounter = 0;
        }
        return;
    }

    if (can_start_new_fidget) {
        if (elapsed_time(fidgetLastTime) >= tocksWaiting) {
            can_start_new_fidget = false;
            dialogue_seconds_since_last_input += tocksWaiting / 1000;
            tocksWaiting = 1000 * (roll_random(0, 3) + 4);
            gdSetupFidget(fidgetFID & 0xFFF, (fidgetFID & 0xFF0000) >> 16);
        }
        return;
    }

    if (elapsed_time(fidgetLastTime) >= fidgetTocksPerFrame) {
        if (art_frame_max_frame(fidgetFp) <= fidgetFrameCounter) {
            gdDisplayFrame(fidgetFp, 0);
            can_start_new_fidget = true;
        } else {
            gdDisplayFrame(fidgetFp, fidgetFrameCounter);
            fidgetLastTime = get_time();
            fidgetFrameCounter += 1;
        }
    }
}

// FIXME: Due to the bug in `gdProcessChoice` this function can receive invalid
// reaction value (50 instead of expected -1, 0, 1). It's handled gracefully by
// the game.
//
// 0x447CA0
void talk_to_critter_reacts(int a1)
{
    int v1 = a1 + 1;

    debug_printf("Dialogue Reaction: ");
    if (v1 < 3) {
        debug_printf("%s\n", react_strs[v1]);
    }

    int v3 = a1 + 50;
    dialogue_seconds_since_last_input = 0;

    switch (v3) {
    case GAME_DIALOG_REACTION_GOOD:
        switch (fidgetAnim) {
        case FIDGET_GOOD:
            gdPlayTransition(HEAD_ANIMATION_VERY_GOOD_REACTION);
            gdSetupFidget(dialogue_head, FIDGET_GOOD);
            break;
        case FIDGET_NEUTRAL:
            gdPlayTransition(HEAD_ANIMATION_NEUTRAL_TO_GOOD);
            gdSetupFidget(dialogue_head, FIDGET_GOOD);
            break;
        case FIDGET_BAD:
            gdPlayTransition(HEAD_ANIMATION_BAD_TO_NEUTRAL);
            gdSetupFidget(dialogue_head, FIDGET_NEUTRAL);
            break;
        }
        break;
    case GAME_DIALOG_REACTION_NEUTRAL:
        break;
    case GAME_DIALOG_REACTION_BAD:
        switch (fidgetAnim) {
        case FIDGET_GOOD:
            gdPlayTransition(HEAD_ANIMATION_GOOD_TO_NEUTRAL);
            gdSetupFidget(dialogue_head, FIDGET_NEUTRAL);
            break;
        case FIDGET_NEUTRAL:
            gdPlayTransition(HEAD_ANIMATION_NEUTRAL_TO_BAD);
            gdSetupFidget(dialogue_head, FIDGET_BAD);
            break;
        case FIDGET_BAD:
            gdPlayTransition(HEAD_ANIMATION_VERY_BAD_REACTION);
            gdSetupFidget(dialogue_head, FIDGET_BAD);
            break;
        }
        break;
    }
}

// 0x447D98
static void gdialog_scroll_subwin(int win, int a2, unsigned char* a3, unsigned char* a4, unsigned char* a5, int a6, int a7, int backPitch)
{
    int v7;
    unsigned char* v9;
    Rect rect;
    unsigned int tick;
    int step = 10 * ui_get_scale();

    v7 = a6;
    v9 = a4;

    if (a2 == 1) {
        rect.ulx = 0;
        rect.lrx = GAME_DIALOG_WINDOW_WIDTH - 1;
        rect.lry = a6 - 1;

        int v18 = a6 / step;
        if (a7 == -1) {
            rect.uly = step;
            v18 = 0;
        } else {
            rect.uly = v18 * step;
            v7 = a6 % step;
            v9 += (GAME_DIALOG_WINDOW_WIDTH) * rect.uly * 4;
        }

        for (; v18 >= 0; v18--) {
            soundContinueAll();
            rect.uly -= step;
            win_draw_rect(win, &rect);
            v7 += step;
            v9 -= step * (GAME_DIALOG_WINDOW_WIDTH) * 4;

            tick = get_time();
            while (elapsed_time(tick) < 33) {
            }

            os_window_present();
        }
    } else {
        rect.lrx = GAME_DIALOG_WINDOW_WIDTH - 1;
        rect.lry = a6 - 1;
        rect.ulx = 0;
        rect.uly = 0;

        for (int index = a6 / step; index > 0; index--) {
            soundContinueAll();

            v9 += step * (GAME_DIALOG_WINDOW_WIDTH) * 4;
            v7 -= step;
            a5 += step * backPitch * 4;

            win_draw_rect(win, &rect);

            rect.uly += step;

            tick = get_time();
            while (elapsed_time(tick) < 33) {
            }

            os_window_present();
        }
    }
}

// 0x447F64
static int text_num_lines(const char* a1, int a2)
{
    int width = text_width(a1);

    int v1 = 0;
    while (width > 0) {
        width -= a2;
        v1++;
    }

    return v1;
}

// NOTE: Inlined.
//
// 0x447F80
static int text_to_rect_wrapped(unsigned char* buffer, Rect* rect, char* string, int* a4, int height, int pitch, int color)
{
    return text_to_rect_func(buffer, rect, string, a4, height, pitch, color, 1);
}

// display_msg
// 0x447FA0
static int text_to_rect_func(unsigned char* buffer, Rect* rect, char* string, int* a4, int height, int pitch, int color, int a7)
{
    char* start;
    if (a4 != NULL) {
        start = string + *a4;
    } else {
        start = string;
    }

    int maxWidth = rect->lrx - rect->ulx;
    char* end = NULL;
    while (start != NULL && *start != '\0') {
        if (text_width(start) > maxWidth) {
            end = start + 1;
            while (*end != '\0' && *end != ' ') {
                end++;
            }

            if (*end != '\0') {
                char* lookahead = end + 1;
                while (lookahead != NULL) {
                    while (*lookahead != '\0' && *lookahead != ' ') {
                        lookahead++;
                    }

                    if (*lookahead == '\0') {
                        lookahead = NULL;
                    } else {
                        *lookahead = '\0';
                        if (text_width(start) >= maxWidth) {
                            *lookahead = ' ';
                            lookahead = NULL;
                        } else {
                            end = lookahead;
                            *lookahead = ' ';
                            lookahead++;
                        }
                    }
                }

                if (*end == ' ') {
                    *end = '\0';
                }
            } else {
                if (rect->lry - text_height() < rect->uly) {
                    return rect->uly;
                }

                if (a7 != 1 || start == string) {
                    text_to_buf(buffer + (pitch * rect->uly + 10) * 4, start, maxWidth, pitch, color);
                } else {
                    text_to_buf(buffer + (pitch * rect->uly) * 4, start, maxWidth, pitch, color);
                }

                if (a4 != NULL) {
                    *a4 += strlen(start) + 1;
                }

                rect->uly += height;
                return rect->uly;
            }
        }

        if (text_width(start) > maxWidth) {
            debug_printf("\nError: display_msg: word too long!");
            break;
        }

        if (a7 != 0) {
            if (rect->lry - text_height() < rect->uly) {
                if (end != NULL && *end == '\0') {
                    *end = ' ';
                }
                return rect->uly;
            }

            unsigned char* dest;
            if (a7 != 1 || start == string) {
                dest = buffer + 10 * 4;
            } else {
                dest = buffer;
            }
            text_to_buf(dest + (pitch * rect->uly) * 4, start, maxWidth, pitch, color);
        }

        if (a4 != NULL && end != NULL) {
            *a4 += strlen(start) + 1;
        }

        rect->uly += height;

        if (end != NULL) {
            start = end + 1;
            if (*end == '\0') {
                *end = ' ';
            }
            end = NULL;
        } else {
            start = NULL;
        }
    }

    if (a4 != NULL) {
        *a4 = 0;
    }

    return rect->uly;
}

// 0x448214
void gdialogSetBarterMod(int modifier)
{
    gdBarterMod = modifier;
}

// gdialog_barter
// 0x44821C
int gdActivateBarter(int modifier)
{
    if (!dialog_state_fix) {
        return -1;
    }

    gdBarterMod = modifier;
    gdialog_barter_pressed(-1, -1);
    barter_requested = true;

    return 0;
}

// 0x44A52C
static void gdialog_barter_pressed(int btn, int keyCode)
{
    if (PID_TYPE(dialog_target->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    Script* script;
    if (scr_ptr(dialog_target->sid, &script) == -1) {
        return;
    }

    Proto* proto;
    proto_ptr(dialog_target->pid, &proto);
    if (proto->critter.data.flags & CRITTER_BARTER) {
        if (gdialog_speech_playing) {
            if (soundPlaying(lip_info.sound)) {
                gdialogFreeSpeech();
            }
        }

        // NOTE: Uninline.
        gdHide();

        barter_requested = true;
    } else {
        MessageListItem messageListItem;
        // This person will not barter with you.
        messageListItem.num = 903;
        if (dialog_target_is_party) {
            // This critter can't carry anything.
            messageListItem.num = 913;
        }

        if (message_search(&proto_main_msg_file, &messageListItem)) {
            gdialogDisplayMsg(messageListItem.text);
        } else {
            debug_printf("\nError: gdialog: Can't find message!");
        }
    }
}

// XA: This draws the initial screen... also does the open/close animation
// XA: This is only the bottom part, not the talking head...
static int gdialog_window_create()
{
    const int screenWidth = GAME_DIALOG_WINDOW_WIDTH;

    if (gdialog_window_created) {
        return -1;
    }

    for (int index = 0; index < 9; index++) {
        gdialog_buttons[index] = -1;
    }

    CacheEntry* backgroundFrmHandle;
    // 389 - di_talkp.frm - dialog screen subwindow (party members)
    // 99 - di_talk.frm - dialog screen subwindow (NPC's)
    int backgroundFid = art_id(OBJ_TYPE_INTERFACE, dialog_target_is_party ? 389 : 99, 0, 0, 0);
    Art* backgroundFrm = art_ptr_lock(backgroundFid, &backgroundFrmHandle);
    if (backgroundFrm == NULL) {
        return -1;
    }

    unsigned char* backgroundFrmData = art_frame_data(backgroundFrm, 0, 0);
    if (backgroundFrmData != NULL) {
        dialogue_subwin_len = art_frame_length(backgroundFrm, 0, 0) * ui_get_scale();

        int dialogSubwindowX = GAME_DIALOG_WINDOW_X;
        int dialogSubwindowY = GAME_DIALOG_WINDOW_Y + GAME_DIALOG_WINDOW_HEIGHT - dialogue_subwin_len;
        dialogueWindow = win_add(dialogSubwindowX, dialogSubwindowY, screenWidth, dialogue_subwin_len, 256, WINDOW_FLAG_0x02 | WINDOW_FLAG_0x20);
        if (dialogueWindow != -1) {

            unsigned char* v10 = win_get_buf(dialogueWindow);
            unsigned char* v14 = win_get_buf(dialogueBackWindow);
            int backPitch = GAME_DIALOG_WINDOW_WIDTH;
            buf_to_buf(v14 + (backPitch * (GAME_DIALOG_WINDOW_HEIGHT - dialogue_subwin_len)) * 4, screenWidth, dialogue_subwin_len, backPitch, v10, screenWidth);
            ui_image_32(backgroundFrm, dialogueWindow, 0, 0, GAME_DIALOG_WINDOW_WIDTH, dialogue_subwin_len);

            if (dialogue_just_started) {
                win_draw(dialogueBackWindow);
                gdialog_scroll_subwin(dialogueWindow, 1, backgroundFrmData, v10, 0, dialogue_subwin_len, -1, 0);
                dialogue_just_started = 0;
            } else {
                gdialog_scroll_subwin(dialogueWindow, 1, backgroundFrmData, v10, 0, dialogue_subwin_len, 0, 0);
            }

            art_ptr_unlock(backgroundFrmHandle);

            gdialog_window_created = true;
            return 0;
        }
    }

    art_ptr_unlock(backgroundFrmHandle);

    return -1;
}

// 0x44A9D8
static void gdialog_window_destroy()
{
    if (dialogueWindow == -1) {
        return;
    }

    for (int index = 0; index < 9; index++) {
        win_delete_button(gdialog_buttons[index]);
        gdialog_buttons[index] = -1;
    }

    int offset = (GAME_DIALOG_WINDOW_WIDTH * (GAME_DIALOG_WINDOW_HEIGHT - dialogue_subwin_len)) * 4;
    unsigned char* backgroundWindowBuffer = win_get_buf(dialogueBackWindow) + offset;

    int frmId;
    if (dialog_target_is_party) {
        // di_talkp.frm - dialog screen subwindow (party members)
        frmId = 389;
    } else {
        // di_talk.frm - dialog screen subwindow (NPC's)
        frmId = 99;
    }

    CacheEntry* backgroundFrmHandle;
    int fid = art_id(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0);
    Art* art = art_ptr_lock(fid, &backgroundFrmHandle);
    if (art != NULL) {
        unsigned char* backgroundFrmData = art_frame_data(art, 0, 0);
        unsigned char* windowBuffer = win_get_buf(dialogueWindow);
        gdialog_scroll_subwin(dialogueWindow, 0, backgroundFrmData, windowBuffer, backgroundWindowBuffer, dialogue_subwin_len, 0, GAME_DIALOG_WINDOW_WIDTH);
        art_ptr_unlock(backgroundFrmHandle);
        win_delete(dialogueWindow);
        gdialog_window_created = 0;
        dialogueWindow = -1;
    }
}

// 0x44AB18
static int talk_to_refresh_background_window()
{
    CacheEntry* backgroundFrmHandle;
    // alltlk.frm - dialog screen background
    int fid = art_id(OBJ_TYPE_INTERFACE, 103, 0, 0, 0);
    Art* art = art_ptr_lock(fid, &backgroundFrmHandle);
    if (art == NULL) {
        return -1;
    }

    ui_image_32(art, dialogueBackWindow, 0, 0, GAME_DIALOG_WINDOW_WIDTH, GAME_DIALOG_WINDOW_HEIGHT);
    art_ptr_unlock(backgroundFrmHandle);

    if (!dialogue_just_started) {
        win_draw(dialogueBackWindow);
    }

    return 0;
}

// 0x44ABA8
static int talkToRefreshDialogWindowRect(Rect* rect)
{
    int frmId;
    if (dialog_target_is_party) {
        // di_talkp.frm - dialog screen subwindow (party members)
        frmId = 389;
    } else {
        // di_talk.frm - dialog screen subwindow (NPC's)
        frmId = 99;
    }

    CacheEntry* backgroundFrmHandle;
    int fid = art_id(OBJ_TYPE_INTERFACE, frmId, 0, 0, 0);
    Art* art = art_ptr_lock(fid, &backgroundFrmHandle);
    if (art == NULL) {
        return -1;
    }

    int ui_scale = ui_get_scale();
    ui_image_32(art, dialogueWindow, 0, 0, GAME_DIALOG_WINDOW_WIDTH, art_frame_length(art, 0, 0) * ui_scale);
    art_ptr_unlock(backgroundFrmHandle);

    win_draw_rect(dialogueWindow, rect);

    return 0;
}

// Reads the raw 8-bit pixel bytes of a single-frame FRM straight off disk,
// bypassing the art pipeline's RGBA conversion. Used for intensity masks whose
// pixel values are blend weights rather than colors. Mirrors the egg-mask load
// in obj_init(). Returns a newly allocated buffer of w*h bytes, or NULL.
static unsigned char* gdialog_load_raw_mask(int fid, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    char* path = art_get_name(fid);
    if (path == NULL) {
        return NULL;
    }

    File* file = db_fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    // Skip the Art header and the single ArtFrame header to reach pixel data.
    db_fseek(file, sizeof(Art) + sizeof(ArtFrame), SEEK_SET);

    int numPixels = width * height;
    unsigned char* mask = (unsigned char*)mem_malloc(numPixels);
    if (mask != NULL) {
        if (db_fread(mask, numPixels, 1, file) != 1) {
            mem_free(mask);
            mask = NULL;
        }
    }

    db_fclose(file);
    return mask;
}

// 32bpp port of the head-highlight (screen-glare) effect. The mask pixel is a
// blend weight; vanilla mapped it to a level and looked up a blend table built
// as result = (tint*L + pixel*(7-L)) / 7 (levels 1..7). We do the same per RGB
// channel directly, and scale the mask into the ui-scaled head area.
static void dialog_hilight_blend(unsigned char* mask, int maskWidth, int maskHeight,
    unsigned char* dest, int destPitch, int destX, int destY, int scale,
    int tintR, int tintG, int tintB)
{
    if (mask == NULL) {
        return;
    }

    int outWidth = maskWidth * scale;
    int outHeight = maskHeight * scale;

    for (int oy = 0; oy < outHeight; oy++) {
        unsigned char* maskRow = mask + (oy / scale) * maskWidth;
        uint32_t* destRow = (uint32_t*)(dest + ((size_t)(destY + oy) * destPitch + destX) * 4);

        for (int ox = 0; ox < outWidth; ox++) {
            unsigned char m = maskRow[ox / scale];
            if (m == 0) {
                continue;
            }

            int level = (256 - m) >> 4;
            if (level <= 0) {
                continue;
            }
            if (level > 7) {
                level = 7;
            }

            uint32_t px = destRow[ox];
            int dr = px & 0xFF;
            int dg = (px >> 8) & 0xFF;
            int db = (px >> 16) & 0xFF;

            int inv = 7 - level;
            int r = (tintR * level + dr * inv) / 7;
            int g = (tintG * level + dg * inv) / 7;
            int b = (tintB * level + db * inv) / 7;

            destRow[ox] = (0xFFu << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}

// 0x44ACFC
static void gdDisplayFrame(Art* headFrm, int frame)
{
    // 0x518BF4
    static int totalHotx = 0;

    bool did_center = false;

    if (dialogueWindow == -1) {
        return;
    }

    if (headFrm != NULL) {
        if (frame == 0) {
            totalHotx = 0;
        }

        int backgroundFid = art_id(OBJ_TYPE_BACKGROUND, backgroundIndex, 0, 0, 0);

        CacheEntry* backgroundHandle;
        Art* backgroundFrm = art_ptr_lock(backgroundFid, &backgroundHandle);
        if (backgroundFrm == NULL) {
            debug_printf("\tError locking background in display...\n");
        }

        unsigned char* backgroundFrmData = art_frame_data(backgroundFrm, 0, 0);
        if (backgroundFrmData != NULL) {
            int scale = ui_get_scale();
            cscale(backgroundFrmData, 388, 200, 388, headWindowBuffer, 388 * scale, 200 * scale, GAME_DIALOG_WINDOW_WIDTH);
        } else {
            debug_printf("\tError getting background data in display...\n");
        }

        art_ptr_unlock(backgroundHandle);

        int width = art_frame_width(headFrm, frame, 0);
        int height = art_frame_length(headFrm, frame, 0);
        unsigned char* data = art_frame_data(headFrm, frame, 0);

        int a3;
        int v8;
        art_frame_offset(headFrm, 0, &a3, &v8);

        int a4;
        int a5;
        art_frame_hot(headFrm, frame, 0, &a4, &a5);

        totalHotx += a4;
        a3 += totalHotx;

        if (data != NULL) {
            // Position the head art in native 640-wide head space (vanilla math),
            // then scale that position and the art into the ui-scaled head area.
            int destOffset = GAME_DIALOG_NATIVE_WIDTH * (200 - height) + a3 + (388 - width) / 2;
            if (destOffset + width * v8 > 0) {
                destOffset += width * v8;
            }

            int scale = ui_get_scale();
            int col = destOffset % GAME_DIALOG_NATIVE_WIDTH;
            int row = destOffset / GAME_DIALOG_NATIVE_WIDTH;

            trans_cscale(
                data,
                width,
                height,
                width,
                headWindowBuffer + (GAME_DIALOG_WINDOW_WIDTH * (row * scale) + col * scale) * 4,
                width * scale,
                height * scale,
                GAME_DIALOG_WINDOW_WIDTH);
        } else {
            debug_printf("\tError getting head data in display...\n");
        }
    } else {
        if (talk_need_to_center == 1) {
            talk_need_to_center = 0;
            tile_refresh_display();
            // tile_refresh_display() blits the whole map straight to the screen,
            // clobbering the dialog windows stacked on top of it. The original
            // dialog filled the entire screen so this was always painted over;
            // now that it's a centered subset we must recomposite it. Only the
            // head rect is redrawn below, so the (opaque) subwindow panel would
            // otherwise be left showing the bare map until the next full refresh.
            did_center = true;
        }

        // The "head" here is a live snapshot of the map (display_win), which is
        // already rendered at ui scale. Grab a scale-sized slice and blit it 1:1
        // into the (also scaled) head area - no rescaling needed.
        int scale = ui_get_scale();
        unsigned char* src = win_get_buf(display_win);
        int screenWidth = scr_size.lrx - scr_size.ulx + 1;
        int screenHeight = scr_size.lry - scr_size.uly + 1;
        int capW = 388 * scale;
        int capH = 200 * scale;
        int capOffset = ((screenHeight - 332 * scale) / 2) * screenWidth + (screenWidth - capW) / 2;
        buf_to_buf(
            src + capOffset * 4,
            capW,
            capH,
            screenWidth,
            headWindowBuffer,
            GAME_DIALOG_WINDOW_WIDTH);
    }

    // Head highlights (the monitor-glare sheen): light tint up top, dark falloff
    // toward the bottom, blended via the raw intensity masks and scaled into the
    // ui-scaled head area.
    int headScale = ui_get_scale();
    unsigned char* dest = win_get_buf(dialogueBackWindow);
    int backPitch = GAME_DIALOG_WINDOW_WIDTH;

    dialog_hilight_blend(upper_hi_mask, upper_hi_wid, upper_hi_len,
        dest, backPitch, 426 * headScale, 15 * headScale, headScale,
        light_tint_r, light_tint_g, light_tint_b);

    dialog_hilight_blend(lower_hi_mask, lower_hi_wid, lower_hi_len,
        dest, backPitch, 129 * headScale, (214 - lower_hi_len - 2) * headScale, headScale,
        dark_tint_r, dark_tint_g, dark_tint_b);

    // NOTE: the frame-border restore below is vestigial - the upper conversation
    // frame art (talk_to_refresh_background_window) isn't drawn, so backgrndBufs
    // hold empty pixels. Left as-is; harmless.
    for (int index = 0; index < 8; ++index) {
        Rect* rect = &(backgrndRects[index]);
        int width = rect->lrx - rect->ulx;

        trans_buf_to_buf(backgrndBufs[index],
            width,
            rect->lry - rect->uly,
            width,
            dest + (backPitch * rect->uly + rect->ulx) * 4,
            backPitch);
    }

    Rect v27;
    v27.ulx = 126 * headScale;
    v27.uly = 14 * headScale;
    v27.lrx = 514 * headScale;
    v27.lry = 214 * headScale;

    win_draw_rect(dialogueBackWindow, &v27);

    if (did_center) {
        // tile_refresh_display() above repainted the whole screen, clobbering the
        // dialog windows stacked on top of it. Drawing only dialogueBackWindow
        // here would leave the subwindow panel (and reply/option windows) showing
        // the bare map until the next full refresh - which is why the bottom
        // section stayed blank until the mouse hovered over it. Recomposite the
        // entire dialog stack over that area, bottom-to-top, just like the mouse
        // refresh path does.
        Rect dialogRect;
        win_get_rect(dialogueBackWindow, &dialogRect);
        win_refresh_all(&dialogRect);
    }
}

// 0x44B080
static void gdBlendTableInit()
{
    // Tint colors for the head-highlight blend, as 8-bit RGB (the source
    // colorTable entries are 5-bit-per-channel, expanded here via << 3).
    int lightRGB = Color2RGB(colorTable[17969]);
    light_tint_r = ((lightRGB >> 10) & 0x1F) << 3;
    light_tint_g = ((lightRGB >> 5) & 0x1F) << 3;
    light_tint_b = (lightRGB & 0x1F) << 3;

    int darkRGB = Color2RGB(colorTable[22187]);
    dark_tint_r = ((darkRGB >> 10) & 0x1F) << 3;
    dark_tint_g = ((darkRGB >> 5) & 0x1F) << 3;
    dark_tint_b = (darkRGB & 0x1F) << 3;

    // hilight1.frm - dialogue upper hilight
    int upperHighlightFid = art_id(OBJ_TYPE_INTERFACE, 115, 0, 0, 0);
    upper_hi_fp = art_ptr_lock(upperHighlightFid, &upper_hi_key);
    upper_hi_wid = art_frame_width(upper_hi_fp, 0, 0);
    upper_hi_len = art_frame_length(upper_hi_fp, 0, 0);
    upper_hi_mask = gdialog_load_raw_mask(upperHighlightFid, upper_hi_wid, upper_hi_len);

    // hilight2.frm - dialogue lower hilight
    int lowerHighlightFid = art_id(OBJ_TYPE_INTERFACE, 116, 0, 0, 0);
    lower_hi_fp = art_ptr_lock(lowerHighlightFid, &lower_hi_key);
    lower_hi_wid = art_frame_width(lower_hi_fp, 0, 0);
    lower_hi_len = art_frame_length(lower_hi_fp, 0, 0);
    lower_hi_mask = gdialog_load_raw_mask(lowerHighlightFid, lower_hi_wid, lower_hi_len);
}

// NOTE: Inlined.
//
// 0x44B1D4
static void gdBlendTableExit()
{
    if (upper_hi_mask != NULL) {
        mem_free(upper_hi_mask);
        upper_hi_mask = NULL;
    }
    if (lower_hi_mask != NULL) {
        mem_free(lower_hi_mask);
        lower_hi_mask = NULL;
    }

    art_ptr_unlock(upper_hi_key);
    art_ptr_unlock(lower_hi_key);
}

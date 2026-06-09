#ifndef OS_INPUT_H
#define OS_INPUT_H

#include <stdbool.h>

typedef struct os_input_mouse_state {
    int delta_x;
    int delta_y;
    int left_button;
    int right_button;
} os_input_mouse_state;

bool os_input_init(void);
void os_input_exit(void);

bool os_input_acquire_mouse(void);
void os_input_unacquire_mouse(void);

bool os_input_get_mouse_state(os_input_mouse_state* state);

enum {
    DIK_ESCAPE = 1, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9, DIK_0,
    DIK_MINUS, DIK_SLASH, DIK_EQUALS, DIK_BACK, DIK_TAB, DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T,
    DIK_Y, DIK_U, DIK_I, DIK_O, DIK_P, DIK_LBRACKET, DIK_RBRACKET, DIK_RETURN, DIK_LCONTROL,
    DIK_A, DIK_S, DIK_D, DIK_F, DIK_G, DIK_H, DIK_J, DIK_K, DIK_L, DIK_SEMICOLON, DIK_APOSTROPHE,
    DIK_GRAVE, DIK_LSHIFT, DIK_BACKSLASH, DIK_Z, DIK_X, DIK_C, DIK_V, DIK_B, DIK_N, DIK_M,
    DIK_COMMA, DIK_PERIOD, DIK_RSHIFT, DIK_MULTIPLY, DIK_LMENU, DIK_SPACE, DIK_CAPITAL,
    DIK_F1, DIK_F2, DIK_F3, DIK_F4, DIK_F5, DIK_F6, DIK_F7, DIK_F8, DIK_F9, DIK_F10,
    DIK_NUMLOCK, DIK_SCROLL, DIK_NUMPAD7, DIK_NUMPAD8, DIK_NUMPAD9, DIK_SUBTRACT, DIK_NUMPAD4,
    DIK_NUMPAD5, DIK_NUMPAD6, DIK_ADD, DIK_NUMPAD1, DIK_NUMPAD2, DIK_NUMPAD3, DIK_NUMPAD0,
    DIK_DECIMAL, DIK_F11, DIK_F12, DIK_F13, DIK_F14, DIK_F15, DIK_KANA, DIK_CONVERT, DIK_NOCONVERT,
    DIK_YEN, DIK_NUMPADEQUALS, DIK_PREVTRACK, DIK_AT, DIK_COLON, DIK_UNDERLINE, DIK_KANJI,
    DIK_STOP, DIK_AX, DIK_UNLABELED, DIK_NUMPADCOMMA, DIK_NUMPADENTER, DIK_RCONTROL,
    DIK_DIVIDE, DIK_SYSRQ, DIK_RMENU, DIK_PAUSE, DIK_HOME, DIK_UP, DIK_PRIOR, DIK_LEFT,
    DIK_RIGHT, DIK_END, DIK_DOWN, DIK_NEXT, DIK_INSERT, DIK_DELETE, DIK_LWIN, DIK_RWIN,
    DIK_APPS, DIK_LALT, DIK_RALT, DIK_OEM_102
};

bool os_input_is_key_toggled(int dik_key);

void os_input_cursor_show(bool show);

#endif

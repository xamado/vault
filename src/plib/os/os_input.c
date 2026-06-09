#include "plib/os/os_input.h"
#include <SDL2/SDL.h>

bool os_input_init(void) {
    return SDL_InitSubSystem(SDL_INIT_EVENTS) == 0;
}

void os_input_exit(void) {
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

bool os_input_acquire_mouse(void) {
    // SDL_SetRelativeMouseMode hides the OS cursor and restricts mouse to the window,
    // which perfectly mimics DirectX exclusive mouse acquisition.
    return SDL_SetRelativeMouseMode(SDL_TRUE) == 0;
}

void os_input_unacquire_mouse(void) {
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

bool os_input_get_mouse_state(os_input_mouse_state* state) {
    if (!state) return false;

    int dx, dy;
    Uint32 buttons = SDL_GetRelativeMouseState(&dx, &dy);

    state->delta_x = dx;
    state->delta_y = dy;
    state->left_button = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
    state->right_button = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) ? 1 : 0;

    return true;
}

void os_input_cursor_show(bool show)
{
    SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
}

bool os_input_is_key_toggled(int dik_key) {
    SDL_Keymod mod = SDL_GetModState();

    switch (dik_key) {
        case DIK_CAPITAL: return (mod & KMOD_CAPS) != 0;
        case DIK_NUMLOCK: return (mod & KMOD_NUM) != 0;
        case DIK_SCROLL: return (mod & KMOD_SCROLL) != 0;
        default: return false;
    }
}


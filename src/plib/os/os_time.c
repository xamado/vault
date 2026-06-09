#include "plib/os/os_time.h"
#include <SDL2/SDL.h>

unsigned int os_get_ticks(void)
{
    return SDL_GetTicks();
}

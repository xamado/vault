#include "plib/gnw/winmain.h"

#include <signal.h>
#include <SDL2/SDL.h>

#include "game/main.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"

// ASan calls this during its own init, before main().
// Without log_path, output goes to stderr (console).
const char *__asan_default_options() {
    return "abort_on_error=1:detect_leaks=0";
}

bool GNW95_isActive = false;

char GNW95_title[256];

void SignalHandler(int signalID)
{
    win_exit();
}

// ASan API to set report output fd
void __sanitizer_set_report_fd(int fd);

int main(int argc, char** argv)
{
    __sanitizer_set_report_fd(1); // 1 = stdout

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    signal(1, SignalHandler);
    signal(3, SignalHandler);
    signal(5, SignalHandler);

    GNW95_isActive = true;
    int ret = RealMain(argc, argv);

    SDL_Quit();
    return ret;
}

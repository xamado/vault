#include "plib/os/os_window.h"

#include <SDL2/SDL.h>

static SDL_Window* GNW95_window = NULL;
static SDL_Renderer* GNW95_renderer = NULL;
static SDL_Texture* GNW95_texture = NULL;
static SDL_Surface* GNW95_surface = NULL;

void os_window_messagebox(const char* title, const char* message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, GNW95_window);
}

void os_window_set_title(const char* title)
{
    if (GNW95_window) {
        SDL_SetWindowTitle(GNW95_window, title);
    }
}

bool os_window_create(const char* title, int width, int height)
{
    if (!GNW95_window) {
        GNW95_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!GNW95_window) { fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return false; }

        GNW95_renderer = SDL_CreateRenderer(GNW95_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!GNW95_renderer) { fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError()); return false; }

        SDL_RenderSetLogicalSize(GNW95_renderer, width, height);

        GNW95_texture = SDL_CreateTexture(GNW95_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!GNW95_texture) { fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError()); return false; }

        GNW95_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 8, SDL_PIXELFORMAT_INDEX8);
        if (!GNW95_surface) { fprintf(stderr, "SDL_CreateRGBSurfaceWithFormat failed: %s\n", SDL_GetError()); return false; }

        SDL_Color current_palette[256];
        for (int i = 0; i < 256; i++) {
            current_palette[i].r = i;
            current_palette[i].g = i;
            current_palette[i].b = i;
            current_palette[i].a = 255;
        }
        SDL_SetPaletteColors(GNW95_surface->format->palette, current_palette, 0, 256);
    }
    return true;
}

void os_window_destroy(void)
{
    if (GNW95_surface) { SDL_FreeSurface(GNW95_surface); GNW95_surface = NULL; }
    if (GNW95_texture) { SDL_DestroyTexture(GNW95_texture); GNW95_texture = NULL; }
    if (GNW95_renderer) { SDL_DestroyRenderer(GNW95_renderer); GNW95_renderer = NULL; }
    if (GNW95_window) { SDL_DestroyWindow(GNW95_window); GNW95_window = NULL; }
}

void os_window_set_palette(int start, int count, const unsigned char* palette)
{
    if (GNW95_surface) {
        SDL_Color colors[256];
        for (int i = 0; i < count; i++) {
            colors[i].r = palette[i * 3] << 2;
            colors[i].g = palette[i * 3 + 1] << 2;
            colors[i].b = palette[i * 3 + 2] << 2;
            colors[i].a = 255;
        }
        SDL_SetPaletteColors(GNW95_surface->format->palette, colors, start, count);
    }
}

void os_window_lock(void** pixels, int* pitch)
{
    if (GNW95_surface) {
        SDL_LockSurface(GNW95_surface);
        *pixels = GNW95_surface->pixels;
        *pitch = GNW95_surface->pitch;
    } else {
        *pixels = NULL;
        *pitch = 0;
    }
}

void os_window_unlock(void)
{
    if (GNW95_surface) {
        SDL_UnlockSurface(GNW95_surface);
    }
}

void os_window_present(void)
{
    if (!GNW95_surface || !GNW95_texture || !GNW95_renderer) return;

    SDL_Surface* rgbSurface = SDL_ConvertSurfaceFormat(GNW95_surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (rgbSurface) {
        SDL_UpdateTexture(GNW95_texture, NULL, rgbSurface->pixels, rgbSurface->pitch);
        SDL_FreeSurface(rgbSurface);
    }

    SDL_RenderClear(GNW95_renderer);
    SDL_RenderCopy(GNW95_renderer, GNW95_texture, NULL, NULL);
    SDL_RenderPresent(GNW95_renderer);
}

#include "plib/gnw/svga.h"

#include <string.h>
#include <stdint.h>

#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/mouse.h"
#include "plib/gnw/winmain.h"
#include "plib/os/os_window.h"

// 0x51E2B0
// 0x51E2B4
// 0x51E2B8
// 0x51E2BC
// (DirectDraw globals GNW95_DDObject / GNW95_DDPrimarySurface /
// GNW95_DDRestoreSurface / GNW95_DDPrimaryPalette removed under SDL.)

// 0x51E2C4
UpdatePaletteFunc* update_palette_func = NULL;

// 0x6AC7F0
// (GNW95_Pal16 removed — 16bpp path no longer needed under SDL.)

// screen rect
Rect scr_size;

// 0x6AC9F8 (former w95 RGB-mask globals removed; SDL handles pixel format)


// 0x6ACA1C
ZeroMemFunc* zero_mem = NULL;

// Backing store for the runtime palette (256 entries * 3 bytes).
static unsigned char current_palette[256 * 3];

// 0x4CADFC
void zero_vid_mem()
{
    if (zero_mem) {
        zero_mem();
    }
}

// 0x4CAE1C
//
// NOTE: Combined with the former GNW95_init_mode_ex. Under SDL there is no
// "ex" variant — the underlying window/surface always lives in INDEX8.
int GNW95_init_mode(int width, int height, int bpp)
{
    if (GNW95_init_window(width, height) == -1) {
        return -1;
    }

    scr_size.ulx = 0;
    scr_size.uly = 0;
    scr_size.lrx = width - 1;
    scr_size.lry = height - 1;

    mouse_blit_trans = NULL;
    zero_mem = GNW95_zero_vid_mem;
    mouse_blit = GNW95_ShowRect;

    return 0;
}

// 0x4CAEDC
int GNW95_init_window(int width, int height)
{
    if (!os_window_create(GNW95_title, width, height)) {
        return -1;
    }

    // initialize dummy grayscale palette so something is visible until the
    // game pushes the real palette in
    unsigned char dummy[256 * 3];
    for (int i = 0; i < 256; i++) {
        dummy[i * 3] = i >> 2;
        dummy[i * 3 + 1] = i >> 2;
        dummy[i * 3 + 2] = i >> 2;
    }
    GNW95_SetPalette(dummy);

    return 0;
}

// 0x4CAF50 (former ffs helper removed — only used by 16bpp init.)

// 0x4CAF9C
//
// NOTE: DirectDraw init collapsed under SDL. Kept the symbol because
// downstream tools may have it indexed against the F2 binary.
int GNW95_init_DirectDraw(int width, int height, int bpp)
{
    return 0;
}

// 0x4CB1B0
void GNW95_reset_mode()
{
    os_window_destroy();
}

// 0x4CB218
void GNW95_SetPaletteEntry(int entry, unsigned char r, unsigned char g, unsigned char b)
{
    current_palette[entry * 3] = r;
    current_palette[entry * 3 + 1] = g;
    current_palette[entry * 3 + 2] = b;

    os_window_set_palette(entry, 1, &current_palette[entry * 3]);

    if (update_palette_func != NULL) {
        update_palette_func();
    }
}

// 0x4CB310
void GNW95_SetPaletteEntries(unsigned char* palette, int start, int count)
{
    for (int i = 0; i < count; i++) {
        current_palette[(start + i) * 3] = palette[i * 3];
        current_palette[(start + i) * 3 + 1] = palette[i * 3 + 1];
        current_palette[(start + i) * 3 + 2] = palette[i * 3 + 2];
    }

    os_window_set_palette(start, count, palette);

    if (update_palette_func != NULL) {
        update_palette_func();
    }
}

// 0x4CB568
void GNW95_SetPalette(unsigned char* palette)
{
    memcpy(current_palette, palette, 256 * 3);

    os_window_set_palette(0, 256, palette);

    if (update_palette_func != NULL) {
        update_palette_func();
    }
}

// 0x4CB68C
unsigned char* GNW95_GetPalette()
{
    return current_palette;
}

// 0x4CB850
void GNW95_ShowRect(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    if (!GNW95_isActive) {
        return;
    }

    void* pixels;
    int pitch;
    os_window_lock(&pixels, &pitch);

    if (pixels) {
        unsigned char* dest = (unsigned char*)pixels;
        for (int y = 0; y < srcHeight; y++) {
            uint32_t* destRow = (uint32_t*)(dest + (destY + y) * pitch + (destX * 4));
            uint32_t* srcRow = (uint32_t*)(src + (srcY + y) * srcPitch * 4 + srcX * 4);
            memcpy(destRow, srcRow, srcWidth * 4);
        }
        os_window_unlock();
    }
}

// 0x4CBAB0
void GNW95_MouseShowTransRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY, unsigned char keyColor)
{
    // Same logic as GNW95_ShowRect but skip key-color pixels.
    if (!GNW95_isActive) {
        return;
    }

    void* pixels;
    int pitch;
    os_window_lock(&pixels, &pitch);

    if (pixels) {
        unsigned char* dest = (unsigned char*)pixels;
        unsigned char* pal = GNW95_GetPalette();
        for (int y = 0; y < srcHeight; y++) {
            unsigned char* destRow = dest + (destY + y) * pitch + (destX * 4);
            unsigned char* srcRow = src + (srcY + y) * srcPitch + srcX;
            for (int x = 0; x < srcWidth; x++) {
                unsigned char c = srcRow[x];
                if (c != keyColor) {
                    unsigned char r = pal[c * 3] << 2;
                    unsigned char g = pal[c * 3 + 1] << 2;
                    unsigned char b = pal[c * 3 + 2] << 2;
                    
                    uint32_t* outPixel = (uint32_t*)(destRow + x * 4);
                    *outPixel = (0xFF << 24) | (b << 16) | (g << 8) | r;
                }
            }
        }
        os_window_unlock();
    }
}

// Used by gmovie to scale up MVE frames to the window size. Nearest-neighbor
// 8bpp scaling matching the pre-WIP implementation used during the Linux port.
void GNW95_ShowMovieRect(unsigned char* src, int srcPitch, int srcX, int srcY, int srcWidth, int srcHeight)
{
    if (!GNW95_isActive) {
        return;
    }

    void* pixels;
    int destPitch;
    os_window_lock(&pixels, &destPitch);

    if (pixels) {
        int destWidth = scr_size.lrx + 1;
        int destHeight = scr_size.lry + 1;

        unsigned char* srcStart = src + (srcY * srcPitch) + srcX;

        cscale_8_to_32(
            srcStart, srcWidth, srcHeight, srcPitch,
            (unsigned char*)pixels, destWidth, destHeight, destPitch / 4, GNW95_GetPalette()
        );

        os_window_unlock();
    }
}

// Clears drawing surface.
//
// 0x4CBBC8
void GNW95_zero_vid_mem()
{
    if (!GNW95_isActive) {
        return;
    }

    void* pixels;
    int pitch;
    os_window_lock(&pixels, &pitch);

    if (pixels) {
        int height = scr_size.lry - scr_size.uly + 1;
        memset(pixels, 0, pitch * height);
        os_window_unlock();
    }
}

Size screen_get_size(void)
{
    Size size;
    size.width = scr_size.lrx - scr_size.ulx + 1;
    size.height = scr_size.lry - scr_size.uly + 1;
    return size;
}

#include "plib/gnw/svga.h"

#include <string.h>

#include "mmx.h"
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

// 0x51E2C8
bool mmxEnabled = true;

// 0x6AC7F0
// (GNW95_Pal16 removed — 16bpp path no longer needed under SDL.)

// screen rect
Rect scr_size;

// 0x6AC9F8 (former w95 RGB-mask globals removed; SDL handles pixel format)

// 0x6ACA18
ScreenBlitFunc* scr_blit = GNW95_ShowRect;

// 0x6ACA1C
ZeroMemFunc* zero_mem = NULL;

// Backing store for the runtime palette (256 entries * 3 bytes).
static unsigned char current_palette[256 * 3];

// 0x4CACD0
void mmxEnable(bool enable)
{
    // 0x51E2CC
    static bool inited = false;

    // 0x6ACA20
    static bool mmx;

    if (!inited) {
        mmx = mmxIsSupported();
        inited = true;
    }

    if (mmx) {
        mmxEnabled = enable;
    }
}

// 0x4CAD08
int init_mode_320_200()
{
    return GNW95_init_mode(320, 200, 8);
}

// 0x4CAD40
int init_mode_320_400()
{
    return GNW95_init_mode(320, 400, 8);
}

// 0x4CAD5C
int init_mode_640_480_16()
{
    return -1;
}

// 0x4CAD64
int init_mode_640_480()
{
    return GNW95_init_mode(640, 480, 8);
}

// 0x4CAD94
int init_mode_640_400()
{
    return GNW95_init_mode(640, 400, 8);
}

// 0x4CADA8
int init_mode_800_600()
{
    return GNW95_init_mode(800, 600, 8);
}

// 0x4CADBC
int init_mode_1024_768()
{
    return GNW95_init_mode(1024, 768, 8);
}

// 0x4CADD0
int init_mode_1280_1024()
{
    return GNW95_init_mode(1280, 1024, 8);
}

// 0x4CADE4
int init_vesa_mode(int mode, int width, int height, int half)
{
    if (half != 0) {
        return -1;
    }

    return GNW95_init_mode(width, height, 8);
}

// 0x4CADF3
int get_start_mode()
{
    return -1;
}

// 0x4CADF8
void reset_mode()
{
}

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

    mmxEnable(true);

    mouse_blit_trans = NULL;
    scr_blit = GNW95_ShowRect;
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
    os_window_present();

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
    os_window_present();

    if (update_palette_func != NULL) {
        update_palette_func();
    }
}

// 0x4CB568
void GNW95_SetPalette(unsigned char* palette)
{
    memcpy(current_palette, palette, 256 * 3);

    os_window_set_palette(0, 256, palette);
    os_window_present();

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
            memcpy(dest + (destY + y) * pitch + destX,
                   src + (srcY + y) * srcPitch + srcX,
                   srcWidth);
        }
        os_window_unlock();
        os_window_present();
    }
}

// 0x4CB93C
//
// NOTE: 16bpp paths collapsed — F2 always runs at 8bpp under SDL.
void GNW95_MouseShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    GNW95_ShowRect(src, srcPitch, a3, srcX, srcY, srcWidth, srcHeight, destX, destY);
}

// 0x4CBA44
void GNW95_ShowRect16(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY)
{
    GNW95_ShowRect(src, srcPitch, a3, srcX, srcY, srcWidth, srcHeight, destX, destY);
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
        for (int y = 0; y < srcHeight; y++) {
            unsigned char* destRow = dest + (destY + y) * pitch + destX;
            unsigned char* srcRow = src + (srcY + y) * srcPitch + srcX;
            for (int x = 0; x < srcWidth; x++) {
                if (srcRow[x] != keyColor) {
                    destRow[x] = srcRow[x];
                }
            }
        }
        os_window_unlock();
        os_window_present();
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
        unsigned char* dest = (unsigned char*)pixels;
        int destWidth = scr_size.lrx + 1;
        int destHeight = scr_size.lry + 1;

        for (int y = 0; y < destHeight; y++) {
            int sy = (y * srcHeight) / destHeight;
            unsigned char* srcRow = src + (srcY + sy) * srcPitch + srcX;
            unsigned char* destRow = dest + y * destPitch;
            for (int x = 0; x < destWidth; x++) {
                int sx = (x * srcWidth) / destWidth;
                destRow[x] = srcRow[sx];
            }
        }

        os_window_unlock();
        os_window_present();
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
        os_window_present();
    }
}

#include "game/ui.h"

#include <stddef.h>

#include "game/art.h"
#include "plib/color/color.h"
#include "plib/gnw/button.h"
#include "plib/gnw/gnw.h"
#include "plib/gnw/grbuf.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/svga.h"
#include "plib/gnw/text.h"
#include "game/fontmgr.h"

#define MAX_UI_BUTTONS 128

typedef struct UiButton {
    int btn_id;
    int win;
    int x;
    int y;
    int width;
    int height;
    Art* up_art;
    Art* down_art;
    Art* hover_art;
} UiButton;

static UiButton ui_buttons[MAX_UI_BUTTONS];
static int ui_button_count = 0;

void ui_scale_art(Art* art, int win, int x, int y, int width, int height)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + (y * pitch) + x;

    if (art == NULL) {
        return;
    }

    unsigned char* frameData = art_frame_data(art, 0, 0);
    int frameWidth = art_frame_width(art, 0, 0);
    int frameHeight = art_frame_length(art, 0, 0);

    int remainingWidth = width - frameWidth;
    int remainingHeight = height - frameHeight;
    if (remainingWidth < 0 || remainingHeight < 0) {
        if (height * frameWidth >= width * frameHeight) {
            trans_cscale(frameData,
                frameWidth,
                frameHeight,
                frameWidth,
                dest + pitch * ((height - width * frameHeight / frameWidth) / 2),
                width,
                width * frameHeight / frameWidth,
                pitch);
        } else {
            trans_cscale(frameData,
                frameWidth,
                frameHeight,
                frameWidth,
                dest + (width - height * frameWidth / frameHeight) / 2,
                height * frameWidth / frameHeight,
                height,
                pitch);
        }
    } else {
        trans_buf_to_buf(frameData,
            frameWidth,
            frameHeight,
            frameWidth,
            dest + pitch * (remainingHeight / 2) + remainingWidth / 2,
            pitch);
    }

}

void ui_scale_art_fid(int fid, int win, int x, int y, int width, int height)
{
    CacheEntry* handle;
    Art* art = art_ptr_lock(fid, &handle);
    if (art != NULL) {
        ui_scale_art(art, win, x, y, width, height);
        art_ptr_unlock(handle);
    }
}

void ui_stretch_art(Art* art, int win, int x, int y, int width, int height)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    cscale(
        art_frame_data(art, 0, 0),
        art_frame_width(art, 0, 0),
        art_frame_length(art, 0, 0),
        art_frame_width(art, 0, 0),
        dest, width, height, pitch
    );
}

void ui_image_indexed(Art* art, int win, int x, int y, int width, int height, unsigned char* pal)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    trans_cscale_8_to_32(
        art_frame_data(art, 0, 0),
        art_frame_width(art, 0, 0),
        art_frame_length(art, 0, 0),
        art_frame_width(art, 0, 0),
        dest, width, height, pitch,
        pal
    );
}

void ui_image(Art* art, int win, int x, int y, int width, int height)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    trans_cscale(
        art_frame_data(art, 0, 0),
        art_frame_width(art, 0, 0),
        art_frame_length(art, 0, 0),
        art_frame_width(art, 0, 0),
        dest, width, height, pitch
    );
}

void ui_image_fill_indexed(Art* art, int win, int x, int y, int width, int height, unsigned char* pal)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    int srcWidth = art_frame_width(art, 0, 0);
    int srcHeight = art_frame_length(art, 0, 0);
    int srcPitch = srcWidth;
    unsigned char* src = art_frame_data(art, 0, 0);

    float scaleX = (float)width / srcWidth;
    float scaleY = (float)height / srcHeight;

    float scale = (scaleX > scaleY) ? scaleX : scaleY;

    int cropWidth = (int)(width / scale);
    int cropHeight = (int)(height / scale);

    int cropX = (srcWidth - cropWidth) / 2;
    int cropY = (srcHeight - cropHeight) / 2;

    src = src + (cropY * srcPitch) + cropX;

    // cscale(src, cropWidth, cropHeight, srcPitch, dest, width, height, pitch);
    cscale_8_to_32(src, cropWidth, cropHeight, srcPitch, dest, width, height, pitch, pal);
}

void ui_image_32(Art* art, int win, int x, int y, int width, int height)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    trans_cscale_32(
        art_frame_data(art, 0, 0),
        art_frame_width(art, 0, 0),
        art_frame_length(art, 0, 0),
        art_frame_width(art, 0, 0),
        dest, width, height, pitch
    );
}

void ui_image_fill_32(Art* art, int win, int x, int y, int width, int height)
{
    unsigned char* dest = win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + ((y * pitch) + x) * 4;

    if (art == NULL) {
        return;
    }

    int srcWidth = art_frame_width(art, 0, 0);
    int srcHeight = art_frame_length(art, 0, 0);
    int srcPitch = srcWidth; // In a 32-bit FRM, the pitch is still srcWidth in pixels! (cscale_32 converts it to bytes internally)
    unsigned char* src = art_frame_data(art, 0, 0);

    float scaleX = (float)width / srcWidth;
    float scaleY = (float)height / srcHeight;

    float scale = (scaleX > scaleY) ? scaleX : scaleY;

    int cropWidth = (int)(width / scale);
    int cropHeight = (int)(height / scale);

    int cropX = (srcWidth - cropWidth) / 2;
    int cropY = (srcHeight - cropHeight) / 2;

    src = src + ((cropY * srcPitch) + cropX) * 4; // * 4 for 32-bit pixel offset

    cscale_32(src, cropWidth, cropHeight, srcPitch, dest, width, height, pitch);
}

void ui_scaled_text(int win, const char* str, int x, int y, float scale, int color)
{
    int base_width = text_width(str);
    int base_height = text_height();

    if (base_width <= 0 || base_height <= 0 || scale <= 0.0f) {
        return;
    }

    unsigned int* dest = (unsigned int*)win_get_buf(win);
    int pitch = win_width(win);
    dest = dest + (y * pitch) + x;

    if (scale == 1.0f) {
        FMtext_to_buf_32((unsigned char*)dest, str, base_width, pitch, color);
        return;
    }

    int destWidth = (int)(base_width * scale);
    int destHeight = (int)(base_height * scale);

    unsigned int* temp_buf = (unsigned int*)mem_malloc(base_width * base_height * 4);
    unsigned int* bg_buf = (unsigned int*)mem_malloc(base_width * base_height * 4);
    
    if (temp_buf == NULL || bg_buf == NULL) {
        if (temp_buf) mem_free(temp_buf);
        if (bg_buf) mem_free(bg_buf);
        return;
    }

    int heightRatio = (destHeight << 16) / base_height;
    int widthRatio = (destWidth << 16) / base_width;

    int v2 = heightRatio;
    for (int by = 0; by < base_height; by++) {
        int v3 = widthRatio;
        int v4 = (heightRatio * by) >> 16;
        int v6 = 0;

        for (int bx = 0; bx < base_width; bx++) {
            int v8 = v6 >> 16;
            
            bg_buf[by * base_width + bx] = dest[v4 * pitch + v8];
            temp_buf[by * base_width + bx] = bg_buf[by * base_width + bx];

            v3 += widthRatio;
            v6 += widthRatio;
        }
        v2 += heightRatio;
    }

    FMtext_to_buf_32((unsigned char*)temp_buf, str, base_width, base_width, color);

    v2 = heightRatio;
    for (int by = 0; by < base_height; by++) {
        int v3 = widthRatio;
        int v4 = (heightRatio * by) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        for (int bx = 0; bx < base_width; bx++) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            if (temp_buf[by * base_width + bx] != bg_buf[by * base_width + bx]) {
                unsigned int c = temp_buf[by * base_width + bx];
                unsigned int* v9 = dest + pitch * v4 + v8;
                for (int destY = v4; destY < v5; destY++) {
                    for (int destX = v8; destX < v7; destX++) {
                        *v9++ = c;
                    }
                    v9 += pitch - (v7 - v8);
                }
            }

            v3 += widthRatio;
            v6 += widthRatio;
        }
        v2 += heightRatio;
    }

    mem_free(bg_buf);
    mem_free(temp_buf);
}

// TODO: This kinda wraps button because we still have both co-exist, but the
// win_register_button has to go.
int ui_register_button(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, Art* up, Art* dn, Art* hover, int flags) {
    unsigned char* upData = art_frame_data(up, 0, 0);
    unsigned char* dnData = art_frame_data(dn, 0, 0);
    unsigned char* hoverData = art_frame_data(hover, 0, 0);

    int srcWidth = art_frame_width(up, 0, 0);
    int srcHeight = art_frame_height(up, 0, 0);

    return win_register_button_scaled(win, x, y, width, height, mouseEnterEventCode, mouseExitEventCode, mouseDownEventCode, mouseUpEventCode, srcWidth, srcHeight, upData, dnData, hoverData, flags);
}

int ui_get_scale()
{
    Size screen_size = screen_get_size();
    int scale = screen_size.height / 480;
    if (scale < 1) {
        scale = 1;
    }
    return scale;
}

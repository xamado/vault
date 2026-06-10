#include "plib/gnw/grbuf.h"

#include <string.h>
#include <stdint.h>

#include "plib/color/color.h"
#include "plib/gnw/input.h"
#include "plib/gnw/svga.h"

// 0x4D2FC0
void draw_line(unsigned char* buf, int pitch, int x1, int y1, int x2, int y2, int color)
{
    int temp;
    int dx;
    int dy;
    unsigned char* p1;
    unsigned char* p2;
    unsigned char* p3;
    unsigned char* p4;

    if (x1 == x2) {
        if (y1 > y2) {
            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf + pitch * y1 + x1;
        p2 = buf + pitch * y2 + x2;
        while (p1 < p2) {
            *p1 = color;
            *p2 = color;
            p1 += pitch;
            p2 -= pitch;
        }
    } else {
        if (x1 > x2) {
            temp = x1;
            x1 = x2;
            x2 = temp;

            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf + pitch * y1 + x1;
        p2 = buf + pitch * y2 + x2;
        if (y1 == y2) {
            memset(p1, color, p2 - p1);
        } else {
            dx = x2 - x1;

            int v23;
            int v22;
            int midX = x1 + (x2 - x1) / 2;
            if (y1 <= y2) {
                dy = y2 - y1;
                v23 = pitch;
                v22 = midX + ((y2 - y1) / 2 + y1) * pitch;
            } else {
                dy = y1 - y2;
                v23 = -pitch;
                v22 = midX + (y1 - (y1 - y2) / 2) * pitch;
            }

            p3 = buf + v22;
            p4 = p3;

            if (dx <= dy) {
                int v28 = dx - (dy / 2);
                int v29 = dy / 4;
                while (true) {
                    *p1 = color;
                    *p2 = color;
                    *p3 = color;
                    *p4 = color;

                    if (v29 == 0) {
                        break;
                    }

                    if (v28 >= 0) {
                        p3++;
                        p2--;
                        p4--;
                        p1++;
                        v28 -= dy;
                    }

                    p3 += v23;
                    p2 -= v23;
                    p4 -= v23;
                    p1 += v23;
                    v28 += dx;

                    v29--;
                }
            } else {
                int v26 = dy - (dx / 2);
                int v27 = dx / 4;
                while (true) {
                    *p1 = color;
                    *p2 = color;
                    *p3 = color;
                    *p4 = color;

                    if (v27 == 0) {
                        break;
                    }

                    if (v26 >= 0) {
                        p3 += v23;
                        p2 -= v23;
                        p4 -= v23;
                        p1 += v23;
                        v26 -= dx;
                    }

                    p3++;
                    p2--;
                    p4--;
                    p1++;
                    v26 += dy;

                    v27--;
                }
            }
        }
    }
}

// 0x4D31A4
void draw_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color)
{
    draw_line(buf, pitch, left, top, right, top, color);
    draw_line(buf, pitch, left, bottom, right, bottom, color);
    draw_line(buf, pitch, left, top, left, bottom, color);
    draw_line(buf, pitch, right, top, right, bottom, color);
}

// 0x4D322C
void draw_shaded_box(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor)
{
    draw_line(buf, pitch, left, top, right, top, ltColor);
    draw_line(buf, pitch, left, bottom, right, bottom, rbColor);
    draw_line(buf, pitch, left, top, left, bottom, ltColor);
    draw_line(buf, pitch, right, top, right, bottom, rbColor);
}

// 0x4D33F0
void cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned char* c = src + v1;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            unsigned char* v9 = dest + destPitch * v4 + v8;
            for (int destY = v4; destY < v5; destY += 1) {
                for (int destX = v8; destX < v7; destX += 1) {
                    *v9++ = *c;
                }
                v9 += destPitch - (v7 - v8);
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

// 0x4D3560
void trans_cscale(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned char* c = src + v1;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            if (*c != 0) {
                unsigned char* v9 = dest + destPitch * v4 + v8;
                for (int destY = v4; destY < v5; destY += 1) {
                    for (int destX = v8; destX < v7; destX += 1) {
                        *v9++ = *c;
                    }
                    v9 += destPitch - (v7 - v8);
                }
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

// 0x4D36D4
void buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    for (int y = 0; y < height; y++) {
        memcpy(dest, src, width);
        dest += destPitch;
        src += srcPitch;
    }
}

// 0x4D3704
void trans_buf_to_buf(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    int destSkip = destPitch - width;
    int srcSkip = srcPitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char c = *src++;
            if (c != 0) {
                *dest = c;
            }
            dest++;
        }
        src += srcSkip;
        dest += destSkip;
    }
}

// 0x4D387C
void buf_fill(unsigned char* buf, int width, int height, int pitch, int a5)
{
    int y;

    for (y = 0; y < height; y++) {
        memset(buf, a5, width);
        buf += pitch;
    }
}

// 0x4D38E0
void buf_texture(unsigned char* buf, int width, int height, int pitch, void* a5, int a6, int a7)
{
    // TODO: Incomplete.
}

// 0x4D3A48
void lighten_buf(unsigned char* buf, int width, int height, int pitch)
{
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned char p = *buf;
            *buf++ = intensityColorTable[p][147];
        }
        buf += skip;
    }
}

// Swaps two colors in the buffer.
//
// 0x4D3A8C
void swap_color_buf(unsigned char* buf, int width, int height, int pitch, int color1, int color2)
{
    int step = pitch - width;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int v1 = *buf & 0xFF;
            if (v1 == color1) {
                *buf = color2 & 0xFF;
            } else if (v1 == color2) {
                *buf = color1 & 0xFF;
            }
            buf++;
        }
        buf += step;
    }
}

// 0x4D3AE0
void buf_outline(unsigned char* buf, int width, int height, int pitch, int color)
{
    unsigned char* ptr = buf + pitch;

    bool cycle;
    for (int y = 0; y < height - 2; y++) {
        cycle = true;

        for (int x = 0; x < width; x++) {
            if (*ptr != 0 && cycle) {
                *(ptr - 1) = color & 0xFF;
                cycle = false;
            } else if (*ptr == 0 && !cycle) {
                *ptr = color & 0xFF;
                cycle = true;
            }

            ptr++;
        }

        ptr += pitch - width;
    }

    for (int x = 0; x < width; x++) {
        ptr = buf + x;
        cycle = true;

        for (int y = 0; y < height; y++) {
            if (*ptr != 0 && cycle) {
                // TODO: Check in debugger, might be a bug.
                *(ptr - pitch) = color & 0xFF;
                cycle = false;
            } else if (*ptr == 0 && !cycle) {
                *ptr = color & 0xFF;
                cycle = true;
            }

            ptr += pitch;
        }
    }
}

void cscale_8_to_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* pal)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned char* c = src + v1;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            // Fetch RGB values and shift from 6-bit (0-63) to 8-bit (0-255)
            unsigned char r = pal[*c * 3] << 2;
            unsigned char g = pal[*c * 3 + 1] << 2;
            unsigned char b = pal[*c * 3 + 2] << 2;
            
            // Multiply X offset (v8) by 4 bytes per pixel
            // destPitch is passed in pixels, multiply by 4 for bytes
            int destPitchBytes = destPitch * 4;
            unsigned char* v9 = dest + (destPitchBytes * v4) + (v8 * 4);
            
            uint32_t outPixel = (0xFF << 24) | (b << 16) | (g << 8) | r;
            for (int destY = v4; destY < v5; destY += 1) {
                for (int destX = v8; destX < v7; destX += 1) {
                    *(uint32_t*)v9 = outPixel;
                    v9 += 4;
                }
                // destPitch is in pixels, so we subtract the bytes written
                v9 += destPitchBytes - ((v7 - v8) * 4);
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

void trans_cscale_8_to_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch, unsigned char* pal)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned char* c = src + v1;
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            if (*c != 0) {
                // Fetch RGB values and shift from 6-bit (0-63) to 8-bit (0-255)
                unsigned char r = pal[*c * 3] << 2;
                unsigned char g = pal[*c * 3 + 1] << 2;
                unsigned char b = pal[*c * 3 + 2] << 2;
                
                // destPitch is passed in pixels, multiply by 4 for bytes
                int destPitchBytes = destPitch * 4;
                unsigned char* v9 = dest + (destPitchBytes * v4) + (v8 * 4);
                
                for (int destY = v4; destY < v5; destY += 1) {
                    for (int destX = v8; destX < v7; destX += 1) {
                        // SDL_PIXELFORMAT_RGBA32 memory layout: R, G, B, A
                        *v9++ = r;
                        *v9++ = g;
                        *v9++ = b;
                        *v9++ = 0xFF;
                    }
                    // destPitch is in pixels, so we subtract the bytes written
                    v9 += destPitchBytes - ((v7 - v8) * 4);
                }
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

void cscale_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned int* c = (unsigned int*)(src + (v1 * 4));
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            unsigned int pixel = *c;
            
            int destPitchBytes = destPitch * 4;
            unsigned char* v9 = dest + (destPitchBytes * v4) + (v8 * 4);
            
            for (int destY = v4; destY < v5; destY += 1) {
                for (int destX = v8; destX < v7; destX += 1) {
                    *(unsigned int*)v9 = pixel;
                    v9 += 4;
                }
                v9 += destPitchBytes - ((v7 - v8) * 4);
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

void trans_cscale_32(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destWidth, int destHeight, int destPitch)
{
    int heightRatio = (destHeight << 16) / srcHeight;
    int widthRatio = (destWidth << 16) / srcWidth;

    int v1 = 0;
    int v2 = heightRatio;
    for (int srcY = 0; srcY < srcHeight; srcY += 1) {
        int v3 = widthRatio;
        int v4 = (heightRatio * srcY) >> 16;
        int v5 = v2 >> 16;
        int v6 = 0;

        unsigned int* c = (unsigned int*)(src + (v1 * 4));
        for (int srcX = 0; srcX < srcWidth; srcX += 1) {
            int v7 = v3 >> 16;
            int v8 = v6 >> 16;

            unsigned int pixel = *c;
            
            // Check Alpha channel (assuming RGBA32 on Little Endian -> 0xAABBGGRR, so Alpha is highest byte)
            // Wait, we just want to skip fully transparent pixels for now.
            // Or true alpha blend.
            unsigned int alpha = (pixel >> 24) & 0xFF;

            if (alpha > 0) {
                int destPitchBytes = destPitch * 4;
                unsigned char* v9 = dest + (destPitchBytes * v4) + (v8 * 4);
                
                for (int destY = v4; destY < v5; destY += 1) {
                    for (int destX = v8; destX < v7; destX += 1) {
                        if (alpha == 255) {
                            *(unsigned int*)v9 = pixel;
                        } else {
                            unsigned int bg = *(unsigned int*)v9;
                            unsigned int bg_r = bg & 0xFF;
                            unsigned int bg_g = (bg >> 8) & 0xFF;
                            unsigned int bg_b = (bg >> 16) & 0xFF;
                            
                            unsigned int fg_r = pixel & 0xFF;
                            unsigned int fg_g = (pixel >> 8) & 0xFF;
                            unsigned int fg_b = (pixel >> 16) & 0xFF;

                            unsigned int r = ((fg_r * alpha) + (bg_r * (255 - alpha))) / 255;
                            unsigned int g = ((fg_g * alpha) + (bg_g * (255 - alpha))) / 255;
                            unsigned int b = ((fg_b * alpha) + (bg_b * (255 - alpha))) / 255;

                            *(unsigned int*)v9 = (0xFF << 24) | (b << 16) | (g << 8) | r;
                        }
                        v9 += 4;
                    }
                    v9 += destPitchBytes - ((v7 - v8) * 4);
                }
            }

            v3 += widthRatio;
            c++;
            v6 += widthRatio;
        }
        v1 += srcPitch;
        v2 += heightRatio;
    }
}

void buf_to_buf_32(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    int srcPitchBytes = srcPitch * 4;
    int destPitchBytes = destPitch * 4;
    for (int y = 0; y < height; y++) {
        memcpy(dest, src, width * 4);
        dest += destPitchBytes;
        src += srcPitchBytes;
    }
}

void trans_buf_to_buf_32(unsigned char* src, int width, int height, int srcPitch, unsigned char* dest, int destPitch)
{
    // Pitches are passed in PIXELS. Convert to bytes for pointer math!
    int srcPitchBytes = srcPitch * 4;
    int destPitchBytes = destPitch * 4;

    for (int y = 0; y < height; y++) {
        // Cast the current row pointers to 32-bit
        unsigned int* src32 = (unsigned int*)src;
        unsigned int* dest32 = (unsigned int*)dest;
        
        for (int x = 0; x < width; x++) {
            unsigned int c = src32[x];
            if (c != 0) { 
                dest32[x] = c;
            }
        }
        
        // Advance by pitch bytes
        src += srcPitchBytes;
        dest += destPitchBytes;
    }
}

void buf_fill_32(unsigned char* buf, int width, int height, int pitch, int color) 
{
    // Pitch is passed in PIXELS. Convert to bytes for pointer math!
    int pitchBytes = pitch * 4;

    for (int y = 0; y < height; y++) {
        unsigned int* row = (unsigned int*)buf;
        for (int x = 0; x < width; x++) {
            row[x] = color;
        }
        buf += pitchBytes; 
    }
}


void draw_line_32(unsigned char* buf, int pitch, int x1, int y1, int x2, int y2, int color)
{
    // Pitch is in PIXELS. Convert buf to unsigned int*.
    unsigned int* buf32 = (unsigned int*)buf;
    int temp;
    int dx;
    int dy;
    unsigned int* p1;
    unsigned int* p2;
    unsigned int* p3;
    unsigned int* p4;

    if (x1 == x2) {
        if (y1 > y2) {
            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf32 + pitch * y1 + x1;
        p2 = buf32 + pitch * y2 + x2;
        while (p1 <= p2) {
            *p1 = color;
            p1 += pitch;
        }
    } else {
        if (x1 > x2) {
            temp = x1;
            x1 = x2;
            x2 = temp;

            temp = y1;
            y1 = y2;
            y2 = temp;
        }

        p1 = buf32 + pitch * y1 + x1;
        p2 = buf32 + pitch * y2 + x2;
        if (y1 == y2) {
            for (int i = 0; i <= (x2 - x1); i++) {
                p1[i] = color;
            }
        } else {
            // Simplified Bresenham for remaining cases, keeping it robust.
            int w = x2 - x1;
            int h = y2 - y1;
            int dx1 = 1, dy1 = 0, dx2 = 1, dy2 = 0;
            if (h < 0) dy1 = -1; else if (h > 0) dy1 = 1;
            if (h < 0) dy2 = -1; else if (h > 0) dy2 = 1;
            int longest = w;
            int shortest = h;
            if (longest < 0) longest = -longest;
            if (shortest < 0) shortest = -shortest;
            if (longest <= shortest) {
                longest = shortest;
                shortest = w;
                if (shortest < 0) shortest = -shortest;
                dy2 = 0;
                if (w < 0) dx2 = -1; else if (w > 0) dx2 = 1;
            }
            int numerator = longest >> 1;
            for (int i = 0; i <= longest; i++) {
                buf32[x1 + y1 * pitch] = color;
                numerator += shortest;
                if (numerator >= longest) {
                    numerator -= longest;
                    x1 += dx1;
                    y1 += dy1;
                } else {
                    x1 += dx2;
                    y1 += dy2;
                }
            }
        }
    }
}

void draw_box_32(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int color)
{
    draw_line_32(buf, pitch, left, top, right, top, color);
    draw_line_32(buf, pitch, left, bottom, right, bottom, color);
    draw_line_32(buf, pitch, left, top, left, bottom, color);
    draw_line_32(buf, pitch, right, top, right, bottom, color);
}

void draw_shaded_box_32(unsigned char* buf, int pitch, int left, int top, int right, int bottom, int ltColor, int rbColor)
{
    draw_line_32(buf, pitch, left, top, right, top, ltColor);
    draw_line_32(buf, pitch, left, bottom, right, bottom, rbColor);
    draw_line_32(buf, pitch, left, top, left, bottom, ltColor);
    draw_line_32(buf, pitch, right, top, right, bottom, rbColor);
}

void lighten_buf_32(unsigned char* buf, int width, int height, int pitch)
{
    // Pitch is in pixels
    unsigned int* buf32 = (unsigned int*)buf;
    int skip = pitch - width;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned int p = *buf32;
            // Simple multiply by ~1.2 or similar to lighten, capping at 255
            unsigned int r = p & 0xFF;
            unsigned int g = (p >> 8) & 0xFF;
            unsigned int b = (p >> 16) & 0xFF;
            
            r = (r * 147) / 100; if (r > 255) r = 255;
            g = (g * 147) / 100; if (g > 255) g = 255;
            b = (b * 147) / 100; if (b > 255) b = 255;
            
            *buf32++ = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }
        buf32 += skip;
    }
}

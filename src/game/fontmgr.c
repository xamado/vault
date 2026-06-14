#include "game/fontmgr.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "plib/color/color.h"
#include "plib/db/db.h"
#include "int/memdbg.h"
#include "plib/gnw/text.h"

// The maximum number of interface fonts.
#define INTERFACE_FONT_MAX 16

typedef struct InterfaceFontGlyph {
    short width;
    short height;
    int offset;
} InterfaceFontGlyph;

typedef struct InterfaceFontDescriptor {
    short maxHeight;
    short letterSpacing;
    short wordSpacing;
    short lineSpacing;
    short field_8;
    short field_A;
    InterfaceFontGlyph glyphs[256];
    unsigned char* data;
} InterfaceFontDescriptor;

static int FMLoadFont(int font);
static void Swap4(unsigned int* value);
static void Swap2(unsigned short* value);

// 0x518680
static bool gFMInit = false;

// 0x518684
static int gNumFonts = 0;

// 0x586838
static InterfaceFontDescriptor gFontCache[INTERFACE_FONT_MAX];

// 0x58E938
static int gCurrentFontNum;

// 0x58E93C
static InterfaceFontDescriptor* gCurrentFont;

// 0x441C80
int FMInit()
{
    int currentFont = -1;

    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (FMLoadFont(font) == -1) {
            gFontCache[font].maxHeight = 0;
            gFontCache[font].data = NULL;
        } else {
            ++gNumFonts;

            if (currentFont == -1) {
                currentFont = font;
            }
        }
    }

    if (currentFont == -1) {
        return -1;
    }

    gFMInit = true;

    FMtext_font(currentFont + 100);

    return 0;
}

// 0x441CEC
void FMExit()
{
    for (int font = 0; font < INTERFACE_FONT_MAX; font++) {
        if (gFontCache[font].data != NULL) {
            myfree(gFontCache[font].data, __FILE__, __LINE__); // FONTMGR.C, 124
        }
    }
}

// 0x441D20
static int FMLoadFont(int font_index)
{
    InterfaceFontDescriptor* fontDescriptor = &(gFontCache[font_index]);

    char path[56];
    sprintf(path, "font%d.aaf", font_index);

    File* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    int fileSize = db_filelength(stream);

    int sig;
    if (db_fread(&sig, 4, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }

    Swap4(&sig);
    if (sig != 0x41414646) {
        db_fclose(stream);
        return -1;
    }

    if (db_fread(&(fontDescriptor->maxHeight), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    Swap2(&(fontDescriptor->maxHeight));

    if (db_fread(&(fontDescriptor->letterSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    Swap2(&(fontDescriptor->letterSpacing));

    if (db_fread(&(fontDescriptor->wordSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    Swap2(&(fontDescriptor->wordSpacing));

    if (db_fread(&(fontDescriptor->lineSpacing), 2, 1, stream) != 1) {
        db_fclose(stream);
        return -1;
    }
    Swap2(&(fontDescriptor->lineSpacing));

    for (int index = 0; index < 256; index++) {
        InterfaceFontGlyph* glyph = &(fontDescriptor->glyphs[index]);

        if (db_fread(&(glyph->width), 2, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        Swap2(&(glyph->width));

        if (db_fread(&(glyph->height), 2, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        Swap2(&(glyph->height));

        if (db_fread(&(glyph->offset), 4, 1, stream) != 1) {
            db_fclose(stream);
            return -1;
        }
        Swap4(&(glyph->offset));
    }

    int glyphDataSize = fileSize - 2060;

    fontDescriptor->data = (unsigned char*)mymalloc(glyphDataSize, __FILE__, __LINE__); // FONTMGR.C, 259
    if (fontDescriptor->data == NULL) {
        db_fclose(stream);
        return -1;
    }

    if (db_fread(fontDescriptor->data, glyphDataSize, 1, stream) != 1) {
        myfree(fontDescriptor->data, __FILE__, __LINE__); // FONTMGR.C, 268
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);

    return 0;
}

// 0x442120
void FMtext_font(int font)
{
    if (!gFMInit) {
        return;
    }

    font -= 100;

    if (gFontCache[font].data != NULL) {
        gCurrentFontNum = font;
        gCurrentFont = &(gFontCache[font]);
    }
}

// 0x442168
int FMtext_height()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->lineSpacing + gCurrentFont->maxHeight;
}

// 0x442188
int FMtext_width(const char* string)
{
    if (!gFMInit) {
        return 0;
    }

    int stringWidth = 0;

    while (*string != '\0') {
        unsigned char ch = (unsigned char)(*string++);

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentFont->wordSpacing;
        } else {
            characterWidth = gCurrentFont->glyphs[ch].width;
        }

        stringWidth += characterWidth + gCurrentFont->letterSpacing;
    }

    return stringWidth;
}

// 0x4421DC
int FMtext_char_width(int ch)
{
    int width;

    if (!gFMInit) {
        return 0;
    }

    if (ch == ' ') {
        width = gCurrentFont->wordSpacing;
    } else {
        width = gCurrentFont->glyphs[ch].width;
    }

    return width;
}

// 0x442210
int FMtext_mono_width(const char* str)
{
    if (!gFMInit) {
        return 0;
    }

    return FMtext_max() * strlen(str);
}

// 0x442240
int FMtext_spacing()
{
    if (!gFMInit) {
        return 0;
    }

    return gCurrentFont->letterSpacing;
}

// 0x442258
int FMtext_size(const char* str)
{
    if (!gFMInit) {
        return 0;
    }

    return FMtext_width(str) * FMtext_height();
}

// 0x442278
int FMtext_max()
{
    if (!gFMInit) {
        return 0;
    }

    int v1;
    if (gCurrentFont->wordSpacing <= gCurrentFont->field_8) {
        v1 = gCurrentFont->lineSpacing;
    } else {
        v1 = gCurrentFont->letterSpacing;
    }

    return v1 + gCurrentFont->maxHeight;
}

// NOTE: Unused.
//
// 0x4422AC
int FMtext_curr()
{
    return gCurrentFontNum;
}

// 0x4422B4
void FMtext_to_buf(unsigned char* buf, const char* string, int length, int pitch, int color)
{
    if (!gFMInit) {
        return;
    }

    int flags = 0;
    unsigned int outPixel = color;

    // Check if it's an 8-bit index + flags. FONT_SHADOW is 0x10000.
    // 32-bit RGBA colors will have the alpha channel (0xFF000000) set.
    if ((color & 0xFF000000) == 0) {
        flags = color & 0xFFFF0000;
        int palIndex = color & 0xFF;
        unsigned char* pal = getColorPalette();
        unsigned int r = pal[palIndex * 3] << 2;
        unsigned int g = pal[palIndex * 3 + 1] << 2;
        unsigned int b = pal[palIndex * 3 + 2] << 2;
        outPixel = (0xFF << 24) | (b << 16) | (g << 8) | r;
    } else {
        flags = 0;
    }

    if ((flags & FONT_SHADOW) != 0) {
        int shadowIndex = colorTable[0] & 0xFF;
        unsigned char* pal = getColorPalette();
        unsigned int shadowPixel = (0xFF << 24) | ((pal[shadowIndex * 3 + 2] << 2) << 16) | ((pal[shadowIndex * 3 + 1] << 2) << 8) | (pal[shadowIndex * 3] << 2);
        
        // pitch is in pixels, so moving down 1 row and right 1 pixel means + pitch*4 + 4 bytes
        FMtext_to_buf(buf + pitch * 4 + 4, string, length, pitch, shadowPixel);
    }

    int monospacedCharacterWidth;
    if ((flags & FONT_MONO) != 0) {
        monospacedCharacterWidth = FMtext_max();
    }

    unsigned int* ptr = (unsigned int*)buf;
    while (*string != '\0') {
        char ch = *string++;

        int characterWidth;
        if (ch == ' ') {
            characterWidth = gCurrentFont->wordSpacing;
        } else {
            characterWidth = gCurrentFont->glyphs[ch & 0xFF].width;
        }

        unsigned int* end;
        if ((flags & FONT_MONO) != 0) {
            end = ptr + monospacedCharacterWidth;
            ptr += (monospacedCharacterWidth - characterWidth - gCurrentFont->letterSpacing) / 2;
        } else {
            end = ptr + characterWidth + gCurrentFont->letterSpacing;
        }

        if ((end - (unsigned int*)buf) > length) {
            break;
        }

        InterfaceFontGlyph* glyph = &(gCurrentFont->glyphs[ch & 0xFF]);
        unsigned char* glyphDataPtr = gCurrentFont->data + glyph->offset;

        // Skip blank pixels (difference between font's line height and glyph height).
        ptr += (gCurrentFont->maxHeight - glyph->height) * pitch;

        for (int y = 0; y < glyph->height; y++) {
            for (int x = 0; x < glyph->width; x++) {
                unsigned char byte = *glyphDataPtr++;
                if (byte != 0) {
                    if (byte >= 7) {
                        // byte >= 7 means 100% opacity (or brighter).
                        // If byte > 7, the original engine blended it with white, but 
                        // for standard text, it shouldn't be common. We cap opacity at 100%.
                        *ptr = outPixel;
                    } else {
                        unsigned int bg = *ptr;
                        unsigned int bg_r = bg & 0xFF;
                        unsigned int bg_g = (bg >> 8) & 0xFF;
                        unsigned int bg_b = (bg >> 16) & 0xFF;

                        unsigned int fg_r = outPixel & 0xFF;
                        unsigned int fg_g = (outPixel >> 8) & 0xFF;
                        unsigned int fg_b = (outPixel >> 16) & 0xFF;

                        // byte goes from 1 to 7 for standard anti-aliasing blending
                        unsigned int alpha = (byte * 255) / 7;
                        unsigned int inv_alpha = 255 - alpha;

                        unsigned int r = (fg_r * alpha + bg_r * inv_alpha) / 255;
                        unsigned int g = (fg_g * alpha + bg_g * inv_alpha) / 255;
                        unsigned int b = (fg_b * alpha + bg_b * inv_alpha) / 255;

                        *ptr = (0xFF << 24) | (b << 16) | (g << 8) | r;
                    }
                }
                ptr++;
            }
            ptr += pitch - glyph->width;
        }

        ptr = end;
    }

    if ((flags & FONT_UNDERLINE) != 0) {
        int drawnLen = ptr - (unsigned int*)buf;
        unsigned int* underlinePtr = (unsigned int*)buf + pitch * (gCurrentFont->maxHeight - 1);
        for (int index = 0; index < drawnLen; index++) {
            *underlinePtr++ = outPixel;
        }
    }
}

// NOTE: Inlined.
//
// 0x442520
static void Swap4(unsigned int* value)
{
    unsigned int swapped = *value;
    unsigned short high = swapped >> 16;
    // NOTE: Uninline.
    Swap2(&high);
    unsigned short low = swapped & 0xFFFF;
    // NOTE: Uninline.
    Swap2(&low);
    *value = (low << 16) | high;
}

// 0x442568
static void Swap2(unsigned short* value)
{
    unsigned short swapped = *value;
    swapped = (swapped >> 8) | (swapped << 8);
    *value = swapped;
}


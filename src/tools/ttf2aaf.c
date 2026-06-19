// ttf2aaf - generate Fallout .AAF bitmap fonts from a TrueType (.ttf) file.
//
// AAF format (all multi-byte fields are big-endian):
//   Header (12 bytes):
//     char[4]  "AAFF"
//     u16      maxHeight     - height of a line cell, in pixels
//     u16      letterSpacing - extra pixels added after each glyph
//     u16      wordSpacing   - width of the space (0x20) character
//     u16      lineSpacing   - extra pixels added between lines
//   Glyph table (256 records x 8 bytes):
//     u16      width         - glyph width / horizontal advance, in pixels
//     u16      height        - number of stored rows (<= maxHeight)
//     u32      offset        - byte offset into the pixel block
//   Pixel block:
//     width*height bytes per glyph, row-major. Each byte is a 3-bit coverage
//     value 0..7 (0 = transparent, 7 = fully opaque).
//
// The engine pads (maxHeight - height) blank rows at the TOP of each glyph,
// so glyphs are baseline-aligned: we store rows from a glyph's topmost row
// down to the bottom of the cell.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Write 16-bit short in Big-Endian
static void write16be(FILE* f, uint16_t v) {
    uint8_t bytes[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    fwrite(bytes, 1, 2, f);
}

// Write 32-bit int in Big-Endian
static void write32be(FILE* f, uint32_t v) {
    uint8_t bytes[4] = {
        (uint8_t)(v >> 24),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)(v & 0xFF)
    };
    fwrite(bytes, 1, 4, f);
}

typedef struct Glyph {
    uint16_t width;
    uint16_t height;
    uint32_t offset;
} Glyph;

// Quantize an 8-bit coverage value (0..255) to the 3-bit AAF level (0..7).
static uint8_t quantize(uint8_t cov) {
    int v = (cov * 7 + 127) / 255;
    if (v > 7) v = 7;
    return (uint8_t)v;
}

static unsigned char* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf || fread(buf, 1, size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = size;
    return buf;
}

int main(int argc, char** argv) {
    int height = 9;       // pixel height of a line cell (matches Fallout FONT1)
    int letterSpacing = 1;
    int wordSpacing = 0;  // 0 => derive from the font's space advance
    int lineSpacing = 1;
    const char* input_path = NULL;
    const char* output_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--letter") == 0 && i + 1 < argc) {
            letterSpacing = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--word") == 0 && i + 1 < argc) {
            wordSpacing = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--line") == 0 && i + 1 < argc) {
            lineSpacing = atoi(argv[++i]);
        } else if (!input_path) {
            input_path = argv[i];
        } else if (!output_path) {
            output_path = argv[i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (!input_path || !output_path) {
        printf("Usage: %s [--height N] [--letter N] [--word N] [--line N] <input.ttf> <output.aaf>\n", argv[0]);
        printf("  --height  line cell height in pixels (default 9)\n");
        printf("  --letter  extra spacing after each glyph (default 1)\n");
        printf("  --word    space character width (default: font's space advance)\n");
        printf("  --line    extra spacing between lines (default 1)\n");
        return 1;
    }

    if (height < 1 || height > 1000) {
        fprintf(stderr, "Error: --height %d out of range\n", height);
        return 1;
    }

    long ttf_size;
    unsigned char* ttf = read_file(input_path, &ttf_size);
    if (!ttf) {
        fprintf(stderr, "Error: Failed to read '%s'\n", input_path);
        return 1;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        fprintf(stderr, "Error: '%s' is not a valid TrueType font\n", input_path);
        free(ttf);
        return 1;
    }

    // ScaleForPixelHeight maps (ascent - descent) to `height` pixels, so the
    // whole cell (including descenders) fits in [0, height).
    float scale = stbtt_ScaleForPixelHeight(&font, (float)height);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    int baseline = (int)lroundf(ascent * scale);

    // Derive the space width from the font if the user didn't override it.
    if (wordSpacing == 0) {
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, ' ', &adv, &lsb);
        wordSpacing = (int)lroundf(adv * scale);
        if (wordSpacing < 1) wordSpacing = height / 2;
    }

    Glyph glyphs[256];
    memset(glyphs, 0, sizeof(glyphs));

    // Growable pixel block.
    size_t data_cap = 1 << 16;
    size_t data_len = 0;
    uint8_t* data = (uint8_t*)malloc(data_cap);

    for (int c = 0; c < 256; c++) {
        // Space is handled by wordSpacing; the engine never reads its bitmap.
        if (c == ' ') {
            continue;
        }
        if (stbtt_FindGlyphIndex(&font, c) == 0) {
            continue; // glyph not present in this font
        }

        int adv, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &adv, &lsb);
        int advance = (int)lroundf(adv * scale);

        int gw, gh, xoff, yoff;
        unsigned char* bmp = stbtt_GetCodepointBitmap(&font, scale, scale, c, &gw, &gh, &xoff, &yoff);

        // top of the glyph ink in cell coordinates (baseline at row `baseline`)
        int top = baseline + yoff;

        // Cell-space width: enough to hold both the advance and the ink.
        int ink_right = xoff + gw;
        int width = advance > ink_right ? advance : ink_right;
        if (width < 0) width = 0;

        // Stored region runs from the topmost ink row to the bottom of the cell.
        int store_top = top < 0 ? 0 : top;
        int store_h = height - store_top;
        if (store_h < 0) store_h = 0;
        if (store_h > height) store_h = height;

        if (width > 0 && store_h > 0) {
            size_t cell_size = (size_t)width * store_h;
            while (data_len + cell_size > data_cap) {
                data_cap *= 2;
                data = (uint8_t*)realloc(data, data_cap);
            }
            uint8_t* cell = data + data_len;
            memset(cell, 0, cell_size);

            // Blit the glyph bitmap into the cell at (xoff, top - store_top).
            for (int y = 0; y < gh; y++) {
                int cy = (top - store_top) + y;
                if (cy < 0 || cy >= store_h) continue;
                for (int x = 0; x < gw; x++) {
                    int cx = xoff + x;
                    if (cx < 0 || cx >= width) continue;
                    cell[cy * width + cx] = quantize(bmp[y * gw + x]);
                }
            }

            glyphs[c].width = (uint16_t)width;
            glyphs[c].height = (uint16_t)store_h;
            glyphs[c].offset = (uint32_t)data_len;
            data_len += cell_size;
        } else {
            // Zero-area glyph (e.g. a control char that maps to nothing visible)
            // still gets its advance width so layout stays correct.
            glyphs[c].width = (uint16_t)(width > 0 ? width : 0);
            glyphs[c].height = 0;
            glyphs[c].offset = (uint32_t)data_len;
        }

        stbtt_FreeBitmap(bmp, NULL);
    }

    FILE* out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "Error: Could not open output '%s'\n", output_path);
        free(data);
        free(ttf);
        return 1;
    }

    // Header
    fwrite("AAFF", 1, 4, out);
    write16be(out, (uint16_t)height);
    write16be(out, (uint16_t)letterSpacing);
    write16be(out, (uint16_t)wordSpacing);
    write16be(out, (uint16_t)lineSpacing);

    // Glyph table
    for (int c = 0; c < 256; c++) {
        write16be(out, glyphs[c].width);
        write16be(out, glyphs[c].height);
        write32be(out, glyphs[c].offset);
    }

    // Pixel block
    fwrite(data, 1, data_len, out);

    fclose(out);

    int rendered = 0;
    for (int c = 0; c < 256; c++) {
        if (glyphs[c].height > 0) rendered++;
    }
    printf("Wrote %s: height=%d, %d glyphs, %zu bytes of pixel data\n",
           output_path, height, rendered, data_len);

    free(data);
    free(ttf);
    return 0;
}

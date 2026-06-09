#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Write 16-bit short in Big-Endian
void write16be(FILE* f, uint16_t v) {
    uint8_t bytes[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    fwrite(bytes, 1, 2, f);
}

// Write 32-bit int in Big-Endian
void write32be(FILE* f, uint32_t v) {
    uint8_t bytes[4] = {
        (uint8_t)(v >> 24),
        (uint8_t)((v >> 16) & 0xFF),
        (uint8_t)((v >> 8) & 0xFF),
        (uint8_t)(v & 0xFF)
    };
    fwrite(bytes, 1, 4, f);
}

typedef struct RGB {
    uint8_t r, g, b;
} RGB;

RGB palette[256];
int has_palette = 0;

void load_palette(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open palette file '%s'\n", path);
        exit(1);
    }
    
    uint8_t raw[768];
    if (fread(raw, 1, 768, f) != 768) {
        fprintf(stderr, "Error: Palette file '%s' must be at least 768 bytes\n", path);
        fclose(f);
        exit(1);
    }
    fclose(f);

    // Assume standard 0-63 VGA palette scale used by Fallout
    for (int i = 0; i < 256; i++) {
        palette[i].r = raw[i * 3 + 0] << 2;
        palette[i].g = raw[i * 3 + 1] << 2;
        palette[i].b = raw[i * 3 + 2] << 2;
    }
    has_palette = 1;
}

uint8_t get_nearest_color(int r, int g, int b, int a) {
    if (a < 128) {
        // Fallout magic transparent color
        return 0;
    }

    int min_dist = 255*255*3 + 1;
    uint8_t best = 1;
    
    // Skip index 0 (reserved for transparency) to avoid "holes" in dark areas
    for (int i = 1; i < 256; i++) {
        int dr = r - palette[i].r;
        int dg = g - palette[i].g;
        int db = b - palette[i].b;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < min_dist) {
            min_dist = dist;
            best = i;
        }
    }
    return best;
}

int main(int argc, char** argv) {
    const char* palette_path = NULL;
    const char* input_path = NULL;
    const char* output_path = NULL;
    int is_v5 = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            palette_path = argv[++i];
        } else if (strcmp(argv[i], "--v5") == 0) {
            is_v5 = 1;
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
        printf("Usage: %s [--v5] [-p color.pal] <input_image> <output.frm>\n", argv[0]);
        return 1;
    }

    if (is_v5) {
        if (palette_path) {
            fprintf(stderr, "Warning: -p palette flag is ignored when --v5 is used.\n");
        }
    } else {
        if (palette_path) {
            load_palette(palette_path);
        } else {
            // Try to load color.pal from current directory as fallback
            FILE* test = fopen("color.pal", "rb");
            if (test) {
                fclose(test);
                load_palette("color.pal");
            } else {
                fprintf(stderr, "Warning: No palette specified (-p color.pal). Output will be grayscale if image is not indexed.\n");
            }
        }
    }

    int width, height, channels;
    // Load as 4 channels (RGBA) so we can check alpha
    unsigned char* img = stbi_load(input_path, &width, &height, &channels, 4);
    if (!img) {
        fprintf(stderr, "Error: Failed to load image '%s'\n", input_path);
        return 1;
    }

    FILE* out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "Error: Could not open output file '%s'\n", output_path);
        stbi_image_free(img);
        return 1;
    }

    // Write Art Header (62 bytes)
    write32be(out, is_v5 ? 5 : 4); // version
    write16be(out, 15); // framesPerSecond
    write16be(out, 0); // actionFrame
    write16be(out, 1); // frameCount
    
    // xOffsets[6]
    for(int i=0; i<6; i++) write16be(out, 0);
    // yOffsets[6]
    for(int i=0; i<6; i++) write16be(out, 0);
    // dataOffsets[6]
    for(int i=0; i<6; i++) write32be(out, 0);

    // frameDataSize
    write32be(out, 12 + (width * height * (is_v5 ? 4 : 1))); 

    // Write ArtFrame Header (12 bytes)
    write16be(out, width);
    write16be(out, height);
    write32be(out, width * height * (is_v5 ? 4 : 1));
    write16be(out, 0); // x offset
    write16be(out, 0); // y offset

    // Write Pixel Data
    if (is_v5) {
        fwrite(img, 1, width * height * 4, out);
    } else {
        unsigned char* palettized = malloc(width * height);
        for (int i = 0; i < width * height; i++) {
            int r = img[i * 4 + 0];
            int g = img[i * 4 + 1];
            int b = img[i * 4 + 2];
            int a = img[i * 4 + 3];
            if (has_palette) {
                palettized[i] = get_nearest_color(r, g, b, a);
            } else {
                // Very naive grayscale fallback
                palettized[i] = (r + g + b) / 3;
            }
        }
        fwrite(palettized, 1, width * height, out);
        free(palettized);
    }

    fclose(out);
    stbi_image_free(img);
    
    printf("Successfully converted %s to %s (%dx%d)\n", input_path, output_path, width, height);

    return 0;
}

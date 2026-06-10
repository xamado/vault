#ifndef FALLOUT_PLIB_GNW_SVGA_H_
#define FALLOUT_PLIB_GNW_SVGA_H_

#include <stdbool.h>

#include "plib/gnw/rect.h"
#include "plib/gnw/svga_types.h"

extern UpdatePaletteFunc* update_palette_func;

extern Rect scr_size;
extern ScreenBlitFunc* scr_blit;
extern ZeroMemFunc* zero_mem;

void zero_vid_mem();
Size screen_get_size(void);
int GNW95_init_mode(int width, int height, int bpp);
int GNW95_init_window(int width, int height);
void GNW95_reset_mode();
void GNW95_SetPaletteEntry(int entry, unsigned char r, unsigned char g, unsigned char b);
void GNW95_SetPaletteEntries(unsigned char* palette, int start, int count);
void GNW95_SetPalette(unsigned char* palette);
unsigned char* GNW95_GetPalette();
void GNW95_ShowRect(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY);
void GNW95_ShowRect32(unsigned char* src, int srcPitch, int a3, int srcX, int srcY, int srcWidth, int srcHeight, int destX, int destY);
void GNW95_ShowMovieRect(unsigned char* src, int srcPitch, int srcX, int srcY, int srcWidth, int srcHeight);
void GNW95_zero_vid_mem();

#endif /* FALLOUT_PLIB_GNW_SVGA_H_ */

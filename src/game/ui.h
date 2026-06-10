#ifndef FALLOUT_GAME_UI_H_
#define FALLOUT_GAME_UI_H_

#include "game/art.h"

void ui_scale_art(Art* art, int win, int x, int y, int width, int height);
void ui_scale_art_fid(int fid, int win, int x, int y, int width, int height);
void ui_stretch_art(Art* art, int win, int x, int y, int width, int height);
void ui_image(Art* art, int win, int x, int y, int width, int height);
void ui_image_indexed(Art* art, int win, int x, int y, int width, int height, unsigned char* pal);
void ui_image_fill_indexed(Art* art, int win, int x, int y, int width, int height, unsigned char* pal);
void ui_image_32(Art* art, int win, int x, int y, int width, int height);
void ui_image_fill_32(Art* art, int win, int x, int y, int width, int height);
int ui_get_scale(void);
void ui_scaled_text(int win, const char* str, int x, int y, float scale, int color);

int ui_register_button(int win, int x, int y, int width, int height, int mouseEnterEventCode, int mouseExitEventCode, int mouseDownEventCode, int mouseUpEventCode, Art* up, Art* dn, Art* hover, int flags);

#endif /* FALLOUT_GAME_UI_H_ */

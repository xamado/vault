#ifndef FALLOUT_GAME_TILE_H_
#define FALLOUT_GAME_TILE_H_

#include <stdbool.h>

#include "plib/gnw/rect.h"
#include "game/map.h"
#include "game/object_types.h"

#define TILE_SET_CENTER_REFRESH_WINDOW 0x01
#define TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS 0x02

typedef void(TileWindowRefreshProc)(Rect* rect);
typedef void(TileWindowRefreshElevationProc)(Rect* rect, int elevation);

extern int off_tile[2][6];

// Tile geometry constants — derived from integer scale factor in tile_init().
extern int hex_w;        // hex tile width (32 * scale)
extern int hex_h;        // hex tile height (16 * scale)
extern int row_step;     // vertical step per hex row (12 * scale)
extern int sq_col_dx;    // square column horizontal step (48 * scale)
extern int sq_row_dy;    // square row vertical step (24 * scale)
extern int half_hex_w;   // hex_w / 2
extern int half_hex_h;   // hex_h / 2
extern int tile_scale;   // integer scale factor

extern int tile_center_tile;

int tile_init(TileData** a1, int squareGridWidth, int squareGridHeight, int hexGridWidth, int hexGridHeight, unsigned char* buf, int windowWidth, int windowHeight, int windowPitch, TileWindowRefreshProc* windowRefreshProc);
void tile_set_border(int windowWidth, int windowHeight, int hexGridWidth, int hexGridHeight);
void tile_reset();
void tile_exit();
void tile_disable_refresh();
void tile_enable_refresh();
void tile_refresh_rect(Rect* rect, int elevation);
void tile_refresh_display();
int tile_set_center(int tile, int flags);
void tile_toggle_roof(int a1);
int tile_roof_visible();
int tile_coord(int tile, int* x, int* y, int elevation);
int tile_num(int x, int y, int elevation);
int tile_dist(int a1, int a2);
bool tile_in_front_of(int tile1, int tile2);
bool tile_to_right_of(int tile1, int tile2);
int tile_num_in_direction(int tile, int rotation, int distance);
int tile_dir(int a1, int a2);
int tile_num_beyond(int from, int to, int distance);
void tile_enable_scroll_blocking();
void tile_disable_scroll_blocking();
bool tile_get_scroll_blocking();
void tile_enable_scroll_limiting();
void tile_disable_scroll_limiting();
bool tile_get_scroll_limiting();
int square_coord(int squareTile, int* coordX, int* coordY, int elevation);
int square_coord_roof(int squareTile, int* screenX, int* screenY, int elevation);
int square_num(int screenX, int screenY, int elevation);
void square_xy(int screenX, int screenY, int elevation, int* coordX, int* coordY);
void square_xy_roof(int screenX, int screenY, int elevation, int* coordX, int* coordY);
void square_render_roof(Rect* rect, int elevation);
void tile_fill_roof(int x, int y, int elevation, int a4);
void square_render_floor(Rect* rect, int elevation);
bool square_roof_intersect(int x, int y, int elevation);
#ifdef ENABLE_TILE_GRID_DEBUG
void grid_toggle();
void grid_on();
void grid_off();
int get_grid_flag();
void grid_render(Rect* rect, int elevation);
void grid_draw(int tile, int elevation);
void draw_grid(int tile, int elevation, Rect* rect);
#endif
void floor_draw(int fid, int x, int y, Rect* rect);
int tile_make_line(int currentCenterTile, int newCenterTile, int* tiles, int tilesCapacity);
int tile_scroll_to(int tile, int flags);

#endif /* FALLOUT_GAME_TILE_H_ */

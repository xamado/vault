#include "game/tile.h"
#include "plib/os/os_string.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "plib/color/color.h"
#include "game/config.h"
#include "plib/gnw/input.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/grbuf.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/light.h"
#include "game/map.h"
#include "game/object.h"

#define TILE_IS_VALID(tile) ((tile) >= 0 && (tile) < grid_size)

typedef struct STRUCT_51D99C {
    int field_0;
    int field_4;
} STRUCT_51D99C;

typedef struct STRUCT_51DA04 {
    int field_0;
    int field_4;
} STRUCT_51DA04;

typedef struct STRUCT_51DA6C {
    int field_0;
    int field_4;
    int field_8;
    int field_C; // something with light level?
} STRUCT_51DA6C;

typedef struct STRUCT_51DB0C {
    int field_0;
    int field_4;
    int field_8;
} STRUCT_51DB0C;

typedef struct STRUCT_51DB48 {
    int field_0;
    int field_4;
    int field_8;
} STRUCT_51DB48;

static void refresh_game(Rect* rect, int elevation);
static bool tile_on_edge(int tile);
static void roof_fill_on(int x, int y, int elevation);
static void roof_fill_off(int x, int y, int elevation);
static void roof_draw(int fid, int x, int y, Rect* rect, int light);

// 0x51D950
static bool borderInitialized = false;

// 0x51D954
static bool scroll_blocking_on = true;

// 0x51D958
static bool scroll_limiting_on = true;

// 0x51D95C
static bool show_roof = true;

// 0x51D960
#ifdef ENABLE_TILE_GRID_DEBUG
static bool show_grid = false;
#endif

// 0x51D964
static TileWindowRefreshElevationProc* tile_refresh = refresh_game;

// 0x51D968
static bool refresh_enabled = true;

// 0x51D96C
int off_tile[2][6] = {
    {
        0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0,
    }
};

// 0x51D99C
static STRUCT_51D99C rightside_up_table[13] = {
    { -1, 2 },
    { 78, 2 },
    { 76, 6 },
    { 73, 8 },
    { 71, 10 },
    { 68, 14 },
    { 65, 16 },
    { 63, 18 },
    { 61, 20 },
    { 58, 24 },
    { 55, 26 },
    { 53, 28 },
    { 50, 32 },
};

// 0x51DA04
static STRUCT_51DA04 upside_down_table[13] = {
    { 0, 32 },
    { 48, 32 },
    { 49, 30 },
    { 52, 26 },
    { 55, 24 },
    { 57, 22 },
    { 60, 18 },
    { 63, 16 },
    { 65, 14 },
    { 67, 12 },
    { 70, 8 },
    { 73, 6 },
    { 75, 4 },
};

// 0x51DA6C
static STRUCT_51DA6C verticies[10] = {
    { 16, -1, -201, 0 },
    { 48, -2, -2, 0 },
    { 960, 0, 0, 0 },
    { 992, 199, -1, 0 },
    { 1024, 198, 198, 0 },
    { 1936, 200, 200, 0 },
    { 1968, 399, 199, 0 },
    { 2000, 398, 398, 0 },
    { 2912, 400, 400, 0 },
    { 2944, 599, 399, 0 },
};

// 0x51DB0C
static STRUCT_51DB0C rightside_up_triangles[5] = {
    { 2, 3, 0 },
    { 3, 4, 1 },
    { 5, 6, 3 },
    { 6, 7, 4 },
    { 8, 9, 6 },
};

// 0x51DB48
static STRUCT_51DB48 upside_down_triangles[5] = {
    { 0, 3, 1 },
    { 2, 5, 3 },
    { 3, 6, 4 },
    { 5, 8, 6 },
    { 6, 9, 7 },
};

// 0x668224
static int intensity_map[3280];

// 0x66B564
static int dir_tile2[2][6];

// Deltas to perform tile calculations in given direction.
//
// 0x66B594
static int dir_tile[2][6];

// 0x66B5C4
#ifdef ENABLE_TILE_GRID_DEBUG
static unsigned char tile_grid_blocked[512 * 4];

// 0x66B7C4
static unsigned char tile_grid_occupied[512 * 4];
#endif


// 0x66BBC4
static Rect tile_border;

// 0x66BBD4
static Rect buf_rect;

// 0x66BBE4
#ifdef ENABLE_TILE_GRID_DEBUG
static unsigned char tile_grid[32 * 16 * 4];
#endif

// 0x66BDE4
static int square_y;

// 0x66BDE8
static int square_x;

// 0x66BDEC
static int square_offx;

// 0x66BDF0
static int square_offy;

// 0x66BDF4
static TileWindowRefreshProc* blit;

// 0x66BDF8
static int tile_offy;

// 0x66BDFC
static int tile_offx;

// 0x66BE00
static int square_size;

// Number of tiles horizontally.
//
// Currently this value is always 200.
//
// 0x66BE04
static int grid_width;

// 0x66BE08
static TileData** squares;

// 0x66BE0C
static unsigned char* buf;

// Number of tiles vertically.
//
// Currently this value is always 200.
//
// 0x66BE10
static int grid_length;

// 0x66BE14
static int buf_length;

// 0x66BE18
static int tile_x;

// 0x66BE1C
static int tile_y;

// The number of tiles in the hex grid.
//
// 0x66BE20
static int grid_size;

// 0x66BE24
static int square_length;

// 0x66BE28
static int buf_full;

// 0x66BE2C
static int square_width;

// 0x66BE30
static int buf_width;

// 0x66BE34
int tile_center_tile;

// Number of hex rows visible in the original 640×480 game viewport
// (480 - 100px interface bar = 380px viewport, 380 / 12 ≈ 32 rows).
#define VIEWPORT_TILES_V 32

// Computed tile geometry — derived from integer scale factor.
// Set once during tile_init().
int tile_scale = 1;   // integer scale factor (1=original, 2=2x, 3=3x, ...)
int hex_w = 32;       // hex tile width in screen pixels
int hex_h = 16;       // hex tile height in screen pixels
int row_step = 12;    // vertical step per hex row
int sq_col_dx = 48;   // square_coord column horizontal step
int sq_row_dy = 24;   // square_coord row vertical step
int half_hex_w = 16;  // hex_w / 2
int half_hex_h = 8;   // hex_h / 2

// 0x4B0C40
int tile_init(TileData** a1, int squareGridWidth, int squareGridHeight, int hexGridWidth, int hexGridHeight, unsigned char* buffer, int windowWidth, int windowHeight, int windowPitch, TileWindowRefreshProc* windowRefreshProc)
{
    int v20;
    int v21;
    int v22;
    int v23;
    int v24;
    int v25;

    // Compute integer scale factor from viewport height.
    // The original viewport was 380px tall (480 - 100px interface bar),
    // showing ~32 hex rows (380 / 12 = 31.67).
    tile_scale = windowHeight / (VIEWPORT_TILES_V * 12);
    if (tile_scale < 1) tile_scale = 1;

    hex_w      = 32 * tile_scale;
    hex_h      = 16 * tile_scale;
    row_step   = 12 * tile_scale;
    sq_col_dx  = 48 * tile_scale;
    sq_row_dy  = 24 * tile_scale;
    half_hex_w = hex_w / 2;
    half_hex_h = hex_h / 2;

    debug_printf("tile_init: scale=%d, hex=%dx%d, row_step=%d\n", tile_scale, hex_w, hex_h, row_step);

    // Initialize per-direction pixel offsets for hex movement.
    // These must stay in unscaled (1x) space because obj->x/y accumulate
    // unscaled FRM frame hotspot offsets (from art_frame_hot).
    off_tile[0][0] =  16;    // NE: dx
    off_tile[0][1] =  32;    // E:  dx
    off_tile[0][2] =  16;    // SE: dx
    off_tile[0][3] = -16;    // SW: dx
    off_tile[0][4] = -32;    // W:  dx
    off_tile[0][5] = -16;    // NW: dx
    off_tile[1][0] = -12;    // NE: dy
    off_tile[1][1] =  0;     // E:  dy
    off_tile[1][2] =  12;    // SE: dy
    off_tile[1][3] =  12;    // SW: dy
    off_tile[1][4] =  0;     // W:  dy
    off_tile[1][5] = -12;    // NW: dy

    square_width = squareGridWidth;
    squares = a1;
    grid_length = hexGridHeight;
    square_length = squareGridHeight;
    grid_width = hexGridWidth;
    dir_tile[0][0] = -1;
    dir_tile[0][4] = 1;
    dir_tile[1][1] = -1;
    grid_size = hexGridWidth * hexGridHeight;
    dir_tile[1][3] = 1;
    buf = buffer;
    dir_tile2[0][0] = -1;
    buf_width = windowWidth;
    dir_tile2[0][3] = -1;
    buf_length = windowHeight;
    dir_tile2[1][1] = 1;
    buf_full = windowPitch;
    dir_tile2[1][2] = 1;
    buf_rect.lrx = windowWidth - 1;
    square_size = squareGridHeight * squareGridWidth;
    buf_rect.lry = windowHeight - 1;
    buf_rect.ulx = 0;
    blit = windowRefreshProc;
    buf_rect.uly = 0;
    dir_tile[0][1] = hexGridWidth - 1;
    dir_tile[0][2] = hexGridWidth;
#ifdef ENABLE_TILE_GRID_DEBUG
    show_grid = 0;
#endif
    dir_tile[0][3] = hexGridWidth + 1;
    dir_tile[1][2] = hexGridWidth;
    dir_tile2[0][4] = hexGridWidth;
    dir_tile2[0][5] = hexGridWidth;
    dir_tile[0][5] = -hexGridWidth;
    dir_tile[1][0] = -hexGridWidth - 1;
    dir_tile[1][4] = 1 - hexGridWidth;
    dir_tile[1][5] = -hexGridWidth;
    dir_tile2[0][1] = -hexGridWidth - 1;
    dir_tile2[1][4] = -hexGridWidth;
    dir_tile2[0][2] = hexGridWidth - 1;
    dir_tile2[1][5] = -hexGridWidth;
    dir_tile2[1][0] = hexGridWidth + 1;
    dir_tile2[1][3] = 1 - hexGridWidth;



#ifdef ENABLE_TILE_GRID_DEBUG
    buf_fill(tile_grid, 32, 16, 32, 0);
    draw_line(tile_grid, 32, 16, 0, 31, 4, colorTable[4228]);
    draw_line(tile_grid, 32, 31, 4, 31, 12, colorTable[4228]);
    draw_line(tile_grid, 32, 31, 12, 16, 15, colorTable[4228]);
    draw_line(tile_grid, 32, 0, 12, 16, 15, colorTable[4228]);
    draw_line(tile_grid, 32, 0, 4, 0, 12, colorTable[4228]);
    draw_line(tile_grid, 32, 16, 0, 0, 4, colorTable[4228]);

    buf_fill(tile_grid_occupied, 32, 16, 32, 0);
    draw_line(tile_grid_occupied, 32, 16, 0, 31, 4, colorTable[31]);
    draw_line(tile_grid_occupied, 32, 31, 4, 31, 12, colorTable[31]);
    draw_line(tile_grid_occupied, 32, 31, 12, 16, 15, colorTable[31]);
    draw_line(tile_grid_occupied, 32, 0, 12, 16, 15, colorTable[31]);
    draw_line(tile_grid_occupied, 32, 0, 4, 0, 12, colorTable[31]);
    draw_line(tile_grid_occupied, 32, 16, 0, 0, 4, colorTable[31]);

    buf_fill(tile_grid_blocked, 32, 16, 32, 0);
    draw_line(tile_grid_blocked, 32, 16, 0, 31, 4, colorTable[31744]);
    draw_line(tile_grid_blocked, 32, 31, 4, 31, 12, colorTable[31744]);
    draw_line(tile_grid_blocked, 32, 31, 12, 16, 15, colorTable[31744]);
    draw_line(tile_grid_blocked, 32, 0, 12, 16, 15, colorTable[31744]);
    draw_line(tile_grid_blocked, 32, 0, 4, 0, 12, colorTable[31744]);
    draw_line(tile_grid_blocked, 32, 16, 0, 0, 4, colorTable[31744]);

    uint32_t* blocked32 = (uint32_t*)tile_grid_blocked;
    for (v20 = 0; v20 < 16; v20++) {
        v21 = v20 * 32;
        v22 = 31;
        v23 = v21 + 31;

        if (blocked32[v23] == 0) {
            do {
                --v22;
                --v23;
            } while (v22 > 0 && blocked32[v23] == 0);
        }

        v24 = v21;
        v25 = 0;
        if (blocked32[v21] == 0) {
            do {
                ++v25;
                ++v24;
            } while (v25 < 32 && blocked32[v24] == 0);
        }

        draw_line(tile_grid_blocked, 32, v25, v20, v22, v20, colorTable[31744]);
    }
#endif

    tile_set_center(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    tile_set_border(windowWidth, windowHeight, hexGridWidth, hexGridHeight);

    return 0;
}

// 0x4B11E4
void tile_set_border(int windowWidth, int windowHeight, int hexGridWidth, int hexGridHeight)
{
    // Compute how many hex tile columns/rows the viewport spans.
    // Each hex column is 32 pixels wide, each row is 12 pixels tall
    // (the vertical step in the staggered grid).
    // Add some padding (+6/+7) to prevent edge artifacts.
    int tileCols = (windowWidth / 2) / hex_w + 6;
    int tileRows = (windowHeight / 2) / row_step + 7;

    tile_border.ulx = tileCols;
    tile_border.uly = tileRows;
    tile_border.lrx = hexGridWidth - tileCols - 1;
    tile_border.lry = hexGridHeight - tileRows - 1;

    if ((tile_border.ulx & 1) == 0) {
        tile_border.ulx++;
    }

    if ((tile_border.lrx & 1) == 0) {
        tile_border.ulx--;
    }

    // If the viewport is large enough that the border range inverts
    // (ulx >= lrx or uly >= lry), the entire map fits on screen
    // and scroll borders should not restrict movement.
    if (tile_border.ulx >= tile_border.lrx || tile_border.uly >= tile_border.lry) {
        borderInitialized = false;
    } else {
        borderInitialized = true;
    }
}

// NOTE: Uncollapsed 0x4B129C.
void tile_reset()
{
}

// NOTE: Uncollapsed 0x4B129C.
void tile_exit()
{
}

// 0x4B12A8
void tile_disable_refresh()
{
    refresh_enabled = false;
}

// 0x4B12B4
void tile_enable_refresh()
{
    refresh_enabled = true;
}

// 0x4B12C0
void tile_refresh_rect(Rect* rect, int elevation)
{
    if (refresh_enabled) {
        if (elevation == map_elevation) {
            tile_refresh(rect, elevation);
        }
    }
}

// 0x4B12D8
void tile_refresh_display()
{
    if (refresh_enabled) {
        tile_refresh(&buf_rect, map_elevation);
    }
}

// 0x4B12F8
int tile_set_center(int tile, int flags)
{
    if (!TILE_IS_VALID(tile)) {
        return -1;
    }

    if ((flags & TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == 0) {
        if (scroll_limiting_on) {
            int tileScreenX;
            int tileScreenY;
            tile_coord(tile, &tileScreenX, &tileScreenY, map_elevation);

            int dudeScreenX;
            int dudeScreenY;
            tile_coord(obj_dude->tile, &dudeScreenX, &dudeScreenY, map_elevation);

            int dx = abs(dudeScreenX - tileScreenX);
            int dy = abs(dudeScreenY - tileScreenY);

            // Cap max scroll distance from the dude at the original
            // 640x480 design values. These represent a game design decision
            // (how far the camera can stray from the player), not a
            // resolution-dependent value.
            int maxDx = 480;  // original: 640 * 3/4
            int maxDy = 400;  // original: 480 - 80

            // Check each axis independently: only block if the axis
            // being scrolled further from the dude exceeds its limit.
            if ((dx > abs(dudeScreenX - tile_offx) && dx >= maxDx)
                || (dy > abs(dudeScreenY - tile_offy) && dy >= maxDy)) {
                return -1;
            }
        }

        if (scroll_blocking_on) {
            if (obj_scroll_blocking_at(tile, map_elevation) == 0) {
                return -1;
            }
        }
    }

    int new_tile_x = grid_width - 1 - tile % grid_width;
    int new_tile_y = tile / grid_width;

    if ((flags & TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == 0) {
        if (borderInitialized) {
            if (new_tile_x <= tile_border.ulx || new_tile_x >= tile_border.lrx || new_tile_y <= tile_border.uly || new_tile_y >= tile_border.lry) {
                return -1;
            }
        }
    }

    tile_y = new_tile_y;
    tile_offx = (buf_width - hex_w) / 2;
    tile_x = new_tile_x;
    tile_offy = (buf_length - hex_h) / 2;

    if (tile_x & 1) {
        tile_x -= 1;
        tile_offx -= hex_w;
    }

    square_x = tile_x / 2;
    square_y = tile_y / 2;
    square_offx = tile_offx - half_hex_w;
    square_offy = tile_offy - (2 * tile_scale);

    if (tile_y & 1) {
        square_offy -= row_step;
        square_offx -= half_hex_w;
    }

    tile_center_tile = tile;

    if ((flags & TILE_SET_CENTER_REFRESH_WINDOW) != 0) {
        // NOTE: Uninline.
        tile_refresh_display();
    }

    return 0;
}

// 0x4B15E8
static void refresh_game(Rect* rect, int elevation)
{
    Rect rectToUpdate;

    if (rect_inside_bound(rect, &buf_rect, &rectToUpdate) == -1) {
        return;
    }

    square_render_floor(&rectToUpdate, elevation);
    obj_render_pre_roof(&rectToUpdate, elevation);
    square_render_roof(&rectToUpdate, elevation);
    obj_render_post_roof(&rectToUpdate, elevation);
#ifdef ENABLE_TILE_GRID_DEBUG
    grid_render(&rectToUpdate, elevation);
#endif
    blit(&rectToUpdate);
}

// 0x4B1634
void tile_toggle_roof(int a1)
{
    show_roof = 1 - show_roof;

    if (a1) {
        // NOTE: Uninline.
        tile_refresh_display();
    }
}

// 0x4B166C
int tile_roof_visible()
{
    return show_roof;
}

// 0x4B1674
int tile_coord(int tile, int* screenX, int* screenY, int elevation)
{
    int v3;
    int v4;
    int v5;
    int v6;

    if (!TILE_IS_VALID(tile)) {
        return -1;
    }

    v3 = grid_width - 1 - tile % grid_width;
    v4 = tile / grid_width;

    *screenX = tile_offx;
    *screenY = tile_offy;

    v5 = (v3 - tile_x) / -2;
    *screenX += sq_col_dx * ((v3 - tile_x) / 2);
    *screenY += row_step * v5;

    if (v3 & 1) {
        if (v3 <= tile_x) {
            *screenX -= half_hex_w;
            *screenY += row_step;
        } else {
            *screenX += hex_w;
        }
    }

    v6 = v4 - tile_y;
    *screenX += half_hex_w * v6;
    *screenY += row_step * v6;

    return 0;
}

// Determine which hex zone a pixel at (px, py) falls into within a hex
// of dimensions hex_w × hex_h. The hex has four diagonal edges forming
// the pointed top and bottom. Returns:
//   0 = center (this hex)
//   1 = top neighbor
//   2 = right neighbor
//   3 = left neighbor
//   4 = bottom neighbor
//
// The diagonal edges are:
//   Top-left:     (0, quarter_h)    → (half_w, 0)
//   Top-right:    (half_w, 0)       → (hex_w, quarter_h)
//   Bottom-left:  (0, 3*quarter_h)  → (half_w, hex_h)
//   Bottom-right: (half_w, hex_h)   → (hex_w, 3*quarter_h)
//
// Zone tests use integer cross-products to avoid division.
static int hex_zone(int px, int py)
{
    int quarter_h = hex_h / 4;

    if (py < quarter_h) {
        // Top region — test against the two top diagonals.
        if (px < half_hex_w) {
            // Top-left edge: zone 1 if point is above the line from (0, quarter_h) to (half_w, 0).
            // Line equation: px * quarter_h + py * half_hex_w < half_hex_w * quarter_h
            if (px * quarter_h + py * half_hex_w < half_hex_w * quarter_h)
                return 1;
        } else {
            // Top-right edge: zone 2 if point is above the line from (half_w, 0) to (hex_w, quarter_h).
            // Line equation: (px - half_w) * quarter_h > py * half_hex_w
            if ((px - half_hex_w) * quarter_h > py * half_hex_w)
                return 2;
        }
    } else if (py >= hex_h - quarter_h) {
        // Bottom region — test against the two bottom diagonals.
        int by = py - (hex_h - quarter_h);
        if (px < half_hex_w) {
            // Bottom-left edge: zone 3 if point is below the line from (0, 3*quarter_h) to (half_w, hex_h).
            if (px * quarter_h <= half_hex_w * by)
                return 3;
        } else {
            // Bottom-right edge: zone 4 if point is below the line from (half_w, hex_h) to (hex_w, 3*quarter_h).
            if ((hex_w - px) * quarter_h <= half_hex_w * by)
                return 4;
        }
    }

    // Middle region or inside the hex boundary — this hex.
    return 0;
}

// 0x4B1754
int tile_num(int screenX, int screenY, int elevation)
{
    int v2;
    int v3;
    int v4;
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;
    int v11;
    int v12;

    v2 = screenY - tile_offy;
    if (v2 >= 0) {
        v3 = v2 / row_step;
    } else {
        v3 = (v2 + 1) / row_step - 1;
    }

    v4 = screenX - tile_offx - half_hex_w * v3;
    v5 = v2 - row_step * v3;

    if (v4 >= 0) {
        v6 = v4 / (hex_w * 2);
    } else {
        v6 = (v4 + 1) / (hex_w * 2) - 1;
    }

    v7 = v6 + v3;
    v8 = v4 - (v6 * (hex_w * 2));
    v9 = 2 * v6;

    if (v8 >= hex_w) {
        v8 -= hex_w;
        v9++;
    }

    v10 = tile_y + v7;
    v11 = tile_x + v9;

    switch (hex_zone(v8, v5)) {
    case 2:
        v11++;
        if (v11 & 1) {
            v10--;
        }
        break;
    case 1:
        v10--;
        break;
    case 3:
        v11--;
        if (!(v11 & 1)) {
            v10++;
        }
        break;
    case 4:
        v10++;
        break;
    default:
        break;
    }

    v12 = grid_width - 1 - v11;
    if (v12 >= 0 && v12 < grid_width && v10 >= 0 && v10 < grid_length) {
        return grid_width * v10 + v12;
    }

    return -1;
}

// tile_distance
// 0x4B185C
int tile_dist(int tile1, int tile2)
{
    int i;
    int v9;
    int v8;
    int v2;

    if (tile1 == -1) {
        return 9999;
    }

    if (tile2 == -1) {
        return 9999;
    }

    int x1;
    int y1;
    tile_coord(tile2, &x1, &y1, 0);

    v2 = tile1;
    for (i = 0; v2 != tile2; i++) {
        // TODO: Looks like inlined rotation_to_tile.
        int x2;
        int y2;
        tile_coord(v2, &x2, &y2, 0);

        int dx = x1 - x2;
        int dy = y1 - y2;

        if (x1 == x2) {
            if (dy < 0) {
                v9 = 0;
            } else {
                v9 = 2;
            }
        } else {
            v8 = (int)trunc(atan2((double)-dy, (double)dx) * 180.0 * 0.3183098862851122);

            v9 = 360 - (v8 + 180) - 90;
            if (v9 < 0) {
                v9 += 360;
            }

            v9 /= 60;

            if (v9 >= 6) {
                v9 = 5;
            }
        }

        v2 += dir_tile[v2 % grid_width & 1][v9];
    }

    return i;
}

// 0x4B1994
bool tile_in_front_of(int tile1, int tile2)
{
    int x1;
    int y1;
    tile_coord(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tile_coord(tile2, &x2, &y2, 0);

    int dx = x2 - x1;
    int dy = y2 - y1;

    return (double)dx <= (double)dy * -4.0;
}

// 0x4B1A00
bool tile_to_right_of(int tile1, int tile2)
{
    int x1;
    int y1;
    tile_coord(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tile_coord(tile2, &x2, &y2, 0);

    int dx = x2 - x1;
    int dy = y2 - y1;

    // NOTE: the value below looks like 4/3, which is 0x3FF55555555555, but it's
    // binary value is slightly different: 0x3FF55555555556. This difference plays
    // important role as seen right in the beginning of the game, comparing tiles
    // 17488 (0x4450) and 15288 (0x3BB8).
    return (double)dx <= (double)dy * 1.3333333333333335;
}

// tile_num_in_direction
// 0x4B1A6C
int tile_num_in_direction(int tile, int rotation, int distance)
{
    int newTile = tile;
    for (int index = 0; index < distance; index++) {
        if (tile_on_edge(newTile)) {
            break;
        }

        int parity = (newTile % grid_width) & 1;
        newTile += dir_tile[parity][rotation];
    }

    return newTile;
}

// rotation_to_tile
// 0x4B1ABC
int tile_dir(int tile1, int tile2)
{
    int x1;
    int y1;
    tile_coord(tile1, &x1, &y1, 0);

    int x2;
    int y2;
    tile_coord(tile2, &x2, &y2, 0);

    int dy = y2 - y1;
    x2 -= x1;
    y2 -= y1;

    if (x2 != 0) {
        // TODO: Check.
        int v6 = (int)trunc(atan2((double)-dy, (double)x2) * 180.0 * 0.3183098862851122);
        int v7 = 360 - (v6 + 180) - 90;
        if (v7 < 0) {
            v7 += 360;
        }

        v7 /= 60;

        if (v7 >= ROTATION_COUNT) {
            v7 = ROTATION_NW;
        }
        return v7;
    }

    return dy < 0 ? ROTATION_NE : ROTATION_SE;
}

// 0x4B1B84
int tile_num_beyond(int from, int to, int distance)
{
    if (distance <= 0 || from == to) {
        return from;
    }

    int fromX;
    int fromY;
    tile_coord(from, &fromX, &fromY, 0);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tile_coord(to, &toX, &toY, 0);
    toX += 16;
    toY += 8;

    int deltaX = toX - fromX;
    int deltaY = toY - fromY;

    int v27 = 2 * abs(deltaX);

    int stepX = 0;
    if (deltaX > 0)
        stepX = 1;
    else if (deltaX < 0)
        stepX = -1;

    int v26 = 2 * abs(deltaY);

    int stepY = 0;
    if (deltaY > 0)
        stepY = 1;
    else if (deltaY < 0)
        stepY = -1;

    int v28 = from;
    int tileX = fromX;
    int tileY = fromY;

    int v6 = 0;

    if (v27 > v26) {
        int middle = v26 - v27 / 2;
        while (true) {
            int tile = tile_num(tileX, tileY, 0);
            if (tile != v28) {
                v6 += 1;
                if (v6 == distance || tile_on_edge(tile)) {
                    return tile;
                }

                v28 = tile;
            }

            if (middle >= 0) {
                middle -= v27;
                tileY += stepY;
            }

            middle += v26;
            tileX += stepX;
        }
    } else {
        int middle = v27 - v26 / 2;
        while (true) {
            int tile = tile_num(tileX, tileY, 0);
            if (tile != v28) {
                v6 += 1;
                if (v6 == distance || tile_on_edge(tile)) {
                    return tile;
                }

                v28 = tile;
            }

            if (middle >= 0) {
                middle -= v26;
                tileX += stepX;
            }

            middle += v27;
            tileY += stepY;
        }
    }

    assert(false && "Should be unreachable");
}

// 0x4B1D20
static bool tile_on_edge(int tile)
{
    if (!TILE_IS_VALID(tile)) {
        return false;
    }

    if (tile < grid_width) {
        return true;
    }

    if (tile >= grid_size - grid_width) {
        return true;
    }

    if (tile % grid_width == 0) {
        return true;
    }

    if (tile % grid_width == grid_width - 1) {
        return true;
    }

    return false;
}

// 0x4B1D80
void tile_enable_scroll_blocking()
{
    scroll_blocking_on = true;
}

// 0x4B1D8C
void tile_disable_scroll_blocking()
{
    scroll_blocking_on = false;
}

// 0x4B1D98
bool tile_get_scroll_blocking()
{
    return scroll_blocking_on;
}

// 0x4B1DA0
void tile_enable_scroll_limiting()
{
    scroll_limiting_on = true;
}

// 0x4B1DAC
void tile_disable_scroll_limiting()
{
    scroll_limiting_on = false;
}

// 0x4B1DB8
bool tile_get_scroll_limiting()
{
    return scroll_limiting_on;
}

// 0x4B1DC0
int square_coord(int squareTile, int* coordX, int* coordY, int elevation)
{
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;

    if (squareTile < 0 || squareTile >= square_size) {
        return -1;
    }

    v5 = square_width - 1 - squareTile % square_width;
    v6 = squareTile / square_width;
    v7 = square_x;

    *coordX = square_offx;
    *coordY = square_offy;

    v8 = v5 - v7;
    *coordX += sq_col_dx * v8;
    *coordY -= row_step * v8;

    v9 = v6 - square_y;
    *coordX += hex_w * v9;
    *coordY += sq_row_dy * v9;

    return 0;
}

// 0x4B1E60
int square_coord_roof(int squareTile, int* screenX, int* screenY, int elevation)
{
    int v5;
    int v6;
    int v7;
    int v8;
    int v9;
    int v10;

    if (squareTile < 0 || squareTile >= square_size) {
        return -1;
    }

    v5 = square_width - 1 - squareTile % square_width;
    v6 = squareTile / square_width;
    v7 = square_x;
    *screenX = square_offx;
    *screenY = square_offy;

    v8 = v5 - v7;
    *screenX += sq_col_dx * v8;
    *screenY -= row_step * v8;

    v9 = v6 - square_y;
    *screenX += hex_w * v9;
    v10 = sq_row_dy * v9 + *screenY;
    *screenY = v10;
    *screenY = v10 - (hex_h * 6);

    return 0;
}

// 0x4B1F04
int square_num(int screenX, int screenY, int elevation)
{
    int coordY;
    int coordX;

    square_xy(screenX, screenY, elevation, &coordX, &coordY);

    if (coordX >= 0 && coordX < square_width && coordY >= 0 && coordY < square_length) {
        return coordX + square_width * coordY;
    }

    return -1;
}

// 0x4B1F94
void square_xy(int screenX, int screenY, int elevation, int* coordX, int* coordY)
{
    int v4;
    int v5;
    int v6;
    int v8;

    v4 = screenX - square_offx;
    v5 = screenY - square_offy - row_step;
    v6 = 3 * v4 - 4 * v5;
    *coordX = v6 >= 0 ? (v6 / (sq_col_dx * 4)) : ((v6 + 1) / (sq_col_dx * 4) - 1);

    v8 = 4 * v5 + v4;
    *coordY = v8 >= 0 ? (v8 / (hex_w * 4)) : ((v8 + 1) / (hex_w * 4) - 1);

    *coordX += square_x;
    *coordY += square_y;

    *coordX = square_width - 1 - *coordX;
}

// 0x4B203C
void square_xy_roof(int screenX, int screenY, int elevation, int* coordX, int* coordY)
{
    int v4;
    int v5;
    int v6;
    int v8;

    v4 = screenX - square_offx;
    v5 = screenY + (hex_h * 6) - square_offy - row_step;
    v6 = 3 * v4 - 4 * v5;

    *coordX = (v6 >= 0) ? (v6 / (sq_col_dx * 4)) : ((v6 + 1) / (sq_col_dx * 4) - 1);

    v8 = 4 * v5 + v4;
    *coordY = v8 >= 0 ? (v8 / (hex_w * 4)) : ((v8 + 1) / (hex_w * 4) - 1);

    *coordX += square_x;
    *coordY += square_y;

    *coordX = square_width - 1 - *coordX;
}

// 0x4B20E8
void square_render_roof(Rect* rect, int elevation)
{
    if (!show_roof) {
        return;
    }

    int temp;
    int minY;
    int minX;
    int maxX;
    int maxY;

    square_xy_roof(rect->ulx, rect->uly, elevation, &temp, &minY);
    square_xy_roof(rect->lrx, rect->uly, elevation, &minX, &temp);
    square_xy_roof(rect->ulx, rect->lry, elevation, &maxX, &temp);
    square_xy_roof(rect->lrx, rect->lry, elevation, &temp, &maxY);

    if (minX < 0) {
        minX = 0;
    }

    if (minX >= square_width) {
        minX = square_width - 1;
    }

    if (minY < 0) {
        minY = 0;
    }

    // FIXME: Probably a bug - testing X, then changing Y.
    if (minX >= square_length) {
        minY = square_length - 1;
    }

    int light = light_get_ambient();

    int baseSquareTile = square_width * minY;

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            int squareTile = baseSquareTile + x;
            int frmId = squares[elevation]->field_0[squareTile];
            frmId >>= 16;
            if ((((frmId & 0xF000) >> 12) & 0x01) == 0) {
                int fid = art_id(OBJ_TYPE_TILE, frmId & 0xFFF, 0, 0, 0);
                if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
                    int screenX;
                    int screenY;
                    square_coord_roof(squareTile, &screenX, &screenY, elevation);
                    roof_draw(fid, screenX, screenY, rect, light);
                }
            }
        }
        baseSquareTile += square_width;
    }
}

// 0x4B22D0
static void roof_fill_on(int a1, int a2, int elevation)
{
    while ((a1 >= 0 && a1 < square_width) && (a2 >= 0 && a2 < square_length)) {
        int squareTile = square_width * a2 + a1;
        int value = squares[elevation]->field_0[squareTile];
        int upper = (value >> 16) & 0xFFFF;

        int id = upper & 0xFFF;
        if (art_id(OBJ_TYPE_TILE, id, 0, 0, 0) == art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
            break;
        }

        int flag = (upper & 0xF000) >> 12;
        if ((flag & 0x01) == 0) {
            break;
        }

        flag &= ~0x01;

        squares[elevation]->field_0[squareTile] = (value & 0xFFFF) | (((flag << 12) | id) << 16);

        roof_fill_on(a1 - 1, a2, elevation);
        roof_fill_on(a1 + 1, a2, elevation);
        roof_fill_on(a1, a2 - 1, elevation);

        a2++;
    }
}

// 0x4B23D4
void tile_fill_roof(int a1, int a2, int elevation, int a4)
{
    if (a4) {
        roof_fill_on(a1, a2, elevation);
    } else {
        roof_fill_off(a1, a2, elevation);
    }
}

// 0x4B23DC
static void roof_fill_off(int a1, int a2, int elevation)
{
    while ((a1 >= 0 && a1 < square_width) && (a2 >= 0 && a2 < square_length)) {
        int squareTile = square_width * a2 + a1;
        int value = squares[elevation]->field_0[squareTile];
        int upper = (value >> 16) & 0xFFFF;

        int id = upper & 0xFFF;
        if (art_id(OBJ_TYPE_TILE, id, 0, 0, 0) == art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
            break;
        }

        int flag = (upper & 0xF000) >> 12;
        if ((flag & 0x03) != 0) {
            break;
        }

        flag |= 0x01;

        squares[elevation]->field_0[squareTile] = (value & 0xFFFF) | (((flag << 12) | id) << 16);

        roof_fill_off(a1 - 1, a2, elevation);
        roof_fill_off(a1 + 1, a2, elevation);
        roof_fill_off(a1, a2 - 1, elevation);

        a2++;
    }
}

// 0x4B24E0
static void roof_draw(int fid, int x, int y, Rect* rect, int light)
{
    CacheEntry* tileFrmHandle;
    Art* tileFrm = art_ptr_lock(fid, &tileFrmHandle);
    if (tileFrm == NULL) {
        return;
    }

    int tileWidth = art_frame_width(tileFrm, 0, 0);
    int tileHeight = art_frame_length(tileFrm, 0, 0);
    int scaledW = tileWidth * tile_scale;
    int scaledH = tileHeight * tile_scale;

    // Full unclipped dest rect for stable proportional mapping.
    Rect fullDstRect;
    fullDstRect.ulx = x;
    fullDstRect.uly = y;
    fullDstRect.lrx = x + scaledW - 1;
    fullDstRect.lry = y + scaledH - 1;

    // Clip against the dirty rect.
    Rect visibleRect;
    if (rect_inside_bound(&fullDstRect, rect, &visibleRect) == 0) {
        unsigned char* tileFrmBuffer = art_frame_data(tileFrm, 0, 0);

        CacheEntry* eggFrmHandle;
        Art* eggFrm = art_ptr_lock(obj_egg->fid, &eggFrmHandle);
        if (eggFrm != NULL && egg_mask_raw != NULL) {
            int eggWidth = art_frame_width(eggFrm, 0, 0);
            int eggHeight = art_frame_length(eggFrm, 0, 0);
            int eggScaledW = eggWidth * tile_scale;
            int eggScaledH = eggHeight * tile_scale;

            int eggScreenX;
            int eggScreenY;
            tile_coord(obj_egg->tile, &eggScreenX, &eggScreenY, obj_egg->elevation);

            eggScreenX += half_hex_w;
            eggScreenY += half_hex_h;

            eggScreenX += eggFrm->xOffsets[0] * tile_scale;
            eggScreenY += eggFrm->yOffsets[0] * tile_scale;

            eggScreenX += obj_egg->x * tile_scale;
            eggScreenY += obj_egg->y * tile_scale;

            Rect eggRect;
            eggRect.ulx = eggScreenX - eggScaledW / 2;
            eggRect.uly = eggScreenY - eggScaledH + 1;
            eggRect.lrx = eggRect.ulx + eggScaledW - 1;
            eggRect.lry = eggScreenY;

            obj_egg->sx = eggRect.ulx;
            obj_egg->sy = eggRect.uly;

            Rect intersectedRect;
            if (rect_inside_bound(&eggRect, &visibleRect, &intersectedRect) == 0) {
                Rect rects[4];

                rects[0].ulx = visibleRect.ulx;
                rects[0].uly = visibleRect.uly;
                rects[0].lrx = visibleRect.lrx;
                rects[0].lry = intersectedRect.uly - 1;

                rects[1].ulx = visibleRect.ulx;
                rects[1].uly = intersectedRect.uly;
                rects[1].lrx = intersectedRect.ulx - 1;
                rects[1].lry = intersectedRect.lry;

                rects[2].ulx = intersectedRect.lrx + 1;
                rects[2].uly = intersectedRect.uly;
                rects[2].lrx = visibleRect.lrx;
                rects[2].lry = intersectedRect.lry;

                rects[3].ulx = visibleRect.ulx;
                rects[3].uly = intersectedRect.lry + 1;
                rects[3].lrx = visibleRect.lrx;
                rects[3].lry = visibleRect.lry;

                for (int i = 0; i < 4; i++) {
                    Rect* cr = &(rects[i]);
                    if (cr->ulx <= cr->lrx && cr->uly <= cr->lry) {
                        dark_trans_buf_to_buf_scaled(tileFrmBuffer, tileWidth, tileHeight, tileWidth,
                            buf, &fullDstRect, cr, buf_full, light);
                    }
                }

                // Render the egg mask region for the roof tile
                // using raw 8-bit intensity weights.
                intensity_mask_buf_to_buf_scaled(
                    tileFrmBuffer, tileWidth, tileHeight, tileWidth, &fullDstRect,
                    egg_mask_raw, egg_mask_width, egg_mask_height, egg_mask_width, &eggRect,
                    &intersectedRect,
                    buf, buf_full, light);
            } else {
                dark_trans_buf_to_buf_scaled(tileFrmBuffer, tileWidth, tileHeight, tileWidth,
                    buf, &fullDstRect, &visibleRect, buf_full, light);
            }

            art_ptr_unlock(eggFrmHandle);
        }
    }

    art_ptr_unlock(tileFrmHandle);
}

// 0x4B2944
void square_render_floor(Rect* rect, int elevation)
{
    int minY;
    int maxX;
    int maxY;
    int minX;
    int temp;

    square_xy(rect->ulx, rect->uly, elevation, &temp, &minY);
    square_xy(rect->lrx, rect->uly, elevation, &minX, &temp);
    square_xy(rect->ulx, rect->lry, elevation, &maxX, &temp);
    square_xy(rect->lrx, rect->lry, elevation, &temp, &maxY);

    if (minX < 0) {
        minX = 0;
    }

    if (minX >= square_width) {
        minX = square_width - 1;
    }

    if (minY < 0) {
        minY = 0;
    }

    if (minX >= square_length) {
        minY = square_length - 1;
    }

    light_get_ambient();

    temp = square_width * minY;
    for (int v15 = minY; v15 <= maxY; v15++) {
        for (int i = minX; i <= maxX; i++) {
            int v3 = temp + i;
            int frmId = squares[elevation]->field_0[v3];
            if ((((frmId & 0xF000) >> 12) & 0x01) == 0) {
                int v12;
                int v13;
                square_coord(v3, &v12, &v13, elevation);
                int fid = art_id(OBJ_TYPE_TILE, frmId & 0xFFF, 0, 0, 0);
                floor_draw(fid, v12, v13, rect);
            }
        }
        temp += square_width;
    }
}

// 0x4B2B10
bool square_roof_intersect(int x, int y, int elevation)
{
    if (!show_roof) {
        return false;
    }

    bool result = false;

    int tileX;
    int tileY;
    square_xy_roof(x, y, elevation, &tileX, &tileY);

    TileData* ptr = squares[elevation];
    int idx = square_width * tileY + tileX;
    int upper = ptr->field_0[square_width * tileY + tileX] >> 16;
    int fid = art_id(OBJ_TYPE_TILE, upper & 0xFFF, 0, 0, 0);
    if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
        if ((((upper & 0xF000) >> 12) & 1) == 0) {
            int fid = art_id(OBJ_TYPE_TILE, upper & 0xFFF, 0, 0, 0);
            CacheEntry* handle;
            Art* art = art_ptr_lock(fid, &handle);
            if (art != NULL) {
                unsigned char* data = art_frame_data(art, 0, 0);
                if (data != NULL) {
                    int v18;
                    int v17;
                    square_coord_roof(idx, &v18, &v17, elevation);

                    int width = art_frame_width(art, 0, 0);
                    // Map screen-space offset back to source art coordinates.
                    int srcPixelX = (x - v18) / tile_scale;
                    int srcPixelY = (y - v17) / tile_scale;
                    if (srcPixelX >= 0 && srcPixelX < width
                        && srcPixelY >= 0 && srcPixelY < art_frame_length(art, 0, 0)
                        && ((uint32_t*)data)[width * srcPixelY + srcPixelX] != 0) {
                        result = true;
                    }
                }
                art_ptr_unlock(handle);
            }
        }
    }

    return result;
}

// NOTE: Unused.
//
#ifdef ENABLE_TILE_GRID_DEBUG
// 0x4B2E60
void grid_toggle()
{
    show_grid = 1 - show_grid;
}

// NOTE: Unused.
//
// 0x4B2E78
void grid_on()
{
    show_grid = 1;
}

// NOTE: Unused.
//
// 0x4B2E84
void grid_off()
{
    show_grid = 0;
}

// 0x4B2E90
int get_grid_flag()
{
    return show_grid;
}

// 0x4B2E98
void grid_render(Rect* rect, int elevation)
{
    if (!show_grid) {
        return;
    }

    for (int y = rect->uly - 12; y < rect->lry + 12; y += 6) {
        for (int x = rect->ulx - 32; x < rect->lrx + 32; x += 16) {
            int tile = tile_num(x, y, elevation);
            draw_grid(tile, elevation, rect);
        }
    }
}

// 0x4B2EF0
void grid_draw(int tile, int elevation)
{
    Rect rect;

    tile_coord(tile, &(rect.ulx), &(rect.uly), elevation);

    rect.lrx = rect.ulx + 32 - 1;
    rect.lry = rect.uly + 16 - 1;
    if (rect_inside_bound(&rect, &buf_rect, &rect) != -1) {
        draw_grid(tile, elevation, &rect);
        blit(&rect);
    }
}

// 0x4B2F4C
void draw_grid(int tile, int elevation, Rect* rect)
{
    if (tile == -1) {
        return;
    }

    int x;
    int y;
    tile_coord(tile, &x, &y, elevation);

    Rect r;
    r.ulx = x;
    r.uly = y;
    r.lrx = x + 32 - 1;
    r.lry = y + 16 - 1;

    if (rect_inside_bound(&r, rect, &r) == -1) {
        return;
    }

    if (obj_blocking_at(NULL, tile, elevation) != NULL) {
        trans_buf_to_buf(tile_grid_blocked + (32 * (r.uly - y) + (r.ulx - x)) * 4,
            r.lrx - r.ulx + 1,
            r.lry - r.uly + 1,
            32,
            buf + (buf_full * r.uly + r.ulx) * 4,
            buf_full);
        return;
    }

    if (obj_occupied(tile, elevation)) {
        trans_buf_to_buf(tile_grid_occupied + (32 * (r.uly - y) + (r.ulx - x)) * 4,
            r.lrx - r.ulx + 1,
            r.lry - r.uly + 1,
            32,
            buf + (buf_full * r.uly + r.ulx) * 4,
            buf_full);
        return;
    }

    translucent_trans_buf_to_buf(tile_grid_occupied + (32 * (r.uly - y) + (r.ulx - x)) * 4,
        r.lrx - r.ulx + 1,
        r.lry - r.uly + 1,
        32,
        buf + (buf_full * r.uly + r.ulx) * 4,
        0,
        0,
        buf_full,
        wallBlendTable,
        commonGrayTable);
}
#endif

// 0x4B30C4
void floor_draw(int fid, int x, int y, Rect* rect)
{
    if (art_get_disable(FID_TYPE(fid)) != 0) {
        return;
    }

    CacheEntry* cacheEntry;
    Art* art = art_ptr_lock(fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int elev = map_elevation;
    int clipLeft = rect->ulx;
    int clipTop = rect->uly;
    int clipWidth = rect->lrx - rect->ulx + 1;
    int clipHeight = rect->lry - rect->uly + 1;
    int frameWidth;
    int frameHeight;
    int srcOffsetX;
    int srcOffsetY;
    int drawWidth;
    int drawHeight;

    int savedX = x;
    int savedY = y;

    if (clipLeft < 0) {
        clipLeft = 0;
    }

    if (clipTop < 0) {
        clipTop = 0;
    }

    if (clipLeft + clipWidth > buf_width) {
        clipWidth = buf_width - clipLeft;
    }

    if (clipTop + clipHeight > buf_length) {
        clipHeight = buf_length - clipTop;
    }

    if (x >= buf_width || x > rect->lrx || y >= buf_length || y > rect->lry) { goto out; }

    frameWidth = art_frame_width(art, 0, 0);
    frameHeight = art_frame_length(art, 0, 0);

    // Scaled tile dimensions on screen.
    int scaledWidth = frameWidth * tile_scale;
    int scaledHeight = frameHeight * tile_scale;

    // Clip horizontally against the scaled tile footprint.
    if (clipLeft < x) {
        srcOffsetX = 0;
        int clipRight = clipLeft + clipWidth;
        drawWidth = scaledWidth + x <= clipRight ? scaledWidth : clipRight - x;
    } else {
        srcOffsetX = clipLeft - x;
        x = clipLeft;
        drawWidth = scaledWidth - srcOffsetX;
        if (drawWidth > clipWidth) {
            drawWidth = clipWidth;
        }
    }

    // Clip vertically against the scaled tile footprint.
    if (clipTop < y) {
        int clipBottom = clipHeight + clipTop;
        srcOffsetY = 0;
        drawHeight = scaledHeight + y <= clipBottom ? scaledHeight : clipBottom - y;
    } else {
        srcOffsetY = clipTop - y;
        y = clipTop;
        drawHeight = scaledHeight - srcOffsetY;
        if (drawHeight > clipHeight) {
            drawHeight = clipHeight;
        }
    }

    if (drawWidth <= 0 || drawHeight <= 0) goto out;

    // Convert scaled pixel offsets to source pixel offsets.
    int srcStartX = srcOffsetX / tile_scale;
    int srcStartY = srcOffsetY / tile_scale;
    // How many full source pixels fit in the visible draw area.
    int srcDrawW = (drawWidth + tile_scale - 1) / tile_scale;
    int srcDrawH = (drawHeight + tile_scale - 1) / tile_scale;
    // Clamp to source frame bounds.
    if (srcStartX + srcDrawW > frameWidth) srcDrawW = frameWidth - srcStartX;
    if (srcStartY + srcDrawH > frameHeight) srcDrawH = frameHeight - srcStartY;

    // Determine the hex tile at the center of this floor tile
    // (offset by 13 scaled pixels down to land on the tile center).
    int centerTile = tile_num(savedX, savedY + 13 * tile_scale, map_elevation);
    if (centerTile != -1) {
        // Sample light intensity at each vertex of the tile's lighting mesh.
        int ambientLight = light_get_ambient();
        for (int i = centerTile & 1; i < 10; i++) {
            // NOTE: calling light_get_tile two times, probably a result of using __min kind macro
            int tileLight = light_get_tile(elev, centerTile + verticies[i].field_4);
            if (tileLight <= ambientLight) {
                tileLight = ambientLight;
            }

            verticies[i].field_C = tileLight;
        }

        // Check if all 10 vertices share the same light intensity.
        int uniformCount = 0;
        for (int i = 0; i < 9; i++) {
            if (verticies[i + 1].field_C != verticies[i].field_C) {
                break;
            }

            uniformCount++;
        }

        // Fast path: uniform lighting across the entire tile.
        // At scale=1, use the original fast blit. At scale>1, fall through to scaled blit.
        if (uniformCount == 9 && tile_scale == 1) {
            unsigned char* frameData = art_frame_data(art, 0, 0);
            dark_trans_buf_to_buf(frameData + (frameWidth * srcStartY + srcStartX) * 4, srcDrawW, srcDrawH, frameWidth, buf, x, y, buf_full, verticies[0].field_C);
            goto out;
        }

        // Slow path: interpolate light across triangles into intensity_map.
        // Each floor tile (80x41 pixels) is subdivided into 10 triangles
        // (5 rightside-up + 5 upside-down). For each triangle, we linearly
        // interpolate the light intensity across its scanlines.

        // Process the 5 rightside-up triangles.
        for (int i = 0; i < 5; i++) {
            STRUCT_51DB0C* tri = &(rightside_up_triangles[i]);
            int rowIntensity = verticies[tri->field_8].field_C;
            int mapOffset = verticies[tri->field_8].field_0;
            int horizDelta = verticies[tri->field_4].field_C - verticies[tri->field_0].field_C;
            // TODO: Probably wrong.
            int horizStep = horizDelta / 32;
            int vertStep = (verticies[tri->field_0].field_C - rowIntensity) / 13;
            int* intensityPtr = &(intensity_map[mapOffset]);
            if (horizStep != 0) {
                if (vertStep != 0) {
                    for (int row = 0; row < 13; row++) {
                        int scanIntensity = rowIntensity;
                        int spanWidth = rightside_up_table[row].field_4;
                        intensityPtr += rightside_up_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = scanIntensity;
                            scanIntensity += horizStep;
                        }
                        rowIntensity += vertStep;
                    }
                } else {
                    for (int row = 0; row < 13; row++) {
                        int scanIntensity = rowIntensity;
                        int spanWidth = rightside_up_table[row].field_4;
                        intensityPtr += rightside_up_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = scanIntensity;
                            scanIntensity += horizStep;
                        }
                    }
                }
            } else {
                if (vertStep != 0) {
                    for (int row = 0; row < 13; row++) {
                        int spanWidth = rightside_up_table[row].field_4;
                        intensityPtr += rightside_up_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = rowIntensity;
                        }
                        rowIntensity += vertStep;
                    }
                } else {
                    for (int row = 0; row < 13; row++) {
                        int spanWidth = rightside_up_table[row].field_4;
                        intensityPtr += rightside_up_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = rowIntensity;
                        }
                    }
                }
            }
        }

        // Process the 5 upside-down triangles.
        for (int i = 0; i < 5; i++) {
            STRUCT_51DB48* tri = &(upside_down_triangles[i]);
            int rowIntensity = verticies[tri->field_0].field_C;
            int mapOffset = verticies[tri->field_0].field_0;
            int horizDelta = verticies[tri->field_8].field_C - rowIntensity;
            // TODO: Probably wrong.
            int horizStep = horizDelta / 32;
            int vertStep = (verticies[tri->field_4].field_C - rowIntensity) / 13;
            int* intensityPtr = &(intensity_map[mapOffset]);
            if (horizStep != 0) {
                if (vertStep != 0) {
                    for (int row = 0; row < 13; row++) {
                        int scanIntensity = rowIntensity;
                        int spanWidth = upside_down_table[row].field_4;
                        intensityPtr += upside_down_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = scanIntensity;
                            scanIntensity += horizStep;
                        }
                        rowIntensity += vertStep;
                    }
                } else {
                    for (int row = 0; row < 13; row++) {
                        int scanIntensity = rowIntensity;
                        int spanWidth = upside_down_table[row].field_4;
                        intensityPtr += upside_down_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = scanIntensity;
                            scanIntensity += horizStep;
                        }
                    }
                }
            } else {
                if (vertStep != 0) {
                    for (int row = 0; row < 13; row++) {
                        int spanWidth = upside_down_table[row].field_4;
                        intensityPtr += upside_down_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = rowIntensity;
                        }
                        rowIntensity += vertStep;
                    }
                } else {
                    for (int row = 0; row < 13; row++) {
                        int spanWidth = upside_down_table[row].field_4;
                        intensityPtr += upside_down_table[row].field_0;
                        for (int col = 0; col < spanWidth; col++) {
                            *intensityPtr++ = rowIntensity;
                        }
                    }
                }
            }
        }

        // Blit the clipped tile with per-pixel lighting from intensity_map.
        // Each source pixel is drawn as a tile_scale × tile_scale block.
        uint32_t* srcBase = (uint32_t*)(art_frame_data(art, 0, 0));
        int lightStride = 80; // intensity_map width (original tile resolution)

        for (int srcRow = 0; srcRow < srcDrawH; srcRow++) {
            uint32_t* srcRowPtr = srcBase + (srcStartY + srcRow) * frameWidth + srcStartX;
            int* lightRowPtr = &(intensity_map[160 + lightStride * (srcStartY + srcRow)]) + srcStartX;

            for (int srcCol = 0; srcCol < srcDrawW; srcCol++) {
                uint32_t pixel = srcRowPtr[srcCol];
                if (pixel != 0) {
                    int lm = lightRowPtr[srcCol] >> 9;
                    unsigned char r = pixel & 0xFF;
                    unsigned char g = (pixel >> 8) & 0xFF;
                    unsigned char b = (pixel >> 16) & 0xFF;
                    if (lm <= 127) {
                        int f = lm * 512;
                        r = (r * f) >> 16;
                        g = (g * f) >> 16;
                        b = (b * f) >> 16;
                    } else {
                        int f = (lm - 128) * 512;
                        r = r + (((255 - r) * f) >> 16);
                        g = g + (((255 - g) * f) >> 16);
                        b = b + (((255 - b) * f) >> 16);
                    }
                    uint32_t lit_pixel = (0xFFu << 24) | (b << 16) | (g << 8) | r;

                    // Write tile_scale × tile_scale block into destination.
                    int destBaseX = srcCol * tile_scale - srcOffsetX % tile_scale;
                    int destBaseY = srcRow * tile_scale - srcOffsetY % tile_scale;
                    for (int dy = 0; dy < tile_scale; dy++) {
                        int destY = y + destBaseY + dy;
                        if (destY < clipTop || destY >= clipTop + clipHeight) continue;
                        for (int dx = 0; dx < tile_scale; dx++) {
                            int destX = x + destBaseX + dx;
                            if (destX < clipLeft || destX >= clipLeft + clipWidth) continue;
                            uint32_t* destPtr = (uint32_t*)(buf + (buf_full * destY + destX) * 4);
                            *destPtr = lit_pixel;
                        }
                    }
                }
            }
        }
    }

out:
    art_ptr_unlock(cacheEntry);
}

// 0x4B372C
int tile_make_line(int from, int to, int* tiles, int tilesCapacity)
{
    if (tilesCapacity <= 1) {
        return 0;
    }

    int count = 0;

    int fromX;
    int fromY;
    tile_coord(from, &fromX, &fromY, map_elevation);
    fromX += 16;
    fromY += 8;

    int toX;
    int toY;
    tile_coord(to, &toX, &toY, map_elevation);
    toX += 16;
    toY += 8;

    tiles[count++] = from;

    int stepX;
    int deltaX = toX - fromX;
    if (deltaX > 0)
        stepX = 1;
    else if (deltaX < 0)
        stepX = -1;
    else
        stepX = 0;

    int stepY;
    int deltaY = toY - fromY;
    if (deltaY > 0)
        stepY = 1;
    else if (deltaY < 0)
        stepY = -1;
    else
        stepY = 0;

    int v28 = 2 * abs(toX - fromX);
    int v27 = 2 * abs(toY - fromY);

    int tileX = fromX;
    int tileY = fromY;

    if (v28 <= v27) {
        int middleX = v28 - v27 / 2;
        while (true) {
            int tile = tile_num(tileX, tileY, map_elevation);
            tiles[count] = tile;

            if (tile == to) {
                count++;
                break;
            }

            if (tile != tiles[count - 1] && (count == 1 || tile != tiles[count - 2])) {
                count++;
                if (count == tilesCapacity) {
                    break;
                }
            }

            if (tileY == toY) {
                break;
            }

            if (middleX >= 0) {
                tileX += stepX;
                middleX -= v27;
            }

            middleX += v28;
            tileY += stepY;
        }
    } else {
        int middleY = v27 - v28 / 2;
        while (true) {
            int tile = tile_num(tileX, tileY, map_elevation);
            tiles[count] = tile;

            if (tile == to) {
                count++;
                break;
            }

            if (tile != tiles[count - 1] && (count == 1 || tile != tiles[count - 2])) {
                count++;
                if (count == tilesCapacity) {
                    break;
                }
            }

            if (tileX == toX) {
                return count;
            }

            if (middleY >= 0) {
                tileY += stepY;
                middleY -= v28;
            }

            middleY += v27;
            tileX += stepX;
        }
    }

    return count;
}

// 0x4B3924
int tile_scroll_to(int tile, int flags)
{
    if (tile == tile_center_tile) {
        return -1;
    }

    int oldCenterTile = tile_center_tile;

    int v9[200];
    int count = tile_make_line(tile_center_tile, tile, v9, 200);
    if (count == 0) {
        return -1;
    }

    int index = 1;
    for (; index < count; index++) {
        if (tile_set_center(v9[index], 0) == -1) {
            break;
        }
    }

    int rc = 0;
    if ((flags & 0x01) != 0) {
        if (index != count) {
            tile_set_center(oldCenterTile, 0);
            rc = -1;
        }
    }

    if ((flags & 0x02) != 0) {
        // NOTE: Uninline.
        tile_refresh_display();
    }

    return rc;
}

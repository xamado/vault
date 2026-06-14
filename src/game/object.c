#include "game/object.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "game/anim.h"
#include "game/art.h"
#include "plib/color/color.h"
#include "game/combat.h"
#include "plib/gnw/input.h"
#include "game/critter.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/grbuf.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gmouse.h"
#include "game/item.h"
#include "game/light.h"
#include "game/map.h"
#include "plib/gnw/memory.h"
#include "game/party.h"
#include "game/proto.h"
#include "game/protinst.h"
#include "game/scripts.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/worldmap.h"
#include "plib/gnw/svga.h"

// 32-bit RGBA translucency tint colors.
// Tint colors derived from original RGB555 colorTable blend indices:
//   red   (31744/0x7C00) -> pure red
//   wall  (25439/0x635F) -> blue-ish
//   glass (10239/0x27FF) -> cyan-green
//   steam (32767/0x7FFF) -> white
//   energy(30689/0x77E1) -> yellow-green
//
// Tint color format: 0xAABBGGRR
#define TINT_RED    0xFF0000FFu  // pure red
#define TINT_WALL   0xFFFF9E63u  // blue/steel (B:255 G:158 R:99)
#define TINT_GLASS  0xFFFFFF84u  // cyan-green (B:255 G:255 R:132)
#define TINT_STEAM  0xFFFFFFFFu  // white (no tint, just alpha)
#define TINT_ENERGY 0xFF08FFEFu  // yellow-green (B:8 G:255 R:239)

static int obj_read_obj(Object* obj, File* stream);
static int obj_load_func(File* stream);
static void obj_fix_combat_cid_for_dude();
static void object_fix_weapon_ammo(Object* obj);
static int obj_write_obj(Object* obj, File* stream);
static int obj_offset_table_init();
static void obj_offset_table_exit();
static int obj_order_table_init();
static int obj_order_comp_func_even(const void* a1, const void* a2);
static int obj_order_comp_func_odd(const void* a1, const void* a2);
static void obj_order_table_exit();
static int obj_render_table_init();
static void obj_render_table_exit();
static void obj_light_table_init();
static void obj_blend_table_init();
static void obj_blend_table_exit();
static int obj_create_object(Object** objectPtr);
static void obj_destroy_object(Object** objectPtr);
static int obj_create_object_node(ObjectListNode** nodePtr);
static void obj_destroy_object_node(ObjectListNode** nodePtr);
static int obj_node_ptr(Object* obj, ObjectListNode** out_node, ObjectListNode** out_prev_node);
static void obj_insert(ObjectListNode* ptr);
static int obj_remove(ObjectListNode* a1, ObjectListNode* a2);
static int obj_connect_to_tile(ObjectListNode* node, int tile_index, int elev, Rect* rect);
static int obj_adjust_light(Object* obj, int a2, Rect* rect);
static void obj_render_outline(Object* object, Rect* rect);
static void obj_render_object(Object* object, Rect* rect, int light);
static int obj_preload_sort(const void* a1, const void* a2);

// 0x5195F8
static bool objInitialized = false;

// 0x5195FC
static int updateHexWidth = 0;

// 0x519600
static int updateHexHeight = 0;

// 0x519604
static int updateHexArea = 0;

// 0x519608
static int* orderTable[2] = {
    NULL,
    NULL,
};

// 0x519610
static int* offsetTable[2] = {
    NULL,
    NULL,
};

// 0x519618
static int* offsetDivTable = NULL;

// 0x51961C
static int* offsetModTable = NULL;

// 0x519620
static ObjectListNode** renderTable = NULL;

// 0x519624
static int outlineCount = 0;

// Contains objects that are not bounded to tiles.
//
// 0x519628
static ObjectListNode* floatingObjects = NULL;

// 0x51962C
static int centerToUpperLeft = 0;

// 0x519630
static int find_elev = 0;

// 0x519634
static int find_tile = 0;

// 0x519638
static ObjectListNode* find_ptr = NULL;

// 0x51963C
static int* preload_list = NULL;

// 0x519640
static int preload_list_index = 0;

// 0x51964C
static Rect light_rect[9] = {
    { 0, 0, 96, 42 },
    { 0, 0, 160, 74 },
    { 0, 0, 224, 106 },
    { 0, 0, 288, 138 },
    { 0, 0, 352, 170 },
    { 0, 0, 416, 202 },
    { 0, 0, 480, 234 },
    { 0, 0, 544, 266 },
    { 0, 0, 608, 298 },
};

// 0x5196DC
static int light_distance[36] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    3,
    4,
    5,
    6,
    7,
    8,
    4,
    5,
    6,
    7,
    8,
    5,
    6,
    7,
    8,
    6,
    7,
    8,
    7,
    8,
    8,
};

// 0x51976C
static int fix_violence_level = -1;

// 0x519770
static int obj_last_roof_x = -1;

// 0x519774
static int obj_last_roof_y = -1;

// 0x519778
static int obj_last_elev = -1;

// 0x51977C
static bool obj_last_is_empty = true;

// 0x519780
unsigned char* wallBlendTable = NULL;

// 0x519784
unsigned char* glassBlendTable = NULL;

// 0x519788
unsigned char* steamBlendTable = NULL;

// 0x51978C
unsigned char* energyBlendTable = NULL;

// 0x519790
unsigned char* redBlendTable = NULL;

// 0x519794
Object* moveBlockObj = NULL;

// 0x519798
static int objItemOutlineState = 0;

// 0x6391D0
static int light_blocked[6][36];

// 0x639530
static int light_offsets[2][6][36];

// 0x639BF0
static Rect buf_rect;

// 0x639C00
static Object* outlinedObjects[100];

// 0x639D90
static Rect updateAreaPixelBounds;

// Contains objects that are bounded to tiles.
//
// 0x639DA0
static ObjectListNode* objectTable[HEX_GRID_SIZE];

// 0x660EA0
unsigned char glassGrayTable[256];

// 0x660FA0
unsigned char commonGrayTable[256];

// 0x6610A0
static int buf_size;

// 0x6610A4
static unsigned char* back_buf;

// 0x6610A8
static int buf_length;

// Translucent "egg" effect around player.
//
// 0x6610AC
Object* obj_egg;

// 0x6610B0
static int buf_full;

// 0x6610B4
static int buf_width;

// 0x6610B8
Object* obj_dude;

// 0x6610BC
static char obj_seen_check[5001];

// 0x662445
static char obj_seen[5001];

// obj_init
// 0x488780
int obj_init(unsigned char* buf, int width, int height, int pitch)
{
    int dudeFid;
    int eggFid;

    memset(obj_seen, 0, 5001);
    updateAreaPixelBounds.lrx = width + 320;
    updateAreaPixelBounds.ulx = -320;
    updateAreaPixelBounds.lry = height + 240;
    updateAreaPixelBounds.uly = -240;

    updateHexWidth = (updateAreaPixelBounds.lrx + 320 + 1) / 32 + 1;
    updateHexHeight = (updateAreaPixelBounds.lry + 240 + 1) / 12 + 1;
    updateHexArea = updateHexWidth * updateHexHeight;

    memset(objectTable, 0, sizeof(objectTable));

    if (obj_offset_table_init() == -1) {
        return -1;
    }

    if (obj_order_table_init() == -1) {
        goto err;
    }

    if (obj_render_table_init() == -1) {
        goto err_2;
    }

    if (light_init() == -1) {
        goto err_2;
    }

    if (text_object_init(buf, width, height) == -1) {
        goto err_2;
    }

    obj_light_table_init();
    obj_blend_table_init();

    centerToUpperLeft = tile_num(updateAreaPixelBounds.ulx, updateAreaPixelBounds.uly, 0) - tile_center_tile;
    buf_width = width;
    buf_length = height;
    back_buf = buf;

    buf_rect.ulx = 0;
    buf_rect.uly = 0;
    buf_rect.lrx = width - 1;
    buf_rect.lry = height - 1;

    buf_size = height * width;
    buf_full = pitch;

    dudeFid = art_id(OBJ_TYPE_CRITTER, art_vault_guy_num, 0, 0, 0);
    obj_new(&obj_dude, dudeFid, 0x1000000);

    obj_dude->flags |= OBJECT_FLAG_0x400;
    obj_dude->flags |= OBJECT_TEMPORARY;
    obj_dude->flags |= OBJECT_HIDDEN;
    obj_dude->flags |= OBJECT_LIGHT_THRU;
    obj_set_light(obj_dude, 4, 0x10000, NULL);

    if (partyMemberAdd(obj_dude) == -1) {
        debug_printf("\n  Error: Can't add Player into party!");
        exit(1);
    }

    eggFid = art_id(OBJ_TYPE_INTERFACE, 2, 0, 0, 0);
    obj_new(&obj_egg, eggFid, -1);
    obj_egg->flags |= OBJECT_FLAG_0x400;
    obj_egg->flags |= OBJECT_TEMPORARY;
    obj_egg->flags |= OBJECT_HIDDEN;
    obj_egg->flags |= OBJECT_LIGHT_THRU;

    objInitialized = true;

    return 0;

err_2:

    // NOTE: Uninline.
    obj_order_table_exit();

err:

    obj_offset_table_exit();

    return -1;
}

// 0x488A00
void obj_reset()
{
    if (objInitialized) {
        text_object_reset();
        obj_remove_all();
        memset(obj_seen, 0, 5001);
        light_reset();
    }
}

// 0x488A30
void obj_exit()
{
    if (objInitialized) {
        obj_dude->flags &= ~OBJECT_FLAG_0x400;
        obj_egg->flags &= ~OBJECT_FLAG_0x400;

        obj_remove_all();
        text_object_exit();

        // NOTE: Uninline.
        obj_blend_table_exit();

        light_exit();

        // NOTE: Uninline.
        obj_render_table_exit();

        // NOTE: Uninline.
        obj_order_table_exit();

        obj_offset_table_exit();
    }
}

// 0x488AF4
static int obj_read_obj(Object* obj, File* stream)
{
    int field_74;

    if (db_freadInt(stream, &(obj->id)) == -1) return -1;
    if (db_freadInt(stream, &(obj->tile)) == -1) return -1;
    if (db_freadInt(stream, &(obj->x)) == -1) return -1;
    if (db_freadInt(stream, &(obj->y)) == -1) return -1;
    if (db_freadInt(stream, &(obj->sx)) == -1) return -1;
    if (db_freadInt(stream, &(obj->sy)) == -1) return -1;
    if (db_freadInt(stream, &(obj->frame)) == -1) return -1;
    if (db_freadInt(stream, &(obj->rotation)) == -1) return -1;
    if (db_freadInt(stream, &(obj->fid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->flags)) == -1) return -1;
    if (db_freadInt(stream, &(obj->elevation)) == -1) return -1;
    if (db_freadInt(stream, &(obj->pid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->cid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->lightDistance)) == -1) return -1;
    if (db_freadInt(stream, &(obj->lightIntensity)) == -1) return -1;
    if (db_freadInt(stream, &field_74) == -1) return -1;
    if (db_freadInt(stream, &(obj->sid)) == -1) return -1;
    if (db_freadInt(stream, &(obj->field_80)) == -1) return -1;

    obj->outline = 0;
    obj->owner = NULL;

    if (proto_read_protoUpdateData(obj, stream) != 0) {
        return -1;
    }

    if (obj->pid < 0x5000010 || obj->pid > 0x5000017) {
        if (PID_TYPE(obj->pid) == 0 && !(map_data.flags & 0x01)) {
            object_fix_weapon_ammo(obj);
        }
    } else {
        if (obj->data.misc.map <= 0) {
            if ((obj->fid & 0xFFF) < 33) {
                obj->fid = art_id(OBJ_TYPE_MISC, (obj->fid & 0xFFF) + 16, FID_ANIM_TYPE(obj->fid), 0, 0);
            }
        }
    }

    return 0;
}

// 0x488CE4
int obj_load(File* stream)
{
    int rc = obj_load_func(stream);

    fix_violence_level = -1;

    return rc;
}

// 0x488CF8
static int obj_load_func(File* stream)
{
    if (stream == NULL) {
        return -1;
    }

    if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &fix_violence_level)) {
        fix_violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    }

    int objectCount;
    if (db_freadInt(stream, &objectCount) == -1) {
        return -1;
    }

    if (preload_list != NULL) {
        mem_free(preload_list);
    }

    if (objectCount != 0) {
        preload_list = (int*)mem_malloc(sizeof(*preload_list) * objectCount);
        memset(preload_list, 0, sizeof(*preload_list) * objectCount);
        if (preload_list == NULL) {
            return -1;
        }
        preload_list_index = 0;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation;
        if (db_freadInt(stream, &objectCountAtElevation) == -1) {
            return -1;
        }

        for (int objectIndex = 0; objectIndex < objectCountAtElevation; objectIndex++) {
            ObjectListNode* objectListNode;

            // NOTE: Uninline.
            if (obj_create_object_node(&objectListNode) == -1) {
                return -1;
            }

            if (obj_create_object(&(objectListNode->obj)) == -1) {
                // NOTE: Uninline.
                obj_destroy_object_node(&objectListNode);
                return -1;
            }

            if (obj_read_obj(objectListNode->obj, stream) != 0) {
                // NOTE: Uninline.
                obj_destroy_object(&(objectListNode->obj));

                // NOTE: Uninline.
                obj_destroy_object_node(&objectListNode);

                return -1;
            }

            objectListNode->obj->outline = 0;
            preload_list[preload_list_index++] = objectListNode->obj->fid;

            if (objectListNode->obj->sid != -1) {
                Script* script;
                if (scr_ptr(objectListNode->obj->sid, &script) == -1) {
                    objectListNode->obj->sid = -1;
                    debug_printf("\nError connecting object to script!");
                } else {
                    script->owner = objectListNode->obj;
                    objectListNode->obj->field_80 = script->field_14;
                }
            }

            obj_fix_violence_settings(&(objectListNode->obj->fid));
            objectListNode->obj->elevation = elevation;

            obj_insert(objectListNode);

            if ((objectListNode->obj->flags & OBJECT_FLAG_0x400) && PID_TYPE(objectListNode->obj->pid) == OBJ_TYPE_CRITTER && objectListNode->obj->pid != 18000) {
                objectListNode->obj->flags &= ~OBJECT_FLAG_0x400;
            }

            Inventory* inventory = &(objectListNode->obj->data.inventory);
            if (inventory->length != 0) {
                inventory->items = (InventoryItem*)mem_malloc(sizeof(InventoryItem) * inventory->capacity);
                if (inventory->items == NULL) {
                    return -1;
                }

                for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
                    InventoryItem* inventoryItem = &(inventory->items[inventoryItemIndex]);
                    if (db_freadInt(stream, &(inventoryItem->quantity)) != 0) {
                        debug_printf("Error loading inventory\n");
                        return -1;
                    }

                    if (obj_load_obj(stream, &(inventoryItem->item), elevation, objectListNode->obj) != 0) {
                        return -1;
                    }
                }
            } else {
                inventory->capacity = 0;
                inventory->items = NULL;
            }
        }
    }

    obj_rebuild_all_light();

    return 0;
}

// 0x48909C
static void obj_fix_combat_cid_for_dude()
{
    Object** critterList;
    int critterListLength = obj_create_list(-1, map_elevation, OBJ_TYPE_CRITTER, &critterList);

    if (obj_dude->data.critter.combat.whoHitMeCid == -1) {
        obj_dude->data.critter.combat.whoHitMe = NULL;
    } else {
        int index = find_cid(0, obj_dude->data.critter.combat.whoHitMeCid, critterList, critterListLength);
        if (index != critterListLength) {
            obj_dude->data.critter.combat.whoHitMe = critterList[index];
        } else {
            obj_dude->data.critter.combat.whoHitMe = NULL;
        }
    }

    if (critterListLength != 0) {
        // NOTE: Uninline.
        obj_delete_list(critterList);
    }
}

// Fixes ammo pid and number of charges.
//
// 0x48911C
static void object_fix_weapon_ammo(Object* obj)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_ITEM) {
        return;
    }

    Proto* proto;
    if (proto_ptr(obj->pid, &proto) == -1) {
        debug_printf("\nError: obj_load: proto_ptr failed on pid");
        exit(1);
    }

    int charges;
    if (item_get_type(obj) == ITEM_TYPE_WEAPON) {
        int ammoTypePid = obj->data.item.weapon.ammoTypePid;
        if (ammoTypePid == 0xCCCCCCCC || ammoTypePid == -1) {
            obj->data.item.weapon.ammoTypePid = proto->item.data.weapon.ammoTypePid;
        }

        charges = obj->data.item.weapon.ammoQuantity;
        if (charges == 0xCCCCCCCC || charges == -1 || charges != proto->item.data.weapon.ammoCapacity) {
            obj->data.item.weapon.ammoQuantity = proto->item.data.weapon.ammoCapacity;
        }
    } else {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_MISC) {
            // FIXME: looks like this code in unreachable
            charges = obj->data.item.misc.charges;
            if (charges == 0xCCCCCCCC) {
                charges = proto->item.data.misc.charges;
                obj->data.item.misc.charges = charges;
                if (charges == 0xCCCCCCCC) {
                    debug_printf("\nError: Misc Item Prototype %s: charges incorrect!", proto_name(obj->pid));
                    obj->data.item.misc.charges = 0;
                }
            } else {
                if (charges != proto->item.data.misc.charges) {
                    obj->data.item.misc.charges = proto->item.data.misc.charges;
                }
            }
        }
    }
}

// 0x489200
static int obj_write_obj(Object* obj, File* stream)
{
    if (db_fwriteInt(stream, obj->id) == -1) return -1;
    if (db_fwriteInt(stream, obj->tile) == -1) return -1;
    if (db_fwriteInt(stream, obj->x) == -1) return -1;
    if (db_fwriteInt(stream, obj->y) == -1) return -1;
    if (db_fwriteInt(stream, obj->sx) == -1) return -1;
    if (db_fwriteInt(stream, obj->sy) == -1) return -1;
    if (db_fwriteInt(stream, obj->frame) == -1) return -1;
    if (db_fwriteInt(stream, obj->rotation) == -1) return -1;
    if (db_fwriteInt(stream, obj->fid) == -1) return -1;
    if (db_fwriteInt(stream, obj->flags) == -1) return -1;
    if (db_fwriteInt(stream, obj->elevation) == -1) return -1;
    if (db_fwriteInt(stream, obj->pid) == -1) return -1;
    if (db_fwriteInt(stream, obj->cid) == -1) return -1;
    if (db_fwriteInt(stream, obj->lightDistance) == -1) return -1;
    if (db_fwriteInt(stream, obj->lightIntensity) == -1) return -1;
    if (db_fwriteInt(stream, obj->outline) == -1) return -1;
    if (db_fwriteInt(stream, obj->sid) == -1) return -1;
    if (db_fwriteInt(stream, obj->field_80) == -1) return -1;
    if (proto_write_protoUpdateData(obj, stream) == -1) return -1;

    return 0;
}

// 0x48935C
int obj_save(File* stream)
{
    if (stream == NULL) {
        return -1;
    }

    obj_process_seen();

    int objectCount = 0;

    long objectCountPos = db_ftell(stream);
    if (db_fwriteInt(stream, objectCount) == -1) {
        return -1;
    }

    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int objectCountAtElevation = 0;

        long objectCountAtElevationPos = db_ftell(stream);
        if (db_fwriteInt(stream, objectCountAtElevation) == -1) {
            return -1;
        }

        for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
            for (ObjectListNode* objectListNode = objectTable[tile]; objectListNode != NULL; objectListNode = objectListNode->next) {
                Object* object = objectListNode->obj;
                if (object->elevation != elevation) {
                    continue;
                }

                if ((object->flags & OBJECT_TEMPORARY) != 0) {
                    continue;
                }

                CritterCombatData* combatData = NULL;
                Object* whoHitMe = NULL;
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    combatData = &(object->data.critter.combat);
                    whoHitMe = combatData->whoHitMe;
                    if (whoHitMe != 0) {
                        if (combatData->whoHitMeCid != -1) {
                            combatData->whoHitMeCid = whoHitMe->cid;
                        }
                    } else {
                        combatData->whoHitMeCid = -1;
                    }
                }

                if (obj_write_obj(object, stream) == -1) {
                    return -1;
                }

                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    combatData->whoHitMe = whoHitMe;
                }

                Inventory* inventory = &(object->data.inventory);
                for (int index = 0; index < inventory->length; index++) {
                    InventoryItem* inventoryItem = &(inventory->items[index]);

                    if (db_fwriteInt(stream, inventoryItem->quantity) == -1) {
                        return -1;
                    }

                    if (obj_save_obj(stream, inventoryItem->item) == -1) {
                        return -1;
                    }
                }

                objectCountAtElevation++;
            }
        }

        long pos = db_ftell(stream);
        db_fseek(stream, objectCountAtElevationPos, SEEK_SET);
        db_fwriteInt(stream, objectCountAtElevation);
        db_fseek(stream, pos, SEEK_SET);

        objectCount += objectCountAtElevation;
    }

    long pos = db_ftell(stream);
    db_fseek(stream, objectCountPos, SEEK_SET);
    db_fwriteInt(stream, objectCount);
    db_fseek(stream, pos, SEEK_SET);

    return 0;
}

// 0x489550
void obj_render_pre_roof(Rect* rect, int elevation)
{
    if (!objInitialized) {
        return;
    }

    Rect updatedRect;
    if (rect_inside_bound(rect, &buf_rect, &updatedRect) != 0) {
        return;
    }

    int ambientLight = light_get_ambient();
    int minX = updatedRect.ulx - 320;
    int minY = updatedRect.uly - 240;
    int maxX = updatedRect.lrx + 320;
    int maxY = updatedRect.lry + 240;
    int topLeftTile = tile_num(minX, minY, elevation);
    int updateAreaHexWidth = (maxX - minX + 1) / 32;
    int updateAreaHexHeight = (maxY - minY + 1) / 12;

    int parity = tile_center_tile & 1;
    int* orders = orderTable[parity];
    int* offsets = offsetTable[parity];

    outlineCount = 0;

    int renderCount = 0;
    for (int i = 0; i < updateHexArea; i++) {
        int offsetIndex = *orders++;
        if (updateAreaHexHeight > offsetDivTable[offsetIndex] && updateAreaHexWidth > offsetModTable[offsetIndex]) {
            int light;

            int tileIndex = topLeftTile + offsets[offsetIndex];
            if (tileIndex < 0 || tileIndex >= HEX_GRID_SIZE) {
                continue;
            }
            ObjectListNode* objectListNode = objectTable[tileIndex];
            if (objectListNode != NULL) {
                // NOTE: calls light_get_tile two times, probably result of min/max macro
                int tileLight = light_get_tile(elevation, objectListNode->obj->tile);
                if (tileLight >= ambientLight) {
                    light = tileLight;
                } else {
                    light = ambientLight;
                }
            }

            while (objectListNode != NULL) {
                if (elevation < objectListNode->obj->elevation) {
                    break;
                }

                if (elevation == objectListNode->obj->elevation) {
                    if ((objectListNode->obj->flags & OBJECT_FLAT) == 0) {
                        break;
                    }

                    if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                        obj_render_object(objectListNode->obj, &updatedRect, light);

                        if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                            if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && outlineCount < 100) {
                                outlinedObjects[outlineCount++] = objectListNode->obj;
                            }
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }

            if (objectListNode != NULL) {
                renderTable[renderCount++] = objectListNode;
            }
        }
    }

    for (int i = 0; i < renderCount; i++) {
        int light;

        ObjectListNode* objectListNode = renderTable[i];
        if (objectListNode != NULL) {
            // NOTE: calls light_get_tile two times, probably result of min/max macro
            int tileLight = light_get_tile(elevation, objectListNode->obj->tile);
            if (tileLight >= ambientLight) {
                light = tileLight;
            } else {
                light = ambientLight;
            }
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (elevation < object->elevation) {
                break;
            }

            if (elevation == objectListNode->obj->elevation) {
                if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                    obj_render_object(object, &updatedRect, light);

                    if ((objectListNode->obj->outline & OUTLINE_TYPE_MASK) != 0) {
                        if ((objectListNode->obj->outline & OUTLINE_DISABLED) == 0 && outlineCount < 100) {
                            outlinedObjects[outlineCount++] = objectListNode->obj;
                        }
                    }
                }
            }

            objectListNode = objectListNode->next;
        }
    }
}

// 0x4897EC
void obj_render_post_roof(Rect* rect, int elevation)
{
    if (!objInitialized) {
        return;
    }

    Rect updatedRect;
    if (rect_inside_bound(rect, &buf_rect, &updatedRect) != 0) {
        return;
    }

    for (int index = 0; index < outlineCount; index++) {
        obj_render_outline(outlinedObjects[index], &updatedRect);
    }

    text_object_render(&updatedRect);

    ObjectListNode* objectListNode = floatingObjects;
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if ((object->flags & OBJECT_HIDDEN) == 0) {
            obj_render_object(object, &updatedRect, 0x10000);
        }
        objectListNode = objectListNode->next;
    }
}

// 0x489A84
int obj_new(Object** objectPtr, int fid, int pid)
{
    ObjectListNode* objectListNode;

    // NOTE: Uninline;
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    if (obj_create_object(&(objectListNode->obj)) == -1) {
        // Uninline.
        obj_destroy_object_node(&objectListNode);
        return -1;
    }

    objectListNode->obj->fid = fid;
    obj_insert(objectListNode);

    if (objectPtr) {
        *objectPtr = objectListNode->obj;
    }

    objectListNode->obj->pid = pid;
    objectListNode->obj->id = new_obj_id();

    if (pid == -1 || PID_TYPE(pid) == OBJ_TYPE_TILE) {
        Inventory* inventory = &(objectListNode->obj->data.inventory);
        inventory->length = 0;
        inventory->items = NULL;
        return 0;
    }

    proto_update_init(objectListNode->obj);

    Proto* proto = NULL;
    if (proto_ptr(pid, &proto) == -1) {
        return 0;
    }

    obj_set_light(objectListNode->obj, proto->lightDistance, proto->lightIntensity, NULL);

    if ((proto->flags & 0x08) != 0) {
        obj_toggle_flat(objectListNode->obj, NULL);
    }

    if ((proto->flags & 0x10) != 0) {
        objectListNode->obj->flags |= OBJECT_NO_BLOCK;
    }

    if ((proto->flags & 0x800) != 0) {
        objectListNode->obj->flags |= OBJECT_MULTIHEX;
    }

    if ((proto->flags & 0x8000) != 0) {
        objectListNode->obj->flags |= OBJECT_TRANS_NONE;
    } else {
        if ((proto->flags & 0x10000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_WALL;
        } else if ((proto->flags & 0x20000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_GLASS;
        } else if ((proto->flags & 0x40000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_STEAM;
        } else if ((proto->flags & 0x80000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_ENERGY;
        } else if ((proto->flags & 0x4000) != 0) {
            objectListNode->obj->flags |= OBJECT_TRANS_RED;
        }
    }

    if ((proto->flags & 0x20000000) != 0) {
        objectListNode->obj->flags |= OBJECT_LIGHT_THRU;
    }

    if ((proto->flags & 0x80000000) != 0) {
        objectListNode->obj->flags |= OBJECT_SHOOT_THRU;
    }

    if ((proto->flags & 0x10000000) != 0) {
        objectListNode->obj->flags |= OBJECT_WALL_TRANS_END;
    }

    if ((proto->flags & 0x1000) != 0) {
        objectListNode->obj->flags |= OBJECT_NO_HIGHLIGHT;
    }

    obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));

    return 0;
}

// 0x489C9C
int obj_pid_new(Object** objectPtr, int pid)
{
    Proto* proto;

    *objectPtr = NULL;

    if (proto_ptr(pid, &proto) == -1) {
        return -1;
    }

    return obj_new(objectPtr, proto->fid, pid);
}

// 0x489CCC
int obj_copy(Object** a1, Object* a2)
{
    if (a2 == NULL) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    if (obj_create_object(&(objectListNode->obj)) == -1) {
        // NOTE: Uninline.
        obj_destroy_object_node(&objectListNode);
        return -1;
    }

    clear_pupdate_data(objectListNode->obj);

    memcpy(objectListNode->obj, a2, sizeof(Object));

    if (a1 != NULL) {
        *a1 = objectListNode->obj;
    }

    obj_insert(objectListNode);

    objectListNode->obj->id = new_obj_id();

    if (objectListNode->obj->sid != -1) {
        objectListNode->obj->sid = -1;
        obj_new_sid(objectListNode->obj, &(objectListNode->obj->sid));
    }

    if (obj_set_rotation(objectListNode->obj, a2->rotation, NULL) == -1) {
        // TODO: Probably leaking object allocated with obj_create_object.
        // NOTE: Uninline.
        obj_destroy_object_node(&objectListNode);
        return -1;
    }

    objectListNode->obj->flags &= ~OBJECT_USED;

    Inventory* newInventory = &(objectListNode->obj->data.inventory);
    newInventory->length = 0;
    newInventory->capacity = 0;

    Inventory* oldInventory = &(a2->data.inventory);
    for (int index = 0; index < oldInventory->length; index++) {
        InventoryItem* oldInventoryItem = &(oldInventory->items[index]);

        Object* newItem;
        if (obj_copy(&newItem, oldInventoryItem->item) == -1) {
            // TODO: Probably leaking object allocated with obj_create_object.
            // NOTE: Uninline.
            obj_destroy_object_node(&objectListNode);
            return -1;
        }

        if (item_add_force(objectListNode->obj, newItem, oldInventoryItem->quantity) == 1) {
            // TODO: Probably leaking object allocated with obj_create_object.
            // NOTE: Uninline.
            obj_destroy_object_node(&objectListNode);
            return -1;
        }
    }

    return 0;
}

// 0x489EC4
int obj_connect(Object* object, int tile, int elevation, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* objectListNode;

    // NOTE: Uninline.
    if (obj_create_object_node(&objectListNode) == -1) {
        return -1;
    }

    objectListNode->obj = object;

    return obj_connect_to_tile(objectListNode, tile, elevation, rect);
}

// 0x489F34
int obj_disconnect(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prev_node;
    if (obj_node_ptr(obj, &node, &prev_node) != 0) {
        return -1;
    }

    if (obj_adjust_light(obj, 1, rect) == -1) {
        if (rect != NULL) {
            obj_bound(obj, rect);
        }
    }

    if (prev_node != NULL) {
        prev_node->next = node->next;
    } else {
        int tile = node->obj->tile;
        if (tile == -1) {
            floatingObjects = floatingObjects->next;
        } else {
            objectTable[tile] = objectTable[tile]->next;
        }
    }

    if (node != NULL) {
        mem_free(node);
    }

    obj->tile = -1;

    return 0;
}

// 0x489FF8
int obj_offset(Object* obj, int x, int y, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    ObjectListNode* node = NULL;
    ObjectListNode* previousNode = NULL;
    if (obj_node_ptr(obj, &node, &previousNode) == -1) {
        return -1;
    }

    if (obj == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rectCopy(rect, &eggRect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            rectOffset(&eggRect, x, y);

            obj_offset(obj_egg, x, y, NULL);
            rect_min_bound(rect, &eggRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            obj_offset(obj_egg, x, y, NULL);
        }
    } else {
        if (rect != NULL) {
            obj_bound(obj, rect);

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);

            Rect objectRect;
            rectCopy(&objectRect, rect);

            rectOffset(&objectRect, x, y);

            rect_min_bound(rect, &objectRect, rect);
        } else {
            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile == -1) {
                    floatingObjects = floatingObjects->next;
                } else {
                    objectTable[tile] = objectTable[tile]->next;
                }
            }

            obj->x += x;
            obj->sx += x;

            obj->y += y;
            obj->sy += y;

            obj_insert(node);
        }
    }

    return 0;
}

// 0x48A324
int obj_move(Object* a1, int a2, int a3, int elevation, Rect* a5)
{
    if (a1 == NULL) {
        return -1;
    }

    // TODO: Get rid of initialization.
    ObjectListNode* node = NULL;
    ObjectListNode* previousNode;
    int v22 = 0;

    int tile = a1->tile;
    if (hexGridTileIsValid(tile)) {
        if (obj_node_ptr(a1, &node, &previousNode) == -1) {
            return -1;
        }

        if (obj_adjust_light(a1, 1, a5) == -1) {
            if (a5 != NULL) {
                obj_bound(a1, a5);
            }
        }

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }

        a1->tile = -1;
        a1->elevation = elevation;
        v22 = 1;
    } else {
        if (elevation == a1->elevation) {
            if (a5 != NULL) {
                obj_bound(a1, a5);
            }
        } else {
            if (obj_node_ptr(a1, &node, &previousNode) == -1) {
                return -1;
            }

            if (a5 != NULL) {
                obj_bound(a1, a5);
            }

            if (previousNode != NULL) {
                previousNode->next = node->next;
            } else {
                int tile = node->obj->tile;
                if (tile != -1) {
                    objectTable[tile] = objectTable[tile]->next;
                } else {
                    floatingObjects = floatingObjects->next;
                }
            }

            a1->elevation = elevation;
            v22 = 1;
        }
    }

    CacheEntry* cacheHandle;
    int width;
    int height;
    Art* art = art_ptr_lock(a1->fid, &cacheHandle);
    if (art != NULL) {
        art_frame_width_length(art, a1->frame, a1->rotation, &width, &height);
        a1->sx = a2 - width / 2;
        a1->sy = a3 - (height - 1);
        art_ptr_unlock(cacheHandle);
    }

    if (v22) {
        obj_insert(node);
    }

    if (a5 != NULL) {
        Rect rect;
        obj_bound(a1, &rect);
        rect_min_bound(a5, &rect, a5);
    }

    if (a1 == obj_dude) {
        if (a1 != NULL) {
            Rect rect;
            obj_move(obj_egg, a2, a3, elevation, &rect);
            rect_min_bound(a5, &rect, a5);
        } else {
            obj_move(obj_egg, a2, a3, elevation, NULL);
        }
    }

    return 0;
}

// 0x48A568
int obj_move_to_tile(Object* obj, int tile, int elevation, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* prevNode;
    if (obj_node_ptr(obj, &node, &prevNode) == -1) {
        return -1;
    }

    Rect v23;
    int v5 = obj_adjust_light(obj, 1, rect);
    if (rect != NULL) {
        if (v5 == -1) {
            obj_bound(obj, rect);
        }

        rectCopy(&v23, rect);
    }

    int oldElevation = obj->elevation;
    if (prevNode != NULL) {
        prevNode->next = node->next;
    } else {
        int tileIndex = node->obj->tile;
        if (tileIndex == -1) {
            floatingObjects = floatingObjects->next;
        } else {
            objectTable[tileIndex] = objectTable[tileIndex]->next;
        }
    }

    if (obj_connect_to_tile(node, tile, elevation, rect) == -1) {
        return -1;
    }

    if (isInCombat()) {
        if (FID_TYPE(obj->fid) == OBJ_TYPE_CRITTER) {
            bool v8 = obj->outline != 0 && (obj->outline & OUTLINE_DISABLED) == 0;
            combat_update_critter_outline_for_los(obj, v8);
        }
    }

    if (rect != NULL) {
        rect_min_bound(rect, &v23, rect);
    }

    if (obj == obj_dude) {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            int elev = obj->elevation;
            if (elevation < elev) {
                break;
            }

            if (elevation == elev) {
                if (FID_TYPE(obj->fid) == OBJ_TYPE_MISC) {
                    if (obj->pid >= 0x5000010 && obj->pid <= 0x5000017) {
                        ObjectData* data = &(obj->data);

                        MapTransition transition;
                        memset(&transition, 0, sizeof(transition));

                        transition.map = data->misc.map;
                        transition.tile = data->misc.tile;
                        transition.elevation = data->misc.elevation;
                        transition.rotation = data->misc.rotation;
                        map_leave_map(&transition);

                        wmMapMarkMapEntranceState(transition.map, transition.elevation, 1);
                    }
                }
            }

            objectListNode = objectListNode->next;
        }

        // NOTE: Uninline.
        obj_set_seen(tile);

        int roofX = tile % 200 / 2;
        int roofY = tile / 200 / 2;
        if (roofX != obj_last_roof_x || roofY != obj_last_roof_y || elevation != obj_last_elev) {
            int currentSquare = square[elevation]->field_0[roofX + 100 * roofY];
            int currentSquareFid = art_id(OBJ_TYPE_TILE, (currentSquare >> 16) & 0xFFF, 0, 0, 0);
            int previousSquare = 0;
            if (obj_last_roof_x != -1) {
                previousSquare = square[elevation]->field_0[obj_last_roof_x + 100 * obj_last_roof_y];
            }
            bool isEmpty = art_id(OBJ_TYPE_TILE, 1, 0, 0, 0) == currentSquareFid;

            if (isEmpty != obj_last_is_empty || (obj_last_roof_x == -1) || (((currentSquare >> 16) & 0xF000) >> 12) != (((previousSquare >> 16) & 0xF000) >> 12)) {
                if (!obj_last_is_empty && obj_last_roof_x != -1) {
                    tile_fill_roof(obj_last_roof_x, obj_last_roof_y, elevation, 1);
                }

                if (!isEmpty) {
                    tile_fill_roof(roofX, roofY, elevation, 0);
                }

                if (rect != NULL) {
                    rect_min_bound(rect, &scr_size, rect);
                }
            }

            obj_last_roof_x = roofX;
            obj_last_roof_y = roofY;
            obj_last_elev = elevation;
            obj_last_is_empty = isEmpty;
        }

        if (rect != NULL) {
            Rect r;
            obj_move_to_tile(obj_egg, tile, elevation, &r);
            rect_min_bound(rect, &r, rect);
        } else {
            obj_move_to_tile(obj_egg, tile, elevation, 0);
        }

        if (elevation != oldElevation) {
            map_set_elevation(elevation);
            tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
            if (isInCombat()) {
                game_user_wants_to_quit = 1;
            }
        }
    } else {
        if (elevation != obj_last_elev && PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            combat_delete_critter(obj);
        }
    }

    return 0;
}

// 0x48A9A0
int obj_reset_roof()
{
    if (obj_last_roof_x == -1) {
        return 0;
    }
    
    int fid = art_id(OBJ_TYPE_TILE, (square[obj_dude->elevation]->field_0[obj_last_roof_x + 100 * obj_last_roof_y] >> 16) & 0xFFF, 0, 0, 0);
    if (fid != art_id(OBJ_TYPE_TILE, 1, 0, 0, 0)) {
        tile_fill_roof(obj_last_roof_x, obj_last_roof_y, obj_dude->elevation, 1);
    }
    return 0;
}

// Sets object fid.
//
// 0x48AA3C
int obj_change_fid(Object* obj, int fid, Rect* dirtyRect)
{
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);

        obj->fid = fid;

        obj_bound(obj, &new_rect);
        rect_min_bound(dirtyRect, &new_rect, dirtyRect);
    } else {
        obj->fid = fid;
    }

    return 0;
}

// Sets object frame.
//
// 0x48AA84
int obj_set_frame(Object* obj, int frame, Rect* rect)
{
    Rect new_rect;
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    if (frame >= framesPerDirection) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(obj, rect);
        obj->frame = frame;
        obj_bound(obj, &new_rect);
        rect_min_bound(rect, &new_rect, rect);
    } else {
        obj->frame = frame;
    }

    return 0;
}

// 0x48AAF0
int obj_inc_frame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int nextFrame;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    nextFrame = obj->frame + 1;
    if (nextFrame >= framesPerDirection) {
        nextFrame = 0;
    }

    if (dirtyRect != NULL) {

        obj_bound(obj, dirtyRect);

        obj->frame = nextFrame;

        Rect updatedRect;
        obj_bound(obj, &updatedRect);
        rect_min_bound(dirtyRect, &updatedRect, dirtyRect);
    } else {
        obj->frame = nextFrame;
    }

    return 0;
}

// 0x48AB60
//
int obj_dec_frame(Object* obj, Rect* dirtyRect)
{
    Art* art;
    CacheEntry* cache_entry;
    int framesPerDirection;
    int prevFrame;
    Rect newRect;

    if (obj == NULL) {
        return -1;
    }

    art = art_ptr_lock(obj->fid, &cache_entry);
    if (art == NULL) {
        return -1;
    }

    framesPerDirection = art->frameCount;

    art_ptr_unlock(cache_entry);

    prevFrame = obj->frame - 1;
    if (prevFrame < 0) {
        prevFrame = framesPerDirection - 1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);
        obj->frame = prevFrame;
        obj_bound(obj, &newRect);
        rect_min_bound(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->frame = prevFrame;
    }

    return 0;
}

// 0x48ABD4
int obj_set_rotation(Object* obj, int direction, Rect* dirtyRect)
{
    if (obj == NULL) {
        return -1;
    }

    if (direction >= ROTATION_COUNT) {
        return -1;
    }

    if (dirtyRect != NULL) {
        obj_bound(obj, dirtyRect);
        obj->rotation = direction;

        Rect newRect;
        obj_bound(obj, &newRect);
        rect_min_bound(dirtyRect, &newRect, dirtyRect);
    } else {
        obj->rotation = direction;
    }

    return 0;
}

// 0x48AC20
int obj_inc_rotation(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation + 1;
    if (rotation >= ROTATION_COUNT) {
        rotation = ROTATION_NE;
    }

    return obj_set_rotation(obj, rotation, dirtyRect);
}

// 0x48AC38
int obj_dec_rotation(Object* obj, Rect* dirtyRect)
{
    int rotation = obj->rotation - 1;
    if (rotation < 0) {
        rotation = ROTATION_NW;
    }

    return obj_set_rotation(obj, rotation, dirtyRect);
}

// 0x48AC54
void obj_rebuild_all_light()
{
    light_reset_tiles();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            obj_adjust_light(objectListNode->obj, 0, NULL);
            objectListNode = objectListNode->next;
        }
    }
}

// 0x48AC90
int obj_set_light(Object* obj, int lightDistance, int lightIntensity, Rect* rect)
{
    int v7;
    Rect new_rect;

    if (obj == NULL) {
        return -1;
    }

    v7 = obj_turn_off_light(obj, rect);
    if (lightIntensity > 0) {
        if (lightDistance >= 8) {
            lightDistance = 8;
        }

        obj->lightIntensity = lightIntensity;
        obj->lightDistance = lightDistance;

        if (rect != NULL) {
            v7 = obj_turn_on_light(obj, &new_rect);
            rect_min_bound(rect, &new_rect, rect);
        } else {
            v7 = obj_turn_on_light(obj, NULL);
        }
    } else {
        obj->lightIntensity = 0;
        obj->lightDistance = 0;
    }

    return v7;
}

// 0x48AD04
int obj_get_visible_light(Object* obj)
{
    int lightLevel = light_get_ambient();
    int lightIntensity = light_get_tile_true(obj->elevation, obj->tile);

    if (obj == obj_dude) {
        lightIntensity -= obj_dude->lightIntensity;
    }

    if (lightIntensity >= lightLevel) {
        if (lightIntensity > LIGHT_LEVEL_MAX) {
            lightIntensity = LIGHT_LEVEL_MAX;
        }
    } else {
        lightIntensity = lightLevel;
    }

    return lightIntensity;
}

// 0x48AD48
int obj_turn_on_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        obj->flags |= OBJECT_LIGHTING;

        if (obj_adjust_light(obj, 0, rect) == -1) {
            if (rect != NULL) {
                obj_bound(obj, rect);
            }
        }
    }

    return 0;
}

// 0x48AD9C
int obj_turn_off_light(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        obj->flags &= ~OBJECT_LIGHTING;
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) != 0) {
        if (obj_adjust_light(obj, 1, rect) == -1) {
            if (rect != NULL) {
                obj_bound(obj, rect);
            }
        }

        obj->flags &= ~OBJECT_LIGHTING;
    }

    return 0;
}

// 0x48ADF0
int obj_turn_on(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) == 0) {
        return -1;
    }

    obj->flags &= ~OBJECT_HIDDEN;
    obj->outline &= ~OUTLINE_DISABLED;

    if (obj_adjust_light(obj, 0, rect) == -1) {
        if (rect != NULL) {
            obj_bound(obj, rect);
        }
    }

    if (obj == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rect_min_bound(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x48AE68
int obj_turn_off(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if (obj_adjust_light(object, 1, rect) == -1) {
        if (rect != NULL) {
            obj_bound(object, rect);
        }
    }

    object->flags |= OBJECT_HIDDEN;

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (object == obj_dude) {
        if (rect != NULL) {
            Rect eggRect;
            obj_bound(obj_egg, &eggRect);
            rect_min_bound(rect, &eggRect, rect);
        }
    }

    return 0;
}

// 0x48AEE4
int obj_turn_on_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    object->outline &= ~OUTLINE_DISABLED;

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    return 0;
}

// 0x48AF00
int obj_turn_off_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if ((object->outline & OUTLINE_TYPE_MASK) != 0) {
        object->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    return 0;
}

// 0x48AF2C
int obj_toggle_flat(Object* object, Rect* rect)
{
    Rect v1;

    if (object == NULL) {
        return -1;
    }

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (obj_node_ptr(object, &node, &previousNode) == -1) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(object, rect);

        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile_index = node->obj->tile;
            if (tile_index == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile_index] = objectTable[tile_index]->next;
            }
        }

        object->flags ^= OBJECT_FLAT;

        obj_insert(node);
        obj_bound(object, &v1);
        rect_min_bound(rect, &v1, rect);
    } else {
        if (previousNode != NULL) {
            previousNode->next = node->next;
        } else {
            int tile = node->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }

        object->flags ^= OBJECT_FLAT;

        obj_insert(node);
    }

    return 0;
}

// 0x48B0FC
int obj_erase_object(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    gmouse_remove_item_outline(object);

    ObjectListNode* node;
    ObjectListNode* previousNode;
    if (obj_node_ptr(object, &node, &previousNode) == 0) {
        if (obj_adjust_light(object, 1, rect) == -1) {
            if (rect != NULL) {
                obj_bound(object, rect);
            }
        }

        if (obj_remove(node, previousNode) != 0) {
            return -1;
        }

        return 0;
    }

    // NOTE: Uninline.
    if (obj_create_object_node(&node) == -1) {
        return -1;
    }

    node->obj = object;

    if (obj_remove(node, node) == -1) {
        return -1;
    }

    return 0;
}

// 0x48B1B0
int obj_inven_free(Inventory* inventory)
{
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        ObjectListNode* node;
        // NOTE: Uninline.
        obj_create_object_node(&node);

        node->obj = inventoryItem->item;
        node->obj->flags &= ~OBJECT_FLAG_0x400;
        obj_remove(node, node);

        inventoryItem->item = NULL;
    }

    if (inventory->items != NULL) {
        mem_free(inventory->items);
        inventory->items = NULL;
        inventory->capacity = 0;
        inventory->length = 0;
    }

    return 0;
}

// 0x48B24C
bool obj_action_can_use(Object* obj)
{
    int pid = obj->pid;
    if (pid != PROTO_ID_LIT_FLARE && pid != PROTO_ID_DYNAMITE_II && pid != PROTO_ID_PLASTIC_EXPLOSIVES_II) {
        return proto_action_can_use(pid);
    } else {
        return false;
    }
}

// 0x48B278
bool obj_action_can_talk_to(Object* obj)
{
    return proto_action_can_talk_to(obj->pid) && (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) && critter_is_active(obj);
}

// 0x48B2A8
bool obj_portal_is_walk_thru(Object* obj)
{
    if (PID_TYPE(obj->pid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    Proto* proto;
    if (proto_ptr(obj->pid, &proto) == -1) {
        return false;
    }

    return (proto->scenery.data.generic.field_0 & 0x04) != 0;
}

// 0x48B2E8
Object* objFindObjPtrFromID(int a1)
{
    Object* obj = obj_find_first();
    while (obj != NULL) {
        if (obj->id == a1) {
            return obj;
        }
        obj = obj_find_next();
    }

    return NULL;
}

// Returns root owner of given object.
//
// 0x48B304
Object* obj_top_environment(Object* object)
{
    Object* owner = object->owner;
    if (owner == NULL) {
        return NULL;
    }

    while (owner->owner != NULL) {
        owner = owner->owner;
    }

    return owner;
}

// 0x48B318
void obj_remove_all()
{
    ObjectListNode* node;
    ObjectListNode* prev;
    ObjectListNode* next;

    scr_remove_all();

    for (int tile = 0; tile < HEX_GRID_SIZE; tile++) {
        node = objectTable[tile];
        prev = NULL;

        while (node != NULL) {
            next = node->next;
            if (obj_remove(node, prev) == -1) {
                prev = node;
            }
            node = next;
        }
    }

    node = floatingObjects;
    prev = NULL;

    while (node != NULL) {
        next = node->next;
        if (obj_remove(node, prev) == -1) {
            prev = node;
        }
        node = next;
    }

    obj_last_roof_y = -1;
    obj_last_elev = -1;
    obj_last_is_empty = true;
    obj_last_roof_x = -1;
}

// 0x48B3A8
Object* obj_find_first()
{
    find_elev = 0;

    ObjectListNode* objectListNode;
    for (find_tile = 0; find_tile < HEX_GRID_SIZE; find_tile++) {
        objectListNode = objectTable[find_tile];
        if (objectListNode) {
            break;
        }
    }

    if (find_tile == HEX_GRID_SIZE) {
        find_ptr = NULL;
        return NULL;
    }

    while (objectListNode != NULL) {
        if (art_get_disable(FID_TYPE(objectListNode->obj->fid)) == 0) {
            find_ptr = objectListNode;
            return objectListNode->obj;
        }
        objectListNode = objectListNode->next;
    }

    find_ptr = NULL;
    return NULL;
}

// 0x48B41C
Object* obj_find_next()
{
    if (find_ptr == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = find_ptr->next;

    while (find_tile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = objectTable[find_tile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (!art_get_disable(FID_TYPE(object->fid))) {
                find_ptr = objectListNode;
                return object;
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x48B48C
Object* obj_find_first_at(int elevation)
{
    find_elev = elevation;
    find_tile = 0;

    for (find_tile = 0; find_tile < HEX_GRID_SIZE; find_tile++) {
        ObjectListNode* objectListNode = objectTable[find_tile];
        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == elevation) {
                if (!art_get_disable(FID_TYPE(object->fid))) {
                    find_ptr = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x48B510
Object* obj_find_next_at()
{
    if (find_ptr == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = find_ptr->next;

    while (find_tile < HEX_GRID_SIZE) {
        if (objectListNode == NULL) {
            objectListNode = objectTable[find_tile++];
        }

        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if (object->elevation == find_elev) {
                if (!art_get_disable(FID_TYPE(object->fid))) {
                    find_ptr = objectListNode;
                    return object;
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    find_ptr = NULL;
    return NULL;
}

// 0x48B5A8
Object* obj_find_first_at_tile(int elevation, int tile)
{
    find_elev = elevation;
    find_tile = tile;

    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation) {
            if (!art_get_disable(FID_TYPE(object->fid))) {
                find_ptr = objectListNode;
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    find_ptr = NULL;
    return NULL;
}

// 0x48B608
Object* obj_find_next_at_tile()
{
    if (find_ptr == NULL) {
        return NULL;
    }

    ObjectListNode* objectListNode = find_ptr->next;

    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == find_elev) {
            if (!art_get_disable(FID_TYPE(object->fid))) {
                find_ptr = objectListNode;
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    find_ptr = NULL;
    return NULL;
}

// 0x0x48B66C
void obj_bound(Object* obj, Rect* rect)
{
    if (obj == NULL) {
        return;
    }

    if (rect == NULL) {
        return;
    }

    bool isOutlined = false;
    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        isOutlined = true;
    }

    CacheEntry* artHandle;
    Art* art = art_ptr_lock(obj->fid, &artHandle);
    if (art == NULL) {
        rect->ulx = 0;
        rect->uly = 0;
        rect->lrx = 0;
        rect->lry = 0;
        return;
    }

    int width;
    int height;
    art_frame_width_length(art, obj->frame, obj->rotation, &width, &height);

    if (obj->tile == -1) {
        rect->ulx = obj->sx;
        rect->uly = obj->sy;
        rect->lrx = obj->sx + width - 1;
        rect->lry = obj->sy + height - 1;
    } else {
        int tileScreenY;
        int tileScreenX;
        if (tile_coord(obj->tile, &tileScreenX, &tileScreenY, obj->elevation) == 0) {
            tileScreenX += 16;
            tileScreenY += 8;

            tileScreenX += art->xOffsets[obj->rotation];
            tileScreenY += art->yOffsets[obj->rotation];

            tileScreenX += obj->x;
            tileScreenY += obj->y;

            rect->ulx = tileScreenX - width / 2;
            rect->uly = tileScreenY - height + 1;
            rect->lrx = width + rect->ulx - 1;
            rect->lry = tileScreenY;
        } else {
            rect->ulx = 0;
            rect->uly = 0;
            rect->lrx = 0;
            rect->lry = 0;
            isOutlined = false;
        }
    }

    art_ptr_unlock(artHandle);

    if (isOutlined) {
        rect->ulx--;
        rect->uly--;
        rect->lrx++;
        rect->lry++;
    }
}

// 0x48B7F8
bool obj_occupied(int tile, int elevation)
{
    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        if (objectListNode->obj->elevation == elevation
            && objectListNode->obj != obj_mouse
            && objectListNode->obj != obj_mouse_flat) {
            return true;
        }
        objectListNode = objectListNode->next;
    }

    return false;
}

// 0x48B848
Object* obj_blocking_at(Object* a1, int tile, int elev)
{
    ObjectListNode* objectListNode;
    Object* v7;
    int type;

    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        v7 = objectListNode->obj;
        if (v7->elevation == elev) {
            if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                type = FID_TYPE(v7->fid);
                if (type == OBJ_TYPE_CRITTER
                    || type == OBJ_TYPE_SCENERY
                    || type == OBJ_TYPE_WALL) {
                    return v7;
                }
            }
        }
        objectListNode = objectListNode->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int neighboor = tile_num_in_direction(tile, rotation, 1);
        if (hexGridTileIsValid(neighboor)) {
            objectListNode = objectTable[neighboor];
            while (objectListNode != NULL) {
                v7 = objectListNode->obj;
                if ((v7->flags & OBJECT_MULTIHEX) != 0) {
                    if (v7->elevation == elev) {
                        if ((v7->flags & OBJECT_HIDDEN) == 0 && (v7->flags & OBJECT_NO_BLOCK) == 0 && v7 != a1) {
                            type = FID_TYPE(v7->fid);
                            if (type == OBJ_TYPE_CRITTER
                                || type == OBJ_TYPE_SCENERY
                                || type == OBJ_TYPE_WALL) {
                                return v7;
                            }
                        }
                    }
                }
                objectListNode = objectListNode->next;
            }
        }
    }

    return NULL;
}

// 0x48B930
Object* obj_shoot_blocking_at(Object* obj, int tile, int elev)
{
    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    ObjectListNode* objectListItem = objectTable[tile];
    while (objectListItem != NULL) {
        Object* candidate = objectListItem->obj;
        if (candidate->elevation == elev) {
            unsigned int flags = candidate->flags;
            if ((flags & OBJECT_HIDDEN) == 0 && ((flags & OBJECT_NO_BLOCK) == 0 || (flags & OBJECT_SHOOT_THRU) == 0) && candidate != obj) {
                int type = FID_TYPE(candidate->fid);
                if (type == OBJ_TYPE_CRITTER || type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
                    return candidate;
                }
            }
        }
        objectListItem = objectListItem->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int adjacentTile = tile_num_in_direction(tile, rotation, 1);
        if (!hexGridTileIsValid(adjacentTile)) {
            continue;
        }

        ObjectListNode* objectListItem = objectTable[adjacentTile];
        while (objectListItem != NULL) {
            Object* candidate = objectListItem->obj;
            unsigned int flags = candidate->flags;
            if ((flags & OBJECT_MULTIHEX) != 0) {
                if (candidate->elevation == elev) {
                    if ((flags & OBJECT_HIDDEN) == 0 && (flags & OBJECT_NO_BLOCK) == 0 && candidate != obj) {
                        int type = FID_TYPE(candidate->fid);
                        if (type == OBJ_TYPE_CRITTER || type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
                            return candidate;
                        }
                    }
                }
            }
            objectListItem = objectListItem->next;
        }
    }

    return NULL;
}

// 0x48BA20
Object* obj_ai_blocking_at(Object* a1, int tile, int elevation)
{
    if (!hexGridTileIsValid(tile)) {
        return NULL;
    }

    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation) {
            if ((object->flags & OBJECT_HIDDEN) == 0
                && (object->flags & OBJECT_NO_BLOCK) == 0
                && object != a1) {
                int objectType = FID_TYPE(object->fid);
                if (objectType == OBJ_TYPE_CRITTER
                    || objectType == OBJ_TYPE_SCENERY
                    || objectType == OBJ_TYPE_WALL) {
                    if (moveBlockObj != NULL || objectType != OBJ_TYPE_CRITTER) {
                        return object;
                    }

                    moveBlockObj = object;
                }
            }
        }
        objectListNode = objectListNode->next;
    }

    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int candidate = tile_num_in_direction(tile, rotation, 1);
        if (!hexGridTileIsValid(candidate)) {
            continue;
        }

        objectListNode = objectTable[candidate];
        while (objectListNode != NULL) {
            Object* object = objectListNode->obj;
            if ((object->flags & OBJECT_MULTIHEX) != 0) {
                if (object->elevation == elevation) {
                    if ((object->flags & OBJECT_HIDDEN) == 0
                        && (object->flags & OBJECT_NO_BLOCK) == 0
                        && object != a1) {
                        int objectType = FID_TYPE(object->fid);
                        if (objectType == OBJ_TYPE_CRITTER
                            || objectType == OBJ_TYPE_SCENERY
                            || objectType == OBJ_TYPE_WALL) {
                            if (moveBlockObj != NULL || objectType != OBJ_TYPE_CRITTER) {
                                return object;
                            }

                            moveBlockObj = object;
                        }
                    }
                }
            }
            objectListNode = objectListNode->next;
        }
    }

    return NULL;
}

// 0x48BB44
int obj_scroll_blocking_at(int tile, int elev)
{
    // TODO: Might be an error - why tile 0 is excluded?
    if (tile <= 0 || tile >= 40000) {
        return -1;
    }

    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        if (elev < objectListNode->obj->elevation) {
            break;
        }

        if (objectListNode->obj->elevation == elev && objectListNode->obj->pid == 0x500000C) {
            return 0;
        }

        objectListNode = objectListNode->next;
    }

    return -1;
}

// 0x48BB88
Object* obj_sight_blocking_at(Object* a1, int tile, int elevation)
{
    ObjectListNode* objectListNode = objectTable[tile];
    while (objectListNode != NULL) {
        Object* object = objectListNode->obj;
        if (object->elevation == elevation
            && (object->flags & OBJECT_HIDDEN) == 0
            && (object->flags & OBJECT_LIGHT_THRU) == 0
            && object != a1) {
            int objectType = FID_TYPE(object->fid);
            if (objectType == OBJ_TYPE_SCENERY || objectType == OBJ_TYPE_WALL) {
                return object;
            }
        }
        objectListNode = objectListNode->next;
    }

    return NULL;
}

// 0x48BBD4
int obj_dist(Object* object1, Object* object2)
{
    if (object1 == NULL || object2 == NULL) {
        return 0;
    }

    int distance = tile_dist(object1->tile, object2->tile);

    if ((object1->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if ((object2->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if (distance < 0) {
        distance = 0;
    }

    return distance;
}

// 0x48BC08
int obj_dist_with_tile(Object* object1, int tile1, Object* object2, int tile2)
{
    if (object1 == NULL || object2 == NULL) {
        return 0;
    }

    int distance = tile_dist(tile1, tile2);

    if ((object1->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if ((object2->flags & OBJECT_MULTIHEX) != 0) {
        distance -= 1;
    }

    if (distance < 0) {
        distance = 0;
    }

    return distance;
}

// 0x48BC38
int obj_create_list(int tile, int elevation, int objectType, Object*** objectListPtr)
{
    if (objectListPtr == NULL) {
        return -1;
    }

    int count = 0;
    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = objectTable[index];
            while (objectListNode != NULL) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && FID_TYPE(obj->fid) == objectType) {
                    count++;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && FID_TYPE(objectListNode->obj->fid) == objectType) {
                count++;
            }
            objectListNode = objectListNode->next;
        }
    }

    if (count == 0) {
        return 0;
    }

    Object** objects = *objectListPtr = (Object**)mem_malloc(sizeof(*objects) * count);
    if (objects == NULL) {
        return -1;
    }

    if (tile == -1) {
        for (int index = 0; index < HEX_GRID_SIZE; index++) {
            ObjectListNode* objectListNode = objectTable[index];
            while (objectListNode) {
                Object* obj = objectListNode->obj;
                if ((obj->flags & OBJECT_HIDDEN) == 0
                    && obj->elevation == elevation
                    && FID_TYPE(obj->fid) == objectType) {
                    *objects++ = obj;
                }
                objectListNode = objectListNode->next;
            }
        }
    } else {
        ObjectListNode* objectListNode = objectTable[tile];
        while (objectListNode != NULL) {
            Object* obj = objectListNode->obj;
            if ((obj->flags & OBJECT_HIDDEN) == 0
                && obj->elevation == elevation
                && FID_TYPE(obj->fid) == objectType) {
                *objects++ = obj;
            }
            objectListNode = objectListNode->next;
        }
    }

    return count;
}

// 0x48BDCC
void obj_delete_list(Object** objectList)
{
    if (objectList != NULL) {
        mem_free(objectList);
    }
}

// 0x48BDD8
// Original: processes ALL pixels (including transparent), blends src with dest via gray+blend tables.
// 32-bit: alpha blend every pixel at 50% with destination. No transparency skip, no lighting.
void translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, unsigned char* a9, unsigned char* a10)
{
    uint32_t* sp = (uint32_t*)src;
    uint32_t* dp = (uint32_t*)dest + destPitch * destY + destX;
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            uint32_t pixel = *sp;
            uint32_t dpx = *dp;
            unsigned int sr = pixel & 0xFF;
            unsigned int sg = (pixel >> 8) & 0xFF;
            unsigned int sb = (pixel >> 16) & 0xFF;
            unsigned int dr = dpx & 0xFF;
            unsigned int dg = (dpx >> 8) & 0xFF;
            unsigned int db = (dpx >> 16) & 0xFF;
            unsigned int r = (sr + dr) >> 1;
            unsigned int g = (sg + dg) >> 1;
            unsigned int b = (sb + db) >> 1;
            *dp = (0xFFu << 24) | (b << 16) | (g << 8) | r;
            sp++;
            dp++;
        }
        sp += srcStep;
        dp += destStep;
    }
}

// 0x48BEFC
// Original: skips transparent pixels, applies intensityColorTable darkening.
// 32-bit: simple darken by scaling color channels. lightModifier 0=black, 128=full brightness.
void dark_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light)
{
    uint32_t* sp = (uint32_t*)src;
    uint32_t* dp = (uint32_t*)dest + destPitch * destY + destX;

    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    // light is fixed-point 16.16 where 0x10000 = full brightness.
    // >> 9 gives 0..128 range. Clamp to 128 max.
    int lightModifier = light >> 9;
    if (lightModifier > 128) lightModifier = 128;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            uint32_t pixel = *sp;
            if (pixel != 0) {
                unsigned int r = pixel & 0xFF;
                unsigned int g = (pixel >> 8) & 0xFF;
                unsigned int b = (pixel >> 16) & 0xFF;
                r = (r * lightModifier) >> 7;
                g = (g * lightModifier) >> 7;
                b = (b * lightModifier) >> 7;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                *dp = (0xFFu << 24) | (b << 16) | (g << 8) | r;
            }
            sp++;
            dp++;
        }
        sp += srcStep;
        dp += destStep;
    }
}

// 0x48BF88
// Original: skips transparent, converts source to grayscale (gray table), uses that
// luminance to index into blend table (tintColor * luminance blended with dest),
// THEN applies intensityColorTable darkening on the blended result.
// 32-bit: grayscale source → scale tint by luminance → blend with dest → darken.
void dark_translucent_trans_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destX, int destY, int destPitch, int light, int alpha, uint32_t tint_color)
{
    uint32_t* sp = (uint32_t*)src;
    uint32_t* dp = (uint32_t*)dest + destPitch * destY + destX;
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;

    // light is fixed-point 16.16 where 0x10000 = full brightness.
    // >> 9 gives 0..128 range. Clamp to 128 max.
    int lightModifier = light >> 9;
    if (lightModifier > 128) lightModifier = 128;

    // Extract tint channels (0-255 each)
    unsigned int tint_r = tint_color & 0xFF;
    unsigned int tint_g = (tint_color >> 8) & 0xFF;
    unsigned int tint_b = (tint_color >> 16) & 0xFF;

    unsigned int inv_alpha = 255 - alpha;

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            uint32_t pixel = *sp;
            if (pixel != 0) {
                // Step 1: Convert source to grayscale luminance (matching gray table)
                unsigned int sr = pixel & 0xFF;
                unsigned int sg = (pixel >> 8) & 0xFF;
                unsigned int sb = (pixel >> 16) & 0xFF;
                unsigned int lum = (sr * 77 + sg * 150 + sb * 29) >> 8;

                // Step 2: Scale tint color by source luminance
                // This matches the original: tintColor * grayscale(src) / 255
                unsigned int tr = (tint_r * lum) / 255;
                unsigned int tg = (tint_g * lum) / 255;
                unsigned int tb = (tint_b * lum) / 255;

                // Step 3: Alpha blend tinted source over destination
                uint32_t dpx = *dp;
                unsigned int dr = dpx & 0xFF;
                unsigned int dg = (dpx >> 8) & 0xFF;
                unsigned int db = (dpx >> 16) & 0xFF;
                unsigned int r = (tr * alpha + dr * inv_alpha) / 255;
                unsigned int g = (tg * alpha + dg * inv_alpha) / 255;
                unsigned int b = (tb * alpha + db * inv_alpha) / 255;

                // Step 4: Apply lighting AFTER blend (matching original order)
                r = (r * lightModifier) >> 7;
                g = (g * lightModifier) >> 7;
                b = (b * lightModifier) >> 7;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                *dp = (0xFFu << 24) | (b << 16) | (g << 8) | r;
            }
            sp++;
            dp++;
        }
        sp += srcStep;
        dp += destStep;
    }
}

// 0x48C03C
void intensity_mask_buf_to_buf(unsigned char* src, int srcWidth, int srcHeight, int srcPitch, unsigned char* dest, int destPitch, unsigned char* mask, int maskPitch, int light)
{
    // 32-bit: blend src and dest using mask as alpha, with lighting.
    // mask pixel: 0 = fully opaque src, >0 = blend (mask is alpha for dest).
    uint32_t* sp = (uint32_t*)src;
    uint32_t* dp = (uint32_t*)dest;
    // mask is from the egg FRM which is now 32-bit RGBA
    uint32_t* mp = (uint32_t*)mask;
    int srcStep = srcPitch - srcWidth;
    int destStep = destPitch - srcWidth;
    int maskStep = maskPitch - srcWidth;
    int lightModifier = light >> 9;
    int factor = (lightModifier <= 127)
        ? lightModifier * 512
        : (lightModifier - 128) * 512;
    int darken = (lightModifier <= 127);

    for (int y = 0; y < srcHeight; y++) {
        for (int x = 0; x < srcWidth; x++) {
            uint32_t pixel = *sp;
            if (pixel != 0) {
                // Apply lighting
                unsigned char sr = pixel & 0xFF;
                unsigned char sg = (pixel >> 8) & 0xFF;
                unsigned char sb = (pixel >> 16) & 0xFF;
                if (darken) {
                    sr = (sr * factor) >> 16;
                    sg = (sg * factor) >> 16;
                    sb = (sb * factor) >> 16;
                } else {
                    sr = sr + (((255 - sr) * factor) >> 16);
                    sg = sg + (((255 - sg) * factor) >> 16);
                    sb = sb + (((255 - sb) * factor) >> 16);
                }
                uint32_t mval = *mp;
                // If mask is 0 (transparent), use full lit src. Otherwise blend.
                if (mval != 0) {
                    unsigned char alpha = mval & 0xFF;
                    uint32_t dpx = *dp;
                    unsigned char dr = dpx & 0xFF;
                    unsigned char dg = (dpx >> 8) & 0xFF;
                    unsigned char db = (dpx >> 16) & 0xFF;
                    unsigned char r = (sr * (255 - alpha) + dr * alpha) / 255;
                    unsigned char g = (sg * (255 - alpha) + dg * alpha) / 255;
                    unsigned char b = (sb * (255 - alpha) + db * alpha) / 255;
                    *dp = (0xFFu << 24) | (b << 16) | (g << 8) | r;
                } else {
                    *dp = (0xFFu << 24) | (sb << 16) | (sg << 8) | sr;
                }
            }
            sp++;
            dp++;
            mp++;
        }
        sp += srcStep;
        dp += destStep;
        mp += maskStep;
    }
}


// 0x48C2B4
int obj_outline_object(Object* obj, int outlineType, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if ((obj->outline & OUTLINE_TYPE_MASK) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_NO_HIGHLIGHT) != 0) {
        return -1;
    }

    obj->outline = outlineType;

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        obj->outline |= OUTLINE_DISABLED;
    }

    if (rect != NULL) {
        obj_bound(obj, rect);
    }

    return 0;
}

// 0x48C2F0
int obj_remove_outline(Object* object, Rect* rect)
{
    if (object == NULL) {
        return -1;
    }

    if (rect != NULL) {
        obj_bound(object, rect);
    }

    object->outline = 0;

    return 0;
}

// 0x48C340
int obj_intersects_with(Object* object, int x, int y)
{
    int flags = 0;

    if (object == obj_egg || (object->flags & OBJECT_HIDDEN) == 0) {
        CacheEntry* handle;
        Art* art = art_ptr_lock(object->fid, &handle);
        if (art != NULL) {
            int width;
            int height;
            art_frame_width_length(art, object->frame, object->rotation, &width, &height);

            int minX;
            int minY;
            int maxX;
            int maxY;
            if (object->tile == -1) {
                minX = object->sx;
                minY = object->sy;
                maxX = minX + width - 1;
                maxY = minY + height - 1;
            } else {
                int tileScreenX;
                int tileScreenY;
                tile_coord(object->tile, &tileScreenX, &tileScreenY, object->elevation);
                tileScreenX += 16;
                tileScreenY += 8;

                tileScreenX += art->xOffsets[object->rotation];
                tileScreenY += art->yOffsets[object->rotation];

                tileScreenX += object->x;
                tileScreenY += object->y;

                minX = tileScreenX - width / 2;
                maxX = minX + width - 1;

                minY = tileScreenY - height + 1;
                maxY = tileScreenY;
            }

            if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                unsigned char* data = art_frame_data(art, object->frame, object->rotation);
                if (data != NULL) {
                    if (((uint32_t*)data)[width * (y - minY) + x - minX] != 0) {
                        flags |= 0x01;

                        if ((object->flags & OBJECT_FLAG_0xFC000) != 0) {
                            if ((object->flags & OBJECT_TRANS_NONE) == 0) {
                                flags &= ~0x03;
                                flags |= 0x02;
                            }
                        } else {
                            int type = FID_TYPE(object->fid);
                            if (type == OBJ_TYPE_SCENERY || type == OBJ_TYPE_WALL) {
                                Proto* proto;
                                proto_ptr(object->pid, &proto);

                                bool v20;
                                int extendedFlags = proto->scenery.extendedFlags;
                                if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile);
                                } else if ((extendedFlags & 0x10000000) != 0) {
                                    // NOTE: Original code uses bitwise or, but given the fact that these functions return
                                    // bools, logical or is more suitable.
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile) || tile_to_right_of(obj_dude->tile, object->tile);
                                } else if ((extendedFlags & 0x20000000) != 0) {
                                    v20 = tile_in_front_of(object->tile, obj_dude->tile) && tile_to_right_of(obj_dude->tile, object->tile);
                                } else {
                                    v20 = tile_to_right_of(obj_dude->tile, object->tile);
                                }

                                if (v20) {
                                    if (obj_intersects_with(obj_egg, x, y) != 0) {
                                        flags |= 0x04;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            art_ptr_unlock(handle);
        }
    }

    return flags;
}

// 0x48C5C4
int obj_create_intersect_list(int x, int y, int elevation, int objectType, ObjectWithFlags** entriesPtr)
{
    int v5 = tile_num(x - 320, y - 240, elevation);
    *entriesPtr = NULL;

    if (updateHexArea <= 0) {
        return 0;
    }

    int count = 0;

    int parity = tile_center_tile & 1;
    for (int index = 0; index < updateHexArea; index++) {
        int v7 = orderTable[parity][index];
        if (offsetDivTable[v7] < 30 && offsetModTable[v7] < 20) {
            int intersectTileIndex = offsetTable[parity][v7] + v5;
            if (intersectTileIndex < 0 || intersectTileIndex >= HEX_GRID_SIZE) {
                continue;
            }
            ObjectListNode* objectListNode = objectTable[intersectTileIndex];
            while (objectListNode != NULL) {
                Object* object = objectListNode->obj;
                if (object->elevation > elevation) {
                    break;
                }

                if (object->elevation == elevation
                    && (objectType == -1 || FID_TYPE(object->fid) == objectType)
                    && object != obj_egg) {
                    int flags = obj_intersects_with(object, x, y);
                    if (flags != 0) {
                        ObjectWithFlags* entries = (ObjectWithFlags*)mem_realloc(*entriesPtr, sizeof(*entries) * (count + 1));
                        if (entries != NULL) {
                            *entriesPtr = entries;
                            entries[count].object = object;
                            entries[count].flags = flags;
                            count++;
                        }
                    }
                }

                objectListNode = objectListNode->next;
            }
        }
    }

    return count;
}

// 0x48C74C
void obj_delete_intersect_list(ObjectWithFlags** entriesPtr)
{
    if (entriesPtr != NULL && *entriesPtr != NULL) {
        mem_free(*entriesPtr);
        *entriesPtr = NULL;
    }
}

// NOTE: Inlined.
//
// 0x48C76C
void obj_set_seen(int tile)
{
    obj_seen[tile >> 3] |= 1 << (tile & 7);
}

// 0x48C788
void obj_clear_seen()
{
    memset(obj_seen, 0, sizeof(obj_seen));
}

// 0x48C7A0
void obj_process_seen()
{
    int i;
    int v7;
    int v8;
    int v5;
    int v0;
    int v3;
    ObjectListNode* obj_entry;

    memset(obj_seen_check, 0, 5001);

    v0 = 400;
    for (i = 0; i < 5001; i++) {
        if (obj_seen[i] != 0) {
            for (v3 = i - 400; v3 != v0; v3 += 25) {
                if (v3 >= 0 && v3 < 5001) {
                    obj_seen_check[v3] = -1;
                    if (v3 > 0) {
                        obj_seen_check[v3 - 1] = -1;
                    }
                    if (v3 < 5000) {
                        obj_seen_check[v3 + 1] = -1;
                    }
                    if (v3 > 1) {
                        obj_seen_check[v3 - 2] = -1;
                    }
                    if (v3 < 4999) {
                        obj_seen_check[v3 + 2] = -1;
                    }
                }
            }
        }
        v0++;
    }

    v7 = 0;
    for (i = 0; i < 5001; i++) {
        if (obj_seen_check[i] != 0) {
            v8 = 1;
            for (v5 = v7; v5 < v7 + 8; v5++) {
                if (v8 & obj_seen_check[i]) {
                    if (v5 < 40000) {
                        for (obj_entry = objectTable[v5]; obj_entry != NULL; obj_entry = obj_entry->next) {
                            if (obj_entry->obj->elevation == obj_dude->elevation) {
                                obj_entry->obj->flags |= OBJECT_SEEN;
                            }
                        }
                    }
                }
                v8 *= 2;
            }
        }
        v7 += 8;
    }

    memset(obj_seen, 0, 5001);
}

// 0x48C8E4
char* object_name(Object* obj)
{
    int objectType = FID_TYPE(obj->fid);
    switch (objectType) {
    case OBJ_TYPE_ITEM:
        return item_name(obj);
    case OBJ_TYPE_CRITTER:
        return critter_name(obj);
    default:
        return proto_name(obj->pid);
    }
}

// 0x48C914
char* object_description(Object* obj)
{
    if (FID_TYPE(obj->fid) == OBJ_TYPE_ITEM) {
        return item_description(obj);
    }

    return proto_description(obj->pid);
}

// Warm objects cache?
//
// 0x48C938
void obj_preload_art_cache(int flags)
{
    if (preload_list == NULL) {
        return;
    }

    unsigned char arr[4096];
    memset(arr, 0, sizeof(arr));

    if ((flags & 0x02) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[0]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x04) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[1]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    if ((flags & 0x08) == 0) {
        for (int i = 0; i < SQUARE_GRID_SIZE; i++) {
            int v3 = square[2]->field_0[i];
            arr[v3 & 0xFFF] = 1;
            arr[(v3 >> 16) & 0xFFF] = 1;
        }
    }

    qsort(preload_list, preload_list_index, sizeof(*preload_list), obj_preload_sort);

    int v11 = preload_list_index;
    int v12 = preload_list_index;

    if (FID_TYPE(preload_list[v12 - 1]) == OBJ_TYPE_WALL) {
        int objectType = OBJ_TYPE_ITEM;
        do {
            v11--;
            objectType = FID_TYPE(preload_list[v12 - 1]);
            v12--;
        } while (objectType == OBJ_TYPE_WALL);
        v11++;
    }

    CacheEntry* cache_handle;
    if (art_ptr_lock(*preload_list, &cache_handle) != NULL) {
        art_ptr_unlock(cache_handle);
    }

    for (int i = 1; i < v11; i++) {
        if (preload_list[i - 1] != preload_list[i]) {
            if (art_ptr_lock(preload_list[i], &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    for (int i = 0; i < 4096; i++) {
        if (arr[i] != 0) {
            int fid = art_id(OBJ_TYPE_TILE, i, 0, 0, 0);
            if (art_ptr_lock(fid, &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    for (int i = v11; i < preload_list_index; i++) {
        if (preload_list[i - 1] != preload_list[i]) {
            if (art_ptr_lock(preload_list[i], &cache_handle) != NULL) {
                art_ptr_unlock(cache_handle);
            }
        }
    }

    mem_free(preload_list);
    preload_list = NULL;

    preload_list_index = 0;
}

// 0x48CB88
static int obj_offset_table_init()
{
    int i;

    if (offsetTable[0] != NULL) {
        return -1;
    }

    if (offsetTable[1] != NULL) {
        return -1;
    }

    offsetTable[0] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetTable[0] == NULL) {
        goto err;
    }

    offsetTable[1] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetTable[1] == NULL) {
        goto err;
    }

    for (int parity = 0; parity < 2; parity++) {
        int originTile = tile_num(updateAreaPixelBounds.ulx, updateAreaPixelBounds.uly, 0);
        if (originTile != -1) {
            int* offsets = offsetTable[tile_center_tile & 1];
            int originTileX;
            int originTileY;
            tile_coord(originTile, &originTileX, &originTileY, 0);

            int parityShift = 16;
            originTileX += 16;
            originTileY += 8;
            if (originTileX > updateAreaPixelBounds.ulx) {
                parityShift = -parityShift;
            }

            int tileX = originTileX;
            for (int y = 0; y < updateHexHeight; y++) {
                for (int x = 0; x < updateHexWidth; x++) {
                    int tile = tile_num(tileX, originTileY, 0);
                    if (tile == -1) {
                        goto err;
                    }

                    tileX += 32;
                    *offsets++ = tile - originTile;
                }

                tileX = parityShift + originTileX;
                originTileY += 12;
                parityShift = -parityShift;
            }
        }

        if (tile_set_center(tile_center_tile + 1, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) == -1) {
            goto err;
        }
    }

    offsetDivTable = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetDivTable == NULL) {
        goto err;
    }

    for (i = 0; i < updateHexArea; i++) {
        offsetDivTable[i] = i / updateHexWidth;
    }

    offsetModTable = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (offsetModTable == NULL) {
        goto err;
    }

    for (i = 0; i < updateHexArea; i++) {
        offsetModTable[i] = i % updateHexWidth;
    }

    return 0;

err:
    obj_offset_table_exit();

    return -1;
}

// 0x48CDA0
static void obj_offset_table_exit()
{
    if (offsetModTable != NULL) {
        mem_free(offsetModTable);
        offsetModTable = NULL;
    }

    if (offsetDivTable != NULL) {
        mem_free(offsetDivTable);
        offsetDivTable = NULL;
    }

    if (offsetTable[1] != NULL) {
        mem_free(offsetTable[1]);
        offsetTable[1] = NULL;
    }

    if (offsetTable[0] != NULL) {
        mem_free(offsetTable[0]);
        offsetTable[0] = NULL;
    }
}

// 0x48CE10
static int obj_order_table_init()
{
    if (orderTable[0] != NULL || orderTable[1] != NULL) {
        return -1;
    }

    orderTable[0] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (orderTable[0] == NULL) {
        goto err;
    }

    orderTable[1] = (int*)mem_malloc(sizeof(int) * updateHexArea);
    if (orderTable[1] == NULL) {
        goto err;
    }

    for (int index = 0; index < updateHexArea; index++) {
        orderTable[0][index] = index;
        orderTable[1][index] = index;
    }

    qsort(orderTable[0], updateHexArea, sizeof(int), obj_order_comp_func_even);
    qsort(orderTable[1], updateHexArea, sizeof(int), obj_order_comp_func_odd);

    return 0;

err:

    // NOTE: Uninline.
    obj_order_table_exit();

    return -1;
}

// 0x48CF20
static int obj_order_comp_func_even(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return offsetTable[0][v1] - offsetTable[0][v2];
}

// 0x48CF38
static int obj_order_comp_func_odd(const void* a1, const void* a2)
{
    int v1 = *(int*)a1;
    int v2 = *(int*)a2;
    return offsetTable[1][v1] - offsetTable[1][v2];
}

// NOTE: Inlined.
//
// 0x48CF50
static void obj_order_table_exit()
{
    if (orderTable[1] != NULL) {
        mem_free(orderTable[1]);
        orderTable[1] = NULL;
    }

    if (orderTable[0] != NULL) {
        mem_free(orderTable[0]);
        orderTable[0] = NULL;
    }
}

// 0x48CF8C
static int obj_render_table_init()
{
    if (renderTable != NULL) {
        return -1;
    }

    renderTable = (ObjectListNode**)mem_malloc(sizeof(*renderTable) * updateHexArea);
    if (renderTable == NULL) {
        return -1;
    }

    for (int index = 0; index < updateHexArea; index++) {
        renderTable[index] = NULL;
    }

    return 0;
}

// NOTE: Inlined.
//
// 0x48D000
static void obj_render_table_exit()
{
    if (renderTable != NULL) {
        mem_free(renderTable);
        renderTable = NULL;
    }
}

// 0x48D020
static void obj_light_table_init()
{
    for (int s = 0; s < 2; s++) {
        int v4 = tile_center_tile + s;
        for (int i = 0; i < ROTATION_COUNT; i++) {
            int v15 = 8;
            int* p = light_offsets[v4 & 1][i];
            for (int j = 0; j < 8; j++) {
                int tile = tile_num_in_direction(v4, (i + 1) % ROTATION_COUNT, j);

                for (int m = 0; m < v15; m++) {
                    *p++ = tile_num_in_direction(tile, i, m + 1) - v4;
                }

                v15--;
            }
        }
    }
}

// 0x48D1E4
static void obj_blend_table_init()
{
    for (int index = 0; index < 256; index++) {
        int r = (Color2RGB(index) & 0x7C00) >> 10;
        int g = (Color2RGB(index) & 0x3E0) >> 5;
        int b = Color2RGB(index) & 0x1F;
        glassGrayTable[index] = ((r + 5 * g + 4 * b) / 10) >> 2;
        commonGrayTable[index] = ((b + 3 * r + 6 * g) / 10) >> 2;
    }

    glassGrayTable[0] = 0;
    commonGrayTable[0] = 0;

    wallBlendTable = getColorBlendTable(colorTable[25439]);
    glassBlendTable = getColorBlendTable(colorTable[10239]);
    steamBlendTable = getColorBlendTable(colorTable[32767]);
    energyBlendTable = getColorBlendTable(colorTable[30689]);
    redBlendTable = getColorBlendTable(colorTable[31744]);
}

// NOTE: Inlined.
//
// 0x48D2E8
static void obj_blend_table_exit()
{
    freeColorBlendTable(colorTable[25439]);
    freeColorBlendTable(colorTable[10239]);
    freeColorBlendTable(colorTable[32767]);
    freeColorBlendTable(colorTable[30689]);
    freeColorBlendTable(colorTable[31744]);
}

// 0x48D348
int obj_save_obj(File* stream, Object* object)
{
    if ((object->flags & OBJECT_TEMPORARY) != 0) {
        return 0;
    }

    CritterCombatData* combatData = NULL;
    Object* whoHitMe = NULL;
    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combatData = &(object->data.critter.combat);
        whoHitMe = combatData->whoHitMe;
        if (whoHitMe != 0) {
            if (combatData->whoHitMeCid != -1) {
                combatData->whoHitMeCid = whoHitMe->cid;
            }
        } else {
            combatData->whoHitMeCid = -1;
        }
    }

    if (obj_write_obj(object, stream) == -1) {
        return -1;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combatData->whoHitMe = whoHitMe;
    }

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);

        if (db_fwriteInt(stream, inventoryItem->quantity) == -1) {
            return -1;
        }

        if (obj_save_obj(stream, inventoryItem->item) == -1) {
            return -1;
        }

        if ((inventoryItem->item->flags & OBJECT_TEMPORARY) != 0) {
            return -1;
        }
    }

    return 0;
}

// 0x48D414
int obj_load_obj(File* stream, Object** objectPtr, int elevation, Object* owner)
{
    Object* obj;

    if (obj_create_object(&obj) == -1) {
        *objectPtr = NULL;
        return -1;
    }

    if (obj_read_obj(obj, stream) != 0) {
        *objectPtr = NULL;
        return -1;
    }

    if (obj->sid != -1) {
        Script* script;
        if (scr_ptr(obj->sid, &script) == -1) {
            obj->sid = -1;
        } else {
            script->owner = obj;
        }
    }

    obj_fix_violence_settings(&(obj->fid));

    if (!art_fid_valid(obj->fid)) {
        debug_printf("\nError: invalid object art fid: %u\n", obj->fid);
        // NOTE: Uninline.
        obj_destroy_object(&obj);
        return -2;
    }

    if (elevation == -1) {
        elevation = obj->elevation;
    } else {
        obj->elevation = elevation;
    }

    obj->owner = owner;

    Inventory* inventory = &(obj->data.inventory);
    if (inventory->length <= 0) {
        inventory->capacity = 0;
        inventory->items = NULL;
        *objectPtr = obj;
        return 0;
    }

    InventoryItem* inventoryItems = inventory->items = (InventoryItem*)mem_malloc(sizeof(*inventoryItems) * inventory->capacity);
    if (inventoryItems == NULL) {
        return -1;
    }

    for (int inventoryItemIndex = 0; inventoryItemIndex < inventory->length; inventoryItemIndex++) {
        InventoryItem* inventoryItem = &(inventoryItems[inventoryItemIndex]);
        if (db_freadInt(stream, &(inventoryItem->quantity)) != 0) {
            return -1;
        }

        if (obj_load_obj(stream, &(inventoryItem->item), elevation, obj) != 0) {
            return -1;
        }
    }

    *objectPtr = obj;

    return 0;
}

// obj_save_dude
// 0x48D59C
int obj_save_dude(File* stream)
{
    int field_78 = obj_dude->sid;

    obj_dude->flags &= ~OBJECT_TEMPORARY;
    obj_dude->sid = -1;

    int rc = obj_save_obj(stream, obj_dude);

    obj_dude->sid = field_78;
    obj_dude->flags |= OBJECT_TEMPORARY;

    if (db_fwriteInt(stream, tile_center_tile) == -1) {
        db_fclose(stream);
        return -1;
    }

    return rc;
}

// obj_load_dude
// 0x48D600
int obj_load_dude(File* stream)
{
    int savedTile = obj_dude->tile;
    int savedElevation = obj_dude->elevation;
    int savedRotation = obj_dude->rotation;
    int savedOid = obj_dude->id;

    scr_clear_dude_script();

    Object* temp;
    int rc = obj_load_obj(stream, &temp, -1, NULL);

    memcpy(obj_dude, temp, sizeof(*obj_dude));

    obj_dude->flags |= OBJECT_TEMPORARY;

    scr_clear_dude_script();

    obj_dude->id = savedOid;

    scr_set_dude_script();

    int newTile = obj_dude->tile;
    obj_dude->tile = savedTile;

    int newElevation = obj_dude->elevation;
    obj_dude->elevation = savedElevation;

    int newRotation = obj_dude->rotation;
    obj_dude->rotation = newRotation;

    scr_set_dude_script();

    if (rc != -1) {
        obj_move_to_tile(obj_dude, newTile, newElevation, NULL);
        obj_set_rotation(obj_dude, newRotation, NULL);
    }

    // Set ownership of inventory items from temporary instance to dude.
    Inventory* inventory = &(obj_dude->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        inventoryItem->item->owner = obj_dude;
    }

    obj_fix_combat_cid_for_dude();

    // Dude has claimed ownership of items in temporary instance's inventory.
    // We don't need object's dealloc routine to remove these items from the
    // game, so simply nullify temporary inventory as if nothing was there.
    Inventory* tempInventory = &(temp->data.inventory);
    tempInventory->length = 0;
    tempInventory->capacity = 0;
    tempInventory->items = NULL;

    temp->flags &= ~OBJECT_FLAG_0x400;

    if (obj_erase_object(temp, NULL) == -1) {
        debug_printf("\nError: obj_load_dude: Can't destroy temp object!\n");
    }

    inven_reset_dude();

    int tile;
    if (db_freadInt(stream, &tile) == -1) {
        db_fclose(stream);
        return -1;
    }

    tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);

    return rc;
}

// 0x48D778
static int obj_create_object(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return -1;
    }

    Object* object = *objectPtr = (Object*)mem_malloc(sizeof(Object));
    if (object == NULL) {
        return -1;
    }

    memset(object, 0, sizeof(Object));

    object->id = -1;
    object->tile = -1;
    object->cid = -1;
    object->outline = 0;
    object->pid = -1;
    object->sid = -1;
    object->owner = NULL;
    object->field_80 = -1;

    return 0;
}

// This is an ugly fucking hack... handles should probably be handled at the object_ namespace, 
// to avoid this intrusion
extern void script_object_handle_remove(Object* obj);

// NOTE: Inlined.
//
// 0x48D7F8
static void obj_destroy_object(Object** objectPtr)
{
    if (objectPtr == NULL) {
        return;
    }

    if (*objectPtr == NULL) {
        return;
    }

    script_object_handle_remove(*objectPtr);
    mem_free(*objectPtr);

    *objectPtr = NULL;
}

// NOTE: Inlined.
//
// 0x48D818
static int obj_create_object_node(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return -1;
    }

    ObjectListNode* node = *nodePtr = (ObjectListNode*)mem_malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->obj = NULL;
    node->next = NULL;

    return 0;
}

// NOTE: Inlined.
//
// 0x48D84C
static void obj_destroy_object_node(ObjectListNode** nodePtr)
{
    if (nodePtr == NULL) {
        return;
    }

    if (*nodePtr == NULL) {
        return;
    }

    mem_free(*nodePtr);

    *nodePtr = NULL;
}

// 0x48D86C
static int obj_node_ptr(Object* object, ObjectListNode** nodePtr, ObjectListNode** previousNodePtr)
{
    if (object == NULL) {
        return -1;
    }

    if (nodePtr == NULL) {
        return -1;
    }

    int tile = object->tile;
    if (tile != -1) {
        *nodePtr = objectTable[tile];
    } else {
        *nodePtr = floatingObjects;
    }

    if (previousNodePtr != NULL) {
        *previousNodePtr = NULL;
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *previousNodePtr = *nodePtr;

            *nodePtr = (*nodePtr)->next;
        }
    } else {
        while (*nodePtr != NULL) {
            if (object == (*nodePtr)->obj) {
                break;
            }

            *nodePtr = (*nodePtr)->next;
        }
    }

    if (*nodePtr != NULL) {
        return 0;
    }

    return -1;
}

// 0x48D8E8
static void obj_insert(ObjectListNode* objectListNode)
{
    ObjectListNode** objectListNodePtr;

    if (objectListNode == NULL) {
        return;
    }

    if (objectListNode->obj->tile == -1) {
        objectListNodePtr = &floatingObjects;
    } else {
        Art* art = NULL;
        CacheEntry* cacheHandle = NULL;

        objectListNodePtr = &(objectTable[objectListNode->obj->tile]);

        while (*objectListNodePtr != NULL) {
            Object* obj = (*objectListNodePtr)->obj;
            if (obj->elevation > objectListNode->obj->elevation) {
                break;
            }

            if (obj->elevation == objectListNode->obj->elevation) {
                if ((obj->flags & OBJECT_FLAT) == 0 && (objectListNode->obj->flags & OBJECT_FLAT) != 0) {
                    break;
                }

                if ((obj->flags & OBJECT_FLAT) == (objectListNode->obj->flags & OBJECT_FLAT)) {
                    bool v11 = false;
                    CacheEntry* a2;
                    Art* v12 = art_ptr_lock(obj->fid, &a2);
                    if (v12 != NULL) {

                        if (art == NULL) {
                            art = art_ptr_lock(objectListNode->obj->fid, &cacheHandle);
                        }

                        // TODO: Incomplete.

                        art_ptr_unlock(a2);

                        if (v11) {
                            break;
                        }
                    }
                }
            }

            objectListNodePtr = &((*objectListNodePtr)->next);
        }

        if (art != NULL) {
            art_ptr_unlock(cacheHandle);
        }
    }

    objectListNode->next = *objectListNodePtr;
    *objectListNodePtr = objectListNode;
}

// 0x48DA58
static int obj_remove(ObjectListNode* a1, ObjectListNode* a2)
{
    if (a1->obj == NULL) {
        return -1;
    }

    if ((a1->obj->flags & OBJECT_FLAG_0x400) != 0) {
        return -1;
    }

    obj_inven_free(&(a1->obj->data.inventory));

    if (a1->obj->sid != -1) {
        exec_script_proc(a1->obj->sid, SCRIPT_PROC_DESTROY);
        scr_remove(a1->obj->sid);
    }

    if (a1 != a2) {
        if (a2 != NULL) {
            a2->next = a1->next;
        } else {
            int tile = a1->obj->tile;
            if (tile == -1) {
                floatingObjects = floatingObjects->next;
            } else {
                objectTable[tile] = objectTable[tile]->next;
            }
        }
    }

    // NOTE: Uninline.
    obj_destroy_object(&(a1->obj));

    // NOTE: Uninline.
    obj_destroy_object_node(&a1);

    return 0;
}

// 0x48DB28
static int obj_connect_to_tile(ObjectListNode* node, int tile, int elevation, Rect* rect)
{
    if (node == NULL) {
        return -1;
    }

    if (!hexGridTileIsValid(tile)) {
        return -1;
    }

    if (!elevationIsValid(elevation)) {
        return -1;
    }

    node->obj->tile = tile;
    node->obj->elevation = elevation;
    node->obj->x = 0;
    node->obj->y = 0;
    node->obj->owner = 0;

    obj_insert(node);

    if (obj_adjust_light(node->obj, 0, rect) == -1) {
        if (rect != NULL) {
            obj_bound(node->obj, rect);
        }
    }

    return 0;
}

// 0x48DC28
static int obj_adjust_light(Object* obj, int a2, Rect* rect)
{
    if (obj == NULL) {
        return -1;
    }

    if (obj->lightIntensity <= 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_HIDDEN) != 0) {
        return -1;
    }

    if ((obj->flags & OBJECT_LIGHTING) == 0) {
        return -1;
    }

    if (!hexGridTileIsValid(obj->tile)) {
        return -1;
    }

    AdjustLightIntensityProc* adjustLightIntensity = a2 ? light_subtract_from_tile : light_add_to_tile;
    adjustLightIntensity(obj->elevation, obj->tile, obj->lightIntensity);

    Rect objectRect;
    obj_bound(obj, &objectRect);

    if (obj->lightDistance > 8) {
        obj->lightDistance = 8;
    }

    if (obj->lightIntensity > 65536) {
        obj->lightIntensity = 65536;
    }

    int(*v70)[36] = light_offsets[obj->tile & 1];
    int v7 = (obj->lightIntensity - 655) / (obj->lightDistance + 1);
    int v28[36];
    v28[0] = obj->lightIntensity - v7;
    v28[1] = v28[0] - v7;
    v28[8] = v28[0] - v7;
    v28[2] = v28[0] - v7 - v7;
    v28[9] = v28[2];
    v28[15] = v28[0] - v7 - v7;
    v28[3] = v28[2] - v7;
    v28[10] = v28[2] - v7;
    v28[16] = v28[2] - v7;
    v28[21] = v28[2] - v7;
    v28[4] = v28[2] - v7 - v7;
    v28[11] = v28[4];
    v28[17] = v28[2] - v7 - v7;
    v28[22] = v28[2] - v7 - v7;
    v28[26] = v28[2] - v7 - v7;
    v28[5] = v28[4] - v7;
    v28[12] = v28[4] - v7;
    v28[18] = v28[4] - v7;
    v28[23] = v28[4] - v7;
    v28[27] = v28[4] - v7;
    v28[30] = v28[4] - v7;
    v28[6] = v28[4] - v7 - v7;
    v28[13] = v28[6];
    v28[19] = v28[4] - v7 - v7;
    v28[24] = v28[4] - v7 - v7;
    v28[28] = v28[4] - v7 - v7;
    v28[31] = v28[4] - v7 - v7;
    v28[33] = v28[4] - v7 - v7;
    v28[7] = v28[6] - v7;
    v28[14] = v28[6] - v7;
    v28[20] = v28[6] - v7;
    v28[25] = v28[6] - v7;
    v28[29] = v28[6] - v7;
    v28[32] = v28[6] - v7;
    v28[34] = v28[6] - v7;
    v28[35] = v28[6] - v7;

    for (int index = 0; index < 36; index++) {
        if (obj->lightDistance >= light_distance[index]) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int v14;
                int nextRotation = (rotation + 1) % ROTATION_COUNT;
                int eax;
                int edx;
                int ebx;
                int esi;
                int edi;
                switch (index) {
                case 0:
                    v14 = 0;
                    break;
                case 1:
                    v14 = light_blocked[rotation][0];
                    break;
                case 2:
                    v14 = light_blocked[rotation][1];
                    break;
                case 3:
                    v14 = light_blocked[rotation][2];
                    break;
                case 4:
                    v14 = light_blocked[rotation][3];
                    break;
                case 5:
                    v14 = light_blocked[rotation][4];
                    break;
                case 6:
                    v14 = light_blocked[rotation][5];
                    break;
                case 7:
                    v14 = light_blocked[rotation][6];
                    break;
                case 8:
                    v14 = light_blocked[rotation][0] & light_blocked[nextRotation][0];
                    break;
                case 9:
                    v14 = light_blocked[rotation][1] & light_blocked[rotation][8];
                    break;
                case 10:
                    v14 = light_blocked[rotation][2] & light_blocked[rotation][9];
                    break;
                case 11:
                    v14 = light_blocked[rotation][3] & light_blocked[rotation][10];
                    break;
                case 12:
                    v14 = light_blocked[rotation][4] & light_blocked[rotation][11];
                    break;
                case 13:
                    v14 = light_blocked[rotation][5] & light_blocked[rotation][12];
                    break;
                case 14:
                    v14 = light_blocked[rotation][6] & light_blocked[rotation][13];
                    break;
                case 15:
                    v14 = light_blocked[rotation][8] & light_blocked[nextRotation][1];
                    break;
                case 16:
                    v14 = light_blocked[rotation][8] | (light_blocked[rotation][9] & light_blocked[rotation][15]);
                    break;
                case 17:
                    edx = light_blocked[rotation][9];
                    edx |= light_blocked[rotation][10];
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[rotation][16];
                    ebx &= edx;
                    edx &= esi;
                    edi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][10];
                    eax = light_blocked[rotation][9];
                    edx |= edi;
                    eax &= edx;
                    v14 = ebx | eax;
                    break;
                case 18:
                    edx = light_blocked[rotation][0];
                    ebx = light_blocked[rotation][9];
                    esi = light_blocked[rotation][10];
                    edx |= ebx;
                    edi = light_blocked[rotation][11];
                    edx |= esi;
                    ebx = light_blocked[rotation][17];
                    edx |= edi;
                    ebx &= edx;
                    edx = esi;
                    esi = light_blocked[rotation][16];
                    edi = light_blocked[rotation][9];
                    edx &= esi;
                    edx |= edi;
                    edx |= ebx;
                    v14 = edx;
                    break;
                case 19:
                    edx = light_blocked[rotation][17];
                    edi = light_blocked[rotation][18];
                    ebx = light_blocked[rotation][11];
                    edx |= edi;
                    esi = light_blocked[rotation][10];
                    ebx &= edx;
                    edx = light_blocked[rotation][9];
                    edx |= esi;
                    ebx |= edx;
                    edx = light_blocked[rotation][12];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 20:
                    edx = light_blocked[rotation][2];
                    esi = light_blocked[rotation][11];
                    edi = light_blocked[rotation][12];
                    ebx = light_blocked[rotation][8];
                    edx |= esi;
                    esi = light_blocked[rotation][9];
                    edx |= edi;
                    edi = light_blocked[rotation][10];
                    ebx &= edx;
                    edx &= esi;
                    esi = light_blocked[rotation][17];
                    ebx |= edx;
                    edx = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    esi = light_blocked[rotation][19];
                    edx |= edi;
                    eax = light_blocked[rotation][11];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 21:
                    v14 = (light_blocked[rotation][8] & light_blocked[nextRotation][1])
                        | (light_blocked[rotation][15] & light_blocked[nextRotation][2]);
                    break;
                case 22:
                    edx = light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][15];
                    esi = light_blocked[rotation][21];
                    edx |= ebx;
                    ebx = light_blocked[rotation][8];
                    edx |= esi;
                    ebx &= edx;
                    edx = light_blocked[rotation][9];
                    edi = esi;
                    edx |= esi;
                    esi = light_blocked[rotation][15];
                    edx &= esi;
                    ebx |= edx;
                    edx = esi;
                    esi = light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 23:
                    edx = light_blocked[rotation][3];
                    ebx = light_blocked[rotation][16];
                    esi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    edx &= esi;
                    edi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][17];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 24:
                    edx = light_blocked[rotation][0];
                    edi = light_blocked[rotation][9];
                    ebx = light_blocked[rotation][10];
                    edx |= edi;
                    esi = light_blocked[rotation][17];
                    edx |= ebx;
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    ebx = light_blocked[rotation][16];
                    edx |= edi;
                    esi = light_blocked[rotation][16];
                    ebx &= edx;
                    edx = light_blocked[rotation][15];
                    edi = light_blocked[rotation][23];
                    edx |= esi;
                    esi = light_blocked[rotation][9];
                    edx |= edi;
                    edi = light_blocked[rotation][8];
                    edx &= esi;
                    edx |= edi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][15];
                    edi = light_blocked[rotation][23];
                    edx |= esi;
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = light_blocked[rotation][18];
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 25:
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][15];
                    ebx = light_blocked[rotation][16];
                    edx |= edi;
                    esi = light_blocked[rotation][23];
                    edx |= ebx;
                    edi = light_blocked[rotation][24];
                    edx |= esi;
                    ebx = light_blocked[rotation][9];
                    edx |= edi;
                    esi = light_blocked[rotation][1];
                    ebx &= edx;
                    edx = light_blocked[rotation][8];
                    edx &= esi;
                    edi = light_blocked[rotation][16];
                    ebx |= edx;
                    edx = light_blocked[rotation][8];
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    edi = light_blocked[rotation][24];
                    esi |= edx;
                    esi |= edi;
                    esi &= light_blocked[rotation][10];
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][17];
                    edx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][24];
                    edi = light_blocked[rotation][18];
                    edx |= esi;
                    edx &= edi;
                    esi = light_blocked[rotation][19];
                    ebx |= edx;
                    edx = light_blocked[rotation][0];
                    eax = light_blocked[rotation][24];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 26:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    eax = light_blocked[rotation][21];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][3];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 27:
                    edx = light_blocked[nextRotation][0];
                    edi = light_blocked[rotation][15];
                    esi = light_blocked[rotation][21];
                    edx |= edi;
                    edi = light_blocked[rotation][26];
                    edx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    edi = light_blocked[nextRotation][1];
                    esi &= edx;
                    edx = light_blocked[rotation][8];
                    ebx = light_blocked[rotation][15];
                    edx &= edi;
                    edx |= ebx;
                    edi = light_blocked[rotation][16];
                    esi |= edx;
                    edx = light_blocked[rotation][8];
                    eax = light_blocked[rotation][21];
                    edx |= edi;
                    eax &= edx;
                    esi |= eax;
                    v14 = esi;
                    break;
                case 28:
                    ebx = light_blocked[rotation][9];
                    edi = light_blocked[rotation][16];
                    esi = light_blocked[rotation][23];
                    edx = light_blocked[nextRotation][0];
                    ebx |= edi;
                    edi = light_blocked[rotation][15];
                    ebx |= esi;
                    esi = light_blocked[rotation][8];
                    ebx &= edi;
                    edi = light_blocked[rotation][21];
                    ebx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][16];
                    edx |= edi;
                    edx &= esi;
                    edi = light_blocked[rotation][17];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    esi = light_blocked[rotation][23];
                    edx |= edi;
                    edi = light_blocked[rotation][22];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    edx = esi;
                    edx &= light_blocked[rotation][27];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 29:
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][16];
                    ebx = light_blocked[rotation][23];
                    edx |= edi;
                    esi = light_blocked[rotation][15];
                    ebx |= edx;
                    edx = light_blocked[rotation][9];
                    edx &= esi;
                    edi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][17];
                    edx &= edi;
                    esi = light_blocked[rotation][28];
                    ebx |= edx;
                    edx = light_blocked[rotation][24];
                    edx &= esi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 30:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    eax = light_blocked[rotation][26];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][4];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 31:
                    edx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[rotation][15];
                    edx &= esi;
                    ebx = light_blocked[rotation][21];
                    edx |= edi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][26];
                    edx |= edi;
                    edx &= esi;
                    ebx |= edx;
                    edx = edi;
                    edx &= light_blocked[rotation][30];
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 32:
                    ebx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][9];
                    esi = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][28];
                    ebx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][15];
                    esi &= ebx;
                    edx = light_blocked[rotation][8];
                    edx &= light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][16];
                    esi |= edx;
                    edx = light_blocked[rotation][8];
                    edx |= ebx;
                    ebx = light_blocked[rotation][28];
                    edi = light_blocked[rotation][21];
                    ebx |= edx;
                    ebx &= edi;
                    edi = light_blocked[rotation][23];
                    ebx |= esi;
                    esi = light_blocked[rotation][22];
                    edx |= edi;
                    ebx |= esi;
                    esi = light_blocked[rotation][28];
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    edx &= edi;
                    esi = light_blocked[rotation][31];
                    ebx |= edx;
                    edx = light_blocked[rotation][0];
                    edi = light_blocked[rotation][28];
                    edx |= esi;
                    edx &= edi;
                    ebx |= edx;
                    v14 = ebx;
                    break;
                case 33:
                    esi = light_blocked[rotation][8];
                    edi = light_blocked[nextRotation][1];
                    ebx = light_blocked[rotation][15];
                    esi &= edi;
                    ebx &= light_blocked[nextRotation][2];
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = light_blocked[rotation][26];
                    ebx &= edi;
                    eax = light_blocked[rotation][30];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][5];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 34:
                    edx = light_blocked[nextRotation][2];
                    edi = light_blocked[rotation][26];
                    ebx = light_blocked[rotation][30];
                    edx |= edi;
                    esi = light_blocked[rotation][15];
                    edx |= ebx;
                    ebx = light_blocked[rotation][8];
                    edi = light_blocked[rotation][21];
                    ebx &= edx;
                    edx &= esi;
                    esi = light_blocked[rotation][22];
                    ebx |= edx;
                    edx = light_blocked[rotation][16];
                    ebx |= edi;
                    edi = light_blocked[rotation][27];
                    edx |= esi;
                    esi = light_blocked[rotation][31];
                    edx |= edi;
                    eax = light_blocked[rotation][26];
                    edx |= esi;
                    eax &= edx;
                    ebx |= eax;
                    v14 = ebx;
                    break;
                case 35:
                    ebx = light_blocked[rotation][8];
                    esi = light_blocked[nextRotation][1];
                    edi = light_blocked[nextRotation][2];
                    esi &= ebx;
                    ebx = light_blocked[rotation][15];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][3];
                    esi |= ebx;
                    ebx = light_blocked[rotation][21];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][4];
                    esi |= ebx;
                    ebx = light_blocked[rotation][26];
                    ebx &= edi;
                    edi = light_blocked[nextRotation][5];
                    esi |= ebx;
                    ebx = light_blocked[rotation][30];
                    ebx &= edi;
                    eax = light_blocked[rotation][33];
                    ebx |= esi;
                    eax &= light_blocked[nextRotation][6];
                    ebx |= eax;
                    v14 = ebx;
                    break;
                default:
                    assert(false && "Should be unreachable");
                }

                if (v14 == 0) {
                    // TODO: Check.
                    int tile = obj->tile + v70[rotation][index];
                    if (hexGridTileIsValid(tile)) {
                        bool v12 = true;

                        ObjectListNode* objectListNode = objectTable[tile];
                        while (objectListNode != NULL) {
                            if ((objectListNode->obj->flags & OBJECT_HIDDEN) == 0) {
                                if (objectListNode->obj->elevation > obj->elevation) {
                                    break;
                                }

                                if (objectListNode->obj->elevation == obj->elevation) {
                                    Rect v29;
                                    obj_bound(objectListNode->obj, &v29);
                                    rect_min_bound(&objectRect, &v29, &objectRect);

                                    v14 = (objectListNode->obj->flags & OBJECT_LIGHT_THRU) == 0;

                                    if (FID_TYPE(objectListNode->obj->fid) == OBJ_TYPE_WALL) {
                                        if ((objectListNode->obj->flags & OBJECT_FLAT) == 0) {
                                            Proto* proto;
                                            proto_ptr(objectListNode->obj->pid, &proto);
                                            if ((proto->wall.extendedFlags & 0x8000000) != 0 || (proto->wall.extendedFlags & 0x40000000) != 0) {
                                                if (rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_NE || index >= 8)
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x10000000) != 0) {
                                                if (rotation != ROTATION_NE && rotation != ROTATION_NW) {
                                                    v12 = false;
                                                }
                                            } else if ((proto->wall.extendedFlags & 0x20000000) != 0) {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && rotation != ROTATION_W
                                                    && rotation != ROTATION_NW
                                                    && (rotation != ROTATION_SW || index <= 15)) {
                                                    v12 = false;
                                                }
                                            } else {
                                                if (rotation != ROTATION_NE
                                                    && rotation != ROTATION_E
                                                    && (rotation != ROTATION_NW || index <= 7)) {
                                                    v12 = false;
                                                }
                                            }
                                        }
                                    } else {
                                        if (v14 && rotation >= ROTATION_E && rotation <= ROTATION_SW) {
                                            v12 = false;
                                        }
                                    }

                                    if (v14) {
                                        break;
                                    }
                                }
                            }
                            objectListNode = objectListNode->next;
                        }

                        if (v12) {
                            adjustLightIntensity(obj->elevation, tile, v28[index]);
                        }
                    }
                }

                light_blocked[rotation][index] = v14;
            }
        }
    }

    if (rect != NULL) {
        Rect* lightDistanceRect = &(light_rect[obj->lightDistance]);
        memcpy(rect, lightDistanceRect, sizeof(*lightDistanceRect));

        int x;
        int y;
        tile_coord(obj->tile, &x, &y, obj->elevation);
        x += 16;
        y += 8;

        x -= rect->lrx / 2;
        y -= rect->lry / 2;

        rectOffset(rect, x, y);
        rect_min_bound(rect, &objectRect, rect);
    }

    return 0;
}

// 0x48EABC
static void obj_render_outline(Object* object, Rect* rect)
{
    CacheEntry* cacheEntry;
    Art* art = art_ptr_lock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = 0;
    int frameHeight = 0;
    art_frame_width_length(art, object->frame, object->rotation, &frameWidth, &frameHeight);

    Rect clipRect;
    clipRect.ulx = 0;
    clipRect.uly = 0;
    clipRect.lrx = frameWidth - 1;

    // FIXME: I'm not sure why it ignores frameHeight and makes separate call
    // to obtain height.
    int artHeight = art_frame_length(art, object->frame, object->rotation);
    clipRect.lry = artHeight - 1;

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.ulx = object->sx;
        objectRect.uly = object->sy;
        objectRect.lrx = object->sx + frameWidth - 1;
        objectRect.lry = object->sy + frameHeight - 1;
    } else {
        int x;
        int y;
        tile_coord(object->tile, &x, &y, object->elevation);
        x += 16;
        y += 8;

        x += art->xOffsets[object->rotation];
        y += art->yOffsets[object->rotation];

        x += object->x;
        y += object->y;

        objectRect.ulx = x - frameWidth / 2;
        objectRect.uly = y - (frameHeight - 1);
        objectRect.lrx = objectRect.ulx + frameWidth - 1;
        objectRect.lry = y;

        object->sx = objectRect.ulx;
        object->sy = objectRect.uly;
    }

    Rect expandedRect;
    rectCopy(&expandedRect, rect);

    expandedRect.ulx--;
    expandedRect.uly--;
    expandedRect.lrx++;
    expandedRect.lry++;

    rect_inside_bound(&expandedRect, &buf_rect, &expandedRect);

    if (rect_inside_bound(&objectRect, &expandedRect, &objectRect) == 0) {
        clipRect.ulx += objectRect.ulx - object->sx;
        clipRect.uly += objectRect.uly - object->sy;
        clipRect.lrx = clipRect.ulx + (objectRect.lrx - objectRect.ulx);
        clipRect.lry = clipRect.uly + (objectRect.lry - objectRect.uly);

        unsigned char* src = art_frame_data(art, object->frame, object->rotation);

        unsigned char* dest = back_buf + (buf_full * object->sy + object->sx) * 4;
        int destStep = buf_full - frameWidth;

        unsigned char color;
        unsigned char* grayTable = NULL;
        unsigned char* blendTable = NULL;
        int paletted = object->outline & OUTLINE_PALETTED;
        int outlineType = object->outline & OUTLINE_TYPE_MASK;
        int cycleLen;
        int cycleInterval;

        switch (outlineType) {
        case OUTLINE_TYPE_HOSTILE:
            color = 243;
            paletted = 0;
            cycleLen = 5;
            cycleInterval = frameHeight / 5;
            break;
        case OUTLINE_TYPE_2:
            color = colorTable[31744];
            cycleInterval = 0;
            if (paletted != 0) {
                grayTable = commonGrayTable;
                blendTable = redBlendTable;
            }
            break;
        case OUTLINE_TYPE_4:
            color = colorTable[15855];
            cycleInterval = 0;
            if (paletted != 0) {
                grayTable = commonGrayTable;
                blendTable = wallBlendTable;
            }
            break;
        case OUTLINE_TYPE_FRIENDLY:
            cycleLen = 4;
            cycleInterval = frameHeight / 4;
            color = 229;
            paletted = 0;
            break;
        case OUTLINE_TYPE_ITEM:
            cycleInterval = 0;
            color = colorTable[30632];
            if (paletted != 0) {
                grayTable = commonGrayTable;
                blendTable = redBlendTable;
            }
            break;
        case OUTLINE_TYPE_32:
            color = 61;
            paletted = 0;
            cycleLen = 1;
            cycleInterval = frameHeight;
            break;
        default:
            color = colorTable[31775];
            paletted = 0;
            cycleInterval = 0;
            break;
        }

        // Use system palette (not cmap) because some outline colors like
        // fire_fast (indices 243-247) are animated via palette cycling and
        // only have live values in the system palette.
        unsigned char* pal = getSystemPalette();
        uint32_t color32 = (0xFFu << 24)
            | ((pal[color * 3 + 2] << 2) << 16)
            | ((pal[color * 3 + 1] << 2) << 8)
            | (pal[color * 3] << 2);

        uint32_t* dest32 = (uint32_t*)dest;
        uint32_t* src32 = (uint32_t*)src;
        int destStepPixels = buf_full - frameWidth;

        // Horizontal edge detection pass
        unsigned char curIdx = color;
        uint32_t curColor = color32;
        uint32_t* destRow = dest32;
        uint32_t* srcRow = src32;
        for (int y = 0; y < frameHeight; y++) {
            bool cycle = true;
            if (cycleInterval != 0) {
                if (y % cycleInterval == 0) {
                    curIdx++;
                    curColor = (0xFFu << 24)
                        | ((pal[curIdx * 3 + 2] << 2) << 16)
                        | ((pal[curIdx * 3 + 1] << 2) << 8)
                        | (pal[curIdx * 3] << 2);
                }

                if (curIdx > cycleLen + color - 1) {
                    curIdx = color;
                    curColor = color32;
                }
            }

            int destOfs = (int)(destRow - (uint32_t*)back_buf);
            for (int x = 0; x < frameWidth; x++) {
                destOfs = (int)(destRow - (uint32_t*)back_buf);
                if (*srcRow != 0 && cycle) {
                    if (x >= clipRect.ulx && x <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry && destOfs > 0 && destOfs % buf_full != 0) {
                        *(destRow - 1) = curColor;
                    }
                    cycle = false;
                } else if (*srcRow == 0 && !cycle) {
                    if (x >= clipRect.ulx && x <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry) {
                        *destRow = curColor;
                    }
                    cycle = true;
                }
                destRow++;
                srcRow++;
            }

            if (*(srcRow - 1) != 0) {
                if (destOfs < buf_full * buf_length) {
                    int v23 = frameWidth - 1;
                    if (v23 >= clipRect.ulx && v23 <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry) {
                        *destRow = curColor;
                    }
                }
            }

            destRow += destStepPixels;
        }

        // Vertical edge detection pass
        for (int x = 0; x < frameWidth; x++) {
            bool cycle = true;
            unsigned char curIdx = color;
            uint32_t curColor = color32;
            uint32_t* destCol = dest32 + x;
            uint32_t* srcCol = src32 + x;
            for (int y = 0; y < frameHeight; y++) {
                if (cycleInterval != 0) {
                    if (y % cycleInterval == 0) {
                        curIdx++;
                        curColor = (0xFFu << 24)
                            | ((pal[curIdx * 3 + 2] << 2) << 16)
                            | ((pal[curIdx * 3 + 1] << 2) << 8)
                            | (pal[curIdx * 3] << 2);
                    }

                    if (curIdx > color + cycleLen - 1) {
                        curIdx = color;
                        curColor = color32;
                    }
                }

                if (*srcCol != 0 && cycle) {
                    if (x >= clipRect.ulx && x <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry) {
                        uint32_t* above = destCol - buf_full;
                        if (above >= (uint32_t*)back_buf) {
                            *above = curColor;
                        }
                    }
                    cycle = false;
                } else if (*srcCol == 0 && !cycle) {
                    if (x >= clipRect.ulx && x <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry) {
                        *destCol = curColor;
                    }
                    cycle = true;
                }

                destCol += buf_full;
                srcCol += frameWidth;
            }

            if (srcCol[-frameWidth] != 0) {
                if ((destCol - (uint32_t*)back_buf) < buf_full * buf_length) {
                    int y = frameHeight - 1;
                    if (x >= clipRect.ulx && x <= clipRect.lrx && y >= clipRect.uly && y <= clipRect.lry) {
                        *destCol = curColor;
                    }
                }
            }
        }
    }

    art_ptr_unlock(cacheEntry);
}

// 0x48F1B0
static void obj_render_object(Object* object, Rect* rect, int light)
{
    int type = FID_TYPE(object->fid);
    if (art_get_disable(type)) {
        return;
    }

    CacheEntry* cacheEntry;
    Art* art = art_ptr_lock(object->fid, &cacheEntry);
    if (art == NULL) {
        return;
    }

    int frameWidth = art_frame_width(art, object->frame, object->rotation);
    int frameHeight = art_frame_length(art, object->frame, object->rotation);

    Rect objectRect;
    if (object->tile == -1) {
        objectRect.ulx = object->sx;
        objectRect.uly = object->sy;
        objectRect.lrx = object->sx + frameWidth - 1;
        objectRect.lry = object->sy + frameHeight - 1;
    } else {
        int objectScreenX;
        int objectScreenY;
        tile_coord(object->tile, &objectScreenX, &objectScreenY, object->elevation);
        objectScreenX += 16;
        objectScreenY += 8;

        objectScreenX += art->xOffsets[object->rotation];
        objectScreenY += art->yOffsets[object->rotation];

        objectScreenX += object->x;
        objectScreenY += object->y;

        objectRect.ulx = objectScreenX - frameWidth / 2;
        objectRect.uly = objectScreenY - (frameHeight - 1);
        objectRect.lrx = objectRect.ulx + frameWidth - 1;
        objectRect.lry = objectScreenY;

        object->sx = objectRect.ulx;
        object->sy = objectRect.uly;
    }

    if (rect_inside_bound(&objectRect, rect, &objectRect) != 0) {
        art_ptr_unlock(cacheEntry);
        return;
    }

    unsigned char* src = art_frame_data(art, object->frame, object->rotation);
    unsigned char* src2 = src;
    int v50 = objectRect.ulx - object->sx;
    int v49 = objectRect.uly - object->sy;
    src += (frameWidth * v49 + v50) * 4;
    int objectWidth = objectRect.lrx - objectRect.ulx + 1;
    int objectHeight = objectRect.lry - objectRect.uly + 1;

    if (type == 6) {
        trans_buf_to_buf(src,
            objectWidth,
            objectHeight,
            frameWidth,
            back_buf + (buf_full * objectRect.uly + objectRect.ulx) * 4,
            buf_full);
        art_ptr_unlock(cacheEntry);
        return;
    }

    if (type == 2 || type == 3) {
        if ((obj_dude->flags & OBJECT_HIDDEN) == 0 && (object->flags & OBJECT_FLAG_0xFC000) == 0) {
            Proto* proto;
            proto_ptr(object->pid, &proto);

            bool v17;
            int extendedFlags = proto->critter.extendedFlags;
            if ((extendedFlags & 0x8000000) != 0 || (extendedFlags & 0x80000000) != 0) {
                // TODO: Probably wrong.
                v17 = tile_in_front_of(object->tile, obj_dude->tile);
                if (!v17
                    || !tile_to_right_of(object->tile, obj_dude->tile)
                    || (object->flags & OBJECT_WALL_TRANS_END) == 0) {
                    // nothing
                } else {
                    v17 = false;
                }
            } else if ((extendedFlags & 0x10000000) != 0) {
                // NOTE: Uses bitwise OR, so both functions are evaluated.
                v17 = tile_in_front_of(object->tile, obj_dude->tile)
                    || tile_to_right_of(obj_dude->tile, object->tile);
            } else if ((extendedFlags & 0x20000000) != 0) {
                v17 = tile_in_front_of(object->tile, obj_dude->tile)
                    && tile_to_right_of(obj_dude->tile, object->tile);
            } else {
                v17 = tile_to_right_of(obj_dude->tile, object->tile);
                if (v17
                    && tile_in_front_of(obj_dude->tile, object->tile)
                    && (object->flags & OBJECT_WALL_TRANS_END) != 0) {
                    v17 = 0;
                }
            }

            if (v17) {
                CacheEntry* eggHandle;
                Art* egg = art_ptr_lock(obj_egg->fid, &eggHandle);
                if (egg == NULL) {
                    return;
                }

                int eggWidth;
                int eggHeight;
                art_frame_width_length(egg, 0, 0, &eggWidth, &eggHeight);

                int eggScreenX;
                int eggScreenY;
                tile_coord(obj_egg->tile, &eggScreenX, &eggScreenY, obj_egg->elevation);
                eggScreenX += 16;
                eggScreenY += 8;

                eggScreenX += egg->xOffsets[0];
                eggScreenY += egg->yOffsets[0];

                eggScreenX += obj_egg->x;
                eggScreenY += obj_egg->y;

                Rect eggRect;
                eggRect.ulx = eggScreenX - eggWidth / 2;
                eggRect.uly = eggScreenY - (eggHeight - 1);
                eggRect.lrx = eggRect.ulx + eggWidth - 1;
                eggRect.lry = eggScreenY;

                obj_egg->sx = eggRect.ulx;
                obj_egg->sy = eggRect.uly;

                Rect updatedEggRect;
                if (rect_inside_bound(&eggRect, &objectRect, &updatedEggRect) == 0) {
                    Rect rects[4];

                    rects[0].ulx = objectRect.ulx;
                    rects[0].uly = objectRect.uly;
                    rects[0].lrx = objectRect.lrx;
                    rects[0].lry = updatedEggRect.uly - 1;

                    rects[1].ulx = objectRect.ulx;
                    rects[1].uly = updatedEggRect.uly;
                    rects[1].lrx = updatedEggRect.ulx - 1;
                    rects[1].lry = updatedEggRect.lry;

                    rects[2].ulx = updatedEggRect.lrx + 1;
                    rects[2].uly = updatedEggRect.uly;
                    rects[2].lrx = objectRect.lrx;
                    rects[2].lry = updatedEggRect.lry;

                    rects[3].ulx = objectRect.ulx;
                    rects[3].uly = updatedEggRect.lry + 1;
                    rects[3].lrx = objectRect.lrx;
                    rects[3].lry = objectRect.lry;

                    for (int i = 0; i < 4; i++) {
                        Rect* v21 = &(rects[i]);
                        if (v21->ulx <= v21->lrx && v21->uly <= v21->lry) {
                            unsigned char* sp = src + (frameWidth * (v21->uly - objectRect.uly) + (v21->ulx - objectRect.ulx)) * 4;
                            dark_trans_buf_to_buf(sp, v21->lrx - v21->ulx + 1, v21->lry - v21->uly + 1, frameWidth, back_buf, v21->ulx, v21->uly, buf_full, light);
                        }
                    }

                    unsigned char* mask = art_frame_data(egg, 0, 0);
                    intensity_mask_buf_to_buf(
                        src + (frameWidth * (updatedEggRect.uly - objectRect.uly) + (updatedEggRect.ulx - objectRect.ulx)) * 4,
                        updatedEggRect.lrx - updatedEggRect.ulx + 1,
                        updatedEggRect.lry - updatedEggRect.uly + 1,
                        frameWidth,
                        back_buf + (buf_full * updatedEggRect.uly + updatedEggRect.ulx) * 4,
                        buf_full,
                        mask + (eggWidth * (updatedEggRect.uly - eggRect.uly) + (updatedEggRect.ulx - eggRect.ulx)) * 4,
                        eggWidth,
                        light);
                    art_ptr_unlock(eggHandle);
                    art_ptr_unlock(cacheEntry);
                    return;
                }

                art_ptr_unlock(eggHandle);
            }
        }
    }

    switch (object->flags & OBJECT_FLAG_0xFC000) {
    case OBJECT_TRANS_RED:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, 128, TINT_RED);
        break;
    case OBJECT_TRANS_WALL:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, 0x10000, 128, TINT_WALL);
        break;
    case OBJECT_TRANS_GLASS:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, 128, TINT_GLASS);
        break;
    case OBJECT_TRANS_STEAM:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, 96, TINT_STEAM);
        break;
    case OBJECT_TRANS_ENERGY:
        dark_translucent_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light, 128, TINT_ENERGY);
        break;
    default:
        dark_trans_buf_to_buf(src, objectWidth, objectHeight, frameWidth, back_buf, objectRect.ulx, objectRect.uly, buf_full, light);
        break;
    }

    art_ptr_unlock(cacheEntry);
}

// Updates fid according to current violence level.
//
// 0x48FA14
void obj_fix_violence_settings(int* fid)
{
    if (FID_TYPE(*fid) != OBJ_TYPE_CRITTER) {
        return;
    }

    bool shouldResetViolenceLevel = false;
    if (fix_violence_level == -1) {
        if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &fix_violence_level)) {
            fix_violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
        }
        shouldResetViolenceLevel = true;
    }

    int start;
    int end;

    switch (fix_violence_level) {
    case VIOLENCE_LEVEL_NONE:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FALL_FRONT_BLOOD_SF;
        break;
    case VIOLENCE_LEVEL_MINIMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_FIRE_DANCE_SF;
        break;
    case VIOLENCE_LEVEL_NORMAL:
        start = ANIM_BIG_HOLE_SF;
        end = ANIM_SLICED_IN_HALF_SF;
        break;
    default:
        // Do not replace anything.
        start = ANIM_COUNT + 1;
        end = ANIM_COUNT + 1;
        break;
    }

    int anim = FID_ANIM_TYPE(*fid);
    if (anim >= start && anim <= end) {
        anim = (anim == ANIM_FALL_BACK_BLOOD_SF)
            ? ANIM_FALL_BACK_SF
            : ANIM_FALL_FRONT_SF;
        *fid = art_id(OBJ_TYPE_CRITTER, *fid & 0xFFF, anim, (*fid & 0xF000) >> 12, (*fid & 0x70000000) >> 28);
    }

    if (shouldResetViolenceLevel) {
        fix_violence_level = -1;
    }
}

// 0x48FB08
static int obj_preload_sort(const void* a1, const void* a2)
{
    // 0x51979C
    static int cd_order[9] = {
        1,
        0,
        3,
        5,
        4,
        2,
        0,
        0,
        0,
    };

    int v1 = *(int*)a1;
    int v2 = *(int*)a2;

    int v3 = cd_order[FID_TYPE(v1)];
    int v4 = cd_order[FID_TYPE(v2)];

    int cmp = v3 - v4;
    if (cmp != 0) {
        return cmp;
    }

    cmp = (v1 & 0xFFF) - (v2 & 0xFFF);
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xF000) >> 12) - (((v2 & 0xF000) >> 12));
    if (cmp != 0) {
        return cmp;
    }

    cmp = ((v1 & 0xFF0000) >> 16) - (((v2 & 0xFF0000) >> 16));
    return cmp;
}

/******************************************************************************/
// Syndicate Wars Port - open-source reimplementation of SW.
/******************************************************************************/
/** @file game_data.c
 *     Memory managing routines.
 * @par Purpose:
 *     Structures and functions defining data used by the game.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     10 Feb 2020 - 16 Oct 2020
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_data.h"

#include <assert.h>
#include "globals.h"
#include "bflib_basics.h"
#include "bffile.h"
#include "bfdir.h"
#include "bffnuniq.h"
#include "bfstrut.h"
#include "campaign.h"
#include "command.h"
#include "enginlights.h"
#include "enginpriobjs.h"
#include "enginpritxtr.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "lvobjctv.h"
#include "bigmap.h"
#include "game.h"
#include "swlog.h"
#include "unix.h"
#include "vehtraffic.h"
#include "windows.h"
#include "dos.h"

static char data_path_user[DISKPATH_SIZE] = "";
static char data_path_hdd[DISKPATH_SIZE] = "";
static char game_dir_savegame[] = "qdata/savegame";
static char game_dir_screenshots[] = "qdata/screenshots";
static char game_file_text_dat[] = "data/text.dat";

/******************************************************************************/

MemSystem mem_game[] = {
  { "my_big_map",		(void **)&game_my_big_map,		18u, 16513, 0, 0, 0 },
  { "textures",			(void **)&game_textures,		18u, 4512, 0, 0, 0 },
  { "face_textures",	(void **)&game_face_textures,	16u, 4000, 0, 0, 0 },
  { "object_points",	(void **)&game_object_points,    10u, 20000, 0, 0, 0 },
  { "object_faces",		(void **)&game_object_faces,	32u, 15000, 0, 0, 0 },
  { "objects",			(void **)&game_objects,			36u, 2000, 0, 0, 0 },
  { "quick_lights",		(void **)&game_quick_lights,	6u, 64000, 0, 0, 0 },
  { "full_lights",		(void **)&game_full_lights,		32u, 4000, 0, 0, 0 },
  { "normals",			(void **)&game_normals,			16u, 10000, 0, 0, 0 },
  { "object_faces4",	(void **)&game_object_faces4,	40u, 10000, 0, 0, 0 },
  { "anim_tmaps",		(void **)&game_anim_tmaps,		54u, 66, 0, 0, 0 },
  { "traffic_nodes",	(void **)&game_traffic_nodes,	36u, 500, 0, 0, 0 },
  { "light_commands",	(void **)&game_light_commands,	36u, 1000, 0, 0, 0 },
  { "col_vects_list",	(void **)&game_col_vects_list,	6u, 26000, 0, 0, 0 },
  { "col_vects",		(void **)&game_col_vects,		14u, 13000, 0, 0, 0 },
  { "walk_headers",		(void **)&game_walk_headers,	4u, 3000, 0, 0, 0 },
  { "walk_items",		(void **)&game_walk_items,		2u, 18000, 0, 0, 0 },
  { "col_columns",		(void **)&game_col_columns,		16u, 6000, 0, 0, 0 },
  { "prim4_textures",	(void **)&prim4_textures,		18u, 1200, 0, 0, 0 },
  { "prim_face_textures", (void **)&prim_face_textures,	16u, 500, 0, 0, 0 },
  { "prim_object_points", (void **)&prim_object_points,	10u, 2000, 0, 0, 0 },
  { "prim_object_faces", (void **)&prim_object_faces,	32u, 800, 0, 0, 0 },
  { "prim_object_faces4", (void **)&prim_object_faces4,	40u, 1300, 0, 0, 0 },
  { "prim_objects",		(void **)&prim_objects,			36u, 60, 0, 0, 0 },
  { "special_object_faces", (void **)&game_special_object_faces, 32u, 1400, 0, 0, 0 },
  { "special_object_faces4",(void **)&game_special_object_faces4, 40u, 1400, 0, 0, 0 },
  { "floor_tiles",		(void **)&game_floor_tiles,		39u, 18000, 0, 0, 0 },
  { "used_objectives",	(void **)&game_used_objectives,	32u, 1200, 0, 0, 0 },
  { "objectives",		(void **)&game_objectives,		32u, 1200, 0, 0, 0 },
  { "user_heap",		(void **)&game_user_heap,		1u, 292228, 0, 0, 0 },
  { "screen_point_pool",(void **)&game_screen_point_pool,8u, 45000, 0, 0, 0 },
  { "draw_list",		(void **)&game_draw_list,		5u, 45001, 0, 0, 0 },
  { "sort_sprites",		(void **)&game_sort_sprites,	16u, 4001, 0, 0, 0 },
  { "sort_lines",		(void **)&game_sort_lines,		11u, 4001, 0, 0, 0 },
  { "commands",			(void **)&game_commands,		32u, 3100, 0, 0, 0 },
  { "bez edit",			(void **)&bezier_pts,			28u, 2000, 0, 0, 0 },
  { "spare map buffer",	(void **)&spare_map_buffer,		1u, 101, 0, 0, 1 },
  { "used_lvl_objectives", (void **)&game_used_lvl_objectives,32u, 20, 0, 0, 0 },
  { "level_miscs",		(void *)&game_level_miscs,		22u, 200, 0, 0, 0 },
  { NULL,				NULL,							0u, 0, 0, 0, 0 }
};

const char *
GetDirectoryUser(void)
{
    if (data_path_user[0] == '\0')
    {
        if (!sys_get_user_path(data_path_user, sizeof(data_path_user)))
        {
            snprintf(data_path_user, sizeof(data_path_user), ".");
        }
        LOGDBG("Dir for user files '%s'", data_path_user);
        //char path_create[DISKPATH_SIZE];
        //snprintf(path_create, sizeof(path_create), "%s/%s", data_path_user, game_dir_savegame);
        LbDirectoryMake(game_dir_savegame, true);
        //snprintf(path_create, sizeof(path_create), "%s/%s", data_path_user, game_dir_screenshots);
        LbDirectoryMake(game_dir_screenshots, true);
    }
    return data_path_user;
}

const char *
GetDirectoryHdd(void)
{
    if (data_path_hdd[0] == '\0')
    {
        if (!sys_get_data_path(data_path_hdd, sizeof(data_path_hdd)))
        {
            snprintf(data_path_hdd, sizeof(data_path_hdd), "%s", ".");
        }
        LOGDBG("Dir with HDD data '%s'",data_path_hdd);
    }
    return data_path_hdd;
}


const char *
GetDirectoryCd(void)
{
    return cd_drive;
}

int
GetDirPlaceFromPath(const char *fname)
{
    char *last_fs_sep;
    int path_len;
    int dir_place;

    last_fs_sep = strrchr(fname, '/');
    if (last_fs_sep == NULL)
        last_fs_sep = strrchr(fname, '\\');
    if (last_fs_sep == NULL)
        return DirPlace_None;
    path_len = (int)(last_fs_sep - fname);
    // First pass - try matching whole path
    for (dir_place = 0; dir_place < (int)(sizeof(game_dirs)/sizeof(game_dirs[0])); dir_place++)
    {
        PathInfo *pinfo;
        pinfo = &game_dirs[dir_place];
        if (pinfo->directory != NULL) {
            if (strncasecmp(fname, pinfo->directory, path_len) == 0)
                return dir_place;
        }
    }
    // Second pass - match starting part of the path
    for (dir_place = 0; dir_place < (int)(sizeof(game_dirs)/sizeof(game_dirs[0])); dir_place++)
    {
        PathInfo *pinfo;
        pinfo = &game_dirs[dir_place];
        if (pinfo->directory != NULL) {
            path_len = strlen(pinfo->directory);
            if (strncasecmp(fname, pinfo->directory, path_len) == 0) {
                if ((fname[path_len] == '/') || (fname[path_len] == '\\'))
                    return dir_place;
            }
        }
    }
    return DirPlace_None;
}

static void
replace_backslash_with_fs_separator(char *path)
{
    size_t n;
    for (n = 0; path[n] != '\0'; n++)
    {
#if FS_SEP == '\\'
        if (path[n] == '/')
#else
        if (path[n] == '\\')
#endif
            path[n] = FS_SEP;
    }
}

void
SyndFileNameTransform(char *out_fname, const char *inp_fname)
{
    char fs_fname[DISKPATH_SIZE];
    int dir_place;
    const char *base_dir;

    // Figure out whether the base folder should be data folder, user folder or CD
    dir_place = GetDirPlaceFromPath(inp_fname);
    if ( (dir_place == DirPlace_QData) && (strncasecmp(inp_fname, game_dir_savegame, strlen(game_dir_savegame)) == 0) ) {
        base_dir = GetDirectoryUser();
    }
    else if ( (dir_place == DirPlace_QData) && (strncasecmp(inp_fname, game_dir_screenshots, strlen(game_dir_screenshots)) == 0) ) {
        base_dir = GetDirectoryUser();
    }
    else if (dir_place != DirPlace_None) {
        if (game_dirs[dir_place].use_cd) {
            base_dir = GetDirectoryCd();
        } else {
            base_dir = GetDirectoryHdd();
        }
    } else {
        base_dir = GetDirectoryHdd();
    }

    // Special file name switch for using language-specific files from CD
    if ( (dir_place == DirPlace_Data) && game_dirs[dir_place].use_cd &&
      (strcasecmp(inp_fname, game_file_text_dat) == 0) ) {
        snprintf(fs_fname, DISKPATH_SIZE, "language/%s/text.dat", language_3str);
    } else {
        strncpy(fs_fname, inp_fname, DISKPATH_SIZE);
    }
    // Switch the input folder separators to proper ones for current os
    replace_backslash_with_fs_separator(fs_fname);
    // Add base path only if the input one is not absolute
    if (fs_fname[0] == FS_SEP || (strlen(fs_fname) >= 2 && fs_fname[1] == ':')) {
        snprintf (out_fname, DISKPATH_SIZE, "%s", fs_fname);
    } else {
        snprintf(out_fname, DISKPATH_SIZE, "%s" FS_SEP_STR "%s", base_dir, fs_fname);
    }
}

void
setup_file_names(void)
{
    lbFileNameTransform = SyndFileNameTransform;
    // This fills the path variable; for user, it also creates the folder
    GetDirectoryHdd();
    GetDirectoryUser();
}

int get_highest_used_packet_record_no(int campgn, int missi)
{
    char fname[DISKPATH_SIZE];
    sprintf(fname, "%s/rc%02dm%03d.s*", game_dir_savegame, campgn, missi);
    return get_highest_file_no(fname, strrchr(fname, '*') - fname, game_dir_savegame);
}

void get_packet_record_fname(char *fname, int campgn, int missi, int file_no)
{
    sprintf(fname, "%s/rc%02dm%03d.s%02d", game_dir_savegame, campgn, missi, file_no);
}

void get_user_settings_fname(char *fname, const char *name)
{
    if (strlen(name) == 0)
        name = "default";
    sprintf(fname, "%s/%.8s.ini", game_dir_savegame, name);
    LbStringToLower(fname+strlen(game_dir_savegame));
}

void get_saved_game_fname(char *fname, ushort slot)
{
    if (slot == 0)
        sprintf(fname, "%s/synwarsm.sav", game_dir_savegame);
    else if (slot < 9)
        sprintf(fname, "%s/synwars%d.sav", game_dir_savegame, (int)slot - 1);
    else
        sprintf(fname, "%s/swars%03d.sav", game_dir_savegame, (int)slot - 1);
}

int get_memory_ptr_index(void **mgptr)
{
    int i;
    for (i = 0; mem_game[i].Name != NULL; i++)
    {
        if (mem_game[i].BufferPtr == mgptr)
            return i;
    }
    return -1;
}

long get_memory_ptr_allocated_count(void **mgptr)
{
    int i;
    for (i = 0; mem_game[i].Name != NULL; i++)
    {
        if (mem_game[i].BufferPtr == mgptr)
            return mem_game[i].N;
    }
    return -1;
}

TbBool mem_game_index_is_prim(int index)
{
    static int prim_first = 18;
    static int prim_last = 23;

    return (index >= prim_first && index <= prim_last);
}

/** Decrease the size of some arrays to reduce memory usage.
 */
void adjust_memory_use(void)
{
    if (ingame.LowerMemoryUse == 1)
    {
        mem_game[35].N = 1; // bez_edit
        mem_game[28].N = 1; // objectives
        mem_game[36].N = 1; // spare_map_buffer
        mem_game[27].N = 1000; // used_objectives
        mem_game[26].N = 1124; // floor_tiles
        mem_game[31].N = 2500; // draw_list
        mem_game[32].N = 1000; // sort_sprites
        mem_game[33].N = 700; // sort_lines
        mem_game[30].N = 3000; // screen_point_pool
        if ( is_single_game || cmdln_param_bcg )
        {
            mem_game[20].N = 2000; // prim_object_points
            mem_game[21].N = 2000; // prim_object_faces increase(?)
            mem_game[22].N = 2000; // prim_object_faces4 increase(?)
        }
        mem_game[4].N = 11000; // object_faces
        mem_game[9].N = 11000; // object_faces4 increase(?)
        mem_game[5].N = 1500; // objects
        mem_game[7].N = 1000; // full_lights
        mem_game[13].N = 16000; // col_vects_list
        mem_game[14].N = 9000; // col_vects
        engine_mem_alloc_size = 2700000;
        game_perspective = (mem_game[5].N >> 8) & 0xff; // = 5
    }
}

uint memory_table_entries(MemSystem *mem_table)
{
    MemSystem *ment;
    uint i;

    i = 0;
    ment = mem_table;
    while (ment->BufferPtr != NULL) {
        ment++;
        i++;
    }
    return i;
}

void init_memory(MemSystem *mem_table)
{
#if 0
    asm volatile ("call ASM_init_memory\n"
        :  : "a" (mem_table));
#endif
    MemSystem *ment;
    int mem_table_len;
    ulong totlen;
    ubyte *p;
    int i;
    ulong k;

    totlen = 8192;
    mem_table_len = memory_table_entries(mem_table);

    p = scratch_malloc_mem;
    for (i = mem_table_len - 1; i >= 0; i--)
    {
        ment = &mem_table[i];
        if (i == 17)
            p = engine_mem_alloc_ptr + totlen;

        if (ment->PrivBuffer == NULL)
        {
            if ((engine_mem_alloc_size & 0x80000000) == 0)
              ment->PrivBuffer = engine_mem_alloc_ptr + totlen;
            else
              exit_game = 1;

            if (ment->N * (ulong)ment->ESize >= dword_1810D5 || mem_game_index_is_prim(i))
            {
                k = ment->N * ment->ESize;
                k = (k + 4) & ~0x3;
                totlen += k;
                assert(totlen <= engine_mem_alloc_size);
            }
            else
            {
                ment->PrivBuffer = dword_1810D1;
                k = ment->N * ment->ESize;
                k = (k + 4) & ~0x3;
                dword_1810D5 -= k;
                dword_1810D1 += k;
            }
            *(ment->BufferPtr) = ment->PrivBuffer;
        }
    }
    scratch_malloc_mem = p;
    memset(game_sort_sprites, 0, 0x20u);
    fade_data = *ment[23].BufferPtr;
}

/******************************************************************************/

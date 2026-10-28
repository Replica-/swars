/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file lvdraw3d.h
 *     Header file for lvdraw3d.c.
 * @par Purpose:
 *     Routines for level and map drawing using 3D rendering.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     24 Dec 2023 - 10 Nov 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef LVDRAW3D_H
#define LVDRAW3D_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)


#pragma pack()
/******************************************************************************/
extern ubyte byte_1C8444;

void clear_super_quick_lights(void);
void apply_super_quick_light(short lx, short lz, ushort b);

void lvdraw_do_floor(void);
void engine_draw_whole_screen_flyby(void);

void engine_draw_whole_screen_top_down(void);

/** Starts a spray of blood from a hit person, drawn by the GPU: drops which fly in the direction of
 * the shot (map coordinates; the direction is on the ground), and leave splatters where they land.
 * @return False if the GPU is not drawing the scene; the caller then makes the plain blood sparks. */
TbBool blood_fx_spawn(int x, int y, int z, int dirx, int dirz, int amount);
/** Starts the sparks of a ricochet, drawn by the GPU: hot streaks which fly back from the surface (the direction
 * is on the ground, from the surface toward where the shot came from), and a small light which fades.
 * @return False if the GPU is not drawing the scene; the caller then makes the plain sparks. */
TbBool spark_fx_spawn(int x, int y, int z, int dirx, int dirz, int amount);

void prepare_drawlist(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

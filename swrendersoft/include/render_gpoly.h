/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file render_gpoly.h
 *     Header file for render_gpoly.c.
 * @par Purpose:
 *     Wrappers for rendering functions for drawing 3D view elements.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     29 Oct 2023 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef RENDER_GPOLY_H
#define RENDER_GPOLY_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

#define SINGLE_TEXTURE_DIM 32

struct PolyPoint;

#pragma pack()
/******************************************************************************/

void gpoly_enable_pentium_pro(TbBool state);
void draw_trigpoly(struct PolyPoint *point_a, struct PolyPoint *point_b,
  struct PolyPoint *point_c);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

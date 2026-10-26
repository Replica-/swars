/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file engindrwlstx_tng.h
 *     Header file for engindrwlstx_tng.c.
 * @par Purpose:
 *     Drawlists execution for the 3D engine, items related to things.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     22 Apr 2024 - 12 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef ENGINDRWLSTX_TNG_H
#define ENGINDRWLSTX_TNG_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

#pragma pack(1)

struct Thing;

#pragma pack()
/******************************************************************************/

// Drawing outside of the drawlist
void number_player(struct Thing *p_person, ubyte n);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

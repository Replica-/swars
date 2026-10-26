/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_line.h
 *     Header file for app_hvline.c.
 * @par Purpose:
 *     App-specific line drawing with soft transparency.
 * @par Comment:
 *     This is a modification of `bfline.h` from bflibrary.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef APP_LINE_H_
#define APP_LINE_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

TbResult LbDrawHVLineLowTransGrey(s32 X1, s32 Y1, s32 X2, s32 Y2, TbPixel colour);

#ifdef __cplusplus
};
#endif

#endif // APP_LINE_H_
/******************************************************************************/

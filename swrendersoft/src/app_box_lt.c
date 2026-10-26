/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_box_lt.c
 *     Implementation of related functions.
 * @par Purpose:
 *     App-specific box drawing with soft transparency.
 * @par Comment:
 *     This is a modification of `gbox.c` from bflibrary.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 02 Oct 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "app_box.h"

#include <string.h>
#include <assert.h>
#include "bfbox.h"
#include "bfscreen.h"
#include "bfline.h"

#include "app_line.h"
#include "app_sprite.h"

/******************************************************************************/

TbResult AppDrawBoxLowTransGreyClip(s32 destX, s32 destY, u32 width, u32 height,
  TbPixel colour)
{
    short cx, cy;
    short ch;

    cx = destX;
    cy = destY;
    for (ch = height; ch > 0; ch--)
    {
        LbDrawHVLineLowTransGrey(cx, cy, cx + width, cy, colour);
        ++cy;
    }
    return Lb_SUCCESS;
}

TbResult AppDrawBoxLowTransGrey(s32 X, s32 Y, s32 Width, s32 Height, TbPixel colour)
{
    if ((lbDisplay.DrawFlags & (Lb_SPRITE_TRANSPAR4|Lb_SPRITE_TRANSPAR8)) != 0)
    {
        return AppDrawBoxLowTransGreyClip(X, Y, Width, Height, colour);
    } else
    {
        return LbDrawBox(X, Y, Width, Height, colour);
    }
}

TbResult AppDrawSlantBoxLowTransGreyClip(s32 destX, s32 destY, u32 width, u32 height,
  TbPixel colour)
{
    short cx, cy;
    short ch;

    cx = destX;
    cy = destY;
    for (ch = height; ch > 0; ch--)
    {
        LbDrawHVLineLowTransGrey(cx, cy, cx + width, cy, colour);
        --cx;
        ++cy;
    }
    return Lb_SUCCESS;
}

TbResult AppDrawSlantBoxLowTransGrey(s32 X, s32 Y, s32 Width, s32 Height, TbPixel colour)
{
    if ((lbDisplay.DrawFlags & (Lb_SPRITE_TRANSPAR4|Lb_SPRITE_TRANSPAR8)) != 0)
    {
        return AppDrawSlantBoxLowTransGreyClip(X, Y, Width, Height, colour);
    } else
    {
        return AppDrawSlantBox(X, Y, Width, Height, colour);
    }
}

TbResult AppDrawVSlantBoxLowTransGreyClip(s32 destX, s32 destY, s32 width, s32 height,
  TbPixel colour)
{
    s32 cx, cy;
    s32 cw;

    cx = destX;
    cy = destY;
    for (cw = width; cw > 0; cw--)
    {
        LbDrawHVLineLowTransGrey(cx, cy, cx, cy + height, colour);
        ++cx;
        ++cy;
    }
    return Lb_SUCCESS;
}

TbResult AppDrawVSlantBoxLowTransGrey(s32 X, s32 Y, s32 Width, s32 Height, TbPixel colour)
{
    if ((lbDisplay.DrawFlags & (Lb_SPRITE_TRANSPAR4|Lb_SPRITE_TRANSPAR8)) != 0)
    {
        return AppDrawVSlantBoxLowTransGreyClip(X, Y, Width, Height, colour);
    } else
    {
        return AppDrawVSlantBox(X, Y, Width, Height, colour);
    }
}

/******************************************************************************/

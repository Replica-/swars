/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_box.c
 *     Implementation of related functions.
 * @par Purpose:
 *     App-specific box drawing, for slanted box at 45 degrees.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "bfscreen.h"
#include "bfline.h"

/******************************************************************************/

TbResult AppDrawSlantBoxClip(s32 destX, s32 destY, s32 width, s32 height,
  TbPixel colour)
{
    s32 cx, cy;
    s32 ch;

    cx = destX;
    cy = destY;
    if (height >= 0)
    {
        for (ch = height; ch > 0; ch--) {
            LbDrawLine(cx, cy, cx + width, cy, colour);
            --cx;
            ++cy;
        }
    }
    else
    {
        for (ch = height; ch < 0; ch++) {
            LbDrawLine(cx, cy, cx + width, cy, colour);
            ++cx;
            --cy;
        }
    }
    return Lb_SUCCESS;
}

TbResult AppDrawSlantBoxOutline(s32 destX, s32 destY, s32 width, s32 height,
  TbPixel colour)
{
    s32 dtWidth;

    if (width == 0)
        width++;
    if (height == 0)
        height++;

    // Skew at 45 degrees
    if (height >= 0)
        dtWidth = height - 1;
    else
        dtWidth = height + 1;

    LbDrawHVLine(destX, destY,
      destX + width, destY, colour);
    if (height > 1)
        LbDrawHVLine(destX - dtWidth, destY + height - 1,
          destX + width - dtWidth, destY + height - 1, colour);
    else if (height < -1)
        LbDrawHVLine(destX - dtWidth, destY + height + 1,
          destX + width - dtWidth, destY + height + 1, colour);
    if (height > 2)
    {
        LbDrawLine(destX - 1, destY + 1,
          destX + 1 - dtWidth, destY + height - 2, colour);
        if (width != 1)
            LbDrawLine(destX + width - 1, destY + 1,
              destX + width - dtWidth + 1, destY + height - 2, colour);
    }
    else if (height < -2)
    {
        LbDrawLine(destX + 1, destY - 1,
          destX - 1 - dtWidth, destY + height + 2, colour);
        if (width != 1)
            LbDrawLine(destX + width + 1, destY - 1,
              destX + width - dtWidth - 1, destY + height + 2, colour);
    }
    return Lb_SUCCESS;
}

TbResult AppDrawSlantBox(s32 X, s32 Y, s32 Width, s32 Height, TbPixel colour)
{
    TbResult ret;
    if (lbDisplay.DrawFlags & Lb_SPRITE_OUTLINE)
    {
        ret = AppDrawSlantBoxOutline(X, Y, Width, Height, colour);
    } else
    {
        ret = AppDrawSlantBoxClip(X, Y, Width, Height, colour);
    }
    return ret;
}

TbResult AppDrawVSlantBoxClip(s32 destX, s32 destY, s32 width, s32 height,
  TbPixel colour)
{
    s32 cx, cy;
    s32 cw;

    cx = destX;
    cy = destY;
    for (cw = width; cw > 0; cw--)
    {
        LbDrawLine(cx, cy, cx, cy + height, colour);
        ++cx;
        ++cy;
    }
    return Lb_SUCCESS;
}

TbResult AppDrawVSlantBoxOutline(s32 destX, s32 destY, s32 width, s32 height,
  TbPixel colour)
{
    s32 dtHeight;

    if (width == 0)
        width++;
    if (height == 0)
        height++;

    dtHeight = width - 1; // Skew at 45 degrees
    LbDrawHVLine(destX, destY,
      destX, destY + height, colour);
    if (height != 1)
        LbDrawHVLine(destX + width - 1, destY + dtHeight,
          destX + width - 1, destY + height + dtHeight, colour);
    if (abs(width) > 2)
    {
        LbDrawLine(destX + 1, destY + 1,
          destX + width - 2, destY + dtHeight - 1, colour);
        if (width != 1)
            LbDrawLine(destX + 1, destY + height + 1,
              destX + width - 2, destY + height + dtHeight - 1, colour);
    }
    return Lb_SUCCESS;
}

TbResult AppDrawVSlantBox(s32 X, s32 Y, s32 Width, s32 Height, TbPixel colour)
{
    TbResult ret;
    if (lbDisplay.DrawFlags & Lb_SPRITE_OUTLINE)
    {
        ret = AppDrawVSlantBoxOutline(X, Y, Width, Height, colour);
    } else
    {
        ret = AppDrawVSlantBoxClip(X, Y, Width, Height, colour);
    }
    return ret;
}

/******************************************************************************/

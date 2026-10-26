/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_hvline_lt.c
 *     Implementation of related functions.
 * @par Purpose:
 *     App-specific line drawing with soft transparency.
 * @par Comment:
 *     This is a modification of `hvline.c` from bflibrary.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <string.h>
#include <assert.h>
#include "bfscreen.h"

#include "app_line.h"
#include "app_sprite.h"

TbResult LbDrawHVLineLowTransGrey(s32 X1, s32 Y1, s32 X2, s32 Y2, TbPixel colour)
{
    s32 clipX1, clipX2, clipY1, clipY2;
    s32 maxX, maxY, x, y;
    ubyte *ptr;
    ubyte *m;

    // Sort the points on X axis
    if (X1 > X2)
    {
        clipX1 = X2;
        clipX2 = X1;
    }
    else
    {
        clipX1 = X1;
        clipX2 = X2;
    }
    // Clip them
    maxX = lbDisplay.GraphicsWindowWidth - 1;
    if (clipX2 < 0)
        return Lb_FAIL;
    if (clipX1 > maxX)
        return Lb_FAIL;
    if (clipX1 < 0)
        clipX1 = 0;
    if (clipX2 > maxX)
        clipX2 = maxX;

    // Sort the points on Y axis
    if (Y1 > Y2)
    {
        clipY1 = Y2;
        clipY2 = Y1;
    }
    else
    {
        clipY1 = Y1;
        clipY2 = Y2;
    }
    // Clip them
    maxY = lbDisplay.GraphicsWindowHeight - 1;
    if (clipY2 < 0)
        return Lb_FAIL;
    if (clipY1 > maxY)
        return Lb_FAIL;
    if (clipY1 < 0)
        clipY1 = 0;
    if (clipY2 > maxY)
        clipY2 = maxY;

    ptr = &lbDisplay.GraphicsWindowPtr[clipX1 + lbDisplay.GraphicsScreenWidth * clipY1];
    if (clipX2 == clipX1)
    { // Vertical line
        y = clipY2 - clipY1 + 1;
        assert(y > 0);
        if (lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR4)
        {
            m = lbDisplay.FadeTable;
            for (; y > 0; y--)
            {
                *ptr = LbBlendPixelLowTrans4Remap(m, 1, 1, colour, *ptr);
                ptr += lbDisplay.GraphicsScreenWidth;
            }
        }
        else if (lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR8)
        {
            m = lbDisplay.FadeTable;
            for (; y > 0; y--)
            {
                *ptr = LbBlendPixelLowTrans8Remap(m, 1, 1, colour, *ptr);
                ptr += lbDisplay.GraphicsScreenWidth;
            }
        }
        else
        {
            for (; y > 0; y--)
            {
                *ptr = colour;
                ptr += lbDisplay.GraphicsScreenWidth;
            }
        }
    }
    else
    { // Horizonal line
        x = clipX2 - clipX1 + 1;
        assert(x > 0);
        if (lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR4)
        {
            m = lbDisplay.FadeTable;
            for (; x > 0; x--)
            {
                *ptr = LbBlendPixelLowTrans4Remap(m, 1, 1, colour, *ptr);
                ptr++;
            }
        }
        else if (lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR8)
        {
            m = lbDisplay.FadeTable;
            for (; x > 0; x--)
            {
                *ptr = LbBlendPixelLowTrans8Remap(m, 1, 1, colour, *ptr);
                ptr++;
            }
        }
        else
        {
            memset(ptr, colour, x);
        }
    }
    return Lb_SUCCESS;
}

/******************************************************************************/

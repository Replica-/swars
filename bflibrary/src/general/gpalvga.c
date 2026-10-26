/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file gpalvga.c
 *     Implementation of related functions.
 * @par Purpose:
 *     Generate the default VGA palette data.
 * @par Comment:
 *     Constructs the standard VGA palette and outputs into a buffer.
 *     Algorithm based on `vgapal.c` from Noah Johnson <canidlogic@github>.
 * @author   Noah Johnson
 * @author   Tomasz Lis
 * @date     05 Oct 2017 - 18 Dec 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bfpalette.h"

#include "privbflog.h"

/**
 * Add another color entry to the palette.
 *
 * All palette entries added here must use 64-level values.
 *
 * @param r the red channel value
 * @param g the green channel value
 * @param b the blue channel value
 */
static void addColor(ubyte *pal, short col, short r, short g, short b)
{
    uint i = 0;

    /* Check ranges */
    if ((r < 0) || (r > 63) ||
      (g < 0) || (g > 63) ||
      (b < 0) || (b > 63)) {
        LOGWARN("outranged colour value in (%hd,%hd,%hd) at index %hd", r, g, b, col);
    }

    if ((col >= 256) || (col < 0)) {
        LOGWARN("outranged colour index %hd", col);
        return;
    }

    /* Calculate starting offset in palette */
    i = col * 3;

    /* Write the color to palette */
    pal[i + 0] = (ubyte)r;
    pal[i + 1] = (ubyte)g;
    pal[i + 2] = (ubyte)b;
}

/**
 * Add a grayscale color to the palette.
 *
 * This calls through to addColor with all channel values equal to
 * the provided v.
 *
 * @param v the grayscale value
 */
static void addGrayColor(ubyte *pal, short col, short v)
{
    addColor(pal, col, v, v, v);
}

/**
 * Add the 16 color entries of the standard 16-color palette.
 *
 * @param lo the low intensity value to use
 * @param melo the medium-low intensity value to use
 * @param mehi the medium-high intensity value to use
 * @param hi the high intensity value to use
 */
static void add16Color(ubyte *pal, short col, short lo, short melo, short mehi, short hi)
{
    short r = 0;
    short g = 0;
    short b = 0;
    short i = 0;
    short h = 0;
    short l = 0;

    /* The passed values must be in 64-level range */
    if ((lo < 0) || (lo > 63) ||
      (melo < 0) || (melo > 63) ||
      (mehi < 0) || (mehi > 63) ||
      (hi < 0) || (hi > 63)) {
        LOGWARN("outranged intensity at index %hd", col);
    }

    /* Generate each of the 16 values */
    for(i = 0; i < 16; i++)
    {
        /* Determine whether low-intensity series or high-intensity */
        if ((i & 8) == 8) {
            /* Intensity bit set -- define intense range */
            h = hi;
            l = melo;
        } else {
            /* Intensity bit clear -- define low-intensity range */
            h = mehi;
            l = lo;
        }

        /* Start by setting each channel to low value */
        r = l;
        g = l;
        b = l;

        /* Activate channels selected by the appropriate bit */
        if ((i & 4) == 4) {
            r = h;
        }
        if ((i & 2) == 2) {
            g = h;
        }
        if ((i & 1) == 1) {
            b = h;
        }

        /* As an exceptional case, for index 6 only, change the green
           component to medium-low intensity so that we get brown instead
           of dark yellow */
        if (i == 6) {
            g = melo;
        }

        /* Add this color */
        addColor(pal, col + i, r, g, b);
    } 
}

/**
 * Add four colors to the palette corresponding to a "run" within an
 * RGB cycle.
 *
 * start, ch, and the return value are encoded such that only the three
 * least significant bits are used.  The most significant of these
 * corresponds to red, the next to green, and the least significant to
 * blue.
 *
 * Intensity values must be 64-level.
 *
 * @param start high and low starting states for each channel at the start
 *   of the run
 * @param ch the channel to change from high to low or low to high (only
 *   one bit may be set)
 * @param lo the low intensity value
 * @param melo the medium-low intensity value
 * @param me the medium intensity value
 * @param mehi the medium-high intensity value
 * @param hi the high intensity value
 *
 * @return the high and low starting states for each channel at the start
 *   of the run after this one
 */
static short addRun(ubyte *pal, short col, short start, short ch, short lo,
   short melo, short me, short mehi, short hi)
{
    short r = 0;
    short g = 0;
    short b = 0;
    short i = 0;
    short up = 0;
    short v = 0;

    /* Check parameters */
    if ((start < 0) || (start > 7)) {
        LOGWARN("unsupported start state at index %hd", col);
    }
    if ((ch != 1) && (ch != 2) && (ch != 4)) {
        LOGWARN("unsupported channel at index %hd", col);
    }
    if ((lo < 0) || (lo > 63) ||
      (melo < 0) || (melo > 63) ||
      (me < 0) || (me > 63) ||
      (mehi < 0) || (mehi > 63) ||
      (hi < 0) || (hi > 63)) {
        LOGWARN("outranged intensity at index %hd", col);
    }

    /* Get the starting RGB color and add it */
    r = lo;
    g = lo;
    b = lo;

    if ((start & 4) == 4) {
      r = hi;
    }
    if ((start & 2) == 2) {
      g = hi;
    }
    if ((start & 1) == 1) {
      b = hi;
    }

    addColor(pal, col + 0, r, g, b);

    /* If selected channel starts high, we're going down; else, we're
       going up */
    if ((start & ch) == ch) {
        up = 0;
    } else {
        up = 1;
    }

    /* Add remaining three colors of the run */
    for(i = 0; i < 3; i++)
    {
        /* Value depends if we're going up or down */
        if (up) {
            /* Going up */
            if (i == 0) {
              v = melo;
            } else if (i == 1) {
              v = me;
            } else {
              v = mehi;
            }
        } else {
            /* Going down */
            if (i == 0) {
              v = mehi;
            } else if (i == 1) {
              v = me;
            } else {
              v = melo;
            }
        }

        /* Adjust the proper channel */
        if (ch == 4) {
          r = v;
        } else if (ch == 2) {
          g = v;
        } else {
          b = v;
        }

        /* Add the color */
        addColor(pal, col + 1 + i, r, g, b);
    }

    /* The next run starts at the starting position of this run, with the
       channel we just handled flipped */
    return (start ^ ch);
}

/**
 * Add a 24-color RGB cycle to the palette.
 *
 * A cycle consists of six 4-color runs, each of which transitions from
 * one hue to another until arriving back at starting position.
 *
 * Intensities must be provided in 64-level scale.
 *
 * @param lo low intensity value
 * @param melo medium-low intensity value
 * @param me medium intensity value
 * @param mehi medium-high intensity value
 * @param hi high intensity value
 */
static void addCycle(ubyte *pal, short col, short lo, short melo, short me, short mehi, short hi)
{
    short hue = 0;

    /* Check parameters */
    if ((lo < 0) || (lo > 63) ||
      (melo < 0) || (melo > 63) ||
      (me < 0) || (me > 63) ||
      (mehi < 0) || (mehi > 63) ||
      (hi < 0) || (hi > 63)) {
        LOGWARN("outranged intensity at index %hd", col);
    }

    /* We start out at blue */
    hue = 1;

    /* Add each run, updating the hue each time */
    hue = addRun(pal, col, hue, 4, lo, melo, me, mehi, hi);
    col += 4;
    hue = addRun(pal, col, hue, 1, lo, melo, me, mehi, hi);
    col += 4;
    hue = addRun(pal, col, hue, 2, lo, melo, me, mehi, hi);
    col += 4;

    hue = addRun(pal, col, hue, 4, lo, melo, me, mehi, hi);
    col += 4;
    hue = addRun(pal, col, hue, 1, lo, melo, me, mehi, hi);
    col += 4;
    hue = addRun(pal, col, hue, 2, lo, melo, me, mehi, hi);
    col += 4;
}

TbResult LbPaletteDataFillDefVGA(ubyte *pal)
{
    short col;

    col = 0;
    /* Add the 16-color palette first */
    add16Color(pal, col, 0, 21, 42, 63);
    col += 16;

    /* Next we have 16 shades of gray */
    addGrayColor(pal, col++,  0);
    addGrayColor(pal, col++,  5);
    addGrayColor(pal, col++,  8);
    addGrayColor(pal, col++, 11);
    addGrayColor(pal, col++, 14);
    addGrayColor(pal, col++, 17);
    addGrayColor(pal, col++, 20);
    addGrayColor(pal, col++, 24);
    addGrayColor(pal, col++, 28);
    addGrayColor(pal, col++, 32);
    addGrayColor(pal, col++, 36);
    addGrayColor(pal, col++, 40);
    addGrayColor(pal, col++, 45);
    addGrayColor(pal, col++, 50);
    addGrayColor(pal, col++, 56);
    addGrayColor(pal, col++, 63);

    /* Now come the nine RGB cycles, organized in three groups of three
       cycles -- the groups represent high value, medium value, and low
       value, and the cycles within the groups represent high saturation,
       medium saturation, low saturation */
    addCycle(pal, col,  0, 16, 31, 47, 63);
    col += 24;
    addCycle(pal, col, 31, 39, 47, 55, 63);
    col += 24;
    addCycle(pal, col, 45, 49, 54, 58, 63);
    col += 24;

    addCycle(pal, col,  0,  7, 14, 21, 28);
    col += 24;
    addCycle(pal, col, 14, 17, 21, 24, 28);
    col += 24;
    addCycle(pal, col, 20, 22, 24, 26, 28);
    col += 24;

    addCycle(pal, col,  0,  4,  8, 12, 16);
    col += 24;
    addCycle(pal, col,  8, 10, 12, 14, 16);
    col += 24;
    addCycle(pal, col, 11, 12, 13, 15, 16);
    col += 24;

    /* Fill the last eight palette entries with full black */
    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);

    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);
    addGrayColor(pal, col++, 0);

    return (col == 256) ? Lb_SUCCESS : Lb_FAIL;
}

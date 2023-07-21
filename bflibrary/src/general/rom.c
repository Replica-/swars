/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file rom.asm
 *     Implementation of related functions.
 * @par Purpose:
 *     Unknown.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "rom.h"
#include "bftypes.h"
#include "bfpalette.h"

long tabwidth = 46;

const ubyte font[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    4, 0,
    0b01000000,
    0b01000000,
    0b01000000,
    0b00000000,
    0b01000000,
    0b00000000,
    2, 0,
    0b01010000,
    0b01010000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    4, 0,
    0b00101000,
    0b10000000,
    0b00101000,
    0b10000000,
    0b00101000,
    0b00000000,
    6, 0,
    0b00111100,
    0b01010000,
    0b00111000,
    0b00010100,
    0b01111000,
    0b00000000,
    6, 0,
    0b01100100,
    0b01001000,
    0b00010000,
    0b00100100,
    0b01001100,
    0b00000000,
    6, 0,
    0b00100000,
    0b01010000,
    0b00110100,
    0b01001000,
    0b00110100,
    0b00000000,
    6, 0,
    0b00100000,
    0b01000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    3, 0,
    0b00100000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b00100000,
    0b00000000,
    3, 0,
    0b01000000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b01000000,
    0b00000000,
    3, 0,
    0b01010100,
    0b00111000,
    0b10000000,
    0b00111000,
    0b01010100,
    0b00000000,
    6, 0,
    0b00100000,
    0b00100000,
    0b01110000,
    0b00100000,
    0b00100000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00100000,
    0b01000000,
    3, 0,
    0b00000000,
    0b00000000,
    0b01110000,
    0b00000000,
    0b00000000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01000000,
    0b00000000,
    2, 0,
    0b00000100,
    0b00001000,
    0b00010000,
    0b00100000,
    0b01000000,
    0b00000000,
    6, 0,
    0b00110000,
    0b01001000,
    0b01001000,
    0b01001000,
    0b00110000,
    0b00000000,
    5, 0,
    0b00010000,
    0b00110000,
    0b00010000,
    0b00010000,
    0b00111000,
    0b00000000,
    5, 0,
    0b01110000,
    0b00001000,
    0b00110000,
    0b01000000,
    0b01111000,
    0b00000000,
    5, 0,
    0b01110000,
    0b00001000,
    0b00110000,
    0b00001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b01001000,
    0b01001000,
    0b01111000,
    0b00001000,
    0b00001000,
    0b00000000,
    5, 0,
    0b01111000,
    0b01000000,
    0b01110000,
    0b00001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b00111000,
    0b01000000,
    0b01110000,
    0b01001000,
    0b00110000,
    0b00000000,
    5, 0,
    0b01111000,
    0b00010000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b00000000,
    5, 0,
    0b00110000,
    0b01001000,
    0b00110000,
    0b01001000,
    0b00110000,
    0b00000000,
    5, 0,
    0b00110000,
    0b01001000,
    0b00111000,
    0b00001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b00000000,
    0b01000000,
    0b00000000,
    0b01000000,
    0b00000000,
    0b00000000,
    2, 0,
    0b00000000,
    0b00100000,
    0b00000000,
    0b00100000,
    0b01000000,
    0b00000000,
    2, 0,
    0b00010000,
    0b00100000,
    0b01000000,
    0b00100000,
    0b00010000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01110000,
    0b00000000,
    0b01110000,
    0b00000000,
    0b00000000,
    4, 0,
    0b01000000,
    0b00100000,
    0b00010000,
    0b00100000,
    0b01000000,
    0b00000000,
    4, 0,
    0b00111000,
    0b01000100,
    0b00011000,
    0b00000000,
    0b00010000,
    0b00000000,
    6, 0,
    0b00111000,
    0b01010100,
    0b01011000,
    0b01000000,
    0b00111100,
    0b00000000,
    6, 0,
    0b00110000,
    0b01001000,
    0b01111000,
    0b01001000,
    0b01001000,
    0b00000000,
    5, 0,
    0b01110000,
    0b01001000,
    0b01110000,
    0b01001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b00111000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b00111000,
    0b00000000,
    5, 0,
    0b01110000,
    0b01001000,
    0b01001000,
    0b01001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b01111000,
    0b01000000,
    0b01110000,
    0b01000000,
    0b01111000,
    0b00000000,
    5, 0,
    0b01111000,
    0b01000000,
    0b01110000,
    0b01000000,
    0b01000000,
    0b00000000,
    5, 0,
    0b00111000,
    0b01000000,
    0b01011000,
    0b01001000,
    0b00111000,
    0b00000000,
    5, 0,
    0b01001000,
    0b01001000,
    0b01111000,
    0b01001000,
    0b01001000,
    0b00000000,
    5, 0,
    0b01110000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b01110000,
    0b00000000,
    4, 0,
    0b00010000,
    0b00010000,
    0b00010000,
    0b00010000,
    0b01100000,
    0b00000000,
    4, 0,
    0b01001000,
    0b01010000,
    0b01100000,
    0b01010000,
    0b01001000,
    0b00000000,
    5, 0,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01111000,
    0b00000000,
    5, 0,
    0b01000100,
    0b01101100,
    0b01010100,
    0b01000100,
    0b01000100,
    0b00000000,
    6, 0,
    0b01001000,
    0b01101000,
    0b01011000,
    0b01001000,
    0b01001000,
    0b00000000,
    5, 0,
    0b00110000,
    0b01001000,
    0b01001000,
    0b01001000,
    0b00110000,
    0b00000000,
    5, 0,
    0b01110000,
    0b01001000,
    0b01110000,
    0b01000000,
    0b01000000,
    0b00000000,
    5, 0,
    0b00110000,
    0b01001000,
    0b01001000,
    0b01010000,
    0b00101000,
    0b00000000,
    5, 0,
    0b01110000,
    0b01001000,
    0b01110000,
    0b01001000,
    0b01001000,
    0b00000000,
    5, 0,
    0b00111000,
    0b01000000,
    0b00110000,
    0b00001000,
    0b01110000,
    0b00000000,
    5, 0,
    0b10000000,
    0b00010000,
    0b00010000,
    0b00010000,
    0b00010000,
    0b00000000,
    6, 0,
    0b01001000,
    0b01001000,
    0b01001000,
    0b01001000,
    0b00110000,
    0b00000000,
    5, 0,
    0b01000100,
    0b01000100,
    0b01000100,
    0b00101000,
    0b00010000,
    0b00000000,
    6, 0,
    0b01000100,
    0b01000100,
    0b01010100,
    0b01101100,
    0b01000100,
    0b00000000,
    6, 0,
    0b01000100,
    0b00101000,
    0b00010000,
    0b00101000,
    0b01000100,
    0b00000000,
    6, 0,
    0b01000100,
    0b00101000,
    0b00010000,
    0b00010000,
    0b00010000,
    0b00000000,
    6, 0,
    0b01111000,
    0b00010000,
    0b00100000,
    0b01000000,
    0b01111000,
    0b00000000,
    6, 0,
    0b01100000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01100000,
    0b00000000,
    3, 0,
    0b01000000,
    0b00100000,
    0b00010000,
    0b00001000,
    0b00000100,
    0b00000000,
    6, 0,
    0b01100000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b01100000,
    0b00000000,
    3, 0,
    0b00010000,
    0b00101000,
    0b01000100,
    0b00000000,
    0b00000000,
    0b00000000,
    6, 0,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b10000000,
    0b00000000,
    6, 0,
    0b01000000,
    0b00100000,
    0b00010000,
    0b00001000,
    0b00000100,
    0b00000000,
    6, 0,
    0b00000000,
    0b00110000,
    0b01010000,
    0b01010000,
    0b00110000,
    0b00000000,
    4, 0,
    0b01000000,
    0b01100000,
    0b01010000,
    0b01010000,
    0b01100000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00110000,
    0b01000000,
    0b01000000,
    0b00110000,
    0b00000000,
    4, 0,
    0b00010000,
    0b00110000,
    0b01010000,
    0b01010000,
    0b00110000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00110000,
    0b01110000,
    0b01000000,
    0b00110000,
    0b00000000,
    4, 0,
    0b00010000,
    0b00100000,
    0b01110000,
    0b00100000,
    0b00100000,
    0b00100000,
    4, 0,
    0b00000000,
    0b00100000,
    0b01010000,
    0b00110000,
    0b00010000,
    0b01100000,
    4, 0,
    0b01000000,
    0b01100000,
    0b01010000,
    0b01010000,
    0b01010000,
    0b00000000,
    4, 0,
    0b01000000,
    0b00000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b00000000,
    2, 0,
    0b00100000,
    0b00000000,
    0b00100000,
    0b00100000,
    0b00100000,
    0b01000000,
    3, 0,
    0b01000000,
    0b01000000,
    0b01010000,
    0b01100000,
    0b01010000,
    0b00000000,
    4, 0,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b00000000,
    2, 0,
    0b00000000,
    0b01101000,
    0b01010100,
    0b01000100,
    0b01000100,
    0b00000000,
    6, 0,
    0b00000000,
    0b01100000,
    0b01010000,
    0b01010000,
    0b01010000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00100000,
    0b01010000,
    0b01010000,
    0b00100000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01100000,
    0b01010000,
    0b01010000,
    0b01100000,
    0b01000000,
    4, 0,
    0b00000000,
    0b00110000,
    0b01010000,
    0b01010000,
    0b00110000,
    0b00010000,
    4, 0,
    0b00000000,
    0b01010000,
    0b01100000,
    0b01000000,
    0b01000000,
    0b00000000,
    4, 0,
    0b00000000,
    0b00110000,
    0b01000000,
    0b00010000,
    0b01100000,
    0b00000000,
    4, 0,
    0b00100000,
    0b01110000,
    0b00100000,
    0b00100000,
    0b00010000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01010000,
    0b01010000,
    0b01010000,
    0b00110000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01010000,
    0b01010000,
    0b01010000,
    0b00100000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01000100,
    0b01010100,
    0b01010100,
    0b00101000,
    0b00000000,
    6, 0,
    0b00000000,
    0b01010000,
    0b00100000,
    0b01010000,
    0b01010000,
    0b00000000,
    4, 0,
    0b00000000,
    0b01010000,
    0b01010000,
    0b00110000,
    0b00010000,
    0b01100000,
    4, 0,
    0b00000000,
    0b01110000,
    0b00100000,
    0b01000000,
    0b01110000,
    0b00000000,
    4, 0,
    0b00110000,
    0b00100000,
    0b01000000,
    0b00100000,
    0b00110000,
    0b00000000,
    4, 0,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    0b01000000,
    2, 0,
    0b01100000,
    0b00100000,
    0b00010000,
    0b00100000,
    0b01100000,
    0b00000000,
    4, 0,
    0b00101000,
    0b01010000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    5, 0,
    0b10000000,
    0b10000000,
    0b10000000,
    0b10000000,
    0b10000000,
    0b10000000,
    6, 0,
    0b10000000,
    0b01000100,
    0b01010100,
    0b01000100,
    0b10000000,
    0b00000000,
    6, 0,
    0b00010000,
    0b00010000,
    0b00111000,
    0b00111000,
    0b10000000,
    0b00000000,
    6, 0,
    0b01000000,
    0b01110000,
    0b10000000,
    0b01110000,
    0b01000000,
    0b00000000,
    6, 0,
    0b10000000,
    0b00111000,
    0b00111000,
    0b00010000,
    0b00010000,
    0b00000000,
    6, 0,
    0b00000100,
    0b00011100,
    0b10000000,
    0b00011100,
    0b00000100,
    0b00000000,
    6, 0,
    0b00010000,
    0b00101000,
    0b01010100,
    0b00111000,
    0b10000000,
    0b00000000,
    6, 0,
    0b01010000,
    0b01101000,
    0b01111100,
    0b01101000,
    0b01010000,
    0b00000000,
    6, 0,
    0b10000000,
    0b00111000,
    0b01010100,
    0b00101000,
    0b00010000,
    0b00000000,
    6, 0,
    0b00010100,
    0b00101100,
    0b01011100,
    0b00101100,
    0b00010100,
    0b00000110,
    0, 0,
};

static void prop_text_qaz(ubyte *o, ubyte chtype, ushort colr)
{
    switch (chtype)
    {
    case 0:
        break;
    case 0b00000001:
        o[4] = colr;
        break;
    case 0b00000010:
        o[3] = colr;
        break;
    case 0b00000011:
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00000100:
        o[2] = colr;
        break;
    case 0b00000101:
        o[2] = colr;
        o[4] = colr;
        break;
    case 0b00000110:
        *((ushort *)&o[1]) = colr;
        break;
    case 0b00000111:
        o[2] = colr;
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00001000:
        o[1] = colr;
        break;
    case 0b00001001:
        o[1] = colr;
        o[4] = colr;
        break;
    case 0b00001010:
        o[1] = colr;
        o[3] = colr;
        break;
    case 0b00001011:
        o[1] = colr;
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00001100:
        *((ushort *)&o[1]) = colr;
        break;
    case 0b00001101:
        o[4] = colr;
        *((ushort *)&o[1]) = colr;
        break;
    case 0b00001110:
        o[1] = colr;
        *((ushort *)&o[2]) = colr;
        break;
    case 0b00001111:
        *((ushort *)&o[1]) = colr;
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00010000:
        o[0] = colr;
        break;
    case 0b00010001:
        o[0] = colr;
        o[4] = colr;
        break;
    case 0b00010010:
        o[0] = colr;
        o[3] = colr;
        break;
    case 0b00010011:
        o[0] = colr;
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00010100:
        o[0] = colr;
        o[2] = colr;
        break;
    case 0b00010101:
        o[0] = colr;
        o[2] = colr;
        o[4] = colr;
        break;
    case 0b00010110:
        o[0] = colr;
        *((ushort *)&o[2]) = colr;
        break;
    case 0b00010111:
        o[0] = colr;
        *((ushort *)&o[2]) = colr;
        o[4] = colr;
        break;
    case 0b00011000:
        *((ushort *)&o[0]) = colr;
        break;
    case 0b00011001:
        *((ushort *)&o[0]) = colr;
        o[4] = colr;
        break;
    case 0b00011010:
        *((ushort *)&o[0]) = colr;
        o[3] = colr;
        break;
    case 0b00011011:
        *((ushort *)&o[0]) = colr;
        *((ushort *)&o[3]) = colr;
        break;
    case 0b00011100:
        *((ushort *)&o[0]) = colr;
        o[2] = colr;
        break;
    case 0b00011101:
        *((ushort *)&o[0]) = colr;
        o[2] = colr;
        o[4] = colr;
        break;
    case 0b00011110:
        *((ushort *)&o[0]) = colr;
        *((ushort *)&o[2]) = colr;
        break;
    case 0b00011111:
        *((ushort *)&o[0]) = colr;
        *((ushort *)&o[2]) = colr;
        o[4] = colr;
        break;
    case 0b00100000:
        *((ushort *)&o[0]) = colr;
        *((ushort *)&o[2]) = colr;
        o[4] = colr;
        break;
    }
}

static inline short prop_text_draw_char(ubyte *o, ubyte c, long scanline, ushort colr)
{
    ulong fpos; // pos within font data
    short fln; // line within font data

    fpos = 8 * ((c - 32) & 0xFF);
    for (fln = 6; fln > 0; fln--) {
        prop_text_qaz(o, font[fpos], colr);
        o += scanline;
        fpos++;
    }
    return font[fpos];
}

void prop_text(const char *text, TbPixel *out, long scanline, TbPixel colour)
{
    TbPixel *obeg; // current line begin in output buffer
    TbPixel *o; // current pos in output buffer
    ushort colr; // Colour expanded to 16 bits
    const ubyte *pch; // Pointer to currently drawn character
    ulong linesize; // size of one line within the output buffer

    linesize = 6 * scanline;
    colr = (colour << 8) + colour;
    pch = (const ubyte *)text;
    obeg = out;
    o = obeg;

    while (true)
    {
        ulong w;
        ubyte c;
        TbPixel *otmp;
        short chshift;

        c = *pch++;
        switch (c)
        {
        case 9: // Horizontal Tab
            w = tabwidth;
            do
            {
                otmp = &obeg[w];
                w += tabwidth;
            }
            while (otmp <= o);
            o = otmp;
            break;
        case 10: // Line Feed
            obeg += linesize;
            o = obeg;
            break;
        default: // Normal character
            chshift = prop_text_draw_char(o, c, scanline, colr);
            o += chshift;
            break;
        case 0: // NULL char means end
            return;
        }
    }
}

void make_fade_table(const ubyte *pal, ubyte *out, ubyte cr, ubyte cg, ubyte cb,
  ubyte ir, ubyte ig, ubyte ib)
{
    const ubyte *p;
    ubyte *t;
    short k;

    p = pal;
    t = out;
    for (k = 256; k > 0; k--)
    {
        ubyte r, g, b;

        r = ((ushort)(ir * (short)(cr - p[0])) >> 8) + p[0];
        g = ((ushort)(ig * (short)(cg - p[1])) >> 8) + p[1];
        b = ((ushort)(ib * (short)(cb - p[2])) >> 8) + p[2];
        *t = LbPaletteFindColourHalfWaged(pal, r, g, b);
        t++;
        p += 3;
    }
}


/******************************************************************************/

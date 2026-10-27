/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file bfgl.h
 *     Header file for sgl.c.
 * @par Purpose:
 *     OpenGL support: window context, loading of the GL functions, and
 *     presenting the 8-bit game screen through the GPU.
 * @par Comment:
 *     Only SDL2 build has the implementation. Other builds get stubs, which
 *     report the GL as unavailable.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIBRARY_BFGL_H_
#define BFLIBRARY_BFGL_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/** Returns whether the window has a GL context, and the screen is presented through it.
 */
TbBool LbGlIsActive(void);

/** Sets the colour palette used when presenting the 8-bit screen.
 * @param palette 256 colours, 4 bytes each: R, G, B, A (A is ignored).
 */
void LbGlSetPalette(const ubyte *palette_rgba);

/** Presents the 8-bit screen buffer in the window.
 *
 * @param pixels Palette indexes of the pixels.
 * @param width Width of the visible part of the buffer, in pixels.
 * @param height Height of the buffer in pixels.
 * @param pitch Distance between line starts, in pixels.
 */
TbResult LbGlPresentIndexed(const ubyte *pixels, long width, long height, long pitch);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif // BFLIBRARY_BFGL_H_

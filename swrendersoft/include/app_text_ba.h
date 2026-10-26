/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_text_ba.h
 *     Header file for app_text_ba.c.
 * @par Purpose:
 *     Allows drawing brightness adjusted text.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 02 Oct 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef APP_TEXT_BA_H_
#define APP_TEXT_BA_H_

#include <stdarg.h>
#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Draws a string in the current text window.
 *
 * Allows adjusting brightness of the colours drawn.
 * If position starts before the window, or ends before the text ends,
 * partial characters will be drawn - the function does not enforce
 * bounds to full characters.
 *
 * @param posx Position of the text, X coord. Can be negative.
 * @param posy Position of the text, Y coord.
 * @param brig Brightness of the colours drawn.
 * @param text The text to be drawn.
 * @return
 */
TbBool AppTextDrawLineBrigAdjWthParts(int posx, int posy,
  short brig, const char *text);

/**
 * Draws a string in the current text window in given scale.
 *
 * Allows adjusting brightness of the colours drawn.
 * If position starts before the window, or ends before the text ends,
 * partial characters will be drawn - the function does not enforce
 * bounds to full characters.
 *
 * @param posx Position of the text, X coord.
 * @param posy Position of the text, Y coord.
 * @param units_per_px Scale in pixels; 16 is 100%.
 * @param brig Brightness of the colours drawn.
 * @param text The text to be drawn.
 * @return
 */
TbBool AppTextDrawLineBrigAdjWthPartsResized(int posx, int posy,
  int units_per_px, short brig, const char *text);

#ifdef __cplusplus
};
#endif

#endif // APP_TEXT_BA_H_
/******************************************************************************/

/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_gengh.c
 *     Implementation of related functions.
 * @par Purpose:
 *     App-specific colour tables for mixing two colours in a special way.
 * @par Comment:
 *     The engine contains pixmap struct which allows 50-50 colour mixing.
 *     For any different kind of mixing, extra colour tables are required.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "app_gentab.h"


struct ApColorTables appixmap;

/******************************************************************************/

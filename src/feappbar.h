/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file feappbar.h
 *     Header file for feappbar.c.
 * @par Purpose:
 *     Front-end desktop and menu system, application bar at the bottom.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     05 Apr 2024 - 22 Nov 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef FEAPPBAR_H
#define FEAPPBAR_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

enum AppBarIcons {
  ApBar_SYSTEM = 0,
  ApBar_PANET,
  ApBar_WORLDMAP,
  ApBar_CRYO,
  ApBar_EQUIP,
  ApBar_RESEARCH,
  ApBar_NEWMAIL,
};

#pragma pack()
/******************************************************************************/
extern struct ScreenBoxBase global_apps_bar_box;

/******************************************************************************/

TbBool is_purple_apps_selection_bar_visible(void);
TbBool mouse_over_purple_apps_icon(short iconid);
void show_purple_apps_selection_bar(void);
TbBool input_purple_apps_selection_bar(void);

void init_global_app_bar_box(void);

/** Reset players part of app bar state.
 *
 * Resets all parts of app bar at bottom which are set by the player
 * or related to players game progress.
 */
void reset_app_bar_player_state(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

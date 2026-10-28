/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_sprts.h
 *     Header file for game_sprts.c.
 * @par Purpose:
 *     Lists of sprites used by the game.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     10 Feb 2024 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef GAME_SPRTS_H
#define GAME_SPRTS_H

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

#include "bftypes.h"
#include "game_bstype.h"

/******************************************************************************/

extern struct TbSprite *pointer_sprites;
extern struct TbSprite *pointer_sprites_end;
extern ubyte *pointer_data;
extern ubyte *pointer_data_end;

/** In-game panel sprites.
 */
extern struct TbSprite *pop1_sprites;
extern struct TbSprite *pop1_sprites_end;
extern ubyte *pop1_data;
extern ubyte *pop1_data_end;
extern short pop1_sprites_scale;

extern struct TbSprite *small_font;
extern struct TbSprite *small_font_end;
extern ubyte *small_font_data;
extern ubyte *small_font_data_end;

extern struct TbSprite *small2_font;
extern struct TbSprite *small2_font_end;
extern ubyte *small2_font_data;

extern struct TbSprite *small_med_font;
extern struct TbSprite *small_med_font_end;
extern ubyte *small_med_font_data;

extern struct TbSprite *med_font;
extern struct TbSprite *med_font_end;
extern ubyte *med_font_data;

extern struct TbSprite *med2_font;
extern struct TbSprite *med2_font_end;
extern ubyte *med2_font_data;

extern struct TbSprite *big_font;
extern struct TbSprite *big_font_end;
extern ubyte *big_font_data;

extern struct TbSprite *fe_icons_sprites;
extern struct TbSprite *fe_icons_sprites_end;
extern ubyte *fe_icons_sprites_data;

extern struct TbSprite *wepicons_sprites;
extern struct TbSprite *wepicons_sprites_end;
extern ubyte *wepicons_sprites_data;

extern struct TbSprite *fe_mouseptr_sprites;
extern struct TbSprite *fe_mouseptr_sprites_end;
extern ubyte *fe_mouseptr_sprites_data;

/** Panel sprites, but loaded for front-end menu purposes.
 * The main purpose is to get small weapon sprites.
 */
extern struct TbSprite *fepanel_sprites;
extern struct TbSprite *fepanel_sprites_end;
extern ubyte *fepanel_sprites_data;

/** Data for in-mission sprites.
 *
 * The references to actual sprites are declared within rendering library.
 */
extern ubyte *m_sprites_data;
extern ubyte *m_sprites_data_end;
/** Costume of the enemy agents (the player agents wear ingame.TrenchcoatPreference); 0..5. */
extern ubyte game_enemy_costume;
/** Loads the multicolor sprites of the enemy costume, to the alternative set. */
TbResult load_multicolor_sprites_alt(const char *dir);

/** The costume which has colours that can be replaced: the pixels of its trench coat and shoulders
 * get marker palette indices when the sprites are loaded, and the GPU shows them in any colour
 * chosen by the player. This works only with the hardware rendering. */
#define CUSTOM_COSTUME_BASE 3
#define CUSTOM_COSTUME_COAT 0
#define CUSTOM_COSTUME_SHOULDERS 1
#define CUSTOM_COSTUME_PARTS 2
/** The teams which the player can play: the Eurocorp agents (costume 3 has the colours which can be replaced)
 * and the Epoch members (the zealots, in all costume files). Each has its own parts, colours, and shades
 * as the entry points of the lighting. The team is the one of the campaign which is played. */
#define CUSTOM_TEAM_EUROCORP 0
#define CUSTOM_TEAM_EPOCH 1
#define CUSTOM_TEAMS 2
int game_custom_team(void);
/** The name of the part of the costume of the team which is played. */
const char *game_custom_costume_part_name(int part);
/** Whether the colour of each part is replaced, and the colour (8 bits per channel) which it is
 * replaced with: the colour of the reference shade of the part, the other shades are darker or brighter.
 * game_custom_used and game_custom_rgb are those of the team which is played. */
extern TbBool game_custom_used_all[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS];
extern ubyte game_custom_rgb_all[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS][3];
#define game_custom_used (game_custom_used_all[game_custom_team()])
#define game_custom_rgb (game_custom_rgb_all[game_custom_team()])
/** Tells how many shades the original colour of the part has. */
int game_custom_costume_shades(int part);
/** Gives the palette index to draw a swatch of the given palette entry with, while the screen shows
 * the marker indices in the chosen colours: the same index, or the nearest one which is not a marker. */
int game_custom_costume_swatch(int idx);
/** Palette index which the sprites have for the shade of the part, when its colour is not replaced. */
ubyte game_custom_costume_original(int part, int shade);
/** Palette index which the sprites have for the shade of the part, when its colour is replaced. */
ubyte game_custom_costume_marker(int part, int shade);
/** The original colour of the part, to start with when choosing. */
void game_custom_costume_default_rgb(int part, ubyte *rgb);
/** The colour of a palette entry, 8 bits per channel. */
void game_custom_costume_palette_rgb(int idx, ubyte *rgb);
/** Tells if the replacing of colours can work now: the GPU has to draw the scene. */
TbBool game_custom_costume_hw(void);
/** Tells if any part of the costume has a replaced colour, and it is shown. */
TbBool game_custom_costume_active(void);
/** Gives the GPU the colours chosen; the sprites do not have to be loaded again when only a colour changes. */
void game_custom_costume_publish(void);

/** The colours of the interface in the missions (the panels and the accent: the lines and text of the map): chosen by the player on a screen of the
 * pause menu, shown by the GPU in place of the colours the interface has, and kept in a file. */
#define GAME_UI_COLOUR_PARTS 2
extern TbBool game_ui_colour_used[GAME_UI_COLOUR_PARTS];
extern ubyte game_ui_colour_rgb[GAME_UI_COLOUR_PARTS][3];
/** The opacity of the colour, 255 is solid; below it the scene shows through the interface. */
extern ubyte game_ui_colour_alpha[GAME_UI_COLOUR_PARTS];
void game_ui_colour_default_rgb(int part, ubyte *rgb);
const char *game_ui_colour_part_name(int part);
/** Tells if the colours can be shown now: the GPU has to draw the screen. */
TbBool game_ui_colour_hw(void);
/** Tells if any colour is chosen, and shown. */
TbBool game_ui_colour_active(void);
/** Loads what was chosen before (once) and gives it to the GPU. */
void game_ui_colour_init(void);
void game_ui_colour_publish(void);
void game_ui_colour_set(int part, const ubyte *rgb);
void game_ui_colour_set_alpha(int part, ubyte alpha);
void game_ui_colour_reset(void);
extern ubyte *dword_1C6DE4;
extern ubyte *dword_1C6DE8;

/******************************************************************************/

/** Loads mouse pointer sprites for given style and detail level.
 * Requires the sprite buffers to be already allocated.
 */
TbResult load_sprites_mouse_pointers_up_to(
  const char *dir, short styleno, short max_detail);
/** Sets up previously loaded mouse sprites.
 */
void setup_mouse_pointers(void);
/** Frees up any resources and states associated to the mouse sprites.
 * Does not free the preallocated memory buffers.
 */
void reset_mouse_pointers(void);

TbResult load_sprites_fe_icons(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_fe_icons(void);
void reset_sprites_fe_icons(void);

TbResult load_sprites_wepicons(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_wepicons(void);
void reset_sprites_wepicons(void);

/** Loads panel sprites for frontend menu use.
 *
 * Panels are normally used in game. But few of their sprites are also
 * re-used for the frontend menus. The sprites need to be recolored to
 * match the frontend palette; we require sprites of detail one level
 * smaller than the expected menu detail.
 */
TbResult load_sprites_fepanel(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_fepanel(void);
void reset_sprites_fepanel(void);

TbResult load_sprites_fe_mouse_pointers(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_fe_mouse_pointers(void);
void reset_sprites_fe_mouse_pointers(void);

/** Loads small font sprites for given detail level.
 * Requires the sprite buffers to be already allocated.
 */
TbResult load_sprites_small_font_up_to(const char *dir, short max_detail);
void setup_sprites_small_font(void);
void reset_sprites_small_font(void);

TbResult load_sprites_small_med_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_small_med_font(void);
void reset_sprites_small_med_font(void);

TbResult load_sprites_med_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_med_font(void);
void reset_sprites_med_font(void);

TbResult load_sprites_med2_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_med2_font(void);
void reset_sprites_med2_font(void);

TbResult load_sprites_big_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_big_font(void);
void reset_sprites_big_font(void);

TbResult load_sprites_small2_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail);
void setup_sprites_small2_font(void);
void reset_sprites_small2_font(void);

/** Loads and sets up multicolor sprites for currently set TrenchcoatPreference.
 */
TbResult load_multicolor_sprites(const char *dir);

/** Sets up initially loaded multicolor sprites.
 * Use load_multicolor_sprites() for further reloads.
 */
void setup_multicolor_sprites(void);
void reset_multicolor_sprites(void);
void debug_multicolor_sprite(int idx);

/** Loads panel sprites for given style and detail level.
 * Requires the sprite buffers to be already allocated.
 */
TbResult load_pop_sprites_up_to(const char *dir, const char *name,
  short styleno, short max_detail);
/** Sets up previously loaded panel sprites.
 */
void setup_pop_sprites(void);
/** Frees up any resources and states associated to the panel sprites.
 * Does not free the preallocated memory buffers.
 */
void reset_pop_sprites(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

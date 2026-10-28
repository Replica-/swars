/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file fepause.c
 *     Front-end desktop and menu system, in-game pause screen.
 * @par Purpose:
 *     Implement functions for pause screen displayed in-game.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 22 Oct 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include "fepause.h"

#include "bfbox.h"
#include "bfjoyst.h"
#include "bfline.h"
#include "bfscreen.h"
#include "bfsprite.h"
#include "bftext.h"
#include "bftringl.h"
#include "bfkeybd.h"
#include "bfaudio.h"
#include "bfmusic.h"
#include "bfscd.h"
#include "bfhwrend.h"
#include "ssampply.h"

#include "app_box.h"
#include "engincolour.h"

#include "frame_sprani.h"

#include "campaign.h"
#include "display.h"
#include "game_data.h"
#include "game_options.h"
#include "game_sprts.h"
#include "game.h"
#include "guiboxes.h"
#include "guitext.h"
#include "keyboard.h"
#include "mouse.h"
#include "people.h"
#include "mydraw.h"
#include "sound.h"
#include "swlog.h"
/******************************************************************************/
static struct ScreenBox pause_main_box;
static TbPixel ingame_boxes_colr1, ingame_boxes_colr2;
static struct ScreenBox samplevol_slider_box;
static struct ScreenBox samplevol_arrow_l_box;
static struct ScreenBox samplevol_arrow_r_box;
static struct ScreenBox midivol_slider_box;
static struct ScreenBox midivol_arrow_l_box;
static struct ScreenBox midivol_arrow_r_box;
static struct ScreenBox cdvolume_slider_box;
static struct ScreenBox cdvolume_arrow_l_box;
static struct ScreenBox cdvolume_arrow_r_box;
static struct ScreenButton detail_hi_btn;
static struct ScreenButton detail_lo_btn;
static struct ScreenButton continue_btn;
static struct ScreenButton abort_btn;

/** The box with the game settings, next to the pause box. */
static struct ScreenBox options_box;
/** The button in the settings box which opens the screen choosing the colours of the costume. */
static struct ScreenButton custom_open_btn;
static struct ScreenButton ui_open_btn;
static struct ScreenBox option_arrow_l_box[PAUSE_OPT_COUNT];
static struct ScreenBox option_arrow_r_box[PAUSE_OPT_COUNT];

/** The translucent fill of the boxes. With the GPU drawing the scene, the software screen has nothing
 * to blend with, so it gets an index which the GPU shows as a translucent colour over the scene. */
static ubyte pause_fill_flags(void)
{
    return (LbHwOverlay() >= 0) ? 0 : Lb_SPRITE_TRANSPAR4;
}

static TbPixel pause_fill_colour(TbPixel colr)
{
    return (LbHwOverlay() >= 0) ? HWREND_OVERLAY_INDEX : colr;
}

ubyte draw_ingame_button(struct ScreenButton *p_button, ubyte enabled)
{
    short text_x, text_y;
    short line_h, text_w, margin;

    margin = p_button->Border + 1;

    lbDisplay.DrawFlags = pause_fill_flags();
    LbDrawBox(p_button->X - margin, p_button->Y - margin, p_button->Width + 2 * margin, p_button->Height + 2 * margin,
      pause_fill_colour(p_button->BGColour));

    lbDisplay.DrawFlags = Lb_SPRITE_OUTLINE;
    lbDisplay.DrawColour = colour_lookup[ColLU_WHITE];
    if (enabled || mouse_move_over_box(p_button))
    {
        int i;
        for (i = margin / 2; i >= 0; i--)
            LbDrawBox(p_button->X + i, p_button->Y + i, p_button->Width - 2 * i, p_button->Height - 2 * i, lbDisplay.DrawColour);
        lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
    }
    else
    {
        if (ingame.PanelPermutation == 2 || ingame.PanelPermutation == -3) {
            lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
            lbDisplay.DrawColour = p_button->Colour;
        }
        int i;
        for (i = margin / 2; i >= 0; i--)
            LbDrawBox(p_button->X + i, p_button->Y + i, p_button->Width - 2 * i, p_button->Height - 2 * i, p_button->Colour);
    }

    line_h = my_char_height('A');
    text_w = my_string_width(p_button->Text);
    text_x = p_button->X + 2 * margin + (p_button->Width - (text_w + 4 * margin)) / 2;
    text_y = p_button->Y + 2 * margin + (p_button->Height - (line_h + 4 * margin)) / 2;
    my_draw_text(text_x, text_y, p_button->Text, 0);

    lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;

    return 0;
}

ubyte input_ingame_button(struct ScreenButton *p_button, ubyte enabled)
{
    if (enabled || mouse_move_over_box(p_button))
    {
      if (lbDisplay.LeftButton)
      {
        if (mouse_move_over_box(p_button))
            lbDisplay.LeftButton = 0;

        if (mouse_down_over_box(p_button))
            return 1;
      }
    }
    return 0;
}

TbBool input_slant_left_arrow(struct ScreenBox *box, short *target)
{
    if (mouse_move_over_slant_box(box))
    {
        if (lbDisplay.MLeftButton)
        {
            lbDisplay.LeftButton = 0;
            (*target)--;
            if ((*target) < 0)
                (*target) = 0;
            return true;
        }
    }
    return false;
}

TbBool input_slant_right_arrow(struct ScreenBox *box, short *target)
{
    if (mouse_move_over_slant_box(box))
    {
        if (lbDisplay.MLeftButton)
        {
            lbDisplay.LeftButton = 0;
            (*target)++;
            if ((*target) > STARTSCR_VOLUME_MAX)
                (*target) = STARTSCR_VOLUME_MAX;
            return true;
        }
    }
    return false;
}

TbBool input_slant_box(struct ScreenBox *box, short *target)
{
    int ms_x, ms_y;
    short border;

    border = lbDisplay.GraphicsScreenHeight < 400 ? 1 : 2;

    if (lbDisplay.MLeftButton)
    {
        ms_x = lbDisplay.MMouseX;
        ms_y = lbDisplay.MMouseY;
        if ((ms_y >= box->Y + border) && (ms_y <= box->Y + box->Height - 2 * border))
        {
            int dx, dy;
            dx = ms_x - box->X - border;
            dy = ms_y - box->Y - border;
            if (dx + dy >= 0 && dx + dy <= box->Width - 2 * border - border)
            {
                (*target) = STARTSCR_VOLUME_MAX * (dx + dy) / (box->Width - 2 * border - border);
                return true;
            }
        }
    }
    return false;
}

void draw_box_cutedge(struct ScreenBox *box, TbPixel colr1)
{
    short cut, stp;

    stp = pop1_sprites_scale;
    cut = 25 * pop1_sprites_scale;

    lbDisplay.DrawFlags = pause_fill_flags();
    LbDrawBox(box->X + 0, box->Y + 0, box->Width - cut, box->Height - cut, pause_fill_colour(colr1));
    LbDrawBox(box->X + box->Width - cut, box->Y +  cut, cut, box->Height - 2*cut, pause_fill_colour(colr1));
    LbDrawBox(box->X + cut, box->Y + box->Height - cut, box->Width - cut, cut, pause_fill_colour(colr1));
    LbDrawTriangle(box->X + box->Width - cut, box->Y + 0,
      box->X + box->Width, box->Y + cut,
      box->X + box->Width - cut, box->Y + cut, pause_fill_colour(colr1));
    LbDrawTriangle(box->X + stp, box->Y + box->Height - cut,
      box->X + cut, box->Y + box->Height - cut,
      box->X + cut, box->Y + box->Height - stp, pause_fill_colour(colr1));

    lbDisplay.DrawFlags = 0;
    LbDrawLine(box->X + 0, box->Y + 0, box->X + box->Width - cut, box->Y + 0, colr1);
    LbDrawLine(box->X + 0, box->Y + 0, box->X + 0, box->Y + box->Height - cut - stp, colr1);
    LbDrawLine(box->X + box->Width, box->Y + cut, box->X + box->Width, box->Y + box->Height - stp, colr1);
    LbDrawLine(box->X + cut, box->Y + box->Height - stp, box->X + box->Width, box->Y + box->Height - stp, colr1);
    LbDrawLine(box->X + box->Width - cut, box->Y + 0,
      box->X + box->Width, box->Y + cut, colr1);
    LbDrawLine(box->X + stp, box->Y + box->Height - cut,
      box->X + cut, box->Y + box->Height - stp, colr1);
}

void draw_slant_box(struct ScreenBox *box, TbPixel colr2)
{
    AppDrawSlantBox(box->X, box->Y, box->Width, box->Height, colr2);
}

void draw_slant_left_arrow_spr(struct ScreenBox *box, TbPixel colr2)
{
    short stp;

    stp = pop1_sprites_scale;

    if (mouse_move_over_slant_box(box))
    {
        LbSpriteDrawOneColour(box->X, box->Y - stp, &pop1_sprites[100],
          colour_lookup[ColLU_WHITE]);
    }
    else
    {
        LbSpriteDraw(box->X, box->Y - stp, &pop1_sprites[100]);
    }
}

void draw_slant_right_arrow_spr(struct ScreenBox *box, TbPixel colr2)
{
    short stp;

    stp = pop1_sprites_scale;

    if (mouse_move_over_slant_box(box))
    {
        LbSpriteDrawOneColour(box->X - stp, box->Y - stp,
          &pop1_sprites[101], colour_lookup[ColLU_WHITE]);
    }
    else
    {
        LbSpriteDraw(box->X - stp, box->Y - stp, &pop1_sprites[101]);
    }
}

void draw_slant_left_arrow(struct ScreenBox *box, TbPixel colr2)
{
    short x, y;
    short stp;
    TbPixel colour;

    if (mouse_move_over_slant_box(box))
        colour = colour_lookup[ColLU_WHITE];
    else
        colour = colr2;

    // We expect the rect to be a multiplication of 4x9; so we use height only
    stp = (box->Height + 4) / 9;

    x = box->X;
    y = box->Y - stp;

    LbDrawTriangle(x + 0, y + stp * 5, x + stp * 11, y - 1, x +  0, y + stp * 11, colour);
    AppDrawSlantBox(x + stp * 11 - 1, y + 0, stp * 2 / 2, stp * 11, colour);
}

void draw_slant_right_arrow(struct ScreenBox *box, TbPixel colr2)
{
    short x, y;
    short stp;
    TbPixel colour;

    if (mouse_move_over_slant_box(box))
        colour = colour_lookup[ColLU_WHITE];
    else
        colour = colr2;

    // We expect the rect to be a multiplication of 4x9; so we use height only
    stp = (box->Height + 4) / 9;

    x = box->X;
    y = box->Y - stp;

    AppDrawSlantBox(x + stp * 10 - 1, y + 0, stp * 2 / 2, stp * 11, colour);
    LbDrawTriangle(x + stp * 11, y + stp * 5 + 1, x + 0, y + stp * 11, x +  stp * 11, y + 0, colour);
}

void init_slider_with_arrows_centered(struct ScreenBox *slider_box, struct ScreenBox *arrow_l_box,
  struct ScreenBox *arrow_r_box, const struct ScreenBox *parent_box, int y, int slider_w, int arrow_w, int cutin_w, int h)
{
    slider_box->Width = slider_w;
    slider_box->Height = h;
    slider_box->X = parent_box->X + (parent_box->Width - slider_box->Width) / 2 + 4;
    slider_box->Y = y;

    arrow_l_box->Width = arrow_w;
    arrow_l_box->Height = h;
    arrow_l_box->X = slider_box->X - arrow_w - 2 * cutin_w;
    arrow_l_box->Y = slider_box->Y;

    arrow_r_box->Width = arrow_w;
    arrow_r_box->Height = h;
    arrow_r_box->X = slider_box->X + slider_box->Width - cutin_w;
    arrow_r_box->Y = slider_box->Y;
}

void init_ingame_screen_button(struct ScreenButton *p_button, ushort x, ushort y, const char *text, struct TbSprite *font, int flags)
{
    short line_h, text_w, border;
    short fnt_scale, margin;

    init_screen_button(p_button, x, y, text, 0, font, 0, 0);

    border = pop1_sprites_scale - 1;
    line_h = my_char_height('A');
    text_w = my_string_width(p_button->Text);
    fnt_scale = pop1_sprites_scale / 2 + 1;
    if (line_h < fnt_scale * 6) { // detail 0 font has height equal 6
        fnt_scale = (fnt_scale * 6) / line_h;
        line_h *= fnt_scale;
        text_w *= fnt_scale;
    }

    p_button->Border = border;
    p_button->BGColour = ingame_boxes_colr1;
    p_button->Colour = ingame_boxes_colr2;

    margin = p_button->Border + 1;
    p_button->Width = text_w + 4 * margin;
    p_button->Height = line_h + 4 * margin;
    p_button->X -= (p_button->Width >> 1);
}

void init_pause_screen_boxes(void)
{
    short slider_w, arrow_w, cutin_w, slider_h;
    int x, y;

    if ((ingame.PanelPermutation != 2) && (ingame.PanelPermutation != -3)) {
        ingame_boxes_colr1 = 20;
        ingame_boxes_colr2 = 15;
    } else {
        ingame_boxes_colr1 = 40;
        ingame_boxes_colr2 = 35;
    }

    pause_main_box.Width = 233 * pop1_sprites_scale;
    pause_main_box.Height = 122 * pop1_sprites_scale;
    cutin_w = 5 * pop1_sprites_scale;
    slider_h = 9 * pop1_sprites_scale;

    // The settings box is to the right of the pause box; they are centered together
    options_box.Width = 124 * pop1_sprites_scale;
    options_box.Height = pause_main_box.Height;
    {
        int gap, total;

        gap = 4 * pop1_sprites_scale;
        total = pause_main_box.Width + gap + options_box.Width;
        pause_main_box.X = (lbDisplay.GraphicsScreenWidth - total) / 2;
        if (pause_main_box.X < 0)
            pause_main_box.X = 0;
        options_box.X = pause_main_box.X + pause_main_box.Width + gap;
    }
    pause_main_box.Y = (lbDisplay.GraphicsScreenHeight - pause_main_box.Height) / 2;
    options_box.Y = pause_main_box.Y;
    {
        int i, pitch, row_y, arrow_h;

        arrow_h = 6 * pop1_sprites_scale;
        pitch = options_box.Height * 78 / 100 / PAUSE_OPT_COUNT;
        row_y = options_box.Y + options_box.Height * 15 / 100;
        for (i = 0; i < PAUSE_OPT_COUNT; i++)
        {
            option_arrow_l_box[i].Width = arrow_h * 4 / 3;
            option_arrow_l_box[i].Height = arrow_h;
            option_arrow_l_box[i].X = options_box.X + options_box.Width * 52 / 100;
            option_arrow_l_box[i].Y = row_y + pitch * i + pitch / 8;
            option_arrow_r_box[i].Width = arrow_h * 4 / 3;
            option_arrow_r_box[i].Height = arrow_h;
            option_arrow_r_box[i].X = options_box.X + options_box.Width * 88 / 100;
            option_arrow_r_box[i].Y = option_arrow_l_box[i].Y;
        }
        init_ingame_screen_button(&custom_open_btn,
          options_box.X + options_box.Width * 72 / 100,
          row_y + pitch * PAUSE_OPT_CUSTOM_COSTUME + pitch / 8 - pop1_sprites_scale,
          "EDIT", small_font, 0);
        init_ingame_screen_button(&ui_open_btn,
          options_box.X + options_box.Width * 72 / 100,
          row_y + pitch * PAUSE_OPT_CUSTOM_UI + pitch / 8 - pop1_sprites_scale,
          "EDIT", small_font, 0);
    }

    slider_w = pause_main_box.Width * 131 / 256;
    arrow_w = slider_w * 9 / 256;

    y = pause_main_box.Y + pause_main_box.Height * 17 / 60;
    init_slider_with_arrows_centered(&samplevol_slider_box, &samplevol_arrow_l_box,
      &samplevol_arrow_r_box, &pause_main_box, y, slider_w, arrow_w, cutin_w, slider_h);

    y += pause_main_box.Height * 11 / 60;
    init_slider_with_arrows_centered(&midivol_slider_box, &midivol_arrow_l_box,
      &midivol_arrow_r_box, &pause_main_box, y, slider_w, arrow_w, cutin_w, slider_h);

    y += pause_main_box.Height * 11 / 60;
    init_slider_with_arrows_centered(&cdvolume_slider_box, &cdvolume_arrow_l_box,
      &cdvolume_arrow_r_box, &pause_main_box, y, slider_w, arrow_w, cutin_w, slider_h);

    x = pause_main_box.X + pause_main_box.Width * 67 / 114;
    y = pause_main_box.Y + pause_main_box.Height * 46 / 60;
    init_ingame_screen_button(&detail_hi_btn, x, y, gui_strings[477], small_font, 0);

    x = pause_main_box.X + pause_main_box.Width * 87 / 114;
    y = pause_main_box.Y + pause_main_box.Height * 46 / 60;
    init_ingame_screen_button(&detail_lo_btn, x, y, gui_strings[475], small_font, 0);

    x = pause_main_box.X + pause_main_box.Width * 47 / 114;
    y = pause_main_box.Y + pause_main_box.Height * 53 / 60;
    init_ingame_screen_button(&continue_btn, x, y, gui_strings[455], small_font, 0);

    x = pause_main_box.X + pause_main_box.Width * 76 / 114;
    y = pause_main_box.Y + pause_main_box.Height * 53 / 60;
    init_ingame_screen_button(&abort_btn, x, y, gui_strings[445], small_font, 0);
}

void start_pause_screen(void)
{
    snd_unkn1_volume_all_samples();
    update_danger_music(2);

    do_change_mouse(8);

    pause_main_box.Flags &= ~GBxFlg_BkgndDrawn;

}

/** Wait for the pause toggle key to be released.
 */
void wait_for_keypress_end(ushort game_key, TbBool impatient)
{
    TbBool ended;

    if (impatient) {
        clear_gamekey_pressed(game_key);
    }

    ended = false;
    while (!ended)
    {
        JoyUpdateInputs(&joy);

        if (!is_gamekey_pressed(game_key))
            ended = true;

        PlayCDTrack(ingame.CDTrack);

        swap_wscreen();
        game_update();
    }
}

void draw_pause_screen_static(struct ScreenBox *box)
{
    const char *s;
    int x, y, w;

    draw_box_cutedge(box, ingame_boxes_colr1);

    lbFontPtr = small_font;
    my_set_text_window(0, 0, lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);

    if ((ingame.PanelPermutation == 2) || (ingame.PanelPermutation == -3)) {
        lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
        lbDisplay.DrawColour = ingame_boxes_colr2;
    }
    {
        s = mission_name;
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width - w) / 2 + pause_main_box.Width / 114;
        y = pause_main_box.Y + pause_main_box.Height * 9 / 60;
        my_draw_text(x, y, s, 0);

        s = gui_strings[419]; // "sound volume"
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width - w) / 2 + pause_main_box.Width / 114;
        y += pause_main_box.Height * 5 / 60;
        my_draw_text(x, y, s, 0);

        s = gui_strings[420]; // "tension volume"
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width - w) / 2 + pause_main_box.Width / 114;
        y += pause_main_box.Height * 11 / 60;
        my_draw_text(x, y, s, 0);

        s = gui_strings[516]; // "cd volume"
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width - w) / 2 + pause_main_box.Width / 114;
        y += pause_main_box.Height * 11 / 60;
        my_draw_text(x, y, s, 0);
    }
    if (ingame.PanelPermutation == 2 || ingame.PanelPermutation == -3) {
        lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;
    }

    if (language_3str[0] == 'e')
    {
        // Special sprites with large text are only available for some languages
        struct TbSprite *p_spr;

        if (lbDisplay.GraphicsScreenHeight < 400)
        {
            p_spr = &pop1_sprites[102]; // "uplink paused"
            w = p_spr->SWidth;
            x = pause_main_box.X + (pause_main_box.Width - w) / 2;
            y = pause_main_box.Y + pause_main_box.Height * 2 / 60;
            LbSpriteDraw(x, y, p_spr);
        }
        else
        {
            w = pop1_sprites[102].SWidth + pop1_sprites[104].SWidth;
            p_spr = &pop1_sprites[102]; // "uplink"
            x = pause_main_box.X + (pause_main_box.Width - w) / 2;
            y = pause_main_box.Y + pause_main_box.Height * 2 / 60;
            LbSpriteDraw(x, y, p_spr);
            x += p_spr->SWidth;
            p_spr = &pop1_sprites[104]; // "paused"
            LbSpriteDraw(x, y, p_spr);

        }
        p_spr = &pop1_sprites[103]; // "detail"
        w = p_spr->SWidth;
        x = pause_main_box.X + (pause_main_box.Width / 2 - w) / 2 + pause_main_box.Width * 4 / 114;
        y = pause_main_box.Y + pause_main_box.Height * 46 / 60;
        LbSpriteDraw(x, y, p_spr);
    }
    else
    {
        // If no special sprites, draw the text in the normal font available in-game
        s = gui_strings[633]; // "uplink paused"
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width - w) / 2 + 3;
        y = pause_main_box.Y + pause_main_box.Height * 2 / 60;
        my_draw_text(x, y, s, 0);

        s = gui_strings[470]; // "detail"
        w = my_string_width(s);
        x = pause_main_box.X + (pause_main_box.Width / 2 - w) / 2 + pause_main_box.Width * 4 / 114;
        y = pause_main_box.Y + pause_main_box.Height * 48 / 60;
        my_draw_text(x, y, s, 0);
    }
}

void draw_pause_volume_bar(struct ScreenBox *p_box1, struct ScreenBox *p_box2, struct ScreenBox *p_box3, short *p_target)
{
    // Draw the main slider box
    draw_slant_box(p_box1, ingame_boxes_colr2);
    // Draw the side arrows
    draw_slant_left_arrow(p_box2, ingame_boxes_colr2);
    draw_slant_right_arrow(p_box3, ingame_boxes_colr2);

    if (*p_target) // Draw slider box filling
    {
        struct ScreenBox box4;
        short stp;

        stp = pop1_sprites_scale+1;
        box4.X = p_box1->X + 1 * stp;
        box4.Y = p_box1->Y + 1 * stp;
        box4.Width = (p_box1->Width - 3 * stp) * (*p_target) / STARTSCR_VOLUME_MAX;
        box4.Height = p_box1->Height - 2 * stp;
        draw_slant_box(&box4, colour_lookup[ColLU_WHITE]);
    }
}

/** Draws the box with the game settings: the name, shortcut key and current value of each. */
static void draw_options_box(void)
{
    char name[32], keys[32], val[32];
    int i, pitch, row_y, w;

    draw_box_cutedge(&options_box, ingame_boxes_colr1);

    lbFontPtr = small_font;
    my_set_text_window(0, 0, lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);

    lbDisplay.DrawFlags = 0;
    w = my_string_width("GAME SETTINGS");
    my_draw_text(options_box.X + (options_box.Width - w) / 2,
      options_box.Y + options_box.Height * 4 / 100, "GAME SETTINGS", 0);

    pitch = options_box.Height * 78 / 100 / PAUSE_OPT_COUNT;
    row_y = options_box.Y + options_box.Height * 15 / 100;
    for (i = 0; i < PAUSE_OPT_COUNT; i++)
    {
        int y;

        y = row_y + pitch * i;
        pause_option_label(i, name, keys, sizeof(name));
        pause_option_value(i, val, sizeof(val));

        lbDisplay.DrawFlags = 0;
        my_draw_text(options_box.X + options_box.Width * 5 / 100, y, name, 0);

        // The shortcut key, in the colour of the frames
        lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
        lbDisplay.DrawColour = ingame_boxes_colr2;
        my_draw_text(options_box.X + options_box.Width * 5 / 100,
          y + pitch * 52 / 100, keys, 0);
        lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;

        if (i == PAUSE_OPT_CUSTOM_UI) {
            // Opens the screen for the colours of the interface; lit while colours are chosen
            draw_ingame_button(&ui_open_btn, game_ui_colour_active());
            continue;
        }
        if (i == PAUSE_OPT_CUSTOM_COSTUME) {
            // Opens the screen for the costume and its colours, so it is a button, which is lit while
            // the colours are replaced
            draw_ingame_button(&custom_open_btn, game_custom_costume_active());
            continue;
        }
        draw_slant_left_arrow(&option_arrow_l_box[i], ingame_boxes_colr2);
        draw_slant_right_arrow(&option_arrow_r_box[i], ingame_boxes_colr2);
        w = my_string_width(val);
        my_draw_text(options_box.X + options_box.Width * 70 / 100 - w / 2 + options_box.Width * 2 / 100,
          y + pitch / 8, val, 0);
    }
    lbDisplay.DrawFlags = 0;
}

/** Number of colours of the palette which can be chosen; the last 32 are special colours. */
#define CUSTOM_PAL_COLOURS 224
#define CUSTOM_PAL_COLS 16
/** Number of sides of the agent shown in the preview, and the angles (0..7) of the person animation. */
#define CUSTOM_PREVIEW_ANGLES 2
static const ubyte custom_preview_angle[CUSTOM_PREVIEW_ANGLES] = { 1, 5 };

static struct ScreenBox custom_box;
static struct ScreenButton custom_coat_btn;
static struct ScreenButton custom_shoulders_btn;
static struct ScreenButton custom_reset_btn;
static struct ScreenButton custom_done_btn;
static short custom_cell, custom_grid_x, custom_grid_y;
static short custom_strip_cell, custom_strip_x[CUSTOM_COSTUME_PARTS], custom_strip_y[CUSTOM_COSTUME_PARTS];
static short custom_slider_x, custom_slider_y[4], custom_slider_w, custom_slider_h;
/** The arrows which choose the costume of the agents. */
static struct ScreenBox custom_cost_l_box, custom_cost_r_box;

/** The presets of the colours, on the screens of the costume and of the interface: the slots to choose, which loads what is
 * saved in it, and the button which saves what is set now in the slot which is chosen. */
static struct ScreenButton preset_btn[GAME_PRESETS];
static struct ScreenButton preset_save_btn;
static int costume_preset_sel[CUSTOM_TEAMS];
static int ui_preset_sel;

static int *preset_sel_ptr(TbBool ui)
{
    return ui ? &ui_preset_sel : &costume_preset_sel[game_custom_team()];
}

static TbBool preset_exists(TbBool ui, int slot)
{
    return ui ? game_ui_preset_exists(slot) : game_custom_preset_exists(slot);
}

static void init_preset_row(void)
{
    static const char *const num[GAME_PRESETS] = { "1", "2", "3", "4" };
    short u, y;
    int i;

    u = pop1_sprites_scale;
    y = custom_box.Y + 125 * u;
    for (i = 0; i < GAME_PRESETS; i++)
        init_ingame_screen_button(&preset_btn[i], custom_box.X + (50 + 13 * i) * u, y, num[i], small_font, 0);
    init_ingame_screen_button(&preset_save_btn, custom_box.X + 118 * u, y, "SAVE", small_font, 0);
}

/** Chooses the slot which holds the colours which are set now, if one does; or else the slot which was chosen. */
static void preset_sel_update(TbBool ui)
{
    int m, *p_sel;

    p_sel = preset_sel_ptr(ui);
    m = ui ? game_ui_preset_match() : game_custom_preset_match();
    if (m >= 0)
        *p_sel = m;
}

static void draw_preset_row(TbBool ui)
{
    char buf[32];
    int i, sel, w;
    short u;

    u = pop1_sprites_scale;
    sel = *preset_sel_ptr(ui);
    lbFontPtr = small_font;
    lbDisplay.DrawFlags = 0;
    my_draw_text(custom_box.X + 6 * u, preset_btn[0].Y + u, "PRESET", 0);
    for (i = 0; i < GAME_PRESETS; i++)
        draw_ingame_button(&preset_btn[i], (i == sel));
    draw_ingame_button(&preset_save_btn, 0);
    snprintf(buf, sizeof(buf), "%d %s", sel + 1, preset_exists(ui, sel) ? "SAVED" : "EMPTY");
    w = my_string_width(buf);
    (void)w;
    my_draw_text(custom_box.X + 136 * u, preset_btn[0].Y + u, buf, 0);
}

/** Handles the clicks on the slots and on the save button; true if the colours were loaded from a slot. */
static TbBool input_preset_row(TbBool ui, TbBool enabled)
{
    int i, *p_sel;

    p_sel = preset_sel_ptr(ui);
    for (i = 0; i < GAME_PRESETS; i++)
    {
        if (input_ingame_button(&preset_btn[i], (i == *p_sel)) && enabled)
        {
            *p_sel = i;
            if (preset_exists(ui, i))
            {
                if (ui)
                    game_ui_preset_load(i);
                else
                    game_custom_preset_load(i);
                return true;
            }
        }
    }
    if (input_ingame_button(&preset_save_btn, 0) && enabled)
    {
        if (ui)
            game_ui_preset_save(*p_sel);
        else
            game_custom_preset_save(*p_sel);
    }
    return false;
}
static void init_custom_costume_boxes(void)
{
    short u, cx;
    int i;

    u = pop1_sprites_scale;
    custom_box.Width = 233 * u;
    custom_box.Height = 134 * u;
    custom_box.X = (lbDisplay.GraphicsScreenWidth - custom_box.Width) / 2;
    if (custom_box.X < 0)
        custom_box.X = 0;
    custom_box.Y = (lbDisplay.GraphicsScreenHeight - custom_box.Height) / 2;

    custom_cell = 5 * u;
    custom_grid_x = custom_box.X + 6 * u;
    custom_grid_y = custom_box.Y + custom_box.Height * 16 / 100;

    // Under the palette, the sliders of the three channels of the colour
    custom_cost_l_box.Width = custom_cost_r_box.Width = 8 * u;
    custom_cost_l_box.Height = custom_cost_r_box.Height = 6 * u;
    custom_cost_l_box.X = custom_box.X + 152 * u;
    custom_cost_r_box.X = custom_box.X + 188 * u;
    custom_cost_l_box.Y = custom_cost_r_box.Y = custom_box.Y + custom_box.Height * 4 / 100 - 2 * u;
    custom_slider_x = custom_box.X + 14 * u;
    custom_slider_w = 72 * u;
    custom_slider_h = 5 * u;
    for (i = 0; i < 4; i++)
        custom_slider_y[i] = custom_grid_y + 70 * u + i * 7 * u;

    // The right part has the buttons choosing the part of the costume, with its shades below each
    cx = custom_box.X + 196 * u;
    custom_strip_cell = 4 * u;
    init_ingame_screen_button(&custom_coat_btn, cx, custom_box.Y + custom_box.Height * 17 / 100,
      game_custom_costume_part_name(CUSTOM_COSTUME_COAT), small_font, 0);
    custom_strip_x[CUSTOM_COSTUME_COAT] = cx - game_custom_costume_shades(CUSTOM_COSTUME_COAT) * custom_strip_cell / 2;
    custom_strip_y[CUSTOM_COSTUME_COAT] = custom_box.Y + custom_box.Height * 33 / 100;
    init_ingame_screen_button(&custom_shoulders_btn, cx, custom_box.Y + custom_box.Height * 47 / 100,
      game_custom_costume_part_name(CUSTOM_COSTUME_SHOULDERS), small_font, 0);
    custom_strip_x[CUSTOM_COSTUME_SHOULDERS] = cx - game_custom_costume_shades(CUSTOM_COSTUME_SHOULDERS) * custom_strip_cell / 2;
    custom_strip_y[CUSTOM_COSTUME_SHOULDERS] = custom_box.Y + custom_box.Height * 63 / 100;

    init_ingame_screen_button(&custom_reset_btn, custom_box.X + 150 * u,
      custom_box.Y + custom_box.Height * 80 / 100, "ORIGINAL", small_font, 0);
    init_ingame_screen_button(&custom_done_btn, custom_box.X + 200 * u,
      custom_box.Y + custom_box.Height * 80 / 100, gui_strings[455], small_font, 0); // "continue"
    init_preset_row();
}

/** Draws the standing agent, with the sprites loaded now, scaled to fit in the given rectangle. */
static void draw_custom_costume_agent(short rx, short ry, short rw, short rh, ubyte angle)
{
    const struct Frame *p_frm;
    const struct Element *p_elem;
    ushort frm, el;
    int min_x, min_y, max_x, max_y;
    int k, ox, oy;
    int pass;

    // The team which is played: the agents, or the Epoch members
    frm = nstart_ani[people_frames[(game_custom_team() == CUSTOM_TEAM_EPOCH) ? SubTT_PERS_ZEALOT : SubTT_PERS_AGENT]
      [ANIM_PERS_IDLE] + angle];
    if (&frame[frm] >= frame_end)
        return;
    p_frm = &frame[frm];

    // Pass 0 finds the size of the agent, pass 1 draws it; the elements are positioned in half pixels
    min_x = min_y = 0x7FFFFFF;
    max_x = max_y = -0x7FFFFFF;
    k = 1;
    ox = oy = 0;
    for (pass = 0; pass < 2; pass++)
    {
        for (el = p_frm->FirstElement; el > 0; el = p_elem->Next)
        {
            struct TbSprite *p_spr;
            int ex, ey;

            p_elem = &melement_ani[el];
            p_spr = (struct TbSprite *)((ubyte *)m_sprites + p_elem->ToSprite);
            if ((p_spr <= m_sprites) || (p_spr >= m_sprites_end) || ((p_elem->Flags & 0xFE00) != 0))
                continue;
            ex = p_elem->X >> 1;
            ey = p_elem->Y >> 1;
            if (pass == 0) {
                if (min_x > ex) min_x = ex;
                if (min_y > ey) min_y = ey;
                if (max_x < ex + p_spr->SWidth) max_x = ex + p_spr->SWidth;
                if (max_y < ey + p_spr->SHeight) max_y = ey + p_spr->SHeight;
            } else {
                lbDisplay.DrawFlags = p_elem->Flags & 0x07;
                LbSpriteDrawScaled(ox + (ex - min_x) * k, oy + (ey - min_y) * k, p_spr,
                  p_spr->SWidth * k, p_spr->SHeight * k);
            }
        }
        if (pass == 0) {
            if (max_x <= min_x)
                return;
            // The biggest whole multiplication which fits, with a margin
            k = (rw * 9 / 10) / (max_x - min_x);
            if (k > (rh * 9 / 10) / (max_y - min_y))
                k = (rh * 9 / 10) / (max_y - min_y);
            if (k < 1)
                k = 1;
            ox = rx + (rw - (max_x - min_x) * k) / 2;
            oy = ry + (rh - (max_y - min_y) * k) / 2;
        }
    }
    lbDisplay.DrawFlags = 0;
}

/** Gives the colour of the part which the sliders show: the chosen one, or the original. */
static void custom_costume_part_rgb(int part, ubyte *rgb)
{
    if (game_custom_used[part])
        memcpy(rgb, game_custom_rgb[part], 3);
    else
        game_custom_costume_default_rgb(part, rgb);
}

/** Sets the colour of the part. The colours are of the costume which has them, so it is put on; the
 * sprites get the marker colours the first time; after that only the colour which the GPU shows for
 * the markers changes, so it is immediate. */
static void custom_costume_set_rgb(int part, const ubyte *rgb)
{
    TbBool reload;

    reload = !game_custom_used[part];
    memcpy(game_custom_rgb[part], rgb, 3);
    game_custom_used[part] = true;
    // The colours of the agents are in costume 3; the Epoch members have them in all of the costumes
    if ((game_custom_team() == CUSTOM_TEAM_EUROCORP) && (ingame.TrenchcoatPreference != CUSTOM_COSTUME_BASE)) {
        ingame.TrenchcoatPreference = CUSTOM_COSTUME_BASE;
        reload = true;
    }
    if (reload)
        prep_multicolor_sprites();
    else
        game_custom_costume_publish();
}

/** Changes the costume of the agents by one. */
static void custom_costume_change(int dir)
{
    if (dir > 0)
        game_option_inc(GOpt_TrenchcoatPreference);
    else
        game_option_dec(GOpt_TrenchcoatPreference);
    prep_multicolor_sprites();
}

static void draw_custom_costume_screen(int part)
{
    static const ubyte slider_colr[3] = { 121, 242, 17 };
    static const char *slider_name[3] = { "R", "G", "B" };
    ubyte rgb[3];
    const char *s;
    int i, w, x, y;

    draw_box_cutedge(&custom_box, ingame_boxes_colr1);

    lbFontPtr = small_font;
    my_set_text_window(0, 0, lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);
    lbDisplay.DrawFlags = 0;
    s = (game_custom_team() == CUSTOM_TEAM_EPOCH) ? "EPOCH COSTUME" : "AGENT COSTUME";
    my_draw_text(custom_box.X + 6 * pop1_sprites_scale,
      custom_box.Y + custom_box.Height * 4 / 100, s, 0);
    // The costume of the agents: which one is worn, with arrows to change it; the Epoch members have one costume
    if (game_custom_team() == CUSTOM_TEAM_EUROCORP)
    {
        char buf[16];

        snprintf(buf, sizeof(buf), "%d", (int)ingame.TrenchcoatPreference);
        draw_slant_left_arrow(&custom_cost_l_box, ingame_boxes_colr2);
        draw_slant_right_arrow(&custom_cost_r_box, ingame_boxes_colr2);
        w = my_string_width(buf);
        my_draw_text(custom_box.X + 177 * pop1_sprites_scale - w / 2,
          custom_box.Y + custom_box.Height * 4 / 100, buf, 0);
    }
    if (!game_custom_costume_hw()) {
        s = "COLOURS: GPU RENDERING ONLY";
        w = my_string_width(s);
        my_draw_text(custom_box.X + (custom_box.Width - w) / 2,
          custom_box.Y + custom_box.Height * 93 / 100, s, 0);
    }

    // The palette, as a quick way to get a colour; the colour under the mouse is framed
    for (i = 0; i < CUSTOM_PAL_COLOURS; i++)
    {
        x = custom_grid_x + (i % CUSTOM_PAL_COLS) * custom_cell;
        y = custom_grid_y + (i / CUSTOM_PAL_COLS) * custom_cell;
        lbDisplay.DrawFlags = 0;
        LbDrawBox(x, y, custom_cell - 1, custom_cell - 1, game_custom_costume_swatch(i));
        if (mouse_move_over_box_coords(x, y, x + custom_cell - 1, y + custom_cell - 1)) {
            lbDisplay.DrawFlags = Lb_SPRITE_OUTLINE;
            LbDrawBox(x - 1, y - 1, custom_cell + 1, custom_cell + 1, colour_lookup[ColLU_WHITE]);
        }
    }

    // The sliders of the red, green and blue of the part which is chosen
    custom_costume_part_rgb(part, rgb);
    for (i = 0; i < 3; i++)
    {
        lbDisplay.DrawFlags = 0;
        my_draw_text(custom_box.X + 6 * pop1_sprites_scale, custom_slider_y[i], slider_name[i], 0);
        LbDrawBox(custom_slider_x, custom_slider_y[i], custom_slider_w, custom_slider_h, 72);
        LbDrawBox(custom_slider_x, custom_slider_y[i], custom_slider_w * rgb[i] / 255, custom_slider_h, slider_colr[i]);
    }

    draw_ingame_button(&custom_coat_btn, (part == CUSTOM_COSTUME_COAT));
    draw_ingame_button(&custom_shoulders_btn, (part == CUSTOM_COSTUME_SHOULDERS));
    draw_ingame_button(&custom_reset_btn, 0);
    draw_ingame_button(&custom_done_btn, 0);

    // The preview: the whole agent from two sides, drawn from the sprites; the marker colours in
    // them are shown by the GPU in the exact colours
    {
        short u;

        u = pop1_sprites_scale;
        x = custom_box.X + 92 * u;
        y = custom_box.Y + custom_box.Height * 17 / 100;
        lbDisplay.DrawFlags = 0;
        LbDrawBox(x, y, 76 * u, 76 * u, 72);
        for (i = 0; i < CUSTOM_PREVIEW_ANGLES; i++)
            draw_custom_costume_agent(x + i * 38 * u, y, 38 * u, 76 * u, custom_preview_angle[i]);
    }

    // The shades the parts have now: the markers for the colours which are replaced
    for (w = 0; (w < CUSTOM_COSTUME_PARTS) && game_custom_costume_hw()
      && ((game_custom_team() == CUSTOM_TEAM_EPOCH) || (ingame.TrenchcoatPreference == CUSTOM_COSTUME_BASE)); w++)
    {
        for (i = 0; i < game_custom_costume_shades(w); i++)
        {
            TbPixel colr;

            if (game_custom_used[w])
                colr = game_custom_costume_marker(w, i);
            else
                colr = game_custom_costume_original(w, i);
            lbDisplay.DrawFlags = 0;
            LbDrawBox(custom_strip_x[w] + i * custom_strip_cell, custom_strip_y[w],
              custom_strip_cell, custom_strip_cell, colr);
        }
    }
    draw_preset_row(false);
    lbDisplay.DrawFlags = 0;
}

/** Opens the screen of the agents' costume: the costume which is worn, and the colours of the
 * shoulders and trench coat of costume 3, which can be changed only when the GPU draws the scene.
 * Choosing a colour puts costume 3 on.
 */
static void custom_costume_screen(void)
{
    TbBool done, hw;
    int part, i;

    part = CUSTOM_COSTUME_COAT;
    init_custom_costume_boxes();
    preset_sel_update(false);
    // The screen has the marker indices of the preview, which the GPU replaces
    LbHwCustomLayerSet(true);

    done = false;
    while (!done)
    {
        JoyUpdateInputs(&joy);

        hw = game_custom_costume_hw();
        if (lbDisplay.LeftButton && (game_custom_team() == CUSTOM_TEAM_EUROCORP))
        {
            if (mouse_move_over_slant_box(&custom_cost_l_box)) {
                lbDisplay.LeftButton = 0;
                custom_costume_change(-1);
            } else if (mouse_move_over_slant_box(&custom_cost_r_box)) {
                lbDisplay.LeftButton = 0;
                custom_costume_change(1);
            }
        }
        if (input_ingame_button(&custom_coat_btn, part == CUSTOM_COSTUME_COAT) && hw)
            part = CUSTOM_COSTUME_COAT;
        if (input_ingame_button(&custom_shoulders_btn, part == CUSTOM_COSTUME_SHOULDERS) && hw)
            part = CUSTOM_COSTUME_SHOULDERS;
        if (input_ingame_button(&custom_reset_btn, 0) && hw) {
            for (i = 0; i < CUSTOM_COSTUME_PARTS; i++)
                game_custom_used[i] = false;
            prep_multicolor_sprites();
        }
        if (input_ingame_button(&custom_done_btn, 0))
            done = true;
        if (input_preset_row(false, hw))
        {
            // The colours of the preset are on; the colours of the agents are in costume 3
            if ((game_custom_team() == CUSTOM_TEAM_EUROCORP) && (ingame.TrenchcoatPreference != CUSTOM_COSTUME_BASE))
                ingame.TrenchcoatPreference = CUSTOM_COSTUME_BASE;
            prep_multicolor_sprites();
        }
        if (hw && lbDisplay.LeftButton)
        {
            // A click on the palette takes its colour
            for (i = 0; i < CUSTOM_PAL_COLOURS; i++)
            {
                int x, y;

                x = custom_grid_x + (i % CUSTOM_PAL_COLS) * custom_cell;
                y = custom_grid_y + (i / CUSTOM_PAL_COLS) * custom_cell;
                if (mouse_move_over_box_coords(x, y, x + custom_cell - 1, y + custom_cell - 1)) {
                    ubyte rgb[3];

                    lbDisplay.LeftButton = 0;
                    game_custom_costume_palette_rgb(i, rgb);
                    custom_costume_set_rgb(part, rgb);
                    break;
                }
            }
        }
        if (hw && lbDisplay.MLeftButton)
        {
            // Holding the mouse button on a slider sets the channel to where the mouse is
            for (i = 0; i < 3; i++)
            {
                int mx, my;

                mx = lbDisplay.MMouseX;
                my = lbDisplay.MMouseY;
                if ((my >= custom_slider_y[i]) && (my < custom_slider_y[i] + custom_slider_h)
                  && (mx >= custom_slider_x - custom_slider_h) && (mx <= custom_slider_x + custom_slider_w + custom_slider_h)) {
                    ubyte rgb[3];
                    int v;

                    custom_costume_part_rgb(part, rgb);
                    v = (mx - custom_slider_x) * 255 / custom_slider_w;
                    rgb[i] = (v < 0) ? 0 : ((v > 255) ? 255 : v);
                    custom_costume_set_rgb(part, rgb);
                }
            }
        }

        game_pause_scene_backdrop_restore();
        draw_custom_costume_screen(part);

        lbDisplay.DrawFlags = 0;
        process_sound_heap();
        PlayCDTrack(ingame.CDTrack);

        swap_wscreen();
        game_update();
    }

    LbHwCustomLayerSet(false);
    lbDisplay.LeftButton = 0;
}

static struct ScreenButton ui_panel_btn;
static struct ScreenButton ui_accent_btn;

/** The screen for the colours of the interface: the panels and the accent. It is laid out like the one of
 * the costume: the palette and the sliders of the colour on the left, the parts to choose on the right. */
static void init_ui_colour_boxes(void)
{
    short u, cx;

    u = pop1_sprites_scale;
    // The geometry of the costume screen is used again: the box, the palette, the sliders, the two buttons below
    init_custom_costume_boxes();
    cx = custom_box.X + 196 * u;
    init_ingame_screen_button(&ui_panel_btn, cx, custom_box.Y + custom_box.Height * 17 / 100,
      game_ui_colour_part_name(0), small_font, 0);
    init_ingame_screen_button(&ui_accent_btn, cx, custom_box.Y + custom_box.Height * 47 / 100,
      game_ui_colour_part_name(1), small_font, 0);
}

static void ui_colour_part_rgb(int part, ubyte *rgb)
{
    if (game_ui_colour_used[part])
        memcpy(rgb, game_ui_colour_rgb[part], 3);
    else
        game_ui_colour_default_rgb(part, rgb);
}

static void draw_ui_colour_screen(int part)
{
    static const ubyte slider_colr[4] = { 121, 242, 17, 72 };
    static const char *slider_name[4] = { "R", "G", "B", "A" };
    ubyte rgb[3];
    const char *s;
    int i, w, x, y;
    short u;

    u = pop1_sprites_scale;
    draw_box_cutedge(&custom_box, ingame_boxes_colr1);

    lbFontPtr = small_font;
    my_set_text_window(0, 0, lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);
    lbDisplay.DrawFlags = 0;
    my_draw_text(custom_box.X + 6 * u, custom_box.Y + custom_box.Height * 4 / 100, "INTERFACE COLOUR", 0);
    if (!game_ui_colour_hw()) {
        s = "COLOURS: GPU RENDERING ONLY";
        w = my_string_width(s);
        my_draw_text(custom_box.X + (custom_box.Width - w) / 2,
          custom_box.Y + custom_box.Height * 93 / 100, s, 0);
    }

    // The palette, as a quick way to get a colour; the colour under the mouse is framed
    for (i = 0; i < CUSTOM_PAL_COLOURS; i++)
    {
        x = custom_grid_x + (i % CUSTOM_PAL_COLS) * custom_cell;
        y = custom_grid_y + (i / CUSTOM_PAL_COLS) * custom_cell;
        lbDisplay.DrawFlags = 0;
        LbDrawBox(x, y, custom_cell - 1, custom_cell - 1, game_custom_costume_swatch(i));
        if (mouse_move_over_box_coords(x, y, x + custom_cell - 1, y + custom_cell - 1)) {
            lbDisplay.DrawFlags = Lb_SPRITE_OUTLINE;
            LbDrawBox(x - 1, y - 1, custom_cell + 1, custom_cell + 1, colour_lookup[ColLU_WHITE]);
        }
    }

    // The sliders of the red, green and blue of the part which is chosen, and the opacity of its colour
    ui_colour_part_rgb(part, rgb);
    for (i = 0; i < 4; i++)
    {
        int val = (i < 3) ? rgb[i] : game_ui_colour_alpha[part];

        lbDisplay.DrawFlags = 0;
        my_draw_text(custom_box.X + 6 * u, custom_slider_y[i], slider_name[i], 0);
        LbDrawBox(custom_slider_x, custom_slider_y[i], custom_slider_w, custom_slider_h, 72);
        LbDrawBox(custom_slider_x, custom_slider_y[i], custom_slider_w * val / 255, custom_slider_h,
          (i < 3) ? slider_colr[i] : colour_lookup[ColLU_WHITE]);
    }

    draw_ingame_button(&ui_panel_btn, (part == 0));
    draw_ingame_button(&ui_accent_btn, (part == 1));
    draw_ingame_button(&custom_reset_btn, 0);
    draw_ingame_button(&custom_done_btn, 0);

    // A sample of the interface in the middle: the screen itself has the colours which are chosen
    x = custom_box.X + 92 * u;
    y = custom_box.Y + custom_box.Height * 17 / 100;
    lbDisplay.DrawFlags = 0;
    LbDrawBox(x, y, 76 * u, 76 * u, 72);
    {
        struct ScreenBox sample;

        sample.X = x + 4 * u;
        sample.Y = y + 4 * u;
        sample.Width = 68 * u;
        sample.Height = 26 * u;
        draw_box_cutedge(&sample, ingame_boxes_colr1);
        lbDisplay.DrawFlags = 0;
        my_draw_text(sample.X + 3 * u, sample.Y + 3 * u, "PANEL", 0);
        lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
        lbDisplay.DrawColour = ingame_boxes_colr2;
        my_draw_text(sample.X + 3 * u, sample.Y + 14 * u, "SHADES OF THE PANEL", 0);
        lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;
    }
    // The accent: a line and some text in the colour of the map and its marks
    LbDrawBox(x + 4 * u, y + 38 * u, 68 * u, u, colour_lookup[ColLU_CYAN]);
    LbDrawBox(x + 4 * u, y + 48 * u, 68 * u, u, colour_lookup[ColLU_CYAN]);
    LbDrawBox(x + 4 * u, y + 38 * u, u, 11 * u, colour_lookup[ColLU_CYAN]);
    LbDrawBox(x + 71 * u, y + 38 * u, u, 11 * u, colour_lookup[ColLU_CYAN]);
    lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
    lbDisplay.DrawColour = colour_lookup[ColLU_CYAN];
    my_draw_text(x + 8 * u, y + 40 * u, "ACCENT", 0);
    lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;
    draw_preset_row(true);
    lbDisplay.DrawFlags = 0;
}

static void ui_colour_screen(void)
{
    TbBool done, hw;
    int part, i;

    part = 0;
    init_ui_colour_boxes();
    preset_sel_update(true);

    done = false;
    while (!done)
    {
        JoyUpdateInputs(&joy);

        hw = game_ui_colour_hw();
        if (input_ingame_button(&ui_panel_btn, part == 0) && hw)
            part = 0;
        if (input_ingame_button(&ui_accent_btn, part == 1) && hw)
            part = 1;
        if (input_ingame_button(&custom_reset_btn, 0) && hw)
            game_ui_colour_reset();
        if (input_ingame_button(&custom_done_btn, 0))
            done = true;
        input_preset_row(true, hw);
        if (hw && lbDisplay.LeftButton)
        {
            // A click on the palette takes its colour
            for (i = 0; i < CUSTOM_PAL_COLOURS; i++)
            {
                int x, y;

                x = custom_grid_x + (i % CUSTOM_PAL_COLS) * custom_cell;
                y = custom_grid_y + (i / CUSTOM_PAL_COLS) * custom_cell;
                if (mouse_move_over_box_coords(x, y, x + custom_cell - 1, y + custom_cell - 1)) {
                    ubyte rgb[3];

                    lbDisplay.LeftButton = 0;
                    game_custom_costume_palette_rgb(i, rgb);
                    game_ui_colour_set(part, rgb);
                    break;
                }
            }
        }
        if (hw && lbDisplay.MLeftButton)
        {
            // Holding the mouse button on a slider sets the channel to where the mouse is
            for (i = 0; i < 4; i++)
            {
                int mx, my;

                mx = lbDisplay.MMouseX;
                my = lbDisplay.MMouseY;
                if ((my >= custom_slider_y[i]) && (my < custom_slider_y[i] + custom_slider_h)
                  && (mx >= custom_slider_x - custom_slider_h) && (mx <= custom_slider_x + custom_slider_w + custom_slider_h)) {
                    ubyte rgb[3];
                    int v;

                    v = (mx - custom_slider_x) * 255 / custom_slider_w;
                    v = (v < 0) ? 0 : ((v > 255) ? 255 : v);
                    if (i == 3) {
                        // the opacity; a little opacity is kept, so the interface is not lost
                        game_ui_colour_set_alpha(part, (v < 30) ? 30 : v);
                    } else {
                        ui_colour_part_rgb(part, rgb);
                        rgb[i] = v;
                        game_ui_colour_set(part, rgb);
                    }
                }
            }
        }

        game_pause_scene_backdrop_restore();
        draw_ui_colour_screen(part);

        lbDisplay.DrawFlags = 0;
        process_sound_heap();
        PlayCDTrack(ingame.CDTrack);

        swap_wscreen();
        game_update();
    }

    lbDisplay.LeftButton = 0;
}
/** Applies a click on one of the arrows of the settings. */
static void input_options_box(void)
{
    int i;

    if (!lbDisplay.LeftButton)
        return;
    if (input_ingame_button(&custom_open_btn, 0)) {
        custom_costume_screen();
        return;
    }
    if (input_ingame_button(&ui_open_btn, 0)) {
        ui_colour_screen();
        return;
    }
    for (i = 0; i < PAUSE_OPT_COUNT; i++)
    {
        int dir;

        if ((i == PAUSE_OPT_CUSTOM_COSTUME) || (i == PAUSE_OPT_CUSTOM_UI))
            continue;
        if (mouse_move_over_slant_box(&option_arrow_l_box[i]))
            dir = -1;
        else if (mouse_move_over_slant_box(&option_arrow_r_box[i]))
            dir = 1;
        else
            continue;
        lbDisplay.LeftButton = 0;
        pause_option_change(i, dir);
        return;
    }
}

ubyte show_pause_screen(struct ScreenBox *box)
{
    if ((box->Flags & GBxFlg_BkgndDrawn) == 0)
    {
        draw_pause_screen_static(box);
        draw_options_box();
        box->Flags |= GBxFlg_BkgndDrawn;
    }

    lbDisplay.DrawFlags = 0;
    draw_pause_volume_bar(&samplevol_slider_box, &samplevol_arrow_l_box, &samplevol_arrow_r_box, &startscr_samplevol);
    draw_pause_volume_bar(&midivol_slider_box, &midivol_arrow_l_box, &midivol_arrow_r_box, &startscr_midivol);
    draw_pause_volume_bar(&cdvolume_slider_box, &cdvolume_arrow_l_box, &cdvolume_arrow_r_box, &startscr_cdvolume);

    draw_ingame_button(&detail_hi_btn, (ingame.DetailLevel == 1));
    draw_ingame_button(&detail_lo_btn, (ingame.DetailLevel == 0));
    draw_ingame_button(&continue_btn, 0);
    draw_ingame_button(&abort_btn, 0);

    return 0;
}

void *input_pause_screen(void)
{
    short *target;
    void *affected;

    affected = NULL;

    {
    target = &startscr_samplevol;

    if (input_slant_left_arrow(&samplevol_arrow_l_box, target))
        affected = target;
    if (input_slant_right_arrow(&samplevol_arrow_r_box, target))
        affected = target;
    if (input_slant_box(&samplevol_slider_box, target))
        affected = target;
    }

    {
    target = &startscr_midivol;

    if (input_slant_left_arrow(&midivol_arrow_l_box, target))
        affected = target;
    if (input_slant_right_arrow(&midivol_arrow_r_box, target))
        affected = target;
    if (input_slant_box(&midivol_slider_box, target))
        affected = target;
    }

    {
    target = &startscr_cdvolume;

    if (input_slant_left_arrow(&cdvolume_arrow_l_box, target))
        affected = target;
    if (input_slant_right_arrow(&cdvolume_arrow_r_box, target))
        affected = target;
    if (input_slant_box(&cdvolume_slider_box, target))
        affected = target;
    }

    {
    target = &startscr_cdvolume;

        if (input_ingame_button(&detail_hi_btn, ingame.DetailLevel == 1)) {
            ingame.DetailLevel = 1;
            affected = target;
        }
        if (input_ingame_button(&detail_lo_btn, ingame.DetailLevel == 0)) {
            ingame.DetailLevel = 0;
            affected = target;
        }
    }

    return affected;
}

TbBool pause_screen_handle(void)
{
    void *affected;
    TbBool resume_game;

    init_pause_screen_boxes();
    game_pause_scene_backdrop_begin();
    start_pause_screen();
    // With the scene drawn by the GPU, the boxes are translucent over it by the GPU
    LbHwOverlaySet(game_pause_scene_on_gpu() ? ingame_boxes_colr1 : -1);

    // Wait for the pause key to be released
    wait_for_keypress_end(GKey_PAUSE, true);

    show_pause_screen(&pause_main_box);

    swap_wscreen();
    if (!ingame.fld_unk7DA)
        SetMusicVolume(100, 0x7F);

    // For pictures of the screens (SWFX_OPEN_EDITOR=costume or ui): the screen is opened at once, with colours chosen
    if (getenv("SWFX_OPEN_EDITOR") != NULL)
    {
        if (strcmp(getenv("SWFX_OPEN_EDITOR"), "ui") == 0)
        {
            ui_colour_screen();
        }
        else
        {
            static const ubyte coat[3] = { 190, 40, 50 };
            static const ubyte shoulders[3] = { 225, 190, 60 };

            custom_costume_set_rgb(CUSTOM_COSTUME_COAT, coat);
            custom_costume_set_rgb(CUSTOM_COSTUME_SHOULDERS, shoulders);
            custom_costume_screen();
        }
    }

    resume_game = false;
    while (!resume_game)
    {
        JoyUpdateInputs(&joy);

        input_options_box();
        affected = input_pause_screen();

        if (affected == &startscr_samplevol)
        {
            sfx_apply_samplevol();
            if (!IsSamplePlaying(0, 80, 0))
                play_sample_using_heap(0, 80, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
        }
        else if (affected == &startscr_midivol)
        {
            sfx_apply_midivol();
        }
        else if (affected == &startscr_cdvolume)
        {
            sfx_apply_cdvolume();
        }
        else if (affected == &ingame.DetailLevel)
        {
            bang_set_detail(1 - ingame.DetailLevel);
        }

        // The boxes are translucent, so everything is drawn again over the scene each frame;
        // with the GPU drawing the scene they are translucent by the GPU, and the renderer can change
        LbHwOverlaySet(game_pause_scene_on_gpu() ? ingame_boxes_colr1 : -1);
        game_pause_scene_backdrop_restore();
        pause_main_box.Flags &= ~GBxFlg_BkgndDrawn;
        show_pause_screen(&pause_main_box);

        if (input_ingame_button(&continue_btn, 0))
            resume_game = true;

        if (input_ingame_button(&abort_btn, 0))
        {
            LbHwOverlaySet(-1);
            game_pause_scene_backdrop_end();
            swap_wscreen();
            SetMusicVolume(100, 0);
            StopAllSamples();
            StopCD();
            return 1;
        }

        if (is_gamekey_pressed(GKey_PAUSE))
            resume_game = true;

        lbDisplay.DrawFlags = 0;
        process_sound_heap();
        PlayCDTrack(ingame.CDTrack);

        swap_wscreen();
        game_update();
    }

    LbHwOverlaySet(-1);
    game_pause_scene_backdrop_end();
    // Wait for the pause key to be released
    wait_for_keypress_end(GKey_PAUSE, false);

    lbDisplay.RightButton = 0;
    lbDisplay.LeftButton = 0;
    if (!ingame.fld_unk7DA)
        SetMusicVolume(100, 0);
    return 0;
}

/******************************************************************************/

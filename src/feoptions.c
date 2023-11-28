/******************************************************************************/
// Syndicate Wars Port, source port of the classic strategy game from Bullfrog.
/******************************************************************************/
/** @file feoptions.c
 *     Front-end desktop and menu system, options screen.
 * @par Purpose:
 *     Implement functions for options screen in front-end desktop.
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
#include "feoptions.h"

#include "bftext.h"
#include "bfsprite.h"
#include "bfkeybd.h"
#include "bfaudio.h"
#include "bfscd.h"
#include "bflib_joyst.h"
#include "guiboxes.h"
#include "guitext.h"
#include "display.h"
#include "game.h"
#include "sound.h"
#include "swlog.h"
/******************************************************************************/
extern struct ScreenBox audio_tracks_box;
extern struct ScreenBox audio_volume_boxes[3];
extern struct ScreenButton options_audio_buttons[7];

extern struct ScreenButton options_gfx_buttons[16];

extern char options_title_text[];

extern short word_1C4866[3];

extern struct TbSprite *sprites_Icons0_0;

ubyte ac_change_panel_permutation(ubyte click);
ubyte ac_change_trenchcoat_preference(ubyte click);
ubyte ac_show_audio_tracks_box(struct ScreenBox *box);
ubyte ac_show_audio_volume_box(struct ScreenBox *box);

void show_audio_volume_box_func_02(short a1, short a2, short a3, short a4, TbPixel colour)
{
    asm volatile (
      "push %4\n"
      "call ASM_show_audio_volume_box_func_02\n"
        : : "a" (a1), "d" (a2), "b" (a3), "c" (a4), "g" (colour));
}

void draw_vert_slider_main_body(struct ScreenBox *box, short *target_ptr)
{
    short x1b, x2c;
    short wtext1, wtext2;

    lbDisplay.DrawFlags = 0;
    // text in cut-ins at start and at end of slider
    // TODO cut-in sizes should be stored somewhere, so that text drawing func has access to it
    wtext2 = my_string_width(gui_strings[422]) + 2;
    wtext1 = my_string_width(gui_strings[421]) + 2;
    x1b = box->Width - wtext1 - wtext2 - 14;
    if (*target_ptr < wtext1)
    {
        x2c = *target_ptr;
        show_audio_volume_box_func_02(box->X + 9, box->Y + 11, x2c, 17, 174);
        lbDisplay.DrawFlags = 0x0004;
        show_audio_volume_box_func_02(box->X + 9 + x2c, box->Y + 11, wtext1 - x2c, 17, 174);
        show_audio_volume_box_func_02(box->X + 20 + wtext1, box->Y, x1b, 28, 174);
        show_audio_volume_box_func_02(box->X + box->Width + 6 - wtext2, box->Y, wtext2, 17, 174);
    }
    else if (*target_ptr >= 322 - wtext2)
    {
        x2c = (*target_ptr) - (322 - wtext2);
        show_audio_volume_box_func_02(box->X + 9, box->Y + 11, wtext1, 17, 174);
        show_audio_volume_box_func_02(box->X + 20 + wtext1, box->Y, x1b, 28, 174);
        show_audio_volume_box_func_02(box->X + box->Width + 6 - wtext2, box->Y, x2c, 17, 174);
        lbDisplay.DrawFlags = 0x0004;
        show_audio_volume_box_func_02(box->X + box->Width + 6 + x2c - wtext2, box->Y, wtext2 - x2c, 17, 174);
    }
    else
    {
        x2c = (*target_ptr) - wtext1;
        show_audio_volume_box_func_02(box->X + 9, box->Y + 11, wtext1, 17, 174);
        show_audio_volume_box_func_02(box->X + 20 + wtext1, box->Y, x2c, 28, 174);
        lbDisplay.DrawFlags = 0x0004;
        show_audio_volume_box_func_02(box->X + 20 + x2c + wtext1, box->Y, x1b - x2c, 28, 174);
        show_audio_volume_box_func_02(box->X + box->Width + 6 - wtext2, box->Y, wtext2, 17, 174);
    }
}

void draw_vert_slider_main_body_text(struct ScreenBox *box, struct ScreenBox *tbox, short *target_ptr)
{
    short wtext2;

    lbDisplay.DrawFlags = 0;
    wtext2 = my_string_width(gui_strings[422]) + 2; // TODO reuse from main body rather than re-compute
    draw_text_purple_list2(box->X + 9 - tbox->X, 24, gui_strings[421], 0);
    draw_text_purple_list2(box->X + box->Width - 13 - wtext2 - tbox->X, 41, gui_strings[422], 0);
}

TbBool input_vert_slider_main_body(struct ScreenBox *box, short *target_ptr)
{
    TbBool target_affected;

    target_affected = false;
    if (mouse_move_over_box(box))
    {
        if (lbDisplay.MLeftButton)
        {
            short delta_y, delta_x;

            lbDisplay.LeftButton = 0;
            delta_y = mouse_move_y_coord_over_box(box) - 14;
            delta_x = mouse_move_x_coord_over_box(box) - 9;
            (*target_ptr) = delta_y + delta_x;
            if ((*target_ptr) < 0)
                *target_ptr = 0;
            else if ((*target_ptr) > 322)
                (*target_ptr) = 322;
            target_affected = 1;
        }
    }
    return target_affected;
}

void draw_vert_slider_left_arrow(struct ScreenBox *box)
{
    lbDisplay.DrawFlags |= 0x8000;
    lbDisplay.DrawFlags |= Lb_SPRITE_TRANSPAR4;
    if (mouse_move_over_box(box))
    {
        lbDisplay.DrawFlags &= ~Lb_SPRITE_TRANSPAR4;
    }
    draw_sprite_purple_list(box->X, box->Y, &sprites_Icons0_0[108]);
}

TbBool input_vert_slider_left_arrow(struct ScreenBox *box, short *target_ptr)
{
    TbBool target_affected;

    target_affected = false;
    if (mouse_move_over_box(box))
    {
        if (lbDisplay.MLeftButton || joy.Buttons[0])
        {
            lbDisplay.LeftButton = 0;
            box->Flags |= 0x0800;
            if ((lbShift & 0x01) != 0)
                (*target_ptr)--;
            else
                (*target_ptr) -= 10;
            if ((*target_ptr) < 0)
                (*target_ptr) = 0;
            target_affected = 1;
        }
    }
    return target_affected;
}

void draw_vert_slider_right_arrow(struct ScreenBox *box)
{
    lbDisplay.DrawFlags |= 0x8000;
    lbDisplay.DrawFlags |= Lb_SPRITE_TRANSPAR4;
    if (mouse_move_over_box(box))
    {
        lbDisplay.DrawFlags &= ~Lb_SPRITE_TRANSPAR4;
    }
    draw_sprite_purple_list(box->X - 7, box->Y, &sprites_Icons0_0[109]);
}

TbBool input_vert_slider_right_arrow(struct ScreenBox *box, short *target_ptr)
{
    TbBool target_affected;

    target_affected = false;
    if (mouse_move_over_box(box))
    {
        if (lbDisplay.MLeftButton || joy.Buttons[0])
        {
            lbDisplay.LeftButton = 0;
            box->Flags |= 0x0800;
            if ((lbShift & 0x01) != 0)
                (*target_ptr)++;
            else
                (*target_ptr) += 10;
            if ((*target_ptr) > 322)
                (*target_ptr) = 322;
            target_affected = 1;
        }
    }
    return target_affected;
}

ubyte show_audio_volume_box(struct ScreenBox *box)
{
#if 0
    ubyte ret;
    asm volatile ("call ASM_show_audio_volume_box\n"
        : "=r" (ret) : "a" (box));
    return ret;
#endif
    short *target_ptr;
    char *s;
    ushort w;
    ubyte target_var;
    TbBool change;

    change = false;
    if (box->Flags & 0x0080)
    {
        box->Flags &= ~0x0080;
        word_1C4866[0] = -5;
        word_1C4866[1] = -5;
        word_1C4866[2] = -5;
    }
    if (box->Timer == 255)
    {
        word_1C4866[0] = 99;
        word_1C4866[1] = 99;
        word_1C4866[2] = 99;
    }
    my_set_text_window(box->X + 4, box->Y + 4, box->Width - 6, 480);

    if (box == &audio_volume_boxes[0])
    {
        target_ptr = &startscr_samplevol;
        s = gui_strings[419];
        target_var = 0;
    }
    else if (box == &audio_volume_boxes[1])
    {
        target_ptr = &startscr_midivol;
        s = gui_strings[420];
        target_var = 1;
    }
    else
    {
        target_ptr = &startscr_cdvolume;
        s = gui_strings[516];
        target_var = 2;
    }

    lbFontPtr = med_font;
    w = (box->Width - my_string_width(s)) >> 1;
    if (flashy_draw_text(1 + w, 1, s, 1, 0, &word_1C4866[target_var], 0))
    {
        lbFontPtr = small_med_font;

        struct ScreenBox box1; // Left triangle
        box1.Width = 9;
        box1.Height = 14;

        struct ScreenBox box0; // Main bar
        box0.Width = 336;
        box0.Height = 28;

        struct ScreenBox box2; // Right triangle
        box2.Width = 9;
        box2.Height = 14;

        box0.Y = box->Y + 26;
        box1.Y = box->Y + 26 + 12;
        box2.Y = box->Y + 26 + 1;

        box1.X = box->X + 33;
        box0.X = box->X + 33 + 9 + 2;
        box2.X = box->X + 33 + 9 + 2 + 336 + 1;

        draw_vert_slider_main_body(&box0, target_ptr);
        change |= input_vert_slider_main_body(&box0, target_ptr);

        draw_vert_slider_left_arrow(&box1);
        change |= input_vert_slider_left_arrow(&box1, target_ptr);

        draw_vert_slider_right_arrow(&box2);
        change |= input_vert_slider_right_arrow(&box2, target_ptr);

        draw_vert_slider_main_body_text(&box0, box, target_ptr);

    }
    lbDisplay.DrawFlags = 0;
    if (!change)
        return 0;

    if (target_var == 0)
        SetSoundMasterVolume(127 * (*target_ptr) / 322);
    else if (target_var == 1)
        SetMusicMasterVolume(127 * (*target_ptr) / 322);
    else if (target_var == 2)
      SetCDVolume((70 * (127 * (*target_ptr) / 322) / 100));

    return 0;
}

ubyte show_audio_tracks_box(struct ScreenBox *box)
{
    ubyte ret;
    asm volatile ("call ASM_show_audio_tracks_box\n"
        : "=r" (ret) : "a" (box));
    return ret;
}

void init_options_screen_boxes(void)
{
    int i, h;
    int val;

    h = 72;
    for (i = 0; i < 3; i++)
    {
        init_screen_box(&audio_volume_boxes[i], 213, h, 420, 62, 6);
        h += 71;
    }
    init_screen_box(&audio_tracks_box, 213, h, 420, 62, 6);

    init_screen_button(&options_audio_buttons[0], 393u, 289u,
      gui_strings[531], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[1], 458u, 289u,
      gui_strings[532], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[2], 523u, 289u,
      gui_strings[533], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[3], 458u, 307u,
      gui_strings[531], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[4], 523u, 307u,
      gui_strings[532], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[5], 458u, 325u,
      gui_strings[478], 6, med2_font, 1, 0);
    init_screen_button(&options_audio_buttons[6], 523u, 325u,
      gui_strings[479], 6, med2_font, 1, 0);

    val = 2;
    for (i = 0; i < 3; i++)
    {
        options_audio_buttons[i].Radio = &ingame.CDTrack;
        options_audio_buttons[i].RadioValue = val++;
        options_audio_buttons[i].Flags |= 0x0100;
    }

    val = 1;
    for (i = 3; i < 5; i++)
    {
        options_audio_buttons[i].Radio = &ingame.DangerTrack;
        options_audio_buttons[i].RadioValue = val++;
        options_audio_buttons[i].Flags |= 0x0100;
    }

    val = 0;
    for (i = 5; i < 7; i++)
    {
        options_audio_buttons[i].Radio = &ingame.UseMultiMedia;
        options_audio_buttons[i].RadioValue = val++;
        options_audio_buttons[i].Flags |= 0x0100;
    }
    audio_volume_boxes[0].SpecialDrawFn = ac_show_audio_volume_box;
    audio_volume_boxes[1].SpecialDrawFn = ac_show_audio_volume_box;
    audio_volume_boxes[2].SpecialDrawFn = ac_show_audio_volume_box;
    audio_tracks_box.SpecialDrawFn = ac_show_audio_tracks_box;

    init_screen_button(&options_gfx_buttons[0], 456u, 94u,
      gui_strings[465], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[1], 544u, 94u,
      gui_strings[466], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[2], 456u, 112u,
      gui_strings[473], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[3], 544u, 112u,
      gui_strings[474], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[4], 456u, 130u,
      gui_strings[475], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[5], 544u, 130u,
      gui_strings[477], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[6], 456u, 148u,
      gui_strings[478], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[7], 544u, 148u,
      gui_strings[479], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[8], 456u, 166u,
      gui_strings[478], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[9], 544u, 166u,
      gui_strings[479], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[10], 456u, 184u,
      gui_strings[478], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[11], 544u, 184u,
      gui_strings[479], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[12], 456u, 202u,
      gui_strings[478], 6, med2_font, 1, 0);
    init_screen_button(&options_gfx_buttons[13], 544u, 202u,
      gui_strings[479], 6, med2_font, 1, 0);

    val = abs(ingame.PanelPermutation);
    init_screen_button(&options_gfx_buttons[14], 320u, 274u,
        gui_strings[579 + val], 6, med2_font, 1, 0);

    val = ingame.TrenchcoatPreference;
    init_screen_button(&options_gfx_buttons[15], 320u, 310u,
        gui_strings[583 + val], 6, med2_font, 1, 0);

    options_gfx_buttons[14].CallBackFn = ac_change_panel_permutation;
    options_gfx_buttons[15].CallBackFn = ac_change_trenchcoat_preference;
    options_gfx_buttons[14].Width += 60;
    options_gfx_buttons[15].Width = options_gfx_buttons[14].Width;

    val = 0;
    for (i = 0; i < 2; i++)
    {
        options_gfx_buttons[i].Radio = &game_projector_speed;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        val++;
    }

    val = 0;
    for (i = 2; i < 4; i++)
    {
        options_gfx_buttons[i].Radio = &game_high_resolution;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        val++;
    }

    val = 0;
    for (i = 4; i < 6; i++)
    {
        options_gfx_buttons[i].Radio = &ingame.DetailLevel;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        val++;
    }

    val = 0;
    for (i = 6; i < 8; i++)
    {
        options_gfx_buttons[i].Radio = &game_perspective;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        val += 5;
    }

    val = 0;
    for (i = 8; i < 10; i++)
    {
        options_gfx_buttons[i].Radio = &unkn_gfx_option_2;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        ingame.Flags |= GamF_Unkn0002;
        val++;
    }

    val = 0;
    for (i = 10; i < 12; i++)
    {
        options_gfx_buttons[i].Radio = &unkn_option_3;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        ingame.Flags |= GamF_Unkn0001;
        val++;
    }

    val = 0;
    for (i = 12; i < 14; i++)
    {
        options_gfx_buttons[i].Radio = &unkn_option_4;
        options_gfx_buttons[i].RadioValue = val;
        options_gfx_buttons[i].Flags |= 0x0100;
        ingame.Flags &= ~GamF_Unkn0400;
        val++;
    }
}

void reset_options_screen_boxes_flags(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        audio_volume_boxes[i].Flags = 0x0001;
    }
    for (i = 0; i < 7; i++) {
        options_audio_buttons[i].Flags = 0x0101;
    }
    for (i = 0; i < 14; i++) {
      options_gfx_buttons[i].Flags = 0x0101;
    }
    for (; i < 16; i++) {
      options_gfx_buttons[i].Flags = 0x0001;
    }
}

void update_options_screen_state(void)
{
    const char *text;
    int i;

    i = ingame.PanelPermutation;
    if (i < 0)
        text = gui_strings[579 + abs(i)];
    else
        text = gui_strings[580 + i];
    options_gfx_buttons[14].Text = text;

    i = ingame.TrenchcoatPreference;
    options_gfx_buttons[15].Text = gui_strings[583 + i];
}

/******************************************************************************/

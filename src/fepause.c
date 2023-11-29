/******************************************************************************/
// Syndicate Wars Port, source port of the classic strategy game from Bullfrog.
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
#include "fepause.h"

#include "bfbox.h"
#include "bfline.h"
#include "bfscreen.h"
#include "bfsprite.h"
#include "bftext.h"
#include "bfkeybd.h"
#include "bfaudio.h"
#include "bfmusic.h"
#include "bfscd.h"
#include "ssampply.h"
#include "bflib_vidraw.h"
#include "bflib_joyst.h"
#include "campaign.h"
#include "display.h"
#include "game_data.h"
#include "game.h"
#include "guiboxes.h"
#include "guitext.h"
#include "keyboard.h"
#include "mouse.h"
#include "sound.h"
#include "swlog.h"
/******************************************************************************/

ubyte sub_71694(int a1, int a2, char *text, int a4, ubyte a5, ubyte a6)
{
    ubyte ret;
    asm volatile (
      "push %6\n"
      "push %5\n"
      "call ASM_sub_71694\n"
        : "=r" (ret) : "a" (a1), "d" (a2), "b" (text), "c" (a4), "g" (a5), "g" (a6));
    return ret;
}

void draw_box_cutedge(struct ScreenBox *box, TbPixel colr1)
{
    short cut, stp;
    if (lbDisplay.ScreenMode == 1) {
        stp = 1;
        cut = 25;
    } else {
        stp = 2;
        cut = 50;
    }

    lbDisplay.DrawFlags = Lb_SPRITE_TRANSPAR4;
    LbDrawBox(box->X + 0, box->Y + 0, box->Width - cut, box->Height - cut, colr1);
    LbDrawBox(box->X + box->Width - cut, box->Y +  cut, cut, box->Height - 2*cut, colr1);
    LbDrawBox(box->X + cut, box->Y + box->Height - cut, box->Width - cut, cut, colr1);
    LbDrawTriangle(box->X + box->Width - cut, box->Y + 0,
      box->X + box->Width, box->Y + cut,
      box->X + box->Width - cut, box->Y + cut, colr1);
    LbDrawTriangle(box->X + stp, box->Y + box->Height - cut,
      box->X + cut, box->Y + box->Height - cut,
      box->X + cut, box->Y + box->Height - stp, colr1);

    lbDisplay.DrawFlags = 0;
    LbDrawLine(box->X + 0, box->Y + 0, box->X + box->Width - cut, box->Y + 0, colr1);
    LbDrawLine(box->X + 0, box->Y + 0, box->X + 0, box->Y + box->Height - cut - stp, colr1);
    LbDrawLine(box->X + box->Width, box->Y + cut, box->X + box->Width, box->Y + box->Height - stp, colr1);
    LbDrawLine(box->X + cut, box->Y + box->Height - stp, box->X + box->Width, box->Y + box->Height - stp, colr1);
    LbDrawLine(box->X + box->Width - cut, box->Y + 0, box->X + box->Width, box->Y + cut, colr1);
    LbDrawLine(box->X + stp, box->Y + box->Height - cut, box->X + cut, box->Height + cut + stp, colr1);
}

void draw_parallelogram_45degi(short x, short y, short w, short h, TbPixel colr2)
{
    short cx, i;

    cx = x;
    for (i = 0; i < h; i += 2)
    {
        if (lbDisplay.ScreenMode == 1)
            LbDrawLine((cx) >> 1, (y+i) >> 1, (cx+w) >> 1, (y+i) >> 1, colr2);
        else
            LbDrawLine((cx), (y+i), (cx+w), (y+i), colr2);
        cx -= 2;
    }
}

void draw_kicked_box(struct ScreenBox *box, TbPixel colr2)
{
    draw_parallelogram_45degi(box->X, box->Y, box->Width, box->Height, colr2);
    if (lbDisplay.ScreenMode != 1)
        draw_parallelogram_45degi(box->X - 1, box->Y + 1, box->Width, box->Height, colr2);
}

void draw_kicked_left_arrow(struct ScreenBox *box, TbPixel colr2)
{
    short stp;

    if (lbDisplay.ScreenMode == 1)
        stp = 1;
    else
        stp = 2;

    if (mouse_move_over_kicked_box(box))
    {
        if (lbDisplay.ScreenMode == 1)
            LbSpriteDrawOneColour(box->X >> 1, (box->Y >> 1) - stp, &pop1_sprites[100],
              colour_lookup[1]);
        else
            LbSpriteDrawOneColour(box->X, box->Y - stp, &pop1_sprites[100],
              colour_lookup[1]);
    }
    else
    {
        if (lbDisplay.ScreenMode == 1)
            LbSpriteDraw(box->X >> 1, (box->Y >> 1) - stp, &pop1_sprites[100]);
        else
            LbSpriteDraw(box->X, box->Y - stp, &pop1_sprites[100]);
    }
}

void draw_kicked_right_arrow(struct ScreenBox *box, TbPixel colr2)
{
    short stp;

    if (lbDisplay.ScreenMode == 1)
        stp = 1;
    else
        stp = 2;

    if (mouse_move_over_kicked_box(box))
    {
        if (lbDisplay.ScreenMode == 1)
            LbSpriteDrawOneColour((box->X >> 1) - stp, (box->Y >> 1) - stp,
              &pop1_sprites[101], colour_lookup[1]);
        else
            LbSpriteDrawOneColour(box->X - stp, box->Y - stp,
              &pop1_sprites[101], colour_lookup[1]);
    }
    else
    {
        if (lbDisplay.ScreenMode == 1)
            LbSpriteDraw((box->X >> 1) - stp, (box->Y >> 1) - stp, &pop1_sprites[101]);
        else
            LbSpriteDraw(box->X - stp, box->Y - stp, &pop1_sprites[101]);
    }
}

TbBool pause_screen_handle(void)
{
#if 0
    TbBool ret;
    asm volatile ("call ASM_pause_screen_handle\n"
        : "=r" (ret) : );
    return ret;
#endif
    int w;
    const char *s;
    int x1, y1;
    int ms_x, ms_y, i;
    TbBool is_unkn1;
    TbPixel colr1, colr2;
    struct ScreenBox main_box;

    if (lbDisplay.ScreenMode == 1) {
        main_box.X = 43;
        main_box.Width = 233;
        main_box.Y = 27;
        main_box.Height = 122;
    } else {
        main_box.X = 86;
        main_box.Width = 466;
        main_box.Y = 54;
        main_box.Height = 244;
    }

    if ((ingame.PanelPermutation != 2) && (ingame.PanelPermutation != -3))
        colr1 = 20;
    else
        colr1 = 40;
    snd_unkn1_volume_all_samples();
    person_func_unknown_310(2u);
    lbKeyOn[kbkeys[GKey_PAUSE]] = 0;
    while ((jskeys[GKey_PAUSE] != 0) &&
      (jskeys[GKey_PAUSE] != joy.Buttons[0]))
    {
        joy_func_065(&joy);
    }
    do_change_mouse(8);

    draw_box_cutedge(&main_box, colr1);

    if ((ingame.PanelPermutation != 2) && (ingame.PanelPermutation != -3))
        colr2 = 15;
    else
        colr2 = 35;
    lbFontPtr = small_font;
    my_set_text_window(0, 0, lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);
    if ((ingame.PanelPermutation == 2) || (ingame.PanelPermutation == -3))
    {
        lbDisplay.DrawFlags |= Lb_TEXT_ONE_COLOR;
        lbDisplay.DrawColour = colr2;
    }
    if (lbDisplay.ScreenMode == 1)
    {
        s = mission_name;
        w = my_string_width(s);
        my_draw_text(161 - (w >> 1), 44, s, 0);
        s = gui_strings[419];
        w = my_string_width(s);
        my_draw_text(161 - (w >> 1), 54, s, 0);
        s = gui_strings[420];
        w = my_string_width(s);
        my_draw_text(161 - (w >> 1), 76, s, 0);
        s = gui_strings[516];
        w = my_string_width(s);
        my_draw_text(161 - (w >> 1), 98, s, 0);
    }
    else
    {
        s = mission_name;
        w = my_string_width(s);
        my_draw_text(322 - (w >> 1), 88, s, 0);
        s = gui_strings[419];
        w = my_string_width(s);
        my_draw_text(322 - (w >> 1), 108, s, 0);
        s = gui_strings[420];
        w = my_string_width(s);
        my_draw_text(322 - (w >> 1), 152, s, 0);
        s = gui_strings[516];
        w = my_string_width(s);
        my_draw_text(322 - (w >> 1), 196, s, 0);
    }
    if (ingame.PanelPermutation == 2 || ingame.PanelPermutation == -3)
        lbDisplay.DrawFlags &= ~Lb_TEXT_ONE_COLOR;

    if (language_3str[0] == 'e')
    {
        if (lbDisplay.ScreenMode == 1)
        {
            LbSpriteDraw(89, 31, &pop1_sprites[102]);
            LbSpriteDraw(83, 120, &pop1_sprites[103]);
        }
        else
        {
            LbSpriteDraw(178, 62, &pop1_sprites[102]);
            w = pop1_sprites[102].SWidth;
            LbSpriteDraw(178 + w, 62, &pop1_sprites[104]);
            LbSpriteDraw(166, 240, &pop1_sprites[103]);
        }
    }
    else
    {
        if (lbDisplay.ScreenMode == 1)
        {
            s = gui_strings[606];
            w = my_string_width(s);
            my_draw_text(161 - (w >> 1), 31, s, 0);
            s = gui_strings[470];
            w = my_string_width(s);
            my_draw_text(103 - (w >> 1), 123, s, 0);
        }
        else
        {
            s = gui_strings[606];
            w = my_string_width(s);
            my_draw_text(322 - (w >> 1), 62, s, 0);
            s = gui_strings[470];
            w = my_string_width(s);
            my_draw_text(206 - (w >> 1), 246, s, 0);
        }
    }
    swap_wscreen();
    if (!ingame.fld_unk7DA)
        SetMusicVolume(100, 0x7F);

    is_unkn1 = 0;
    while (!lbKeyOn[kbkeys[GKey_PAUSE]]
        && (!jskeys[GKey_PAUSE] || jskeys[GKey_PAUSE] != joy.Buttons[0])
        && !is_unkn1)
    {
        game_update();
        joy_func_065(&joy);
        PlayCDTrack(ingame.CDTrack);
        {
        struct ScreenBox box1;
        box1.X = 204;
        box1.Y = 122;
        box1.Width = 238;
        box1.Height = 18;

        struct ScreenBox box2;
        box2.X = 176;
        box2.Y = 122;
        box2.Width = 8;
        box2.Height = 18;

        struct ScreenBox box3;
        box3.X = 432;
        box3.Y = 122;
        box3.Width = 8;
        box3.Height = 18;

        draw_kicked_box(&box1, colr2);
        draw_kicked_left_arrow(&box2, colr2);
        draw_kicked_right_arrow(&box3, colr2);

        if (mouse_move_over_kicked_box(&box2))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                --startscr_samplevol;
                if (startscr_samplevol < 0)
                    startscr_samplevol = 0;
                SetSoundMasterVolume(127 * startscr_samplevol / 322);
                if (!IsSamplePlaying(0, 80, 0))
                    play_sample_using_heap(0, 80, 127, 64, 100, 0, 1u);
            }
        }
        if (mouse_move_over_kicked_box(&box3))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                ++startscr_samplevol;
                if (startscr_samplevol > 322)
                    startscr_samplevol = 322;
                SetSoundMasterVolume(127 * startscr_samplevol / 322);
                if (!IsSamplePlaying(0, 80, 0))
                    play_sample_using_heap(0, 80, 127, 64, 100, 0, 1u);
            }
        }

        }
        {
        struct ScreenBox box1;
        box1.X = 204;
        box1.Y = 166;
        box1.Width = 238;
        box1.Height = 18;

        struct ScreenBox box2;
        box2.X = 176;
        box2.Y = 166;
        box2.Width = 8;
        box2.Height = 18;

        struct ScreenBox box3;
        box3.X = 432;
        box3.Y = 166;
        box3.Width = 8;
        box3.Height = 18;

        draw_kicked_box(&box1, colr2);
        draw_kicked_left_arrow(&box2, colr2);
        draw_kicked_right_arrow(&box3, colr2);

        if (mouse_move_over_kicked_box(&box2))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                --startscr_midivol;
                if (startscr_midivol < 0)
                    startscr_midivol = 0;
                SetMusicMasterVolume(127 * startscr_midivol / 322);
            }
        }
        if (mouse_move_over_kicked_box(&box3))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                ++startscr_midivol;
                if (startscr_midivol > 322)
                    startscr_midivol = 322;
                SetMusicMasterVolume(127 * startscr_midivol / 322);
            }
        }

        }
        {
        struct ScreenBox box1;
        box1.X = 204;
        box1.Y = 210;
        box1.Width = 238;
        box1.Height = 18;

        struct ScreenBox box2;
        box2.X = 176;
        box2.Y = 210;
        box2.Width = 8;
        box2.Height = 18;

        struct ScreenBox box3;
        box3.X = 432;
        box3.Y = 210;
        box3.Width = 8;
        box3.Height = 18;

        draw_kicked_box(&box1, colr2);
        draw_kicked_left_arrow(&box2, colr2);
        draw_kicked_right_arrow(&box3, colr2);

        if (mouse_move_over_box(&box2))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                --startscr_cdvolume;
                if (startscr_cdvolume < 0)
                    startscr_cdvolume = 0;
                SetCDVolume(70 * (127 * startscr_cdvolume / 322) / 100);
            }
        }
        if (mouse_move_over_box(&box3))
        {
            if (lbDisplay.MLeftButton)
            {
                lbDisplay.LeftButton = 0;
                ++startscr_cdvolume;
                if (startscr_cdvolume > 322)
                    startscr_cdvolume = 322;
                SetCDVolume(70 * (127 * startscr_cdvolume / 322) / 100);
            }
        }
        }

        if (lbDisplay.MLeftButton)
        {
            ms_x = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseX : lbDisplay.MMouseX;
            ms_y = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseY : lbDisplay.MMouseY;
            if ((ms_y >> 1 >= 62) && (ms_y >> 1 <= 68))
            {
                int ms_delta;
                ms_delta = (ms_y >> 1) + (ms_x >> 1) - 62;
                if (ms_delta >= 103 && ms_delta <= 219)
                {
                    startscr_samplevol = 322 * (ms_delta - 103) / 116;
                    SetSoundMasterVolume(127 * startscr_samplevol / 322);
                    if (!IsSamplePlaying(0, 80, 0))
                        play_sample_using_heap(0, 80, 127, 64, 100, 0, 1u);
                }
            }
        }

        if (lbDisplay.MLeftButton)
        {
            ms_x = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseX : lbDisplay.MMouseX;
            ms_y = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseY : lbDisplay.MMouseY;
            if ((ms_y >> 1 >= 84) && (ms_y >> 1 <= 90))
            {
                int ms_delta;
                ms_delta = (ms_y >> 1) + (ms_x >> 1) - 84;
                if (ms_delta >= 103 && ms_delta <= 219)
                {
                    startscr_midivol = 322 * (ms_delta - 103) / 116;
                    SetMusicMasterVolume(127 * startscr_midivol / 322);
                }
            }
        }

        if (lbDisplay.MLeftButton)
        {
            ms_x = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseX : lbDisplay.MMouseX;
            ms_y = lbDisplay.ScreenMode == 1 ? 2 * lbDisplay.MMouseY : lbDisplay.MMouseY;
            if ((ms_y >> 1 >= 106) && (ms_y >> 1 <= 112))
            {
                int ms_delta;
                ms_delta = (ms_y >> 1) + (ms_x >> 1) - 106;
                if (ms_delta >= 103 && ms_delta <= 219)
                {
                    startscr_cdvolume = 322 * (ms_delta - 103) / 116;
                    SetCDVolume(70 * (127 * startscr_cdvolume / 322) / 100);
                }
            }
        }

        if (startscr_samplevol)
        {
            int val7;
            x1 = 206;
            y1 = 124;
            for (i = 0; i < 7; i++)
            {
                val7 = 103 - i;
                if (lbDisplay.ScreenMode == 1)
                    LbDrawLine(x1 >> 1, y1 >> 1,
                        (2 * (116 * startscr_samplevol / 322 + val7))
                            >> 1, y1 >> 1, colour_lookup[1]);
                else
                    LbDrawLine(x1, y1,
                        2 * (116 * startscr_samplevol / 322 + val7),
                        y1, colour_lookup[1]);
                x1 -= 2;
                y1 += 2;
            }
            if (lbDisplay.ScreenMode != 1)
            {
                x1 = 205;
                y1 = 125;
                for (i = 0; i < 7; i++)
                {
                    val7 = 103 - i;
                    if (lbDisplay.ScreenMode == 1)
                        LbDrawLine(x1 >> 1, y1 >> 1,
                            (2 * (116 * startscr_samplevol / 322 + val7)
                                - 1) >> 1, y1 >> 1, colour_lookup[1]);
                    else
                        LbDrawLine(x1, y1,
                            2 * (val7 + 116 * startscr_samplevol / 322)
                                - 1, y1, colour_lookup[1]);
                    x1 -= 2;
                    y1 += 2;
                }
            }
        }

        if (startscr_midivol)
        {
            int val7;
            x1 = 206;
            y1 = 168;
            for (i = 0; i < 7; i++)
            {
                val7 = 103 - i;
                if (lbDisplay.ScreenMode == 1)
                    LbDrawLine(x1 >> 1, y1 >> 1,
                        (2 * (116 * startscr_midivol / 322 + val7))
                            >> 1, y1 >> 1, colour_lookup[1]);
                else
                    LbDrawLine(x1, y1,
                        2 * (116 * startscr_midivol / 322 + val7),
                        y1, colour_lookup[1]);
                x1 -= 2;
                y1 += 2;
            }
            if (lbDisplay.ScreenMode != 1)
            {
                x1 = 205;
                y1 = 169;
                for (i = 0; i < 7; i++)
                {
                    val7 = 103 - i;
                    if (lbDisplay.ScreenMode == 1)
                        LbDrawLine(x1 >> 1, y1 >> 1,
                            (2
                                * (116 * startscr_midivol / 322
                                    + val7) - 1) >> 1, y1 >> 1,
                            colour_lookup[1]);
                    else
                        LbDrawLine(x1, y1,
                            2
                                * (116 * startscr_midivol / 322
                                    + val7) - 1, y1, colour_lookup[1]);
                    x1 -= 2;
                    y1 += 2;
                }
            }
        }

        if (startscr_cdvolume)
        {
            int val7;
            x1 = 206;
            y1 = 212;
            for (i = 0; i < 7; i++)
            {
                val7 = 103 - i;
                if (lbDisplay.ScreenMode == 1)
                    LbDrawLine(x1 >> 1, y1 >> 1,
                        (2 * (116 * startscr_cdvolume / 322 + val7))
                            >> 1, y1 >> 1, colour_lookup[1]);
                else
                    LbDrawLine(x1, y1,
                        2 * (116 * startscr_cdvolume / 322 + val7),
                        y1, colour_lookup[1]);
                x1 -= 2;
                y1 += 2;
            }
            if (lbDisplay.ScreenMode != 1)
            {
                x1 = 205;
                y1 = 213;
                for (i = 0; i < 7; i++)
                {
                    val7 = 103 - i;
                    if (lbDisplay.ScreenMode == 1)
                        LbDrawLine(x1 >> 1, y1 >> 1,
                            (2 * (val7 + 116 * startscr_cdvolume / 322)
                                - 1) >> 1, y1 >> 1, colour_lookup[1]);
                    else
                        LbDrawLine(x1, y1,
                            2 * (val7 + 116 * startscr_cdvolume / 322)
                                - 1, y1, colour_lookup[1]);
                    x1 -= 2;
                    y1 += 2;
                }
            }
        }

        if (sub_71694(180, 120, gui_strings[477], colr1, colr2, ingame.DetailLevel == 1))
        {
            ingame.DetailLevel = 1;
            bang_set_detail(0);
        }
        if (sub_71694(220, 120, gui_strings[475], colr1, colr2, ingame.DetailLevel == 0))
        {
            ingame.DetailLevel = 0;
            bang_set_detail(1);
        }
        if (sub_71694(140, 134, gui_strings[455], colr1, colr2, 0))
            is_unkn1 = 1;
        if (sub_71694(197, 134, gui_strings[445], colr1, colr2, 0))
        {
            swap_wscreen();
            SetMusicVolume(100, 0);
            StopAllSamples();
            StopCD();
            return 1;
        }
        lbDisplay.DrawFlags = 0;
        process_sound_heap();
        swap_wscreen();
    }

    while ((lbKeyOn[kbkeys[GKey_PAUSE]])
        || (jskeys[GKey_PAUSE] && jskeys[GKey_PAUSE] == joy.Buttons[0]))
    {
        game_update();
        joy_func_065(&joy);
        PlayCDTrack(ingame.CDTrack);
        swap_wscreen();
    }
    lbDisplay.RightButton = 0;
    lbDisplay.LeftButton = 0;
    if (!ingame.fld_unk7DA)
        SetMusicVolume(100, 0);
    return 0;
}

/******************************************************************************/

/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_speed.c
 *     Control of the game speed.
 * @par Purpose:
 *     Variables and functions keeping game speed and frame rate at required pace.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     10 Feb 2024 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_speed.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "bfkeybd.h"
#include "bftime.h"
#include "bfscreen.h"
#include "rom.h"
#include "drawtext.h"
#include "engincolour.h"
#include "enginprops.h"
#include "game.h"
#include "keyboard.h"
#include "swlog.h"

/******************************************************************************/

GameTurn gameturn = 0;
GameTurn prev_gameturn = 0;

ulong turns_delta = 0;

short frameskip = 0;

GameTurn drawturn = 1;

ulong curr_tick_time = 0;
ulong prev_tick_time = 0;

// TODO implement separate turns per second, when drawing frames will get separated from game loop
ushort game_num_fps = 16;

ushort fifties_per_gameturn = 3;

#define TURN_DELAY_STEP 4
#define TURN_DELAY_MAX 400

/** Time to wait between game turns, in milliseconds. */
short turn_delay_ms = 1000/16;

static void turn_delay_clip(void)
{
    if (turn_delay_ms > TURN_DELAY_MAX)
        turn_delay_ms = TURN_DELAY_MAX;
    else if (turn_delay_ms < 0)
        turn_delay_ms = 0;
}

/******************************************************************************/

void render_clock_set_turn(ulong turn)
{
    render_anim_turn = (u32)turn << RENDER_ANIM_TURN_SHIFT;
}

void render_clock_next_frame(u32 anim_turn_incr)
{
    drawturn++;
    render_anim_turn += anim_turn_incr;
}

void frameskip_clip(void)
{
    if (frameskip > 512)
        frameskip = 512;
    else if (frameskip < 0)
        frameskip = 0;
}

void frameskip_increase(void)
{
    if (frameskip < 2)
        frameskip++;
    else if (frameskip < 16)
        frameskip += 2;
    else
        frameskip += (frameskip/3);
    frameskip_clip();
    //show_onscreen_msg(game_num_fps+frameskip, "Frame skip %d",frameskip);
}

void frameskip_decrease(void)
{
    if (frameskip <= 2)
        frameskip--;
    else if (frameskip <= 16)
        frameskip -= 2;
    else
        frameskip -= (frameskip/4);
    frameskip_clip();
    //show_onscreen_msg(game_num_fps+frameskip, "Frame skip %d",frameskip);
}

ubyte get_speed_control_inputs(void)
{
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    if (is_gamekey_pressed(GKey_GAMESPEED_INC))
    {
        clear_gamekey_pressed(GKey_GAMESPEED_INC);
        frameskip_increase();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_GAMESPEED_DEC))
    {
        clear_gamekey_pressed(GKey_GAMESPEED_DEC);
        frameskip_decrease();
        did_inp |= GINPUT_DIRECT;
    }
    // Delay between game turns; [ makes it longer (slower game), ] shorter
    if (is_key_pressed(KC_LBRACKET, KMod_DONTCARE))
    {
        clear_key_pressed(KC_LBRACKET);
        turn_delay_ms += TURN_DELAY_STEP;
        turn_delay_clip();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_RBRACKET, KMod_DONTCARE))
    {
        clear_key_pressed(KC_RBRACKET);
        turn_delay_ms -= TURN_DELAY_STEP;
        turn_delay_clip();
        did_inp |= GINPUT_DIRECT;
    }
    return did_inp;
}

/** Microsecond clock for measuring short phases of the frame. */
static ulong perf_now_us(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER cnt;

    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (ulong)((cnt.QuadPart * 1000000ULL) / freq.QuadPart);
#else
    return LbTimerClock() * 1000;
#endif
}

static ulong perf_phase_start[PerfPhase_COUNT];
static ulong perf_phase_acc[PerfPhase_COUNT];

void perf_phase_begin(enum PerfPhase phase)
{
    perf_phase_start[phase] = perf_now_us();
}

void perf_phase_end(enum PerfPhase phase)
{
    perf_phase_acc[phase] += perf_now_us() - perf_phase_start[phase];
}

#define FPS_TEXT_BUF_W 400
#define FPS_TEXT_BUF_H 10
#define FPS_TEXT_SCALE 2

/** Draws text magnified, on a black plate, with right edge at given position.
 */
static void draw_scaled_text_right(short right_x, short y, const char *text, ubyte colour)
{
    static ubyte tbuf[FPS_TEXT_BUF_W * FPS_TEXT_BUF_H];
    long scrn_w, scrn_h;
    int tw, x0, sx, sy, dx, dy;

    scrn_w = lbDisplay.GraphicsScreenWidth;
    scrn_h = lbDisplay.GraphicsScreenHeight;
    tw = simple_text_width(text);
    if (tw > FPS_TEXT_BUF_W - 2)
        tw = FPS_TEXT_BUF_W - 2;
    memset(tbuf, 0, sizeof(tbuf));
    prop_text(text, tbuf, FPS_TEXT_BUF_W, colour);

    x0 = right_x - (tw + 2) * FPS_TEXT_SCALE;
    if (x0 < 0)
        x0 = 0;
    for (sy = 0; sy < FPS_TEXT_BUF_H; sy++)
    {
        for (dy = 0; dy < FPS_TEXT_SCALE; dy++)
        {
            int py = y + sy * FPS_TEXT_SCALE + dy;
            if (py >= scrn_h)
                break;
            for (sx = 0; sx < tw + 2; sx++)
            {
                ubyte pix = tbuf[sy * FPS_TEXT_BUF_W + sx];
                for (dx = 0; dx < FPS_TEXT_SCALE; dx++)
                {
                    int px = x0 + sx * FPS_TEXT_SCALE + dx;
                    if (px >= scrn_w)
                        break;
                    lbDisplay.WScreen[px + scrn_w * py] = pix;
                }
            }
        }
    }
}

void fps_counter_show(void)
{
    static TbClockMSec window_start = 0;
    static GameTurn window_start_turn = 0;
    static ulong window_frames = 0;
    static ulong shown_fps = 0, shown_tps = 0;
    static ulong shown_draw_us = 0, shown_swap_us = 0;
    TbClockMSec curr_time;
    char locstr[96];
    ulong dt;

    curr_time = LbTimerClock();
    window_frames++;
    dt = curr_time - window_start;
    if (dt >= 500) {
        shown_fps = (window_frames * 1000 + dt / 2) / dt;
        shown_tps = ((gameturn - window_start_turn) * 1000 + dt / 2) / dt;
        shown_draw_us = perf_phase_acc[PerfPhase_Draw] / window_frames;
        shown_swap_us = perf_phase_acc[PerfPhase_Swap] / window_frames;
        perf_phase_acc[PerfPhase_Draw] = 0;
        perf_phase_acc[PerfPhase_Swap] = 0;
        window_start = curr_time;
        window_start_turn = gameturn;
        window_frames = 0;
    }

    snprintf(locstr, sizeof(locstr), "FPS %lu TPS %lu SKIP %d DLY %d  DRAW %lu.%lu SWAP %lu.%lu",
      shown_fps, shown_tps, (int)frameskip, (int)turn_delay_ms,
      shown_draw_us / 1000, (shown_draw_us / 100) % 10,
      shown_swap_us / 1000, (shown_swap_us / 100) % 10);
    lbDisplay.DrawFlags = 0;
    draw_scaled_text_right(lbDisplay.GraphicsScreenWidth - 6, 4, locstr, colour_lookup[ColLU_WHITE]);
}
void wait_next_gameturn(void)
{
    static TbClockMSec last_loop_time = 0;
    TbClockMSec curr_time = LbTimerClock();
    TbClockMSec sleep_end;

    if (frameskip == 0)
        sleep_end = last_loop_time + turn_delay_ms;
    else
        sleep_end = curr_time;
    // If we missed the normal sleep target (ie. there was a slowdown), reset the value and do not sleep
    if ((sleep_end < curr_time) || (sleep_end > curr_time + turn_delay_ms)) {
        LOGNO("missed FPS target, last frame time %ld too far from current %ld",
          (ulong)sleep_end, (ulong)curr_time);
        sleep_end = curr_time;
    }
    LbSleepUntil(sleep_end);
    last_loop_time = sleep_end;
}

/**
 * Checks if the game screen needs redrawing.
 */
TbBool display_needs_redraw_this_turn(void)
{
    if ( (frameskip == 0) || ((gameturn % frameskip) == 0))
        return true;
    return false;
}

void update_tick_time(void)
{
    ulong tick_time = clock();
    tick_time = tick_time / 100;
    curr_tick_time = tick_time;
    if (tick_time != prev_tick_time)
    {
        ulong tmp;
        tmp = gameturn - prev_gameturn;
        prev_gameturn = gameturn;
        turns_delta = tmp;
    }
    if ( turns_delta != 0 ) {
        fifties_per_gameturn = 800 / turns_delta;
    } else {
        fifties_per_gameturn = 50;
    }
    if ( in_network_game )
        fifties_per_gameturn = 80;
    if ( fifties_per_gameturn > 400 )
        fifties_per_gameturn = 400;
    prev_tick_time = curr_tick_time;
}

/******************************************************************************/

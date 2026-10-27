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
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include "bfkeybd.h"
#include "bftime.h"
#include "bfscreen.h"
#include "bfgl.h"
#include "bfhwrend.h"
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

static void profiler_poll_key(void);

#define FPS_CAP_MAX 240

/** Limit of frames per second; 0 means no limit. */
short game_fps_cap = 20;

static void fps_cap_change(int dir)
{
    int cap = game_fps_cap;

    // Finer steps at the low end, where the game pace is changing the most
    if (dir > 0) {
        if (cap == 0)
            return;
        cap += (cap < 30) ? 1 : (cap < 120) ? 5 : 10;
        if (cap > FPS_CAP_MAX)
            cap = 0;
    } else {
        if (cap == 0)
            cap = FPS_CAP_MAX;
        else
            cap -= (cap <= 30) ? 1 : (cap <= 120) ? 5 : 10;
        if (cap < 1)
            cap = 1;
    }
    game_fps_cap = cap;
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
    profiler_poll_key();
    // Frame rate cap; [ lowers it, ] raises it, and going past the top removes the cap
    if (is_key_pressed(KC_LBRACKET, KMod_DONTCARE))
    {
        clear_key_pressed(KC_LBRACKET);
        fps_cap_change(-1);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_RBRACKET, KMod_DONTCARE))
    {
        clear_key_pressed(KC_RBRACKET);
        fps_cap_change(1);
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

#if defined(_WIN32)
/* Sampling profiler: Scroll Lock starts it, the next Scroll Lock stops it
 * and writes "profile_samples.txt" (address and hit count of the instruction
 * the main thread was at), to be resolved against the exe symbols with addr2line.
 */
#define PROF_MAX_SAMPLES 400000

static ulong prof_samples[PROF_MAX_SAMPLES];
static volatile ulong prof_count = 0;
static volatile int prof_running = 0;
static HANDLE prof_main_thread = NULL;
static HANDLE prof_thread = NULL;

static DWORD WINAPI profiler_sampler(LPVOID param)
{
    CONTEXT ctx;

    while (prof_running && prof_count < PROF_MAX_SAMPLES)
    {
        SuspendThread(prof_main_thread);
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(prof_main_thread, &ctx))
            prof_samples[prof_count++] = (ulong)ctx.Eip;
        ResumeThread(prof_main_thread);
        Sleep(1);
    }
    return 0;
}

static int prof_cmp_ulong(const void *a, const void *b)
{
    ulong va = *(const ulong *)a, vb = *(const ulong *)b;
    return (va > vb) - (va < vb);
}

static void profiler_start(void)
{
    HMODULE winmm;
    DWORD (WINAPI *p_timeBeginPeriod)(UINT);

    if (prof_main_thread == NULL)
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
          &prof_main_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    winmm = LoadLibraryA("winmm.dll");
    if (winmm != NULL) {
        p_timeBeginPeriod = (DWORD (WINAPI *)(UINT))GetProcAddress(winmm, "timeBeginPeriod");
        if (p_timeBeginPeriod != NULL)
            p_timeBeginPeriod(1);
    }
    LOGSYNC("profiler started");
    prof_count = 0;
    prof_running = 1;
    prof_thread = CreateThread(NULL, 0, profiler_sampler, NULL, 0, NULL);
}

static void profiler_stop(void)
{
    FILE *fh;
    ulong i, n, run;

    prof_running = 0;
    if (prof_thread != NULL) {
        WaitForSingleObject(prof_thread, 2000);
        CloseHandle(prof_thread);
        prof_thread = NULL;
    }
    n = prof_count;
    qsort(prof_samples, n, sizeof(ulong), prof_cmp_ulong);
    fh = fopen("profile_samples.txt", "w");
    if (fh == NULL) {
        LOGERR("profiler could not open profile_samples.txt for writing");
        return;
    }
    LOGSYNC("profiler stopped, %lu samples written", n);
    fprintf(fh, "total %lu base %08lx\n", n, (ulong)GetModuleHandleA(NULL));
    for (i = 0; i < n; i += run)
    {
        for (run = 1; (i + run < n) && (prof_samples[i + run] == prof_samples[i]); run++)
            ;
        fprintf(fh, "%08lx %lu\n", prof_samples[i], run);
    }
    fclose(fh);
}

static void profiler_poll_key(void)
{
    if (is_key_pressed(KC_SCROLL, KMod_DONTCARE) || is_key_pressed(KC_PAUSE, KMod_DONTCARE)
      || is_key_pressed(KC_F7, KMod_CONTROL))
    {
        clear_key_pressed(KC_SCROLL);
        clear_key_pressed(KC_PAUSE);
        clear_key_pressed(KC_F7);
        if (prof_running)
            profiler_stop();
        else
            profiler_start();
    }
}
#else
static void profiler_poll_key(void) { }
#endif

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

/** Tells which renderer draws the game, for the FPS counter. */
static const char *fps_renderer_name(void)
{
    if (LbGlIsActive() && LbHwRenderEnabled())
        return (LbHwLightingMode() == HwLight_Enhanced) ? "HW-ENH" : "HW-CLS";
    if (LbGlIsActive())
        return "GL-SW";
    return "SW";
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

    {
        char capstr[16];
        if (game_fps_cap > 0)
            snprintf(capstr, sizeof(capstr), "%d", (int)game_fps_cap);
        else
            snprintf(capstr, sizeof(capstr), "OFF");
        snprintf(locstr, sizeof(locstr), "%s FPS %lu TPS %lu SKIP %d CAP %s  DRAW %lu.%lu SWAP %lu.%lu",
          fps_renderer_name(), shown_fps, shown_tps, (int)frameskip, capstr,
          shown_draw_us / 1000, (shown_draw_us / 100) % 10,
          shown_swap_us / 1000, (shown_swap_us / 100) % 10);
    }
    lbDisplay.DrawFlags = 0;
    draw_scaled_text_right(lbDisplay.GraphicsScreenWidth - 6, 4, locstr, colour_lookup[ColLU_WHITE]);
}
void wait_next_gameturn(void)
{
    static ulong last_frame_us = 0;
    ulong now_us, period_us, target_us;

    // With a frame skip, or without a cap, run as fast as possible
    if ((frameskip != 0) || (game_fps_cap <= 0)) {
        last_frame_us = perf_now_us();
        return;
    }
    period_us = 1000000 / game_fps_cap;
    now_us = perf_now_us();
    target_us = last_frame_us + period_us;
    // If we missed the target (there was a slowdown), do not try to catch up
    if (((long)(target_us - now_us) < 0) || ((long)(target_us - now_us) > (long)period_us)) {
        last_frame_us = now_us;
        return;
    }
#if defined(_WIN32)
    // Sleep for most of the wait, then spin for the rest; Sleep() alone is too coarse
    while ((long)(target_us - perf_now_us()) > 2500)
        Sleep(1);
    while ((long)(target_us - perf_now_us()) > 0)
        ;
#else
    LbSleepUntil(LbTimerClock() + (target_us - now_us) / 1000);
#endif
    last_frame_us = target_us;
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

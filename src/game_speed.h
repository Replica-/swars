/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_speed.h
 *     Header file for game_speed.c.
 * @par Purpose:
 *     Control of the game speed.
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
#ifndef GAME_SPEED_H
#define GAME_SPEED_H

#include "bftypes.h"
#include "game_bstype.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/******************************************************************************/
extern ulong curr_tick_time;
extern ulong prev_tick_time;
extern GameTurn gameturn;
extern GameTurn prev_gameturn;

/** Counter of frames drawn to the screen.
 *
 * Advances once per drawn frame. It is the counter for things which only
 * exist while drawing - marking which elements were already drawn within the
 * current frame, for instance. Nothing in the simulation reads it, so drawing
 * code should use it rather than `gameturn` whenever it only needs to tell
 * one frame from the next.
 */
extern GameTurn drawturn;
extern ulong turns_delta;
extern ushort fifties_per_gameturn;

/** Amount of frames per second drawn by the game, and also aount of game
 * turns per second. */
extern ushort game_num_fps;

/** Opens a drawn frame: advances drawturn, and moves the animation clock on
 * by the given amount.
 *
 * The amount is in the units of render_anim_turn, so RENDER_ANIM_TURN_UNIT is
 * one animation turn - the pace the game has always drawn at, one animation
 * frame per game turn. Smaller amounts spread an animation frame over several
 * drawn frames, larger ones run animations faster, and zero opens a frame
 * which shows the animations exactly where the previous one left them.
 */
void render_clock_next_frame(u32 anim_turn_incr);

/** Places the animation clock on the given animation turn, on a mission or
 * the outro starting.
 */
void render_clock_set_turn(ulong turn);

/**
 * Handles game speed control inputs.
 * @return Returns true if packet was created, false otherwise.
 */
ubyte get_speed_control_inputs(void);

void wait_next_gameturn(void);
/** Microsecond clock used for the frame pacing. */
ulong game_now_us(void);
/** Start of the current frame period, and its length; 0 length if the frames are not paced. */
ulong game_period_start_us(void);
ulong game_period_length_us(void);
/** Waits (sleeping, then spinning) until the given time of game_now_us(). */
void game_wait_until_us(ulong target_us);

/** Limit of frames per second, 0 for no limit. Game logic runs once per frame,
 * so this also sets the game speed; 16 is the original pace. */
extern short game_fps_cap;
extern short frameskip;
/** Changes the frame rate cap by one step, up (1) or down (-1). */
void game_fps_cap_step(int dir);
/** Tells that a frame was shown to the user; the counter at the top right counts these. */
void fps_counter_note_present(void);
/** Limit of the frames shown per second by the application (the camera, the mouse): 30, 60, 90 or 120.
 * The game loop (the turns) has its own, game_fps_cap. */
extern ushort game_app_fps_cap;
void game_app_fps_cap_step(int dir);
void frameskip_increase(void);
void frameskip_decrease(void);

/** Counts a drawn frame and draws the FPS counter in the top right corner. */
void fps_counter_show(void);

/** Phases of a frame which have their duration measured for the FPS counter. */
enum PerfPhase {
    PerfPhase_Draw,
    PerfPhase_Swap,
    PerfPhase_COUNT,
};

void perf_phase_begin(enum PerfPhase phase);
void perf_phase_end(enum PerfPhase phase);

TbBool display_needs_redraw_this_turn(void);
void update_tick_time(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

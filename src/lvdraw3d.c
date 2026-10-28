/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file lvdraw3d.c
 *     Routines for level and map drawing using 3D rendering.
 * @par Purpose:
 *     Implement functions for creating drawlist for rendering the 3D view.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     24 Dec 2023 - 10 Nov 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "lvdraw3d.h"

#include "bfgl.h"
#include "bfhwrend.h"
#include "bfsprite.h"
#include "frame_sprani.h"

#include <assert.h>
#include <limits.h>
#include "bfendian.h"
#include "bfmath.h"
#include "bfmemut.h"
#include "bfplanar.h"

#include "enginbckt.h"
#include "engincam.h"
#include "engincolour.h"
#include "engindrwlstm.h"
#include "enginfloor.h"
#include "enginlights.h"
#include "enginpeff.h"
#include "enginprops.h"
#include "engintrns.h"
#include "engintxtrmap.h"
#include "enginzoom.h"

#include "bigmap.h"
#include "embedanim.h"
#include "engindrwlstm_wrp.h"
#include "engindrwlstx.h"
#include "enginsngtxtr.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "hud_draw.h"
#include "player.h"
#include "scanner.h"
#include "swlog.h"
#include "game_sprts.h"
#include "people.h"
#include "thing.h"
#include "thing_expld.h"
#include "thing_onface.h"
#include "thing_ovmous.h"
#include "tngcolisn.h"
#include "tngobjdrw.h"
/******************************************************************************/

struct Range {
    int beg;
    int fin;
};

/******************************************************************************/
#define SUPER_QUICK_LIGHTS_MAX (RENDER_AREA_MAX+1)*(RENDER_AREA_MAX+1)
short super_quick_light[(RENDER_AREA_MAX+1)*(RENDER_AREA_MAX+1)];

s32 dword_152E50 = 256;
s32 dword_152E54 = 80;
s32 dword_152E58 = 410;

s32 dword_176CBC = 0;

short drwfloor_start_cor_x;
short drwfloor_start_cor_z;
TbBool nuclear_overexposure = false;
ubyte byte_1C8444 = 0;

/** Height of the wobbly surface at the given map spot on the given animation
 * turn.
 *
 * Pulled out of shpoint_compute_coord_y() so that the surface can be asked
 * for the turn being drawn and for the one after it, and placed in between
 * when a game turn is drawn in more than one frame.
 */
static int floor_wobble_at_turn(int elcr_x, int elcr_z, int dvfactor, uint anim_turn)
{
    return (waft_table2[(anim_turn + (elcr_x >> 7)) & 0x1F]
         + waft_table2[(anim_turn + (elcr_z >> 7)) & 0x1F]
         + waft_table2[(32 * anim_turn / dvfactor) & 0x1F]) >> 3;
}

int shpoint_compute_coord_y(struct ShEnginePoint *p_sp, struct MyMapElement *p_mapel, int elcr_x, int elcr_z, int mag)
{
    int elcr_y;

    if (game_perspective == ProjM_IsomFloorFlat)
    {
        elcr_y = 0;
        p_sp->ReflShade = 0;
    }
    else if ((p_mapel->Flags & MEF1_Unkn10) == 0)
    {
        elcr_y = 8 * p_mapel->Alt;
        if ((p_mapel->Flags & MEF1_Unkn40) != 0)
            elcr_y += waft_between_turns(render_anim_turn);
        p_sp->ReflShade = 0;
    }
    else
    {
        int wobble, dvfactor;
        uint anim_turn, within_turn;

        elcr_y = 8 * p_mapel->Alt;
        dvfactor = 140 + ((bw_rotl32(0x5D3BA6C3, elcr_z >> 8) ^ bw_rotr32(0xA7B4D8AC, elcr_x >> 8)) & 0x7F);
        anim_turn = render_anim_turn >> RENDER_ANIM_TURN_SHIFT;
        within_turn = render_anim_turn & (RENDER_ANIM_TURN_UNIT - 1);
        wobble = floor_wobble_at_turn(elcr_x, elcr_z, dvfactor, anim_turn);
        if (within_turn != 0)
        {
            int wobble_next;

            wobble_next = floor_wobble_at_turn(elcr_x, elcr_z, dvfactor, anim_turn + 1);
            wobble += ((wobble_next - wobble) * (int)within_turn) / RENDER_ANIM_TURN_UNIT;
        }
        elcr_y += mag * wobble;
        p_sp->ReflShade = (wobble + 32) << 2;
    }
    // Keep edges of the map black
    if ((elcr_x <= 0) || (elcr_z <= 0) || (elcr_x >= MAP_COORD_WIDTH-1) || (elcr_z >= MAP_COORD_HEIGHT-1))
        p_sp->ReflShade = 0;

    return elcr_y;
}

/** Compute shade value for drawing given shaded engine point.
 */
static short calculate_shpoint_shade(short ambient, ushort first_light, short *p_sqlight)
{
    int shade;

    shade = (ambient << 7) + (*p_sqlight << 8);
    shade += cummulate_shade_from_quick_lights(first_light);
    if (shade > 0x7E00)
        shade = 0x7F00;
    return shade;
}

/** Compute shade value for shaded engine point, fading to black beyond some range.
 */
static short calculate_shpoint_shade_fading(short ambient, ushort first_light, int dist)
{
    int shade;

    shade = (ambient << 7);
    shade += cummulate_shade_from_quick_lights(first_light);
    if (dist > 3000) {
        if (3512 - dist > 0)
            shade = ((3512 - dist) * shade) >> 9;
        else
            shade = 0;
    }
    if (shade > 0x7E00)
        shade = 0x7F00;
    return shade;
}

void screen_position_face_render_null_callback(
  struct PolyPoint *p_pt1,
  struct PolyPoint *p_pt2,
  struct PolyPoint *p_pt3,
  ushort face, ubyte type)
{
}

void screen_sorted_sprite_render_null_callback(ushort sspr)
{
}

void screen_position_face_render_callback(
  struct PolyPoint *p_pt1,
  struct PolyPoint *p_pt2,
  struct PolyPoint *p_pt3,
  ushort face, ubyte type)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    if (p_locplayer->TargetType < TrgTp_Unkn3) {
        check_mouse_over_face(p_pt1, p_pt2, p_pt3, face, type);
    }
}

/******************************************************************************/
/* Blood: the drops which fly from a person who is hit, the splatters which they leave on the ground,
 * and the pool which grows under a body. With the GPU drawing the scene, the drops and splatters are
 * drawn by shaders. They are put in the lists of sorted sprites, in their places, so the buildings and
 * the people hide them the way they hide everything else; the sorted sprite which carries them has a
 * tiny frame, and screen_sorted_sprite_effect_replace_callback() draws the shader in its place. */

#define BLOODFX_MAX 320
/** A frame with one tiny sprite; carries the blood in the sorted sprites. */
#define BLOODFX_FRAME 4383
/** Game turns until a splatter is dry, and until it starts to fade away, and until it is gone. */
#define BLOODFX_DRY_TURNS 160.0f
#define BLOODFX_FADE_BEG_TURNS 3000.0f
#define BLOODFX_FADE_END_TURNS 4000.0f
/** In the rain the rain washes the splatters and stains away: they fade in a few seconds. */
#define BLOODFX_RAIN_FADE_BEG_TURNS 40.0f
#define BLOODFX_RAIN_FADE_END_TURNS 120.0f
/** How fast a drop of blood falls: map units per game turn, every turn. Low, so the drops fly far before they land. */
#define BLOODFX_GRAVITY 0.5f

/** The exact test of a path against the walls and people, which the bullets use; gives in `status` what was
 * hit (1 a wall, 2 a solid part of the map, 3 a person), and in x2, y2, z2 the last free place on the path. */
int bul_path_end(int x1, int y1, int z1, int *x2, int *y2, int *z2, int radius, struct Thing *p_owner, ubyte *status);

/** Game turns in which the pool under a body grows to its full size. */
#define BLOODFX_POOL_GROW_TURNS 200.0f

struct BloodFx {
    ubyte kind;             /**< 0 is free, 1 a drop in flight, 2 a splatter on the ground or a roof, 3 a stain on a wall. */
    float x, y, z;          /**< Map coordinates; for a drop, where it started. */
    float vx, vy, vz;       /**< For a drop, how it moves in a game turn; for a splatter, the direction it was spattered;
                                 for a stain on a wall, vy is 0 if the wall faces along X, 1 if along Z. */
    float t0;               /**< Game turn (with fraction) when it started. */
    float lt;               /**< For a drop, the time (from t0) up to which its way was checked for a wall; for a spark, how many times it bounced. */
    ubyte stain;            /**< For a drop of blood: whether it leaves a splatter or a stain where it lands. */
    float born;             /**< For a spark, the game turn when it was made, as it can bounce and start its way again. */
    float size;             /**< Width of a drop, radius of a splatter, in map units. */
    int seed;
    /* What the screen gets, for the frame; made when it is put in the list */
    float s_tx, s_ty, s_w;  /**< A drop: where its tail is, from the head, and its width, in pixels. */
    float s_ux, s_uy, s_vx, s_vy; /**< A splatter or stain: the half-sides of its parallelogram, in pixels. */
};

static struct BloodFx bloodfx[BLOODFX_MAX];
static ulong bloodfx_last_turn = 0;
static ulong bloodfx_rng = 2463534242u;

static float bloodfx_rand(void)
{
    bloodfx_rng = bloodfx_rng * 1664525u + 1013904223u;
    return (float)((bloodfx_rng >> 8) & 0xFFFF) / 65536.0f;
}

static TbBool bloodfx_active(void)
{
    static int disabled = -1;

    if (disabled < 0)
        disabled = (getenv("SWFX_NO_BLOODFX") != NULL) ? 1 : 0;
    return !disabled && LbHwRenderEnabled() && LbGlIsActive();
}

/** The game turn, with the part of the next one which has passed, so the blood moves smoothly
 * between the turns; stops with the game when it is paused. */
static float bloodfx_now(void)
{
    float f;
    ulong len;

    f = 0.0f;
    len = game_period_length_us();
    if (len > 0) {
        f = (float)(game_now_us() - game_period_start_us()) / (float)len;
        if (f > 1.0f)
            f = 1.0f;
        if (f < 0.0f)
            f = 0.0f;
    }
    return (float)gameturn + f;
}

/** Tells how visible a splatter or stain of the given age is, from 1 to 0; 0 means it is gone. */
static float bloodfx_visibility(float age)
{
    float beg, end;

    if (gamep_scene_effect_type == ScEff_RAIN) {
        beg = BLOODFX_RAIN_FADE_BEG_TURNS;
        end = BLOODFX_RAIN_FADE_END_TURNS;
    } else {
        beg = BLOODFX_FADE_BEG_TURNS;
        end = BLOODFX_FADE_END_TURNS;
    }
    if (age <= beg)
        return 1.0f;
    if (age >= end)
        return 0.0f;
    return 1.0f - (age - beg) / (end - beg);
}

static float bloodfx_ground(float x, float z)
{
    return (float)PRCCOORD_TO_MAPCOORD(alt_at_point((short)x, (short)z));
}

/** Tells if the point is inside a building or another solid thing of the map. */
static TbBool bloodfx_solid(float x, float y, float z)
{
    ushort q;

    if ((x < 0.0f) || (z < 0.0f) || (x > 32000.0f) || (z > 32000.0f))
        return false;
    // 0 when free, 0xFFFF when under the ground, and what is in the way otherwise
    q = check_col_collision((int)x, (int)y, (int)z);
    return (q != 0) && (q != 0xFFFF);
}

/** Leaves a splatter on the ground or a roof (kind 2), or a stain on a wall (kind 3; axis is 0 for a wall
 * which faces along X, 1 for one which faces along Z). */
static void bloodfx_add_splat(float x, float y, float z, float dx, float dz, float size, float now,
  int kind, int axis)
{
    struct BloodFx *b, *oldest;
    int i;

    b = NULL;
    oldest = NULL;
    for (i = 0; i < BLOODFX_MAX; i++)
    {
        if (bloodfx[i].kind == 0) {
            b = &bloodfx[i];
            break;
        }
        if (((bloodfx[i].kind == 2) || (bloodfx[i].kind == 3)) && ((oldest == NULL) || (bloodfx[i].t0 < oldest->t0)))
            oldest = &bloodfx[i];
    }
    if (b == NULL)
        b = oldest;
    if (b == NULL)
        return;
    if (kind == 3) {
        // A stain on a wall does not go below the ground
        float g = bloodfx_ground(x, z);

        if (y < g + size * 0.6f)
            y = g + size * 0.6f;
    }
    b->kind = kind;
    b->x = x; b->y = y; b->z = z;
    b->vx = dx; b->vy = (float)axis; b->vz = dz;
    b->t0 = now;
    b->size = size;
    b->seed = (int)(bloodfx_rand() * 1000.0f);
}

TbBool blood_fx_spawn_ex(int x, int y, int z, int dirx, int dirz, int amount, TbBool stain)
{
    float fx, fz, len, now, speed_scale;
    int i, n, slot;

    if (!bloodfx_active())
        return false;
    len = sqrtf((float)(dirx * dirx + dirz * dirz));
    if (len < 1.0f) {
        fx = 1.0f; fz = 0.0f;
    } else {
        fx = (float)dirx / len;
        fz = (float)dirz / len;
    }
    now = bloodfx_now();
    n = amount * 6;
    slot = 0;
    {
        // SWFX_BLOODSPRAY_TEST=<number above 1> makes the drops that many times faster, to reach the buildings
        const char *tv = getenv("SWFX_BLOODSPRAY_TEST");

        speed_scale = (tv != NULL) ? (float)atof(tv) : 1.0f;
        if (speed_scale < 1.0f)
            speed_scale = 1.0f;
    }
    for (i = 0; i < n; i++)
    {
        struct BloodFx *b;
        float ang, spd;

        while ((slot < BLOODFX_MAX) && (bloodfx[slot].kind != 0))
            slot++;
        if (slot >= BLOODFX_MAX)
            break;
        b = &bloodfx[slot];
        // The blood goes on the way the shot went, away from the shooter and out of the body, in a
        // narrow fan; only a few drops go to the sides
        if (bloodfx_rand() < 0.88f) {
            ang = atan2f(fz, fx) + (bloodfx_rand() - 0.5f) * 0.9f;
            spd = 20.0f + bloodfx_rand() * 30.0f;
        } else {
            ang = atan2f(fz, fx) + (bloodfx_rand() - 0.5f) * 3.4f;
            spd = 8.0f + bloodfx_rand() * 14.0f;
        }
        b->kind = 1;
        b->stain = stain;
        b->x = (float)x + (bloodfx_rand() - 0.5f) * 10.0f;
        b->y = (float)y;
        b->z = (float)z + (bloodfx_rand() - 0.5f) * 10.0f;
        b->vx = cosf(ang) * spd * speed_scale;
        b->vz = sinf(ang) * spd * speed_scale;
        // It does not shoot up: it goes along, from the place of the hit, and falls
        b->vy = -1.0f + bloodfx_rand() * 3.0f;
        b->t0 = now;
        b->lt = 0.0f;
        b->size = 5.0f + bloodfx_rand() * 5.0f;
        b->seed = (int)(bloodfx_rand() * 1000.0f);
    }
    return true;
}

TbBool blood_fx_spawn(int x, int y, int z, int dirx, int dirz, int amount)
{
    return blood_fx_spawn_ex(x, y, z, dirx, dirz, amount, true);
}

/** Sparks from a ricochet: hot streaks which fly back from the surface, and fall. */
#define BLOODFX_SPARK_LIFE 26.0f
#define BLOODFX_SPARK_GRAVITY 2.4f
/** A spark which hits the ground bounces this many times, losing speed each time. */
#define BLOODFX_SPARK_BOUNCES 2
/** The small light of a ricochet: its strength at the start (the scale of the lamps and explosions), and how
 * long it lasts, in game turns. */
#define BLOODFX_SPARK_LIGHT 26.0f
#define BLOODFX_SPARK_LIGHT_TURNS 8.0f
#define BLOODFX_SPARK_LIGHTS_MAX 16

struct SparkLight {
    float x, z;
    float t0;
    ubyte used;
};
static struct SparkLight spark_lights[BLOODFX_SPARK_LIGHTS_MAX];
static ulong spark_light_last_turn = 0;

TbBool spark_fx_spawn(int x, int y, int z, int dirx, int dirz, int amount)
{
    float fx, fz, len, now;
    int i, n, slot;

    if (!bloodfx_active())
        return false;
    len = sqrtf((float)(dirx * dirx + dirz * dirz));
    if (len < 1.0f) {
        fx = 1.0f; fz = 0.0f;
    } else {
        fx = (float)dirx / len;
        fz = (float)dirz / len;
    }
    now = bloodfx_now();
    // The light of the ricochet
    for (i = 0; i < BLOODFX_SPARK_LIGHTS_MAX; i++)
    {
        if (!spark_lights[i].used || (now - spark_lights[i].t0 > BLOODFX_SPARK_LIGHT_TURNS)) {
            spark_lights[i].used = 1;
            spark_lights[i].x = (float)x;
            spark_lights[i].z = (float)z;
            spark_lights[i].t0 = now;
            break;
        }
    }
    n = amount * 5;
    slot = 0;
    for (i = 0; i < n; i++)
    {
        struct BloodFx *b;
        float ang, spd;

        while ((slot < BLOODFX_MAX) && (bloodfx[slot].kind != 0))
            slot++;
        if (slot >= BLOODFX_MAX)
            break;
        b = &bloodfx[slot];
        // A ricochet: the opposite of the way the shot went (the direction given is the one toward where it came
        // from), in a narrow fan, along the ground and not up, and a bit slower than the blood
        if (bloodfx_rand() < 0.7f)
            ang = atan2f(fz, fx) + (bloodfx_rand() - 0.5f) * 2.2f;
        else
            ang = bloodfx_rand() * 6.2831853f; // and some in any direction
        spd = 5.0f + bloodfx_rand() * 14.0f;
        b->kind = 4;
        b->x = (float)x + fx * 4.0f;
        b->y = (float)y;
        b->z = (float)z + fz * 4.0f;
        b->vx = cosf(ang) * spd;
        b->vz = sinf(ang) * spd;
        // Some low, some up, so they fly in different ways and fall in arcs
        b->vy = -1.0f + bloodfx_rand() * 13.0f;
        b->t0 = now;
        b->born = now;
        b->lt = 0.0f;
        b->size = 1.6f + bloodfx_rand() * 1.4f;
        b->seed = (int)(bloodfx_rand() * 1000.0f);
    }
    return true;
}

/** Puts the blood in the list of the sorted sprites for the frame, and moves the drops. */
static void blood_fx_enlist(void)
{
    struct ShEnginePoint s0, sx, sz, sv;
    float now, ex, ey, ma, mb, mc, md, sv_x, sv_y;
    int i;

    if (!bloodfx_active())
        return;
    if (gameturn + 20 < bloodfx_last_turn)
        memset(bloodfx, 0, sizeof(bloodfx)); // a new mission has started
    bloodfx_last_turn = gameturn;
    now = bloodfx_now();

    if (getenv("SWFX_BLOODSPRAY_TEST") != NULL) {
        // A hit on the first agent every little while, for looking at what it does
        static ulong next_turn = 0;
        struct Thing *p_ag;

        if ((gameturn >= next_turn) && ((p_ag = players[local_player_no].MyAgent[0]) != NULL)) {
            float a = bloodfx_rand() * 6.2831853f;

            blood_fx_spawn(PRCCOORD_TO_MAPCOORD(p_ag->X), PRCCOORD_TO_MAPCOORD(p_ag->Y) + 25,
              PRCCOORD_TO_MAPCOORD(p_ag->Z), (int)(cosf(a) * 100.0f), (int)(sinf(a) * 100.0f), 4);
            // and a ricochet next to it, for looking at the sparks and their light
            spark_fx_spawn(PRCCOORD_TO_MAPCOORD(p_ag->X) + 60, PRCCOORD_TO_MAPCOORD(p_ag->Y) + 25,
              PRCCOORD_TO_MAPCOORD(p_ag->Z), (int)(cosf(a + 3.0f) * 100.0f), (int)(sinf(a + 3.0f) * 100.0f), 4);
            next_turn = gameturn + 30;
        }
    }

    // The small lights of the ricochets, given to the GPU once in each game turn
    if (gameturn != spark_light_last_turn) {
        int li;

        spark_light_last_turn = gameturn;
        for (li = 0; li < BLOODFX_SPARK_LIGHTS_MAX; li++)
        {
            float la = now - spark_lights[li].t0;

            if (!spark_lights[li].used)
                continue;
            if (la > BLOODFX_SPARK_LIGHT_TURNS) {
                spark_lights[li].used = 0;
                continue;
            }
            LbHwNoteDynamicLight((long)gameturn, spark_lights[li].x, spark_lights[li].z,
              BLOODFX_SPARK_LIGHT * (1.0f - la / BLOODFX_SPARK_LIGHT_TURNS));
        }
    }

    // How the ground is seen: a step on the map to the screen; and how big a circle on the ground is
    transform_shpoint(&s0, 0, -8 * engn_yc, 0);
    transform_shpoint(&sx, 256, -8 * engn_yc, 0);
    transform_shpoint(&sz, 0, -8 * engn_yc, 256);
    // and a step up, for the walls
    transform_shpoint(&sv, 0, -8 * engn_yc + 8 * 256, 0);
    ma = (float)(sx.X - s0.X) / 256.0f;
    mb = (float)(sz.X - s0.X) / 256.0f;
    mc = (float)(sx.Y - s0.Y) / 256.0f;
    md = (float)(sz.Y - s0.Y) / 256.0f;
    sv_x = (float)(sv.X - s0.X) / 256.0f;
    sv_y = (float)(sv.Y - s0.Y) / 256.0f;
    ex = sqrtf(ma * ma + mb * mb);
    ey = sqrtf(mc * mc + md * md);

    for (i = 0; i < BLOODFX_MAX; i++)
    {
        struct BloodFx *b = &bloodfx[i];
        struct ShEnginePoint sh, st;
        float tau, hx, hy, hz, gy;

        if (b->kind == 1)
        {
            float pt, fx, fy, fz, vv;
            int outcome, axis;

            tau = now - b->t0;
            pt = b->lt;
            // Where the drop was at the last frame, and where it is now
            fx = b->x + b->vx * pt;
            fz = b->z + b->vz * pt;
            fy = b->y + b->vy * pt - 0.5f * BLOODFX_GRAVITY * pt * pt;
            hx = b->x + b->vx * tau;
            hz = b->z + b->vz * tau;
            hy = b->y + b->vy * tau - 0.5f * BLOODFX_GRAVITY * tau * tau;
            vv = b->vy - BLOODFX_GRAVITY * tau;
            outcome = 0;
            axis = 0;
            if (hy <= bloodfx_ground(hx, hz)) {
                outcome = 1; // the ground
                fx = hx; fz = hz;
            } else if (bloodfx_solid(hx, hy, hz) || bloodfx_solid((fx + hx) * 0.5f, (fy + hy) * 0.5f, (fz + hz) * 0.5f)) {
                // The test of the map cells is coarse; it only says that a wall may be in the way. The exact
                // test, the one which the bullets have, gives the place where the path meets the wall
                int tx_ = (int)hx, ty_ = (int)hy, tz_ = (int)hz;
                ubyte rstatus = 0;

                bul_path_end((int)fx, (int)fy, (int)fz, &tx_, &ty_, &tz_, 0, NULL, &rstatus);
                if ((rstatus == 1) || (rstatus == 2)) {
                    float hs = sqrtf(b->vx * b->vx + b->vz * b->vz);

                    fx = (float)tx_; fy = (float)ty_; fz = (float)tz_;
                    if (fabsf(vv) > hs) {
                        outcome = 3; // went down onto a roof
                    } else {
                        // The wall faces along X or along Z: the one which has the solid just ahead of the place
                        outcome = 2;
                        if (bloodfx_solid(fx + ((b->vx > 0.0f) ? 10.0f : -10.0f), fy, fz))
                            axis = 0;
                        else if (bloodfx_solid(fx, fy, fz + ((b->vz > 0.0f) ? 10.0f : -10.0f)))
                            axis = 1;
                        else
                            axis = (fabsf(b->vx) > fabsf(b->vz)) ? 0 : 1;
                    }
                } else if (rstatus == 3) {
                    outcome = 4; // a person is in the way
                }
            }
            b->lt = tau;
            if (outcome != 0) {
                // It lands: some of the drops leave a splatter, or a stain on the wall
                if ((outcome != 4) && b->stain && (bloodfx_rand() < 0.6f)) {
                    if (outcome == 1)
                        bloodfx_add_splat(fx, bloodfx_ground(fx, fz), fz, b->vx, b->vz,
                          20.0f + bloodfx_rand() * 24.0f, now, 2, 0);
                    else if (outcome == 2)
                        bloodfx_add_splat(fx, fy, fz, 0.0f, 0.0f, 18.0f + bloodfx_rand() * 14.0f, now, 3, axis);
                    else
                        bloodfx_add_splat(fx, fy, fz, b->vx, b->vz, 12.0f + bloodfx_rand() * 16.0f, now, 2, 0);
                }
                b->kind = 0;
                continue;
            }
            if (tau > 80.0f) {
                b->kind = 0;
                continue;
            }            transform_shpoint(&sh, (int)hx - engn_xc, 8 * (int)hy - 8 * engn_yc, (int)hz - engn_zc);
            {
                float tt = (tau > 0.7f) ? tau - 0.7f : 0.0f;

                transform_shpoint(&st, (int)(b->x + b->vx * tt) - engn_xc,
                  8 * (int)(b->y + b->vy * tt - 0.5f * BLOODFX_GRAVITY * tt * tt) - 8 * engn_yc,
                  (int)(b->z + b->vz * tt) - engn_zc);
            }
            b->s_tx = (float)(st.X - sh.X);
            b->s_ty = (float)(st.Y - sh.Y);
            b->s_w = b->size * (ex + ey) * 0.5f;
            if (b->s_w < 1.5f)
                b->s_w = 1.5f;
            enlist_draw_frame_graphic_scale((int)hx - engn_xc, (int)hy, (int)hz - engn_zc,
              BLOODFX_FRAME, 100, 32, 250, (intptr_t)b);
        }
        else if (b->kind == 4)
        {
            float tt, shx, shy, shz;

            // A spark: it flies, falls, and goes out; it is a streak from where it was a moment ago
            tau = now - b->t0;
            hx = b->x + b->vx * tau;
            hz = b->z + b->vz * tau;
            hy = b->y + b->vy * tau - 0.5f * BLOODFX_SPARK_GRAVITY * tau * tau;
            if (now - b->born > BLOODFX_SPARK_LIFE) {
                b->kind = 0;
                continue;
            }
            gy = bloodfx_ground(hx, hz);
            if (hy <= gy) {
                float vyn = b->vy - BLOODFX_SPARK_GRAVITY * tau;

                // It hits the ground: it bounces up, slower, and goes on, a few times
                if (((int)b->lt >= BLOODFX_SPARK_BOUNCES) || (-vyn < 1.5f)) {
                    b->kind = 0;
                    continue;
                }
                b->x = hx;
                b->z = hz;
                b->y = gy + 0.5f;
                b->vx *= 0.7f;
                b->vz *= 0.7f;
                b->vy = -vyn * 0.5f;
                b->t0 = now;
                b->lt += 1.0f;
                hy = b->y;
                tau = 0.0f;
            }
            tt = (tau > 0.6f) ? tau - 0.6f : 0.0f;
            shx = b->x + b->vx * tt;
            shz = b->z + b->vz * tt;
            shy = b->y + b->vy * tt - 0.5f * BLOODFX_SPARK_GRAVITY * tt * tt;
            transform_shpoint(&sh, (int)hx - engn_xc, 8 * (int)hy - 8 * engn_yc, (int)hz - engn_zc);
            transform_shpoint(&st, (int)shx - engn_xc, 8 * (int)shy - 8 * engn_yc, (int)shz - engn_zc);
            b->s_tx = (float)(st.X - sh.X);
            b->s_ty = (float)(st.Y - sh.Y);
            b->s_w = b->size * (ex + ey) * 0.5f;
            if (b->s_w < 1.2f)
                b->s_w = 1.2f;
            enlist_draw_frame_graphic_scale((int)hx - engn_xc, (int)hy, (int)hz - engn_zc,
              BLOODFX_FRAME, 100, 32, 250, (intptr_t)b);
        }
        else if ((b->kind == 2) || (b->kind == 3))
        {
            float age, dxp, dyp, dl, reach;

            age = now - b->t0;
            if (bloodfx_visibility(age) <= 0.0f) {
                b->kind = 0; // gone: faded out, or washed away by the rain
                continue;
            }
            gy = b->y;
            transform_shpoint(&sh, (int)b->x - engn_xc, 8 * (int)gy - 8 * engn_yc, (int)b->z - engn_zc);
            if (b->kind == 2) {
                // On the ground (or a roof): a circle is an ellipse on the screen, with the axes on the screen axes
                b->s_ux = b->size * ex;
                b->s_uy = 0.0f;
                b->s_vx = 0.0f;
                b->s_vy = b->size * ey;
                // The direction of the spatter in the circle which the shader draws: the step on the screen
                // is divided by the size of the circle on the screen on each axis
                dxp = (ma * b->vx + mb * b->vz) / ex;
                dyp = (mc * b->vx + md * b->vz) / ey;
                dl = sqrtf(dxp * dxp + dyp * dyp);
                b->s_tx = (dl > 0.0001f) ? dxp / dl : 1.0f;
                b->s_ty = (dl > 0.0001f) ? dyp / dl : 0.0f;
            } else {
                // On a wall: along the wall and down; the blood runs down it
                if (b->vy < 0.5f) {
                    b->s_ux = b->size * mb;  // the wall faces along X, so it goes along Z
                    b->s_uy = b->size * md;
                } else {
                    b->s_ux = b->size * ma;
                    b->s_uy = b->size * mc;
                }
                b->s_vx = -b->size * sv_x;
                b->s_vy = -b->size * sv_y;
                {
                    // A round stain, whatever the angle: the sides get the same length on the screen
                    float lu = sqrtf(b->s_ux * b->s_ux + b->s_uy * b->s_uy);
                    float lv = sqrtf(b->s_vx * b->s_vx + b->s_vy * b->s_vy);
                    float rr = 0.4f * (lu + lv);

                    if ((lu > 0.001f) && (lv > 0.001f)) {
                        // Slimmer along the wall than it is tall, so it does not stick out at the edges
                        b->s_ux *= 0.6f * rr / lu; b->s_uy *= 0.6f * rr / lu;
                        b->s_vx *= rr / lv; b->s_vy *= rr / lv;
                    }
                }
                b->s_tx = 0.0f;
                b->s_ty = 1.0f;
            }
            reach = fabsf(b->s_ux) + fabsf(b->s_vx) + 10.0f;
            if ((sh.X < -reach) || (sh.X > lbDisplay.GraphicsScreenWidth + reach)
              || (sh.Y < -(fabsf(b->s_uy) + fabsf(b->s_vy) + 10.0f))
              || (sh.Y > lbDisplay.GraphicsScreenHeight + fabsf(b->s_uy) + fabsf(b->s_vy) + 10.0f))
                continue;
            // A stain on a wall is a bit nearer than the point, so that the wall does not hide it
            enlist_draw_frame_graphic_scale((int)b->x - engn_xc, (int)gy, (int)b->z - engn_zc,
              BLOODFX_FRAME, (b->kind == 3) ? 160 : 100, 32, 250, (intptr_t)b);
        }
    }
}

/** Draws a drop or splatter of blood in the place of its sorted sprite; false if the item is not blood. */
static TbBool blood_fx_draw_item(intptr_t src, int sx, int sy)
{
    struct BloodFx *b;

    if ((src < (intptr_t)&bloodfx[0]) || (src >= (intptr_t)&bloodfx[BLOODFX_MAX]))
        return false;
    b = (struct BloodFx *)src;
    if (b->kind == 1) {
        LbHwRecordBloodStreak((float)sx, (float)sy, (float)sx + b->s_tx, (float)sy + b->s_ty,
          b->s_w, 1.0f, b->seed, 0.9f);
    } else if (b->kind == 4) {
        // How hot the spark still is: it dims as it goes
        float heat = 1.0f - (bloodfx_now() - b->born) / BLOODFX_SPARK_LIFE;

        LbHwRecordSparkStreak((float)sx, (float)sy, (float)sx + b->s_tx, (float)sy + b->s_ty,
          b->s_w, (heat > 0.0f) ? heat * (0.55f + 0.45f * heat) : 0.0f, b->seed);
    } else if ((b->kind == 2) || (b->kind == 3)) {
        float age, alpha;

        age = bloodfx_now() - b->t0;
        alpha = bloodfx_visibility(age);
        // A stain on a wall has a short drip, not the long tongue of a splatter on the ground
        LbHwRecordBloodSplatOriented((float)sx, (float)sy, b->s_ux, b->s_uy, b->s_vx, b->s_vy, b->seed,
          age / BLOODFX_DRY_TURNS, alpha, (b->kind == 3) ? 0.75f : 0.9f, b->s_tx, b->s_ty,
          (b->kind == 3) ? 0.12f : 1.0f);
    }
    return true;
}

/** How big the pool of blood under the person is: it starts as a spot when the person dies, and grows as it bleeds. */
static float screen_blood_pool_growth_callback(intptr_t src_item)
{
    struct Thing *p_thing;
    ulong died;
    float age, g;

    p_thing = (struct Thing *)src_item;
    if ((p_thing == NULL) || (p_thing->Type != TT_PERSON))
        return 1.0f;
    if ((p_thing->ThingOffset <= 0) || (p_thing->ThingOffset > THINGS_LIMIT))
        return 1.0f;
    died = person_death_turn[p_thing->ThingOffset];
    // The ones which were dead at the start of the mission, or from another mission, have full pools
    if ((died == 0) || (died > gameturn))
        return 1.0f;
    age = bloodfx_now() - (float)died;
    if (age >= BLOODFX_POOL_GROW_TURNS)
        return 1.0f;
    if (age < 0.0f)
        age = 0.0f;
    g = 1.0f - age / BLOODFX_POOL_GROW_TURNS;
    return 1.0f - g * g * g * 0.9f;
}

void screen_sorted_sprite_statc_render_callback(ushort sspr)
{
    struct Thing *p_thing;
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    p_thing = (struct Thing *)game_sort_sprites[sspr].SrcItem;

    if ((p_locplayer->TargetType <= TrgTp_DroppedTng) && (p_thing->Type == SmTT_DROPPED_ITEM)) {
        check_mouse_overlap_item(sspr);
    }

    if ((p_locplayer->TargetType < TrgTp_Unkn6) && (p_thing->Type == TT_MINE))
    {
        if ((p_thing->SubType == 7) || (p_thing->SubType == 3))
            check_mouse_overlap_item(sspr);
        else if (p_thing->SubType == 48)
            check_mouse_overlap(sspr);
    }
}

/** The particles of the clouds of gas grenades are replaced by puffs of smoke drawn by the GPU, in the
 * same places and sizes, so they move the same way; the gas particles are the effects with sub type 30
 * and the flags 0x10 (psycho gas) or 0x20 (knock out gas). */
TbBool screen_sorted_sprite_effect_replace_callback(ushort sspr, int x0, int y0, int w, int h)
{
    struct SimpleThing *p_sthing;
    int kind;
    float cx, cy, size;

    if (!lbHwRecording || !LbHwRenderEnabled())
        return false;
    if (blood_fx_draw_item(game_sort_sprites[sspr].SrcItem, game_sort_sprites[sspr].X, game_sort_sprites[sspr].Y))
        return true;
    if (getenv("SWFX_NO_GASSMOKE") != NULL)
        return false;
    p_sthing = (struct SimpleThing *)game_sort_sprites[sspr].SrcItem;
    if ((p_sthing == NULL) || (p_sthing->Type != SmTT_SCALE_EFFECT) || (p_sthing->SubType != 30))
        return false;
    if ((p_sthing->Flag & 0x10) != 0)
        kind = 0;
    else if ((p_sthing->Flag & 0x20) != 0)
        kind = 1;
    else
        return false;
    cx = (float)x0 + (float)w * 0.5f;
    cy = (float)y0 + (float)h * 0.5f;
    size = (float)((w > h) ? w : h) * 1.5f;
    LbHwRecordSmokePuff(cx - size * 0.5f, cy - size * 0.5f, cx + size * 0.5f, cy + size * 0.5f,
      (int)p_sthing->ThingOffset * 7, kind, 0.9f);
    return true;
}

/** Tells if the person is an enemy agent, which wears the enemy costume. */
TbBool screen_sorted_sprite_persn_alt_callback(ushort sspr)
{
    struct Thing *p_thing;

    if (game_enemy_costume == ingame.TrenchcoatPreference)
        return false;
    p_thing = (struct Thing *)game_sort_sprites[sspr].SrcItem;
    if ((p_thing == NULL) || (p_thing->Type != TT_PERSON) || (p_thing->SubType != SubTT_PERS_AGENT))
        return false;
    // SWFX_ESPR_ALL makes all the agents wear the enemy costume, to see how it looks
    if (getenv("SWFX_ESPR_ALL") != NULL)
        return true;
    return (p_thing->Flag & TngF_PlayerAgent) == 0
      && (p_thing->U.UPerson.EffectiveGroup != ingame.MyGroup);
}

void screen_sorted_sprite_persn_render_callback(ushort sspr)
{
    struct Thing *p_thing;

    p_thing = (struct Thing *)game_sort_sprites[sspr].SrcItem;

    if (p_thing->U.UPerson.EffectiveGroup != ingame.MyGroup)
    {
        PlayerInfo *p_locplayer;

        p_locplayer = &players[local_player_no];
        if ((p_thing->Flag & TngF_Destroyed) != 0)
        {
            if (p_locplayer->TargetType < TrgTp_Unkn1)
                check_mouse_overlap_corpse(sspr);
        }
        else
        {
            if (p_locplayer->TargetType < TrgTp_Unkn7)
                check_mouse_overlap(sspr);
        }
    }

    if (in_network_game)
    {
        struct Thing *p_owntng;

        p_owntng = NULL;
        if (person_is_other_players_agent(p_thing, local_player_no))
        {
            p_owntng = p_thing;
        }
        else if ((p_thing->Flag & TngF_Persuaded) != 0)
        {
            p_owntng = &things[p_thing->Owner];
            if (!person_is_other_players_agent(p_owntng, local_player_no))
                p_owntng = NULL;
        }
        if ((p_owntng != NULL) && (p_owntng->U.UPerson.CurrentWeapon != WEP_CLONESHLD)) {
            check_mouse_over_netgame_enemy(sspr, p_owntng);
        }
    }
}

ubyte lvdraw_fill_bound_points(struct TbPoint *bound_pts)
{
    int fctr_x1, fctr_z1, fctr_x2, fctr_z2;
    int mag_x, mag_z;
    ubyte slt, slt_zmin;
    ushort angXZ;
    int sin_angl, cos_angl;

    angXZ = (engn_cam_yaw >> 5) & 0x7FF;
    sin_angl = lbSinTable[angXZ];
    cos_angl = lbSinTable[angXZ + LbFPMath_PI/2];

    fctr_x1 = (dword_152E58 * render_area_a * cos_angl) >> 16;
    fctr_z2 = (dword_152E58 * render_area_a * -sin_angl) >> 16;
    fctr_z1 = (render_area_b * cos_angl) >> 8;
    fctr_x2 = (render_area_b * sin_angl) >> 8;
    mag_z = engn_zc & 0xFF00;
    mag_x = engn_xc & 0xFF00;

    bound_pts[0].y = (mag_z + ((dword_152E50 * (fctr_z1 + fctr_z2)) >> 8)) >> 8;
    bound_pts[1].y = (mag_z + ((dword_152E54 * (fctr_z2 - fctr_z1)) >> 8)) >> 8;
    bound_pts[1].x = (mag_x + ((dword_152E54 * (fctr_x1 - fctr_x2)) >> 8)) >> 8;
    bound_pts[2].x = (mag_x + ((dword_152E54 * (-fctr_x1 - fctr_x2)) >> 8)) >> 8;
    bound_pts[0].x = (mag_x + ((dword_152E50 * (fctr_x2 + fctr_x1)) >> 8)) >> 8;
    bound_pts[2].y = (mag_z + ((dword_152E54 * (-fctr_z2 - fctr_z1)) >> 8)) >> 8;
    bound_pts[3].y = (mag_z + ((dword_152E50 * (fctr_z1 - fctr_z2)) >> 8)) >> 8;
    bound_pts[3].x = (mag_x + ((dword_152E50 * (fctr_x2 - fctr_x1)) >> 8)) >> 8;

    slt_zmin = 0;
    for (slt = 1; slt < 4; slt++) {
        if (bound_pts[slt].y < bound_pts[slt_zmin].y)
            slt_zmin = slt;
    }

    return slt_zmin;
}

int lvdraw_fill_ranges_x(int slt_zmin, struct Range *ranges_x, struct TbPoint *bound_pts)
{
    int rn, ranges_used;
    int slt1_fcx, slt2_fcx;
    int slt1_incx, slt2_incx;
    int z_cur, z_max;
    ubyte slt, slt1, slt2;

    z_max = bound_pts[0].y;
    for (slt = 1; slt < 4; slt++) {
        if (z_max < bound_pts[slt].y)
            z_max = bound_pts[slt].y;
    }

    slt2 = slt_zmin;
    slt1 = slt_zmin;
    slt1_fcx = slt2_fcx = 0;
    slt1_incx = slt2_incx = 0;
    z_cur = bound_pts[slt_zmin].y;
    rn = 0;
    while (z_cur < z_max)
    {
        if (z_cur == bound_pts[slt1].y)
        {
            int dt_z;
            int base_fcx;
            int x_cur;

            base_fcx = 0;
            while (z_cur == bound_pts[slt1].y)
            {
              int x_tmp;
              x_tmp = bound_pts[slt1].x;
              slt1 = (slt1 + 1) & 3;
              base_fcx = x_tmp << 16;
            }
            x_cur = bound_pts[slt1].x;
            dt_z = bound_pts[slt1].y - z_cur;
            slt1_incx = ((x_cur << 16) - base_fcx) / dt_z;
            slt1_fcx = slt1_fcx + 0x10000;
        }

        if (z_cur == bound_pts[slt2].y)
        {
            int dt_z;
            int base_fcx;
            int x_cur;

            base_fcx = 0;
            while (z_cur == bound_pts[slt2].y)
            {
              int x_tmp;
              x_tmp = bound_pts[slt2].x;
              slt2 = (slt2 - 1) & 3;
              base_fcx = x_tmp << 16;
            }
            x_cur = bound_pts[slt2].x;
            dt_z = bound_pts[slt2].y - z_cur;
            slt2_incx = ((x_cur << 16) - base_fcx) / dt_z;
            slt2_fcx = base_fcx + 0x10000;
        }

        ranges_x[rn + 1].beg = slt1_fcx >> 16 << 8;
        ranges_x[rn + 1].fin = slt2_fcx >> 16 << 8;

        z_cur++;
        slt2_fcx += slt2_incx;
        slt1_fcx += slt1_incx;
        rn++;
    }

    ranges_used = rn;
    for (rn = 0; rn < ranges_used; rn++)
    {
        if (ranges_x[rn + 1].beg < 0)
            ranges_x[rn + 1].beg = 0;
        if (ranges_x[rn + 1].fin > 0x7F00)
            ranges_x[rn + 1].fin = 0x7F00;
    }

    return ranges_used;
}

void filter_ranges_max_of_two(int ranges_len, struct Range *ranges_flt, struct Range *ranges_inp)
{
    int rn;

    ranges_flt[0].beg = ranges_inp[1].beg;
    ranges_flt[0].fin = ranges_inp[1].fin + 256;

    for (rn = 1; rn < ranges_len; rn++)
    {
        int x1, x2;

        x1 = ranges_inp[rn].beg;
        if (x1 < ranges_inp[rn + 1].beg)
            x1 = ranges_inp[rn + 1].beg;
        ranges_flt[rn].beg = x1;

        x2 = ranges_inp[rn].fin + 256;
        if (x2 < ranges_inp[rn + 1].fin + 256)
            x2 = ranges_inp[rn + 1].fin + 256;
        ranges_flt[rn].fin = x2;
    }

    ranges_flt[rn].beg = ranges_flt[rn - 1].beg;
    ranges_flt[rn].fin = ranges_flt[rn - 1].fin;
}

void lvdraw_do_objects(int cor_z_beg, uint ranges_x_len, struct Range *ranges_x)
{
    int cor_z;
    int cor_x, cor_x_end;
    uint rn;

    cor_z = cor_z_beg;
    for (rn = 0; rn < ranges_x_len; rn++)
    {
        cor_x = ranges_x[rn + 1].beg;
        cor_x_end = ranges_x[rn + 1].fin;
        for (; cor_x <= cor_x_end; cor_x += TILE_TO_MAPCOORD(1,0))
        {
            struct Thing *p_objtng;
            struct MyMapElement *p_mapel;
            ThingIdx objtng;
            short tile_x, tile_z;

            if ((cor_x <= 0) || (cor_x >= MAP_COORD_WIDTH))
                continue;

            if ((cor_z <= 0) || (cor_z >= MAP_COORD_HEIGHT))
                continue;

            tile_z = MAPCOORD_TO_TILE(cor_z);
            tile_x = MAPCOORD_TO_TILE(cor_x);
            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            objtng = game_col_vects_list[p_mapel->ColHead].Object;
            if (objtng > 0)
            {
                p_objtng = &things[objtng];
                if (p_objtng->U.UObject.DrawTurn != drawturn)
                    draw_thing_object(p_objtng);
            }
        }
        cor_z += TILE_TO_MAPCOORD(1,0);
    }
}

void engine_draw_things(int pos_beg_x, int pos_beg_z, int rend_beg_x, int rend_beg_z, short tlcount_x, short tlcount_z)
{
    int tlno_x, pos_x;
    int view_end_x, view_beg_z;
    int view_beg_x, view_end_z;

    view_end_x = rend_beg_x + 512;
    view_beg_z = rend_beg_z - 512;
    view_beg_x = rend_beg_x - ((render_area_a << 8) + 512);
    view_end_z = rend_beg_z + ((render_area_b << 8) + 512);

    for (tlno_x = 0, pos_x = pos_beg_x; tlno_x < tlcount_x; tlno_x++, pos_x += -256)
    {
        int tlno_z, pos_z;

        for (tlno_z = 0, pos_z = pos_beg_z; tlno_z < tlcount_z; tlno_z++, pos_z += 256)
        {
            struct MyMapElement *p_mapel;

            if (pos_x <= TILE_TO_MAPCOORD(0,0) || pos_x >= MAP_COORD_WIDTH)
                continue;
            if (pos_z <= TILE_TO_MAPCOORD(0,0) || pos_z >= MAP_COORD_HEIGHT)
                continue;

            p_mapel = &game_my_big_map[MAPCOORD_TO_TILE(pos_x) + MAPCOORD_TO_TILE(pos_z) * MAP_TILE_WIDTH];

            if (pos_x >= view_beg_x && pos_x <= view_end_x && pos_z >= view_beg_z && pos_z <= view_end_z)
            {
                ThingIdx thing;
                ushort lv;

                lv = p_mapel->ColHead;
                if (lv != 0)
                {
                    thing = game_col_vects_list[lv].Object;
                    if (thing > 0)
                    {
                        struct Thing *p_thing;
                        p_thing = &things[thing];
                        if ((p_thing->Type == TT_BUILDING)
                         && (p_thing->U.UObject.DrawTurn != drawturn)) {
                            draw_thing_object(p_thing);
                        }
                    }
                }
                thing = p_mapel->Child;
                while (thing != 0)
                {
                    if (thing > 0)
                    {
                        struct Thing *p_thing;
                        p_thing = &things[thing];
                        thing = draw_thing_object(p_thing);
                        continue;
                    }
                    else
                    {
                        struct SimpleThing *p_sthing;
                        p_sthing = &sthings[thing];
                        thing = draw_sthing_object(p_sthing);
                        continue;
                    }
                }
            }
            else
            {
                ThingIdx thing;
                ushort lv;

                lv = p_mapel->ColHead;
                if (lv != 0)
                {
                    thing = game_col_vects_list[lv].Object;
                    if (thing > 0)
                    {
                        struct Thing *p_thing;
                        p_thing = &things[thing];
                        if ((p_thing->Type == TT_BUILDING)
                          && (p_thing->U.UObject.DrawTurn != drawturn)
                          && (p_thing->U.UObject.BHeight > 1400)) {
                            draw_thing_object(p_thing);
                        }
                    }
                }
                thing = p_mapel->Child;
                while (thing != 0)
                {
                    if (thing > 0)
                    {
                        struct Thing *p_thing;
                        p_thing = &things[thing];
                        if ( p_thing->Type == TT_BUILDING
                          && (p_thing->U.UObject.DrawTurn != drawturn)
                          && (p_thing->U.UObject.BHeight > 1400)) {
                            thing = draw_thing_object(p_thing);
                            continue;
                        }
                        thing = p_thing->Next;
                    }
                    else
                    {
                        struct SimpleThing *p_sthing;
                        p_sthing = &sthings[thing];
                        thing = p_sthing->Next;
                    }
                }
            }
        }
    }
}

void fill_floor_tile_pos_and_shade(struct FloorTile *p_floortl, struct MyMapElement *p_mapel,
  ubyte pt, short *p_sqlight, struct ShEnginePoint *p_sp)
{
    p_floortl->X[pt] = p_sp->X;
    p_floortl->Y[pt] = p_sp->Y;
    if (p_sp->Shade < 0) {
        short ambient;
        ambient = p_mapel->Ambient + p_sp->ReflShade + 2;
        p_sp->Shade = calculate_shpoint_shade(ambient, p_mapel->Shade, p_sqlight);
    }
    p_floortl->Shade[pt] = p_sp->Shade;
    p_mapel->ShadeR = p_sp->Shade >> 9;
}

void fill_floor_tile_pos_and_shade_fading(struct FloorTile *p_floortl, struct MyMapElement *p_mapel,
  struct ShEnginePoint *p_dsp, ubyte pt, struct ShEnginePoint *p_ssp)
{
    p_floortl->X[pt] = p_dsp->X;
    p_floortl->Y[pt] = p_dsp->Y;
    if (p_dsp->Shade < 0) {
        short ambient;
        //TODO why do we use p_ssp->ReflShade instead of using only one ShEnginePoint (the p_dsp)?
        // is ReflShade unset in the other ShEnginePoint?
        ambient = p_mapel->Ambient + p_ssp->ReflShade + 2;
        p_dsp->Shade = calculate_shpoint_shade_fading(ambient, p_mapel->Shade, p_dsp->Depth);
    }
    p_floortl->Shade[pt] = p_dsp->Shade;
    p_mapel->ShadeR = p_dsp->Shade >> 9;
}

struct MyMapElement *map_element_for_clipped_cords(int map_x, int map_z)
{
    if (map_x < 0)
        map_x = 0;
    else if (map_x >= MAP_COORD_WIDTH)
        map_x = MAP_COORD_WIDTH - 1;

    if (map_z < 0)
        map_z = 0;
    else if (map_z >= MAP_COORD_HEIGHT)
        map_z = MAP_COORD_HEIGHT - 1;

    return &game_my_big_map[MAP_TILE_WIDTH * MAPCOORD_TO_TILE(map_z) + MAPCOORD_TO_TILE(map_x)];
}

void lvdraw_do_floor(void)
{
#if 0
    asm volatile ("call ASM_lvdraw_do_floor\n"
        :  :  : "eax" );
    return;
#endif
    struct ShEnginePoint loc_unknarrD[(RENDER_AREA_MAX+1)*4];
    struct MyMapElement loc_mapel;
    int shift_a, shift_b;
    int elcr_z, elpv_z; // Coord Z for current and previous map element
    short *p_sqlight;

    drwfloor_start_cor_x = (engn_xc & 0xFF00) - (render_area_a << 7);
    drwfloor_start_cor_z = (engn_zc & 0xFF00) - (render_area_b << 7);
    if (drwfloor_start_cor_z < 0)
        drwfloor_start_cor_z = 0;

    LbMemorySet(&loc_mapel, '\0', sizeof(loc_mapel));

    elcr_z = drwfloor_start_cor_z;
    shift_b = 0;
    { // Separate first row from the rest as it has no previous
        struct MyMapElement *p_mapel;
        struct ShEnginePoint *p_spcr;
        int elcr_x;

        if (elcr_z >= MAP_COORD_HEIGHT)
            elcr_z = MAP_COORD_HEIGHT - 1;

        p_sqlight = &super_quick_light[shift_b * render_area_a];
        p_spcr = &loc_unknarrD[(shift_b) & 1];
        shift_a = 0;
        elcr_x = drwfloor_start_cor_x;
        while (shift_a < render_area_a + 1)
        {
            int elcr_y;
            short ambient;

            p_mapel = map_element_for_clipped_cords(elcr_x, elcr_z);
            elcr_y = shpoint_compute_coord_y(p_spcr, p_mapel, elcr_x, elcr_z, 4);
            transform_shpoint(p_spcr, elcr_x - engn_xc, elcr_y - 8 * engn_yc, elcr_z - engn_zc);

            ambient = p_mapel->Ambient + p_spcr->ReflShade + 2;
            p_spcr->Shade = calculate_shpoint_shade(ambient, p_mapel->Shade, p_sqlight);

            p_sqlight++;
            p_spcr += 2;
            shift_a++;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }
        shift_b++;
        elpv_z = elcr_z;
        elcr_z += TILE_TO_MAPCOORD(1, 0);
    }

    while (shift_b < render_area_b && elcr_z <= MAP_COORD_HEIGHT)
    {
        struct MyMapElement *p_mapel;
        struct ShEnginePoint *p_spcr;
        struct ShEnginePoint *p_spnx;
        int elcr_x, elpv_x;

        if (elcr_z >= MAP_COORD_HEIGHT)
            elcr_z = MAP_COORD_HEIGHT - 1;

        p_spcr = &loc_unknarrD[(shift_b) & 1];
        shift_a = 0;
        elcr_x = drwfloor_start_cor_x;
        while (shift_a < render_area_a + 1)
        {
            int elcr_y;

            p_mapel = map_element_for_clipped_cords(elcr_x, elcr_z);
            elcr_y = shpoint_compute_coord_y(p_spcr, p_mapel, elcr_x, elcr_z, 4);
            transform_shpoint(p_spcr, elcr_x - engn_xc, elcr_y - 8 * engn_yc, elcr_z - engn_zc);
            p_spcr->Shade = -1;

            p_spcr += 2;
            shift_a++;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }

        p_sqlight = &super_quick_light[shift_b * render_area_a];
        p_spnx = &loc_unknarrD[(shift_b + 1) & 1];
        p_spcr = &loc_unknarrD[(shift_b) & 1];
        shift_a = 0;
        elpv_x = drwfloor_start_cor_x;
        if (elpv_x >= MAP_COORD_WIDTH)
            elpv_x = MAP_COORD_WIDTH - 1;
        elcr_x = drwfloor_start_cor_x;
        while (shift_a < render_area_a)
        {
            struct FloorTile *p_floortl;
            struct MyMapElement *p_mapel_p10, *p_mapel_p01, *p_mapel_p11;
            int depth, dpthalt;
            ushort floor_flags2;
            ubyte ditype;

            dpthalt = 0;

            if (elcr_x >= MAP_COORD_WIDTH) {
                elcr_x = MAP_COORD_WIDTH - 1;
                if (elcr_x == elpv_x) // Do not continue rendering last row to end of screen
                    break;
            }
            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (elpv_z >> 8) + (elcr_x >> 8)];

            if ( (((p_spcr[2].Flags | p_spnx[2].Flags | p_spcr[0].Flags | p_spnx[0].Flags) & 0x20) != 0)
              || (((p_spnx[2].Flags & p_spcr[0].Flags & p_spnx[0].Flags & p_spcr[2].Flags) & 0x0F) != 0)
              || (elcr_x < 0) || (elcr_z < 0)
              || ((game_perspective != ProjM_IsomNoBuildng) && ((p_mapel->Flags & 0x80) != 0)))
            {
                p_sqlight++;
                p_spcr += 2;
                p_spnx += 2;
                shift_a++;
                elpv_x = elcr_x;
                elcr_x += TILE_TO_MAPCOORD(1, 0);
                continue;
            }

            depth = INT_MIN;

            if (depth < p_spnx[0].Depth)
                depth = p_spnx[0].Depth;
            if (depth < p_spnx[2].Depth)
                depth = p_spnx[2].Depth;
            if (depth < p_spcr[2].Depth)
                depth = p_spcr[2].Depth;
            if (depth < p_spcr[0].Depth)
                depth = p_spcr[0].Depth;

            floor_flags2 = 0;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (elpv_z >> 8) + (elcr_x >> 8)];
            if ((p_mapel->Texture & 0x8000) != 0)
            {
                floor_flags2 |= 0x01;
                if (byte_1C8444)
                {
                    int alt;
                    if (p_mapel->Alt <= 0)
                        alt = 15000 * overall_scale;
                    else
                        alt = 500 * overall_scale;
                    dpthalt = alt >> 8;
                }
                else
                {
                    if (p_mapel->Alt <= 0)
                        dpthalt = 3500;
                    else
                        dpthalt = 2500;
                }
            }
            dpthalt += 200;

            ditype = (p_mapel->Texture & 0x4000) != 0 ? DrIT_Unkn6 : DrIT_Unkn4;
            p_floortl = draw_item_add_floor_tile(ditype, BUCKET_MID + depth + dpthalt);
            if (p_floortl == NULL) {
                break;
            }

            p_mapel_p10 = p_mapel + 1;
            p_mapel_p01 = p_mapel + MAP_TILE_WIDTH;
            p_mapel_p11 = p_mapel + MAP_TILE_WIDTH + 1;
            if (elcr_z == MAP_COORD_HEIGHT - 1) {
                p_mapel_p01 = &loc_mapel;
                p_mapel_p11 = &loc_mapel;
            }
            if (elcr_x == MAP_COORD_WIDTH - 1) {
                p_mapel_p10 = &loc_mapel;
                p_mapel_p11 = &loc_mapel;
            }

            fill_floor_tile_pos_and_shade(p_floortl, p_mapel,     0, p_sqlight, p_spnx);

            p_spnx += 2;
            p_sqlight += 1;
            fill_floor_tile_pos_and_shade(p_floortl, p_mapel_p10, 1, p_sqlight, p_spnx);

            p_spcr += 2;
            p_sqlight += render_area_a;
            fill_floor_tile_pos_and_shade(p_floortl, p_mapel_p11, 2, p_sqlight, p_spcr);

            p_spcr -= 2;
            p_sqlight -= 1;
            fill_floor_tile_pos_and_shade(p_floortl, p_mapel_p01, 3, p_sqlight, p_spcr);
            p_sqlight -= render_area_a;

            if (p_mapel->Texture != 0)
            {
                struct SingleFloorTexture *p_fltextr;
                p_fltextr = &game_textures[p_mapel->Texture & 0x3FFF];
                p_floortl->Texture = p_fltextr;
                if ((p_mapel->Flags & 0x20) != 0)
                    p_floortl->Flags = RendVec_mode21;
                else
                    p_floortl->Flags = RendVec_mode05;
            }
            else
            {
                p_floortl->Flags = RendVec_mode04;
                p_floortl->Col = colour_grey2;
            }

            if ((p_mapel->Flags & 0x01) != 0)
            {
                p_floortl->Shade[0] = 0x3F00;
                p_floortl->Shade[1] = 0x3F00;
                p_floortl->Shade[2] = 0x3F00;
                p_floortl->Shade[3] = 0x3F00;
            }
            if ((p_mapel->Flags & 0x08) != 0)
                floor_flags2 |= 0x02;

            p_floortl->Flags2 = p_mapel->Flags | floor_flags2;
            p_floortl->Flags2b = p_mapel->Flags2;
            p_floortl->Offset = p_mapel - game_my_big_map;
            p_floortl->Page = p_mapel->ColumnHead >> 12;

            p_sqlight++;
            // p_spnx += 2; -- already done earlier
            p_spcr += 2;
            shift_a++;
            elpv_x = elcr_x;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }
        shift_b++;
        elpv_z = elcr_z;
        elcr_z += TILE_TO_MAPCOORD(1, 0);
    }
}

void lvdraw_do_floor_flyby(int cor_z_beg, int ranges_x_len, struct Range *smrang_x, struct Range *ranges_x)
{
    struct ShEnginePoint loc_unknarrD[(RENDER_AREA_MAX+1)*4];
    struct MyMapElement loc_mapel;
    int elcr_z, elpv_z; // Coord Z for current and previous map element
    int rn;

    drwfloor_start_cor_x = (engn_xc & 0xFF00) - (render_area_a << 7);
    drwfloor_start_cor_z = (engn_zc & 0xFF00) - (render_area_b << 7);

    LbMemorySet(&loc_mapel, '\0', sizeof(loc_mapel));

    elcr_z = cor_z_beg;
    elpv_z = cor_z_beg - TILE_TO_MAPCOORD(1, 0);

    rn = 0;
    { // Separate first row from the rest as it has no previous
        struct MyMapElement *p_mapel;
        struct ShEnginePoint *p_spcr;
        int elcr_x;

        elcr_x = smrang_x[rn].beg;
        p_spcr = &loc_unknarrD[2 * (elcr_x >> 8) + ((rn) & 1)];
        while (elcr_x <= smrang_x[rn].fin)
        {
            int elcr_y;
            short ambient;

            p_mapel = map_element_for_clipped_cords(elcr_x, elcr_z);
            elcr_y = shpoint_compute_coord_y(p_spcr, p_mapel, elcr_x, elcr_z, 8);
            transform_shpoint_fpv(p_spcr, elcr_x - engn_xc, elcr_y - 8 * engn_yc, elcr_z - engn_zc);
            ambient = p_mapel->Ambient + p_spcr->ReflShade + 2;
            p_spcr->Shade = calculate_shpoint_shade_fading(ambient, p_mapel->Shade, p_spcr->Depth);

            p_spcr += 2;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }
    }

    for (rn = 1; rn < ranges_x_len; rn++)
    {
        struct MyMapElement *p_mapel;
        struct ShEnginePoint *p_spcr;
        struct ShEnginePoint *p_spnx;
        int elcr_x;

        elcr_x = smrang_x[rn].beg;
        p_spcr = &loc_unknarrD[2 * (elcr_x >> 8) + (rn & 1)];
        while (elcr_x <= smrang_x[rn].fin)
        {
            int elcr_y;

            p_mapel = map_element_for_clipped_cords(elcr_x, elcr_z);
            elcr_y = shpoint_compute_coord_y(p_spcr, p_mapel, elcr_x, elcr_z, 8);
            transform_shpoint_fpv(p_spcr, elcr_x - engn_xc, elcr_y - 8 * engn_yc, elcr_z - engn_zc);
            p_spcr->Shade = -1;

            p_spcr += 2;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }

        elcr_x = ranges_x[rn].beg;
        p_spnx = &loc_unknarrD[2 * (elcr_x >> 8) + ((rn + 1) & 1)];
        p_spcr = &loc_unknarrD[2 * (elcr_x >> 8) + ((rn) & 1)];
        while (elcr_x <= ranges_x[rn].fin)
        {
            struct FloorTile *p_floortl;
            struct MyMapElement *p_mapel;
            struct MyMapElement *p_mapel_p10, *p_mapel_p01, *p_mapel_p11;
            struct ShEnginePoint *p_spad;
            int depth, dpthalt;
            ushort floor_flags2;
            ubyte ditype;

            dpthalt = 0;
            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (elpv_z >> 8) + (elcr_x >> 8)];

            if ( (((p_spcr[2].Flags | p_spnx[2].Flags | p_spcr[0].Flags | p_spnx[0].Flags) & 0x20) != 0)
              || (((p_spnx[2].Flags & p_spcr[0].Flags & p_spnx[0].Flags & p_spcr[2].Flags) & 0x0F) != 0)
              || (elcr_x <= 0) || (elcr_x >= 0x8000) || (elcr_z <= 0) || (elcr_z >= 0x8000))
            {
                p_spcr += 2;
                p_spnx += 2;
                elcr_x += TILE_TO_MAPCOORD(1, 0);
                continue;
            }

            depth = INT_MIN;

            if (depth < p_spnx[0].Depth)
                depth = p_spnx[0].Depth;
            if (depth < p_spnx[2].Depth)
                depth = p_spnx[2].Depth;
            if (depth < p_spcr[2].Depth)
                depth = p_spcr[2].Depth;
            if (depth < p_spcr[0].Depth)
                depth = p_spcr[0].Depth;

            floor_flags2 = 0;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (elpv_z >> 8) + (elcr_x >> 8)];
            if ((p_mapel->Texture & 0x8000) != 0)
            {
                floor_flags2 |= 0x01;
                if (byte_1C8444)
                {
                    int alt;
                    if (p_mapel->Alt <= 0)
                        alt = 15000 * overall_scale;
                    else
                        alt = 500 * overall_scale;
                    dpthalt = alt >> 8;
                }
                else
                {
                    if (p_mapel->Alt <= 0)
                        dpthalt = 2000;
                    else
                        dpthalt = 1000;
                }
            }

            ditype = (p_mapel->Texture & 0x4000) != 0 ? DrIT_Unkn6 : DrIT_Unkn4;
            p_floortl = draw_item_add_floor_tile(ditype, BUCKET_MID + depth + dpthalt);
            if (p_floortl == NULL) {
                break;
            }

            p_mapel_p10 = p_mapel + 1;
            p_mapel_p01 = p_mapel + MAP_TILE_WIDTH;
            p_mapel_p11 = p_mapel + MAP_TILE_WIDTH + 1;
            if (elcr_z == MAP_COORD_HEIGHT - 1) {
                p_mapel_p01 = &loc_mapel;
                p_mapel_p11 = &loc_mapel;
            }
            if (elcr_x == MAP_COORD_WIDTH - 1) {
                p_mapel_p10 = &loc_mapel;
                p_mapel_p11 = &loc_mapel;
            }

            fill_floor_tile_pos_and_shade_fading(p_floortl, p_mapel, p_spnx, 0, p_spnx);

            p_spnx += 2;
            fill_floor_tile_pos_and_shade_fading(p_floortl, p_mapel_p10, p_spnx, 1, p_spnx);

            p_spad = p_spcr + 2;
            fill_floor_tile_pos_and_shade_fading(p_floortl, p_mapel_p11, p_spad, 2, p_spnx);

            p_spad = p_spcr;
            fill_floor_tile_pos_and_shade_fading(p_floortl, p_mapel_p01, p_spad, 3, p_spnx);

            if (p_mapel->Texture != 0)
            {
                struct SingleFloorTexture *p_fltextr;
                p_fltextr = &game_textures[p_mapel->Texture & 0x3FFF];
                p_floortl->Texture = p_fltextr;
                if ((p_mapel->Flags & 0x20) != 0)
                    p_floortl->Flags = RendVec_mode21;
                else
                    p_floortl->Flags = RendVec_mode05;
                p_floortl->Page = (ushort)p_mapel->ColumnHead >> 12;
            }
            else
            {
                p_floortl->Flags = RendVec_mode04;
                p_floortl->Col = colour_grey2;
            }

            if ((p_mapel->Flags & 0x01) != 0)
            {
                p_floortl->Shade[0] = 0x3F00;
                p_floortl->Shade[1] = 0x3F00;
                p_floortl->Shade[2] = 0x3F00;
                p_floortl->Shade[3] = 0x3F00;
            }
            if ((p_mapel->Flags & 0x08) != 0)
                floor_flags2 |= 0x02;

            p_floortl->Flags2 = p_mapel->Flags | floor_flags2;
            p_floortl->Flags2b = p_mapel->Flags2;
            p_floortl->Offset = p_mapel - game_my_big_map;

            p_spcr = p_spad + 2;
            elcr_x += TILE_TO_MAPCOORD(1, 0);
        }
        elpv_z += TILE_TO_MAPCOORD(1, 0);
        elcr_z += TILE_TO_MAPCOORD(1, 0);
    }
}

#define SUPER_QUICK_RADIUS 5
void apply_super_quick_light(short lx, short lz, ushort b)
{
    short tile_x_beg, tile_x_end;
    short tile_z_beg, tile_z_end;
    short tile_x, tile_z;
    int mapcor_x, mapcor_z;
    short sqlight_start_cor_x, sqlight_start_cor_z;
    short ratile_x_beg, ratile_z_beg;
    short ratile_x, ratile_z;

    tile_z_beg = MAPCOORD_TO_TILE(lz) - SUPER_QUICK_RADIUS/2;
    tile_x_beg = MAPCOORD_TO_TILE(lx) - SUPER_QUICK_RADIUS/2;
    if (tile_z_beg <= -SUPER_QUICK_RADIUS || tile_z_beg >= MAP_TILE_HEIGHT)
        return;
    if (tile_x_beg <= -SUPER_QUICK_RADIUS || tile_x_beg >= MAP_TILE_WIDTH)
        return;

    mapcor_z = TILE_TO_MAPCOORD(render_area_b,0) / 2;
    mapcor_x = TILE_TO_MAPCOORD(render_area_a,0) / 2;
    if ((lz <= engn_zc - mapcor_z) || (lz >= engn_zc + mapcor_z))
        return;
    if ((lx <= engn_xc - mapcor_x) || (lx >= engn_xc + mapcor_x))
        return;

    // Calculate start in the same way as floor drawing function does it
    sqlight_start_cor_x = (engn_xc & 0xFF00) - (render_area_a << 7);
    sqlight_start_cor_z = (engn_zc & 0xFF00) - (render_area_b << 7);
    if (sqlight_start_cor_z < 0)
        sqlight_start_cor_z = 0;


    ratile_z_beg = tile_z_beg - MAPCOORD_TO_TILE(sqlight_start_cor_z) + 1;
    ratile_x_beg = tile_x_beg - MAPCOORD_TO_TILE(sqlight_start_cor_x);
    tile_x_end = tile_x_beg + SUPER_QUICK_RADIUS;
    tile_z_end = tile_z_beg + SUPER_QUICK_RADIUS;

    for (tile_z = tile_z_beg, ratile_z = ratile_z_beg; tile_z < tile_z_end; tile_z++, ratile_z++)
    {
        short *p_sqlight;

        p_sqlight = &super_quick_light[render_area_a * ratile_z + ratile_x_beg];
        mapcor_z = TILE_TO_MAPCOORD(tile_z,0);
        for (tile_x = tile_x_beg, ratile_x = ratile_x_beg; tile_x < tile_x_end; tile_x++, ratile_x++)
        {
            int f, dist;
            short intensity;

            mapcor_x = TILE_TO_MAPCOORD(tile_x,0);

            if (ratile_x < 0 || ratile_x >= render_area_a)
                continue;
            if (ratile_z < 0 || ratile_z >= render_area_b)
                continue;
            if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH)
                continue;
            if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT)
                continue;

            dist = (mapcor_x - lx) * (mapcor_x - lx)
                + (mapcor_z - lz) * (mapcor_z - lz);
            if (dist > 0)
              f = 1088608 / dist;
            else
              f = 32;
            intensity = b * f >> 5;
            if (intensity > 32)
                intensity = 32;
            *p_sqlight += intensity;
            p_sqlight++;
        }
    }
}

void clear_super_quick_lights(void)
{
    int i;

    for (i = 0; i < render_area_a * (render_area_b + 1); i++)
    {
        short *p_sqlight;
        p_sqlight = &super_quick_light[i];
        *p_sqlight = 0;
    }
}

void prepare_drawlist(void)
{
    reset_drawlist_stats();
    transform_reinit_vec_window();
    transform_reinit_camera();
}

void draw_screen(void)
{
    if (nuclear_overexposure)
    {
        draw_drawlist_1();
    }
    else
    {
        draw_drawlist_2();
    }

    reset_drawlist();
    ingame.NextRocket = 0;
}

void engine_check_draw_billboard_frame(void)
{
    if ((ingame.Flags & GamF_BillboardBAT) != 0)
    {
        // Curently BAT has no separation of input and draw, so no action is needed here
    }
    else if ((ingame.Flags & GamF_BillboardMovies) != 0)
    {
        dword_176CBC += fifties_per_gameturn;
        if (dword_176CBC > 80)
        {
            dword_176CBC = 0;
            if (!in_network_game && ((ingame.Flags & GamF_Unkn00040000) != 0))
            {
                ingame.Flags &= ~GamF_Unkn00040000;
                embanim_do_next_frame(AniSl_BILLBOARD);
            }
        }
    }
}

void engine_draw_whole_screen_flyby(void)
{
    ubyte slt_zmin;

    struct Range smrang_x[160];
    struct Range ranges_x[160];
    struct TbPoint bound_pts[4];
    int cor_z_beg, ranges_x_len;

    reset_drawlist();
    ingame.NextRocket = 0;
    screen_position_face_render_cb = screen_position_face_render_null_callback;
    screen_sorted_sprite_statc_render_cb = screen_sorted_sprite_render_null_callback;
    screen_sorted_sprite_persn_render_cb = screen_sorted_sprite_render_null_callback;
    screen_sorted_sprite_persn_alt_cb = NULL;
    screen_sorted_sprite_effect_replace_cb = NULL;

    slt_zmin = lvdraw_fill_bound_points(bound_pts);

    cor_z_beg = bound_pts[slt_zmin].y << 8;

    ranges_x_len = lvdraw_fill_ranges_x(slt_zmin, ranges_x, bound_pts);

    filter_ranges_max_of_two(ranges_x_len, smrang_x, ranges_x);

    player_target_clear(local_player_no);

    engine_check_draw_billboard_frame();

    camera_setup_angle_fractions();

    lvdraw_do_objects(cor_z_beg, ranges_x_len, ranges_x);

    lvdraw_do_floor_flyby(cor_z_beg, ranges_x_len, smrang_x, ranges_x);

    assert(vec_tmap[1] != NULL);
    vec_map = vec_tmap[1];
    face_transp_tinted_surface_col = deep_radar_surface_col;
    face_transp_tinted_line_col = deep_radar_line_col;

    draw_screen();
}

/** Collects the lights near the camera, for the GPU to add their light to the pixels around them: the
 * lights placed on the map (lamp posts and so on), and the short lived ones (explosions, flashes).
 * Each is put on the screen where it touches the ground, so that the light also falls on what stands
 * near it on the screen, such as walls of buildings. */
static void hw_gather_pixel_lights(void)
{
    float lmat[4], lights[HW_PLIGHTS_MAX * 4], dists[HW_PLIGHTS_MAX];
    float scr_w, scr_h, scr_cx, scr_cy, radius, reach_x, reach_y;
    int dx00, dz00, dx10, dz10, dx01, dz01;
    int offs_y, n, i;
    float ax, bx, az, bz, det;
    static float radius_s = -1.0f;

    if (!LbHwPixelLightsEnabled())
        return;
    if (radius_s < 0.0f)
        radius_s = (getenv("SWFX_PL_RADIUS") != NULL) ? (float)atof(getenv("SWFX_PL_RADIUS")) : 700.0f;
    radius = radius_s;

    // How a distance on the screen is a distance on the ground; the same as for the mouse
    offs_y = (engn_yc * overall_scale) >> 8;
    transform_screen_to_map_isometric(&dx00, &dz00, 0, -offs_y);
    transform_screen_to_map_isometric(&dx10, &dz10, 512, -offs_y);
    transform_screen_to_map_isometric(&dx01, &dz01, 0, 512 - offs_y);
    ax = (dx10 - dx00) / 512.0f;
    bx = (dx01 - dx00) / 512.0f;
    az = (dz10 - dz00) / 512.0f;
    bz = (dz01 - dz00) / 512.0f;
    lmat[0] = ax; lmat[1] = bx; lmat[2] = az; lmat[3] = bz;
    det = ax * bz - bx * az;
    if (fabsf(det) < 1.0e-6f)
        return;
    // How far on the screen a light reaches (a ground circle is an ellipse on the screen)
    reach_x = radius * (fabsf(bz) + fabsf(bx)) / fabsf(det);
    reach_y = radius * (fabsf(az) + fabsf(ax)) / fabsf(det);
    reach_x *= 1.6f;
    reach_y *= 1.6f;

    scr_w = (float)lbDisplay.GraphicsScreenWidth;
    scr_h = (float)lbDisplay.GraphicsScreenHeight;
    scr_cx = scr_w * 0.5f;
    scr_cy = scr_h * 0.5f;

    n = 0;
    // The lights of the map; the ones nearest to the middle of the screen
    for (i = 1; i < next_full_light; i++)
    {
        struct FullLight *p_fl = &game_full_lights[i];
        struct ShEnginePoint sp;
        float sx, sy, d, inten;
        int k, alt;

        if (p_fl->Intensity <= 0)
            continue;
        alt = PRCCOORD_TO_YCOORD(alt_at_point(p_fl->X, p_fl->Z));
        transform_shpoint(&sp, p_fl->X - engn_xc, alt - 8 * engn_yc, p_fl->Z - engn_zc);
        sx = (float)sp.X;
        sy = (float)sp.Y;
        if ((sx < -reach_x) || (sx > scr_w + reach_x) || (sy < -reach_y) || (sy > scr_h + reach_y))
            continue;
        d = sqrtf((sx - scr_cx) * (sx - scr_cx) + (sy - scr_cy) * (sy - scr_cy));
        // A few lights are very strong, for effects; they would blind the neighbourhood
        inten = (float)((p_fl->Intensity > 96) ? 96 : p_fl->Intensity) / 64.0f;
        // keep the list sorted, the nearest to the middle of the screen first
        for (k = n; k > 0 && dists[k - 1] > d; k--) {
            if (k < HW_PLIGHTS_MAX) {
                memcpy(&lights[k * 4], &lights[(k - 1) * 4], 4 * sizeof(float));
                dists[k] = dists[k - 1];
            }
        }
        if (k < HW_PLIGHTS_MAX) {
            lights[k * 4 + 0] = sx;
            lights[k * 4 + 1] = sy;
            lights[k * 4 + 2] = radius;
            lights[k * 4 + 3] = inten;
            dists[k] = d;
            if (n < HW_PLIGHTS_MAX)
                n++;
        }
    }
    // Short lived lights, such as explosions, are a bit wider and always kept
    {
        float dyn[HW_PLIGHTS_MAX * 3];
        int nd;

        nd = LbHwGetDynamicLights((long)gameturn, dyn, 8);
        for (i = 0; i < nd; i++)
        {
            struct ShEnginePoint sp;
            int slot, alt;

            alt = PRCCOORD_TO_YCOORD(alt_at_point((int)dyn[i * 3 + 0], (int)dyn[i * 3 + 1]));
            transform_shpoint(&sp, (int)dyn[i * 3 + 0] - engn_xc, alt - 8 * engn_yc,
              (int)dyn[i * 3 + 1] - engn_zc);
            slot = (n < HW_PLIGHTS_MAX) ? n++ : HW_PLIGHTS_MAX - 1;
            lights[slot * 4 + 0] = (float)sp.X;
            lights[slot * 4 + 1] = (float)sp.Y;
            lights[slot * 4 + 2] = radius * 1.5f;
            lights[slot * 4 + 3] = dyn[i * 3 + 2] / 64.0f;
        }
    }
    if (getenv("SWFX_PL_LOG") != NULL) {
        static int logged = 0;

        if ((logged++ % 100) == 0)
            LOGERR("pixel lights: %d on the screen of %d on the map, first at %d,%d inten %d", n,
              (int)next_full_light, (int)lights[0], (int)lights[1], (int)(lights[3] * 64.0f));
    }
    if (getenv("SWFX_CONE_TEST") != NULL) {
        // Eight cones, in the eight directions the people can face, in a ring around the middle
        for (i = 0; i < 8; i++) {
            struct ShEnginePoint tsp;
            float tx = (float)lbDisplay.GraphicsScreenWidth * 0.5f + 420.0f * ((i % 4) - 1.5f) * 0.9f;
            float ty = (float)lbDisplay.GraphicsScreenHeight * 0.5f + ((i < 4) ? -170.0f : 200.0f);
            (void)tsp;
            LbHwNoteMuzzle(tx, ty, colour_lookup[ColLU_YELLOW], 0.9f, 500.0f,
              (float)angle_direction[i].DiX / 256.0f, (float)angle_direction[i].DiY / 256.0f);
        }
    }
    if (getenv("SWFX_FLAME_TEST") != NULL) {
        // A few flames in the middle of the screen, for looking at them
        for (i = 0; i < 9; i++) {
            float fx = (float)lbDisplay.GraphicsScreenWidth * 0.5f + (float)((i % 3) - 1) * 70.0f + (float)(i / 3) * 12.0f;
            float fy = (float)lbDisplay.GraphicsScreenHeight * 0.5f + (float)(i / 3) * 40.0f;
            LbHwRecordFlame(fx - 35.0f, fy - 110.0f, fx + 35.0f, fy + 25.0f, i * 37, 1.0f);
        }
    }
    if (getenv("SWFX_BLOOD_TEST") != NULL) {
        // The pools of blood in the middle of the screen, a few sizes of them, for looking at them
        static const ushort pool_spr[8] = { 885, 888, 889, 890, 893, 899, 900, 886 };

        for (i = 0; i < 8; i++) {
            const struct TbSprite *p_spr = &m_sprites[pool_spr[i]];
            long px = lbDisplay.GraphicsScreenWidth / 2 + ((i % 4) - 2) * 150;
            long py = lbDisplay.GraphicsScreenHeight / 2 + (i / 4) * 130 - 60;

            LbHwRecordBloodPool(p_spr, px, py, px + p_spr->SWidth * 5, py + p_spr->SHeight * 5, pool_spr[i], 1.0f,
              (float)(i + 1) / 8.0f);
        }
    }
    if (getenv("SWFX_BEAM_TEST") != NULL) {
        // A fixed beam in the middle of the screen, for looking at what its light does
        LbHwNoteBeam(scr_cx - 150.0f, scr_cy + 40.0f, scr_cx + 250.0f, scr_cy - 120.0f, colour_lookup[ColLU_RED], 0);
    }
    LbHwSetPixelLights(lmat, lights, n);
}

void engine_draw_whole_screen_top_down(void)
{
    PlayerInfo *p_locplayer;

    reset_drawlist();
    ingame.NextRocket = 0;
    screen_position_face_render_cb = screen_position_face_render_callback;
    screen_sorted_sprite_statc_render_cb = screen_sorted_sprite_statc_render_callback;
    screen_sorted_sprite_persn_render_cb = screen_sorted_sprite_persn_render_callback;
    screen_sorted_sprite_persn_alt_cb = screen_sorted_sprite_persn_alt_callback;
    screen_sorted_sprite_effect_replace_cb = screen_sorted_sprite_effect_replace_callback;
    screen_blood_pool_growth_cb = screen_blood_pool_growth_callback;

    player_target_clear(local_player_no); // set during HUD redraw (should be separated!)

    engine_check_draw_billboard_frame();

    int rend_beg_x, rend_beg_z;
    int pos_beg_x, pos_beg_z;
    int tlcount_x, tlcount_z;

    camera_setup_view(&pos_beg_x, &pos_beg_z, &rend_beg_x, &rend_beg_z, &tlcount_x, &tlcount_z);

    if ((ingame.Flags & GamF_RenderScene) != 0)
    {
        engine_draw_things(pos_beg_x, pos_beg_z, rend_beg_x, rend_beg_z, tlcount_x, tlcount_z);
        blood_fx_enlist();
    }

    if ((ingame.Flags & GamF_RenderScene) != 0)
    {
        if ((gamep_scene_effect_type == ScEff_SPACE) && engine_render_lights)
            draw_background_stars();
        if (game_perspective == ProjM_IsomFloorStars) {
            draw_background_stars();
        } else {
            lvdraw_do_floor();
        }
    }

    if (word_1552F8 != 36 && !byte_1C8444)
    {
        clear_super_quick_lights();
    }
    assert(vec_tmap[1] != NULL);
    vec_map = vec_tmap[1];
    face_transp_tinted_surface_col = deep_radar_surface_col;
    face_transp_tinted_line_col = deep_radar_line_col;

    p_locplayer = &players[local_player_no];
    if ((ingame.Flags & GamF_RenderScene) != 0)
    {
        // With the GPU drawing the scene, the primitives are recorded now, and
        // drawn when the screen is presented
        lbHwRecording = LbHwRenderEnabled() && LbGlIsActive();
        draw_explode();
        draw_screen();
        if (lbHwRecording)
            hw_gather_pixel_lights();
        LbHwFlushSmoke();
        LbHwRecordingEnd();
        draw_hud(p_locplayer->DirectControl[0]);
        if (in_network_game)
            draw_netplayer_name_text_over_thing();
        if (debug_hud_collision)
            draw_engine_unk3_last(engn_xc, engn_zc);
    }
    else
    {
        draw_hud(p_locplayer->DirectControl[0]);
    }
}

/******************************************************************************/

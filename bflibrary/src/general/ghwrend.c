/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file ghwrend.c
 *     Recording of the 3D scene primitives for the hardware rendering.
 * @par Purpose:
 *     Keeps the triangles which the game asked the software renderer to draw,
 *     in the order of drawing, in a form ready to be sent to the GPU.
 * @par Comment:
 *     None.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bfhwrend.h"
#include "bfscreen.h"
#include "bfsprite.h"
#include "insspr.h"
#include "poly.h"
#include "privbflog.h"

volatile TbBool lbHwRecording = false;

/* On by default; it only has effect when the window got a GL context */
static TbBool hw_enabled = true;
static int hw_lighting = HwLight_Enhanced;

static int cust_count = 0;
static float cust_markers[HW_CUSTOM_MAX];
static float cust_data[HW_CUSTOM_MAX * 4];
static TbBool cust_layer = false;
static int overlay_colour = -1;

void LbHwCustomColoursSet(int count, const ubyte *markers, const ubyte *orig, const ubyte *rgb)
{
    int i;

    if (count > HW_CUSTOM_MAX)
        count = HW_CUSTOM_MAX;
    if (count < 0)
        count = 0;
    for (i = 0; i < count; i++)
    {
        cust_markers[i] = (float)markers[i];
        cust_data[i * 4 + 0] = rgb[i * 3 + 0] / 255.0f;
        cust_data[i * 4 + 1] = rgb[i * 3 + 1] / 255.0f;
        cust_data[i * 4 + 2] = rgb[i * 3 + 2] / 255.0f;
        cust_data[i * 4 + 3] = (float)orig[i];
    }
    cust_count = count;
}

int LbHwCustomColoursGet(const float **p_markers, const float **p_data)
{
    *p_markers = cust_markers;
    *p_data = cust_data;
    return cust_count;
}

void LbHwCustomLayerSet(TbBool on)
{
    cust_layer = on;
}

TbBool LbHwCustomLayer(void)
{
    return cust_layer;
}

static float hw_face_alpha = 1.0f;

void LbHwFaceAlphaSet(float alpha)
{
    hw_face_alpha = alpha;
}

static float ui_tint[HW_UI_TINT_PARTS * 4];
static float ui_alpha[HW_UI_TINT_PARTS] = { 1.0f, 1.0f };

void LbHwUiTintSet(int part, TbBool on, const ubyte *rgb, ubyte alpha)
{
    if ((part < 0) || (part >= HW_UI_TINT_PARTS))
        return;
    if (on && (rgb != NULL)) {
        ui_tint[part * 4 + 0] = rgb[0] / 255.0f;
        ui_tint[part * 4 + 1] = rgb[1] / 255.0f;
        ui_tint[part * 4 + 2] = rgb[2] / 255.0f;
    }
    ui_tint[part * 4 + 3] = on ? 1.0f : 0.0f;
    ui_alpha[part] = on ? (alpha / 255.0f) : 1.0f;
}

const float *LbHwUiAlphaGet(void)
{
    return ui_alpha;
}

const float *LbHwUiTintGet(void)
{
    return ui_tint;
}

void LbHwOverlaySet(int colour)
{
    overlay_colour = colour;
}

int LbHwOverlay(void)
{
    return overlay_colour;
}

static int smoke_count = 0;
static float pl_affine[4];
static float pl_lights[HW_PLIGHTS_MAX * 4];
static int pl_count = 0;
static TbBool pl_enabled = true;

#define HW_DYNLIGHT_MAX 32
static float dyn_lights[HW_DYNLIGHT_MAX * 3];
static int dyn_count = 0;
static long dyn_turn = -100;

void LbHwSetPixelLights(const float *aff, const float *lights, int count)
{
    if (count > HW_PLIGHTS_MAX)
        count = HW_PLIGHTS_MAX;
    if (count < 0)
        count = 0;
    memcpy(pl_affine, aff, sizeof(pl_affine));
    memcpy(pl_lights, lights, count * 4 * sizeof(float));
    pl_count = count;
}

int LbHwGetPixelLights(const float **p_aff, const float **p_lights)
{
    *p_aff = pl_affine;
    *p_lights = pl_lights;
    return pl_enabled ? pl_count : 0;
}

TbBool LbHwPixelLightsEnabled(void)
{
    return pl_enabled;
}

void LbHwPixelLightsSetEnabled(TbBool enabled)
{
    pl_enabled = enabled;
}

static float beam_a[HW_BEAMS_MAX * 4];
static float beam_b[HW_BEAMS_MAX * 4];
static float beam_c[HW_BEAMS_MAX * 4];
static int beam_count = 0;
static int arc_light_count = 0;

void LbHwNoteBeam(float x1, float y1, float x2, float y2, int colour, int itime)
{
    float strength;

    // The things are enlisted before the recording of the scene starts, so it is not checked
    if (beam_count >= HW_BEAMS_MAX)
        return;
    // The light which the beam throws on the ground is weaker the older the beam is; some beams have a
    // negative time, for special effects. A flash at the muzzle shows only in the first moments.
    strength = (itime < 0) ? 0.8f : (1.0f - 0.18f * (float)itime);
    if (strength < 0.3f)
        strength = 0.3f;
    beam_a[beam_count * 4 + 0] = x1;
    beam_a[beam_count * 4 + 1] = y1;
    beam_a[beam_count * 4 + 2] = x2;
    beam_a[beam_count * 4 + 3] = y2;
    beam_b[beam_count * 4 + 0] = (float)colour;
    beam_b[beam_count * 4 + 1] = strength * 1.1f;
    beam_b[beam_count * 4 + 2] = 400.0f;
    beam_b[beam_count * 4 + 3] = 0.0f;
    beam_c[beam_count * 4 + 0] = beam_c[beam_count * 4 + 1] = 0.0f;
    beam_c[beam_count * 4 + 2] = 0.0f;
    beam_count++;
}

void LbHwNoteArcLight(float x1, float y1, float x2, float y2, int colour)
{
    // The light goes all along the lightning, from end to end; there are many bolts in an electric
    // discharge, and their lights would pile up on the ground, so only a few of them throw light
    if ((beam_count >= HW_BEAMS_MAX) || (arc_light_count >= 5))
        return;
    arc_light_count++;
    beam_a[beam_count * 4 + 0] = x1;
    beam_a[beam_count * 4 + 1] = y1;
    beam_a[beam_count * 4 + 2] = x2;
    beam_a[beam_count * 4 + 3] = y2;
    beam_b[beam_count * 4 + 0] = (float)colour;
    beam_b[beam_count * 4 + 1] = 1.1f;
    beam_b[beam_count * 4 + 2] = 340.0f;
    beam_b[beam_count * 4 + 3] = 0.0f;
    beam_c[beam_count * 4 + 0] = beam_c[beam_count * 4 + 1] = 0.0f;
    beam_c[beam_count * 4 + 2] = 1.0f;
    beam_count++;
}

void LbHwNoteMuzzle(float sx, float sy, int colour, float strength, float radius, float dirx, float dirz)
{
    if (beam_count >= HW_BEAMS_MAX)
        return;
    beam_a[beam_count * 4 + 0] = sx;
    beam_a[beam_count * 4 + 1] = sy;
    beam_a[beam_count * 4 + 2] = sx;
    beam_a[beam_count * 4 + 3] = sy;
    beam_b[beam_count * 4 + 0] = (float)colour;
    beam_b[beam_count * 4 + 1] = 0.0f;
    beam_b[beam_count * 4 + 2] = radius;
    beam_b[beam_count * 4 + 3] = strength;
    beam_c[beam_count * 4 + 0] = dirx;
    beam_c[beam_count * 4 + 1] = dirz;
    beam_c[beam_count * 4 + 2] = 0.0f;
    beam_count++;
}

int LbHwGetBeams(const float **p_a, const float **p_b, const float **p_c)
{
    *p_a = beam_a;
    *p_b = beam_b;
    *p_c = beam_c;
    return pl_enabled ? beam_count : 0;
}

void LbHwNoteDynamicLight(long turn, float x, float z, float intensity)
{
    if (turn != dyn_turn) {
        dyn_turn = turn;
        dyn_count = 0;
    }
    if (dyn_count >= HW_DYNLIGHT_MAX)
        return;
    dyn_lights[dyn_count * 3 + 0] = x;
    dyn_lights[dyn_count * 3 + 1] = z;
    dyn_lights[dyn_count * 3 + 2] = intensity;
    dyn_count++;
}

int LbHwGetDynamicLights(long turn, float *out, int max_count)
{
    int n;

    if ((turn - dyn_turn) > 1 || (turn - dyn_turn) < -1)
        return 0;
    n = (dyn_count < max_count) ? dyn_count : max_count;
    memcpy(out, dyn_lights, n * 3 * sizeof(float));
    return n;
}
static struct HwVertex *rec_verts = NULL;
static long rec_count = 0;
static long rec_capacity = 0;
static ubyte * const *tex_pages = NULL;
static int tex_pages_num = 0;

#define HW_VERTICES_LIMIT (3 * 600000)

#define HW_ATLAS_W 2048
#define HW_ATLAS_H 2048
#define HW_ATLAS_HASH 8192
#define HW_REMAP_MAX 256

struct HwAtlasEntry {
    const struct TbSprite *spr;
    const ubyte *data;
    ushort w, h;
    short ax, ay;
};

static ubyte *atlas_buf = NULL;
static struct HwAtlasEntry *atlas_tbl = NULL;
static int atlas_x = 0, atlas_y = 0, atlas_row_h = 0;
static int atlas_dirty_y0 = HW_ATLAS_H, atlas_dirty_y1 = 0;

static ubyte remap_tbl[HW_REMAP_MAX][256];
static const ubyte *remap_src[HW_REMAP_MAX];
static int remap_count = 0;

TbBool LbHwRenderEnabled(void)
{
    return hw_enabled;
}

void LbHwRenderSetEnabled(TbBool enabled)
{
    hw_enabled = enabled;
}

int LbHwLightingMode(void)
{
    return hw_lighting;
}

void LbHwLightingSetMode(int mode)
{
    if ((mode >= 0) && (mode < HwLight_COUNT))
        hw_lighting = mode;
}

void LbHwSetTexturePages(ubyte * const *pages, int count)
{
    tex_pages = pages;
    tex_pages_num = count;
}

const ubyte *LbHwTexturePage(int index)
{
    if ((tex_pages == NULL) || (index < 0) || (index >= tex_pages_num))
        return NULL;
    return tex_pages[index];
}

int LbHwTexturePagesCount(void)
{
    return tex_pages_num;
}

static long dbg_fail[8];
static long dbg_frames = 0;
#define DBGF(i) (dbg_fail[i]++)

void LbHwFrameBegin(void)
{
    pl_count = 0;
    beam_count = 0;
    arc_light_count = 0;
    smoke_count = 0;
    if (getenv("SWFX_HW_FAILLOG") != NULL) {
        if ((++dbg_frames % 60) == 0) {
            LOGERR("HW fallbacks per 60 frames: tri_screen=%ld tri_page=%ld tri_grow=%ld spr_atlas=%ld spr_remap=%ld spr_grow=%ld alpha_bounds=%ld",
              dbg_fail[0], dbg_fail[1], dbg_fail[2], dbg_fail[3], dbg_fail[4], dbg_fail[5], dbg_fail[6]);
            memset(dbg_fail, 0, sizeof(dbg_fail));
        }
    }
    rec_count = 0;
    remap_count = 0;
}

#define HW_TEXPAGE_SIZE (256 * 256)

static ubyte *page_snapshot = NULL;
static int page_snapshot_count = 0;

void LbHwRecordingEnd(void)
{
    int i;

    lbHwRecording = false;
    if ((rec_count <= 0) || (tex_pages == NULL))
        return;
    // The texture pages are changed by the game after the drawing, and what the
    // software renderer drew with is what they were at this point. Keep that.
    if ((page_snapshot == NULL) || (page_snapshot_count < tex_pages_num)) {
        free(page_snapshot);
        page_snapshot = (ubyte *)malloc((size_t)tex_pages_num * HW_TEXPAGE_SIZE);
        page_snapshot_count = (page_snapshot != NULL) ? tex_pages_num : 0;
    }
    if (page_snapshot == NULL)
        return;
    for (i = 0; i < tex_pages_num; i++)
    {
        if (tex_pages[i] != NULL)
            memcpy(&page_snapshot[(size_t)i * HW_TEXPAGE_SIZE], tex_pages[i], HW_TEXPAGE_SIZE);
    }
}

const ubyte *LbHwTexturePageSnapshot(int index)
{
    if ((page_snapshot == NULL) || (index < 0) || (index >= page_snapshot_count))
        return NULL;
    if (LbHwTexturePage(index) == NULL)
        return NULL;
    return &page_snapshot[(size_t)index * HW_TEXPAGE_SIZE];
}

static int hw_texture_layer_of_map(const ubyte *map)
{
    int i;

    if (tex_pages == NULL)
        return -1;
    for (i = 0; i < tex_pages_num; i++)
    {
        if (tex_pages[i] == map)
            return i;
    }
    return -1;
}

/** Tells if the render mode takes its colours from the texture. */
static TbBool hw_mode_is_textured(ubyte mode)
{
    switch (mode)
    {
    case RendVec_mode00:
    case RendVec_mode01:
    case RendVec_mode04:
    case RendVec_mode14:
    case RendVec_mode15:
    case RendVec_mode16:
    case RendVec_mode17:
        return false;
    default:
        return true;
    }
}

static TbBool hw_grow(long need)
{
    struct HwVertex *p_new;
    long cap;

    if (rec_count + need <= rec_capacity)
        return true;
    if (rec_count + need > HW_VERTICES_LIMIT)
        return false;
    cap = (rec_capacity == 0) ? 3 * 16384 : rec_capacity * 2;
    while (cap < rec_count + need)
        cap *= 2;
    p_new = (struct HwVertex *)realloc(rec_verts, cap * sizeof(struct HwVertex));
    if (p_new == NULL)
        return false;
    rec_verts = p_new;
    rec_capacity = cap;
    return true;
}

TbBool LbHwRecordTriangle(const struct PolyPoint *point_a,
  const struct PolyPoint *point_b, const struct PolyPoint *point_c)
{
    const struct PolyPoint *pts[3];
    struct HwVertex *p_vtx;
    long cross;
    float layer;
    float umax, vmax;
    int i;

    if (!lbHwRecording)
        return false;
    if (vec_screen != lbDisplay.WScreen) {
        DBGF(0);
        return false;
    }

    layer = -1.0f;
    if (hw_mode_is_textured(vec_mode))
    {
        int page;

        page = hw_texture_layer_of_map(vec_map);
        if (page < 0)
        { DBGF(1); return false; } // texture is not one of the pages; leave it to the software
        layer = (float)page;
    }

    // The software renderer only draws triangles which are given clockwise
    cross = (point_b->X - point_a->X) * (point_c->Y - point_a->Y)
          - (point_b->Y - point_a->Y) * (point_c->X - point_a->X);
    if (cross <= 0)
        return true;

    if (!hw_grow(3)) {
        DBGF(2);
        return false;
    }

    pts[0] = point_a;
    pts[1] = point_b;
    pts[2] = point_c;
    p_vtx = &rec_verts[rec_count];
    // The texture wraps around, so a pixel exactly at the vertex with the greatest texture
    // position would get the texel from the other end of the texture; the software renderer
    // never takes the texel after the last one, and nor can the GPU
    umax = (float)pts[0]->U;

    vmax = (float)pts[0]->V;
    for (i = 1; i < 3; i++)
    {
        if ((float)pts[i]->U > umax)
            umax = (float)pts[i]->U;
        if ((float)pts[i]->V > vmax)
            vmax = (float)pts[i]->V;
    }
    umax *= (1.0f / 65536.0f);
    vmax *= (1.0f / 65536.0f);
    for (i = 0; i < 3; i++)
    {
        p_vtx[i].umax = umax;
        p_vtx[i].vmax = vmax;
        p_vtx[i].tx0 = (float)pts[0]->X;
        p_vtx[i].ty0 = (float)pts[0]->Y;
        p_vtx[i].tx1 = (float)pts[1]->X;
        p_vtx[i].ty1 = (float)pts[1]->Y;
        p_vtx[i].tx2 = (float)pts[2]->X;
        p_vtx[i].ty2 = (float)pts[2]->Y;
        p_vtx[i].x = (float)pts[i]->X;
        p_vtx[i].y = (float)pts[i]->Y;
        p_vtx[i].u = (float)pts[i]->U * (1.0f / 65536.0f);
        p_vtx[i].v = (float)pts[i]->V * (1.0f / 65536.0f);
        p_vtx[i].s = (float)pts[i]->S * (1.0f / 65536.0f);
        p_vtx[i].mode = (float)vec_mode;
        p_vtx[i].colour = (float)vec_colour;
        // A textured triangle which is seen through carries its opacity in the colour, which the textured modes do not use
        if ((hw_face_alpha < 1.0f) && ((vec_mode == 2) || (vec_mode == 3) || (vec_mode == 5) || (vec_mode == 6) || (vec_mode == 27)))
            p_vtx[i].colour = 1000.0f + (float)(int)(hw_face_alpha * 100.0f + 0.5f);
        p_vtx[i].layer = layer;
    }
    rec_count += 3;
    return true;
}

/** Forgets the decoded sprites; the sprite data was loaded again (other costume, other panel). */
void LbHwSpriteCacheClear(void)
{
    if (atlas_tbl != NULL)
        memset(atlas_tbl, 0, HW_ATLAS_HASH * sizeof(struct HwAtlasEntry));
    atlas_x = atlas_y = atlas_row_h = 0;
}

/** Finds the place of the sprite in the atlas, decoding it there if it is not yet. */
static const struct HwAtlasEntry *hw_atlas_sprite(const struct TbSprite *spr)
{
    struct HwAtlasEntry *ent;
    const char *sp;
    ubyte *row;
    int slot, tries, x, y;

    if ((spr == NULL) || (spr->Data == NULL) || (spr->SWidth < 1) || (spr->SHeight < 1))
        return NULL;
    if (atlas_buf == NULL) {
        atlas_buf = (ubyte *)calloc(HW_ATLAS_W * HW_ATLAS_H, 1);
        atlas_tbl = (struct HwAtlasEntry *)calloc(HW_ATLAS_HASH, sizeof(struct HwAtlasEntry));
        if ((atlas_buf == NULL) || (atlas_tbl == NULL))
            return NULL;
    }
    slot = (int)((((uintptr_t)spr) >> 3) * 2654435761u) & (HW_ATLAS_HASH - 1);
    for (tries = 0; tries < HW_ATLAS_HASH; tries++)
    {
        ent = &atlas_tbl[slot];
        if (ent->spr == NULL)
            break; // free slot - the sprite is new
        if (ent->spr == spr) {
            if ((ent->data == spr->Data) && (ent->w == spr->SWidth) && (ent->h == spr->SHeight))
                return ent;
            break; // the sprite was changed - decode again, to new place
        }
        slot = (slot + 1) & (HW_ATLAS_HASH - 1);
    }
    if (tries >= HW_ATLAS_HASH)
        return NULL;

    // Find the place; the atlas is filled in rows
    if (atlas_x + spr->SWidth > HW_ATLAS_W) {
        atlas_x = 0;
        atlas_y += atlas_row_h;
        atlas_row_h = 0;
    }
    if (atlas_y + spr->SHeight > HW_ATLAS_H)
        return NULL;
    ent->spr = spr;
    ent->data = spr->Data;
    ent->w = spr->SWidth;
    ent->h = spr->SHeight;
    ent->ax = atlas_x;
    ent->ay = atlas_y;
    atlas_x += spr->SWidth;
    if (spr->SHeight > atlas_row_h)
        atlas_row_h = spr->SHeight;

    // The sprite data is lines of: n > 0 - n pixels follow; n < 0 - skip n pixels; 0 - end of line
    sp = (const char *)spr->Data;
    for (y = 0; y < spr->SHeight; y++)
    {
        row = &atlas_buf[(ent->ay + y) * HW_ATLAS_W + ent->ax];
        memset(row, 0, spr->SWidth);
        x = 0;
        while (1)
        {
            int n = *sp++;
            if (n == 0)
                break;
            if (n < 0) {
                x += -n;
            } else {
                int cnt = n;
                if (x + cnt > spr->SWidth)
                    cnt = spr->SWidth - x;
                if (cnt > 0)
                    memcpy(&row[x], sp, cnt);
                x += n;
                sp += n;
            }
        }
    }
    if (ent->ay < atlas_dirty_y0)
        atlas_dirty_y0 = ent->ay;
    if (ent->ay + ent->h > atlas_dirty_y1)
        atlas_dirty_y1 = ent->ay + ent->h;
    return ent;
}

static int hw_remap_row(const ubyte *cmap)
{
    int i;

    for (i = 0; i < remap_count; i++)
    {
        // The same table address can hold other colours after the data was loaded again
        if ((remap_src[i] == cmap) && (memcmp(remap_tbl[i], cmap, 256) == 0))
            return i;
    }
    for (i = 0; i < remap_count; i++)
    {
        if (memcmp(remap_tbl[i], cmap, 256) == 0) {
            return i;
        }
    }
    if (remap_count >= HW_REMAP_MAX)
        return -1;
    memcpy(remap_tbl[remap_count], cmap, 256);
    remap_src[remap_count] = cmap;
    return remap_count++;
}

static void hw_quad(struct HwVertex *v, float x0, float y0, float x1, float y1,
  float u0, float v0, float u1, float v1, float mode, float colour, float layer)
{
    static const int ci[6][2] = { {0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1} };
    int i;

    for (i = 0; i < 6; i++)
    {
        // The vertex positions are moved by half a pixel while drawing
        v[i].x = (ci[i][0] ? x1 : x0) - 0.5f;
        v[i].y = (ci[i][1] ? y1 : y0) - 0.5f;
        v[i].u = ci[i][0] ? u1 : u0;
        v[i].v = ci[i][1] ? v1 : v0;
        v[i].s = 0.0f;
        v[i].umax = v[i].vmax = 1.0e9f;
        v[i].tx0 = v[i].ty0 = v[i].tx1 = v[i].ty1 = v[i].tx2 = v[i].ty2 = 0.0f;
        v[i].mode = mode;
        v[i].colour = colour;
        v[i].layer = layer;
    }
}

TbBool LbHwRecordSprite(const struct TbSprite *spr, long x0, long y0, long x1, long y1,
  TbBool flip_h, TbBool flip_v, TbBool ghost, const ubyte *remap, int one_colour)
{
    const struct HwAtlasEntry *ent;
    float u0, u1, v0, v1;
    int row;

    if (!lbHwRecording)
        return false;
    if ((x1 <= x0) || (y1 <= y0))
        return true; // nothing to draw
    ent = hw_atlas_sprite(spr);
    if (ent == NULL) {
        DBGF(3);
        return false;
    }
    row = -1;
    if (remap != NULL) {
        row = hw_remap_row(remap);
        if (row < 0) {
            DBGF(4);
            return false;
        }
    }
    if (!hw_grow(6)) {
        DBGF(5);
        return false;
    }
    u0 = ent->ax;
    u1 = ent->ax + ent->w;
    v0 = ent->ay;
    v1 = ent->ay + ent->h;
    if (flip_h) {
        float t = u0; u0 = u1; u1 = t;
    }
    if (flip_v) {
        float t = v0; v0 = v1; v1 = t;
    }
    hw_quad(&rec_verts[rec_count], x0, y0, x1, y1, u0, v0, u1, v1,
      ghost ? HWREND_MODE_SPRITE_GHOST : HWREND_MODE_SPRITE, (float)one_colour, (float)row);
    rec_count += 6;
    return true;
}

TbBool LbHwRecordSpriteDraw(const struct TbSprite *spr, long x, long y, long dw, long dh,
  const ubyte *remap, int one_colour)
{
    ushort flags;
    TbBool ghost;

    if (!lbHwRecording)
        return false;
    if (spr == NULL)
        return false;
    flags = lbDisplay.DrawFlags;
    ghost = ((flags & (Lb_SPRITE_TRANSPAR4 | Lb_SPRITE_TRANSPAR8)) != 0);
    if ((remap == NULL) && ((flags & Lb_TEXT_UNDERLNSHADOW) != 0) && (lbDisplay.FadeTable != NULL)) {
        // The flag selects the fade table row of the current fade step as the colour map
        remap = lbDisplay.FadeTable + ((lbDisplay.FadeStep & 0x3F) << 8);
        ghost = false;
    }
    if (dw <= 0)
        dw = spr->SWidth;
    if (dh <= 0)
        dh = spr->SHeight;
    x += lbDisplay.GraphicsWindowX;
    y += lbDisplay.GraphicsWindowY;
    return LbHwRecordSprite(spr, x, y, x + dw, y + dh,
      (flags & Lb_SPRITE_FLIP_HORIZ) != 0, (flags & Lb_SPRITE_FLIP_VERTIC) != 0,
      ghost, remap, one_colour);
}

/** Finds where the sprite drawn with the scaling of SetAlphaScalingData() lands on the screen. */
static TbBool hw_alpha_scaled_rect(const struct TbSprite *spr, long posx, long posy,
  long *p_x0, long *p_y0, long *p_x1, long *p_y1)
{
    const long *xs, *ys;

    if ((posx < 0) || (posy < 0) || (posx + spr->SWidth > SPRITE_SCALING_XSTEPS)
      || (posy + spr->SHeight > SPRITE_SCALING_YSTEPS)) {
        DBGF(6);
        return false;
    }
    // The scaling arrays hold, for each pixel of the source, where it goes on the screen
    xs = &alpha_xsteps_array[2 * posx];
    ys = &alpha_ysteps_array[2 * posy];
    *p_x0 = xs[0] + lbDisplay.GraphicsWindowX;
    *p_y0 = ys[0] + lbDisplay.GraphicsWindowY;
    *p_x1 = xs[2 * (spr->SWidth - 1)] + xs[2 * (spr->SWidth - 1) + 1] + lbDisplay.GraphicsWindowX;
    *p_y1 = ys[2 * (spr->SHeight - 1)] + ys[2 * (spr->SHeight - 1) + 1] + lbDisplay.GraphicsWindowY;
    return true;
}

TbBool LbHwRecordSpriteDrawAlphaScaled(const struct TbSprite *spr, long posx, long posy)
{
    ushort flags;
    long x0, y0, x1, y1;

    if (!lbHwRecording)
        return false;
    if ((spr == NULL) || (lbSpriteReMapPtr == NULL))
        return false;
    if (!hw_alpha_scaled_rect(spr, posx, posy, &x0, &y0, &x1, &y1))
        return false;
    flags = lbDisplay.DrawFlags;
    // In this drawing, TRANSPAR8 means the colours go through the colour map, TRANSPAR4
    // means a half transparent sprite with its own colours, and none is a plain sprite
    return LbHwRecordSprite(spr, x0, y0, x1, y1,
      (flags & Lb_SPRITE_FLIP_HORIZ) != 0, (flags & Lb_SPRITE_FLIP_VERTIC) != 0,
      ((flags & Lb_SPRITE_TRANSPAR8) == 0) && ((flags & Lb_SPRITE_TRANSPAR4) != 0),
      ((flags & Lb_SPRITE_TRANSPAR8) != 0) ? lbSpriteReMapPtr : NULL, -1);
}

#define HW_SMOKE_MAX 400
struct HwSmoke { float x, y, size, alpha; int seed, kind; };
static struct HwSmoke smoke_list[HW_SMOKE_MAX];

void LbHwNoteSmoke(float sx, float sy, float size, int seed, int kind, float alpha)
{
    struct HwSmoke *p;

    if (smoke_count >= HW_SMOKE_MAX)
        return;
    p = &smoke_list[smoke_count++];
    p->x = sx; p->y = sy; p->size = size; p->alpha = alpha; p->seed = seed; p->kind = kind;
}

TbBool LbHwRecordSmokePuff(float x0, float y0, float x1, float y1, int seed, int kind, float alpha)
{
    int k;

    if (!lbHwRecording || (hw_lighting != HwLight_Enhanced) || !hw_grow(6))
        return false;
    hw_quad(&rec_verts[rec_count], x0, y0, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f,
      HWREND_MODE_SMOKE, (float)kind, alpha);
    for (k = 0; k < 6; k++)
        rec_verts[rec_count + k].s = (float)(seed & 1023);
    rec_count += 6;
    return true;
}

void LbHwFlushSmoke(void)
{
    int i, j;

    if (!lbHwRecording || (hw_lighting != HwLight_Enhanced)) {
        smoke_count = 0;
        return;
    }
    // Further puffs (higher on the screen) first, so the nearer ones go over them
    for (i = 1; i < smoke_count; i++) {
        struct HwSmoke key = smoke_list[i];
        for (j = i - 1; j >= 0 && smoke_list[j].y > key.y; j--)
            smoke_list[j + 1] = smoke_list[j];
        smoke_list[j + 1] = key;
    }
    for (i = 0; i < smoke_count; i++)
    {
        struct HwSmoke *p = &smoke_list[i];
        int k;

        if (!hw_grow(6))
            break;
        hw_quad(&rec_verts[rec_count], p->x - p->size * 0.5f, p->y - p->size * 0.5f,
          p->x + p->size * 0.5f, p->y + p->size * 0.5f, 0.0f, 0.0f, 1.0f, 1.0f,
          HWREND_MODE_SMOKE, (float)p->kind, p->alpha);
        for (k = 0; k < 6; k++)
            rec_verts[rec_count + k].s = (float)(p->seed & 1023);
        rec_count += 6;
    }
    smoke_count = 0;
}

TbBool LbHwRecordGlowSeg(float x1, float y1, float x2, float y2, int kind, int param, float strength, int colour)
{
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    static const float cs[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const float ct[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float dx, dy, len, half, nx, ny, ex, ey, phase;
    float px[4], py[4];
    struct HwVertex *v;
    int k;

    if (!lbHwRecording || (hw_lighting != HwLight_Enhanced))
        return false;
    dx = x2 - x1;
    dy = y2 - y1;
    len = sqrtf(dx * dx + dy * dy);
    if ((len < 0.5f) || !hw_grow(6))
        return false;
    dx /= len;
    dy /= len;
    if (kind == 2)
    {
        // A tongue of the flame thrower, from the gun to a puff: it widens towards the puff
        half = strength;
        ex = ey = 0.0f;
        phase = 0.0f;
    }
    else if (kind == 0)
    {
        // A piece of the beam of a laser, the pieces lie end to end; the beam is wider while it is fresh,
        // and the glow around the core is wider still
        half = 5.0f + 9.0f * strength;
        ex = ey = 0.0f;
        // Where the piece starts along the beam, so that the flicker goes on across the pieces
        phase = x1 * dx + y1 * dy;
    }
    else
    {
        // A piece of a lightning: the pieces meet at angles, so the ends are round and overlap
        half = (param & 1) ? 6.0f : 9.0f;
        ex = dx * half;
        ey = dy * half;
        phase = 0.0f;
    }
    nx = -dy * half;
    ny = dx * half;
    px[0] = x1 - ex - nx; py[0] = y1 - ey - ny;
    px[1] = x1 - ex + nx; py[1] = y1 - ey + ny;
    px[2] = x2 + ex + nx; py[2] = y2 + ey + ny;
    px[3] = x2 + ex - nx; py[3] = y2 + ey - ny;
    v = &rec_verts[rec_count];
    for (k = 0; k < 6; k++)
    {
        int c = tri[k];

        v[k].x = px[c] - 0.5f;
        v[k].y = py[c] - 0.5f;
        v[k].u = cs[c];
        v[k].v = ct[c];
        v[k].s = 0.0f;
        v[k].umax = v[k].vmax = 1.0e9f;
        v[k].tx0 = len;
        v[k].ty0 = (kind == 0) ? strength : half;
        v[k].tx1 = phase;
        v[k].ty1 = v[k].tx2 = v[k].ty2 = 0.0f;
        v[k].mode = (kind == 0) ? HWREND_MODE_BEAM : (kind == 2) ? HWREND_MODE_JET : HWREND_MODE_ARC;
        v[k].colour = (kind == 0) ? (float)colour
          : (kind == 2) ? (float)(colour & 1023) : (float)(((int)(x1 * 7.0f + y1 * 13.0f)) & 1023);
        v[k].layer = (kind == 0) ? strength : (kind == 2) ? (float)param * 0.01f : (float)param;
    }
    rec_count += 6;
    return true;
}

TbBool LbHwRecordFlame(float x0, float y0, float x1, float y1, int seed, float strength)
{
    if (!lbHwRecording || (hw_lighting != HwLight_Enhanced))
        return false;
    if (!hw_grow(6))
        return false;
    hw_quad(&rec_verts[rec_count], x0, y0, x1, y1, 0.0f, 0.0f, 1.0f, 1.0f,
      HWREND_MODE_FLAME, (float)(seed & 1023), strength);
    rec_count += 6;
    return true;
}

TbBool LbHwRecordBloodPool(const struct TbSprite *spr, long x0, long y0, long x1, long y1, int seed, float bright,
  float growth)
{
    static int disabled = -1;
    const struct HwAtlasEntry *ent;
    float sx, sy;
    int k;

    if (disabled < 0)
        disabled = (getenv("SWFX_NO_BLOODFX") != NULL) ? 1 : 0;
    if (!lbHwRecording || disabled || (hw_lighting != HwLight_Enhanced) || (spr == NULL))
        return false;
    if ((x1 <= x0) || (y1 <= y0))
        return true; // nothing to draw
    ent = hw_atlas_sprite(spr);
    if (ent == NULL)
        return false;
    if (!hw_grow(6))
        return false;
    // The quad is two sprite pixels wider on each side, so the edge of the pool can spread;
    // the texture position is in pixels of the sprite, and the shader finds the sprite by its place in the atlas
    sx = (float)(x1 - x0) / (float)ent->w;
    sy = (float)(y1 - y0) / (float)ent->h;
    hw_quad(&rec_verts[rec_count], (float)x0 - 2.0f * sx, (float)y0 - 2.0f * sy,
      (float)x1 + 2.0f * sx, (float)y1 + 2.0f * sy,
      -2.0f, -2.0f, (float)ent->w + 2.0f, (float)ent->h + 2.0f,
      HWREND_MODE_BLOOD, (float)(seed & 1023), bright);
    for (k = 0; k < 6; k++)
    {
        rec_verts[rec_count + k].tx0 = (float)ent->ax;
        rec_verts[rec_count + k].ty0 = (float)ent->ay;
        rec_verts[rec_count + k].tx1 = (float)(ent->ax + ent->w);
        rec_verts[rec_count + k].ty1 = (float)(ent->ay + ent->h);
        rec_verts[rec_count + k].tx2 = growth;
    }
    rec_count += 6;
    return true;
}

static TbBool hw_record_streak(int mode, float hx, float hy, float tx, float ty, float width, float alpha, int seed, float bright)
{
    static int disabled = -1;
    struct HwVertex *v;
    float dx, dy, len, nx, ny;
    float px[4], py[4], pu[4], pv[4];
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    int i;

    if (disabled < 0)
        disabled = (getenv("SWFX_NO_BLOODFX") != NULL) ? 1 : 0;
    if (!lbHwRecording || disabled || !hw_grow(6))
        return false;
    if (width < 1.0f)
        width = 1.0f;
    dx = hx - tx;
    dy = hy - ty;
    len = sqrtf(dx * dx + dy * dy);
    if (len < width) {
        // Hardly moving: a round drop
        dx = 0.0f; dy = -1.0f; len = width;
        tx = hx; ty = hy + width;
    } else {
        dx /= len; dy /= len;
    }
    if (len < width) len = width;
    nx = -dy * width * 0.5f;
    ny = dx * width * 0.5f;
    // The round cap at the head
    hx += dx * width * 0.5f;
    hy += dy * width * 0.5f;
    px[0] = tx - nx; py[0] = ty - ny; pu[0] = -1.0f; pv[0] = 0.0f;
    px[1] = tx + nx; py[1] = ty + ny; pu[1] = 1.0f;  pv[1] = 0.0f;
    px[2] = hx + nx; py[2] = hy + ny; pu[2] = 1.0f;  pv[2] = 1.0f;
    px[3] = hx - nx; py[3] = hy - ny; pu[3] = -1.0f; pv[3] = 1.0f;
    v = &rec_verts[rec_count];
    for (i = 0; i < 6; i++)
    {
        int c = tri[i];

        v[i].x = px[c] - 0.5f;
        v[i].y = py[c] - 0.5f;
        v[i].u = pu[c];
        v[i].v = pv[c];
        v[i].s = 0.0f;
        v[i].umax = v[i].vmax = 1.0e9f;
        v[i].tx0 = bright; v[i].ty0 = 2.0f * len / width + 1.0f; v[i].tx1 = v[i].ty1 = v[i].tx2 = v[i].ty2 = 0.0f;
        v[i].mode = (float)mode;
        v[i].colour = (float)(seed & 1023);
        v[i].layer = alpha;
    }
    rec_count += 6;
    return true;
}

TbBool LbHwRecordBloodStreak(float hx, float hy, float tx, float ty, float width, float alpha, int seed, float bright)
{
    return hw_record_streak(HWREND_MODE_BLOOD_DROP, hx, hy, tx, ty, width, alpha, seed, bright);
}

TbBool LbHwRecordSparkStreak(float hx, float hy, float tx, float ty, float width, float alpha, int seed)
{
    return hw_record_streak(HWREND_MODE_SPARK, hx, hy, tx, ty, width, alpha, seed, 1.0f);
}

TbBool LbHwRecordBloodSplatOriented(float cx, float cy, float ux, float uy, float vx, float vy, int seed,
  float age, float alpha, float bright, float dirx, float diry, float tongue)
{
    static int disabled = -1;
    static const int tri[6] = { 0, 1, 2, 0, 2, 3 };
    static const float cs[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    static const float ct[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    struct HwVertex *v;
    int i;

    if (disabled < 0)
        disabled = (getenv("SWFX_NO_BLOODFX") != NULL) ? 1 : 0;
    if (!lbHwRecording || disabled || !hw_grow(6))
        return false;
    v = &rec_verts[rec_count];
    for (i = 0; i < 6; i++)
    {
        float s = cs[tri[i]], t = ct[tri[i]];

        v[i].x = cx + s * ux + t * vx - 0.5f;
        v[i].y = cy + s * uy + t * vy - 0.5f;
        v[i].u = s;
        v[i].v = t;
        v[i].s = 0.0f;
        v[i].umax = v[i].vmax = 1.0e9f;
        v[i].tx0 = age; v[i].ty0 = bright; v[i].tx1 = dirx; v[i].ty1 = diry;
        v[i].tx2 = tongue;
        v[i].ty2 = 0.0f;
        v[i].mode = HWREND_MODE_BLOOD_SPLAT;
        v[i].colour = (float)(seed & 1023);
        v[i].layer = alpha;
    }
    rec_count += 6;
    return true;
}

TbBool LbHwRecordBloodSplat(float cx, float cy, float rx, float ry, int seed, float age, float alpha,
  float bright, float dirx, float diry)
{
    return LbHwRecordBloodSplatOriented(cx, cy, rx, 0.0f, 0.0f, ry, seed, age, alpha, bright, dirx, diry, 1.0f);
}

TbBool LbHwRecordBloodPoolAlphaScaled(const struct TbSprite *spr, long posx, long posy, int seed, float bright,
  float growth)
{
    long x0, y0, x1, y1;

    if (!lbHwRecording || (spr == NULL))
        return false;
    if (!hw_alpha_scaled_rect(spr, posx, posy, &x0, &y0, &x1, &y1))
        return false;
    return LbHwRecordBloodPool(spr, x0, y0, x1, y1, seed, bright, growth);
}

TbBool LbHwRecordBox(long x0, long y0, long x1, long y1, ubyte colour, TbBool ghost)
{
    if (!lbHwRecording)
        return false;
    if ((x1 <= x0) || (y1 <= y0))
        return true;
    if (!hw_grow(6))
        return false;
    hw_quad(&rec_verts[rec_count], x0, y0, x1, y1, 0, 0, 0, 0,
      ghost ? HWREND_MODE_FLAT_GHOST : HWREND_MODE_FLAT, (float)colour, -1.0f);
    rec_count += 6;
    return true;
}

TbBool LbHwRecordLine(long x1, long y1, long x2, long y2, ubyte colour, TbBool ghost)
{
    float dx, dy, len, nx, ny;
    struct HwVertex *v;
    int i;
    static const int ci[6][2] = { {0,0}, {1,0}, {1,1}, {0,0}, {1,1}, {0,1} };

    if (!lbHwRecording)
        return false;
    if (!hw_grow(6))
        return false;
    dx = (float)(x2 - x1);
    dy = (float)(y2 - y1);
    len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) {
        nx = 0.0f; ny = 0.5f; dx = 1.0f; dy = 0.0f; len = 1.0f;
    } else {
        // Half of the pixel to each side; and the end pixels are drawn too
        nx = -dy / len * 0.5f;
        ny = dx / len * 0.5f;
    }
    dx = dx / len * 0.5f;
    dy = dy / len * 0.5f;
    v = &rec_verts[rec_count];
    for (i = 0; i < 6; i++)
    {
        float px = ci[i][0] ? (float)x2 + dx : (float)x1 - dx;
        float py = ci[i][0] ? (float)y2 + dy : (float)y1 - dy;
        float side = ci[i][1] ? 1.0f : -1.0f;

        // Positions are given as the pixel centres, the drawing moves them by half a pixel
        v[i].x = px + side * nx;
        v[i].y = py + side * ny;
        v[i].u = v[i].v = v[i].s = 0.0f;
        v[i].umax = v[i].vmax = 1.0e9f;
        v[i].tx0 = v[i].ty0 = v[i].tx1 = v[i].ty1 = v[i].tx2 = v[i].ty2 = 0.0f;
        v[i].mode = ghost ? HWREND_MODE_FLAT_GHOST : HWREND_MODE_FLAT;
        v[i].colour = (float)colour;
        v[i].layer = -1.0f;
    }
    rec_count += 6;
    return true;
}

const ubyte *LbHwSpriteAtlas(int *p_width, int *p_height, int *p_dirty_y0, int *p_dirty_y1)
{
    *p_width = HW_ATLAS_W;
    *p_height = HW_ATLAS_H;
    *p_dirty_y0 = atlas_dirty_y0;
    *p_dirty_y1 = atlas_dirty_y1;
    return atlas_buf;
}

void LbHwSpriteAtlasClean(void)
{
    atlas_dirty_y0 = HW_ATLAS_H;
    atlas_dirty_y1 = 0;
}

const ubyte *LbHwRemapTables(int *p_count)
{
    *p_count = remap_count;
    return &remap_tbl[0][0];
}

const struct HwVertex *LbHwRecordedVertices(long *p_count)
{
    *p_count = rec_count;
    return rec_verts;
}

TbBool LbHwHasRecorded(void)
{
    return (rec_count > 0);
}

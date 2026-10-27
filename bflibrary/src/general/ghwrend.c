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
#include <stdlib.h>
#include <string.h>
#include "bfhwrend.h"
#include "bfscreen.h"
#include "poly.h"
#include "privbflog.h"

volatile TbBool lbHwRecording = false;

/* On by default; it only has effect when the window got a GL context */
static TbBool hw_enabled = true;
static int hw_lighting = HwLight_Enhanced;
static struct HwVertex *rec_verts = NULL;
static long rec_count = 0;
static long rec_capacity = 0;
static ubyte * const *tex_pages = NULL;
static int tex_pages_num = 0;

#define HW_VERTICES_LIMIT (3 * 400000)

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

void LbHwFrameBegin(void)
{
    rec_count = 0;
}

void LbHwRecordingEnd(void)
{
    lbHwRecording = false;
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
    int i;

    if (!lbHwRecording)
        return false;
    if (vec_screen != lbDisplay.WScreen)
        return false;

    layer = -1.0f;
    if (hw_mode_is_textured(vec_mode))
    {
        int page;

        page = hw_texture_layer_of_map(vec_map);
        if (page < 0)
            return false; // texture is not one of the pages; leave it to the software
        layer = (float)page;
    }

    // The software renderer only draws triangles which are given clockwise
    cross = (point_b->X - point_a->X) * (point_c->Y - point_a->Y)
          - (point_b->Y - point_a->Y) * (point_c->X - point_a->X);
    if (cross <= 0)
        return true;

    if (!hw_grow(3))
        return false;

    pts[0] = point_a;
    pts[1] = point_b;
    pts[2] = point_c;
    p_vtx = &rec_verts[rec_count];
    for (i = 0; i < 3; i++)
    {
        p_vtx[i].x = (float)pts[i]->X;
        p_vtx[i].y = (float)pts[i]->Y;
        p_vtx[i].u = (float)pts[i]->U * (1.0f / 65536.0f);
        p_vtx[i].v = (float)pts[i]->V * (1.0f / 65536.0f);
        p_vtx[i].s = (float)pts[i]->S * (1.0f / 65536.0f);
        p_vtx[i].mode = (float)vec_mode;
        p_vtx[i].colour = (float)vec_colour;
        p_vtx[i].layer = layer;
    }
    rec_count += 3;
    return true;
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

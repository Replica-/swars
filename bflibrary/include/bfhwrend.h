/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file bfhwrend.h
 *     Header file for ghwrend.c.
 * @par Purpose:
 *     Hardware (GPU) rendering of the 3D scene. While the scene is drawn by the
 *     game, the primitives which would be rasterized by the software renderer
 *     are recorded instead. When the screen is presented, the OpenGL code draws
 *     them on the GPU, in the same order, and the software screen is put on top.
 * @par Comment:
 *     The recording is independent of OpenGL. Builds which cannot do hardware
 *     rendering simply never enable it.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIBRARY_BFHWREND_H_
#define BFLIBRARY_BFHWREND_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

struct PolyPoint;
struct TbSprite;

/** Palette index of the software screen which means "nothing was drawn here".
 *
 * When the 3D scene is drawn by the GPU, the software screen is cleared to this
 * colour; anything which is still this colour when the frame is shown lets the
 * GPU image through.
 */
#define HWREND_CLEAR_COLOUR 255

/** One vertex of a recorded triangle. All values are floats to be sent to the GPU as they are. */
struct HwVertex {
    float x, y;       /**< Position within the screen buffer, in pixels. */
    float u, v;       /**< Texture position, in texels. */
    float s;          /**< Shade; the integer part is the row of the fade table. */
    float mode;       /**< Software render mode of the triangle (vec_mode). */
    float colour;     /**< Constant colour of the triangle (vec_colour). */
    float layer;      /**< Texture page, or -1 if the triangle is not textured. */
    float umax, vmax; /**< The greatest texture position of the triangle; no pixel of it can take more. */
    float tx0, ty0, tx1, ty1, tx2, ty2; /**< Positions of the three vertices of the triangle, to find where its lines start. */
};

/** True while the 3D scene is being recorded and not rasterized. */
extern volatile TbBool lbHwRecording;

/** Whether the user wants the hardware rendering, and the system is able to do it. */
TbBool LbHwRenderEnabled(void);
void LbHwRenderSetEnabled(TbBool enabled);

/** Lighting of the hardware rendered scene: the classic one repeats the palette based
 * shading of the software renderer, the enhanced one shades in full colour.
 */
enum HwLightingMode {
    HwLight_Classic = 0,
    HwLight_Enhanced,
    HwLight_COUNT,
};

int LbHwLightingMode(void);
void LbHwLightingSetMode(int mode);

/** Starts a new frame, dropping what was recorded before. */
void LbHwFrameBegin(void);

/** Stops recording; what has been recorded stays until the next LbHwFrameBegin(). */
void LbHwRecordingEnd(void);

/** Records a triangle which the software renderer was asked to draw.
 *
 * Uses the current vec_mode, vec_colour and vec_map, like the rasterizer does.
 * @return True if the triangle was recorded, false if it needs to be drawn by the
 *    software renderer.
 */
TbBool LbHwRecordTriangle(const struct PolyPoint *point_a,
  const struct PolyPoint *point_b, const struct PolyPoint *point_c);

/** Render modes of the recorded primitives which are not triangles of the 3D scene. */
#define HWREND_MODE_SPRITE 100       /**< Sprite from the sprite atlas; opaque where the sprite is. */
#define HWREND_MODE_SPRITE_GHOST 101 /**< The same, half transparent. */
#define HWREND_MODE_FLAT 102         /**< Flat colour. */
#define HWREND_MODE_FLAT_GHOST 103   /**< Flat colour, half transparent. */
#define HWREND_MODE_SMOKE 105        /**< A puff of smoke, drawn by the shader; blended over the screen. */
#define HWREND_MODE_FLAME 104        /**< A tongue of flame, drawn by the shader; added to the screen. */

/** Records drawing of a sprite into the given area of the screen buffer.
 *
 * @param x0,y0,x1,y1 The area, from the first pixel to one after the last one.
 * @param ghost The sprite is drawn half transparent.
 * @param remap Colour map for the sprite colours, or NULL.
 * @param one_colour Colour to draw the whole sprite with, or -1.
 * @return True if the sprite was recorded, false if it has to be drawn by the software.
 */
TbBool LbHwRecordSprite(const struct TbSprite *spr, long x0, long y0, long x1, long y1,
  TbBool flip_h, TbBool flip_v, TbBool ghost, const ubyte *remap, int one_colour);

/** Records a sprite draw as the sprite functions get it; takes the flip and transparency
 * from lbDisplay.DrawFlags.
 *
 * @param dw,dh Size to draw the sprite in, or 0 for the sprite size.
 */
TbBool LbHwRecordSpriteDraw(const struct TbSprite *spr, long x, long y, long dw, long dh,
  const ubyte *remap, int one_colour);

/** Records a sprite draw which uses the scaling set by SetAlphaScalingData(). */
TbBool LbHwRecordSpriteDrawAlphaScaled(const struct TbSprite *spr, long posx, long posy);

/** Records a filled rectangle, from the first pixel to one after the last one. */
TbBool LbHwRecordBox(long x0, long y0, long x1, long y1, ubyte colour, TbBool ghost);

/** Records a line, one pixel wide. */
/** Tells that sprite data was loaded again, so the copies of it made for the GPU are outdated. */
void LbHwSpriteCacheClear(void);
/** Records a flame drawn by the GPU, instead of a sprite: a quad on the screen of the given size, in which
 * the shader draws a flickering flame. @param seed makes each flame different. @return false if the GPU
 * is not drawing the scene, and the sprite has to be drawn. */
/** A puff of coloured smoke, for the end of the frame. @param kind 0 - psycho gas (purple), 1 - knock out gas (grey).
 * The puffs are drawn over the scene when LbHwFlushSmoke() is called while recording. */
/** Records a puff of smoke at once, in the order of drawing (in place of a sprite). */
void LbHwRecordSmokePuff(float x0, float y0, float x1, float y1, int seed, int kind, float alpha);
void LbHwNoteSmoke(float sx, float sy, float size, int seed, int kind, float alpha);
void LbHwFlushSmoke(void);
TbBool LbHwRecordFlame(float x0, float y0, float x1, float y1, int seed, float strength);

/** Per-pixel lights. The game gives the lights near the camera of the frame, as positions on the
 * screen (where the light touches the ground) with a radius on the ground and an intensity; and how
 * the ground is seen from the camera; the GPU adds their light to the shade of the pixels around the
 * position on the screen, which also lights what stands there, such as the walls of buildings. */
#define HW_PLIGHTS_MAX 64
/** @param aff Four numbers, a 2x2 matrix which takes a distance on the screen (x, y) to a distance on
 *   the ground: ground X is aff[0] * x + aff[1] * y; ground Z is aff[2] * x + aff[3] * y.
 * @param lights Four numbers for each light: screen X, screen Y, radius on the ground, intensity. */
void LbHwSetPixelLights(const float *aff, const float *lights, int count);
int LbHwGetPixelLights(const float **p_aff, const float **p_lights);
TbBool LbHwPixelLightsEnabled(void);
void LbHwPixelLightsSetEnabled(TbBool enabled);
/** Beams (weapon shots, lasers): the game says where a beam is on the screen, in the frame which is being
 * drawn. The GPU lights the ground around it with the colour of the beam, brighter near the muzzle. */
#define HW_BEAMS_MAX 32
void LbHwNoteBeam(float sx1, float sy1, float sx2, float sy2, int colour, int itime);
/** Gives the beams of the frame: for each, four numbers (screen x1, y1, x2, y2) in `a` and four (palette
 * colour, strength, radius, 0) in `b`; returns how many. */
int LbHwGetBeams(const float **p_a, const float **p_b, const float **p_c);
/** A weapon which is charging or firing: a flash of light at the muzzle on the screen, with the
 * strength from 0 to 1. Not tied to a beam, so it starts as soon as the charging does. */
/** @param radius Reach of the light on the ground, in map units.
 * @param dirx, dirz Direction the light is thrown in, on the ground; (0,0) for a light which goes all round. */
void LbHwNoteMuzzle(float sx, float sy, int colour, float strength, float radius, float dirx, float dirz);

/** The game says where a short lived light (explosion, flash) is in the given game turn. */
void LbHwNoteDynamicLight(long turn, float x, float z, float intensity);
/** Gives the short lived lights of the turn as triples x, z, intensity; returns how many. */
int LbHwGetDynamicLights(long turn, float *out, int max_count);
TbBool LbHwRecordLine(long x1, long y1, long x2, long y2, ubyte colour, TbBool ghost);

/** The sprite atlas, 8-bit indexed picture holding the sprites which were recorded; 0 is transparent.
 * The rows from dirty_y0 up to dirty_y1 (not including) have changed since the previous
 * call of LbHwSpriteAtlasClean().
 */
const ubyte *LbHwSpriteAtlas(int *p_width, int *p_height, int *p_dirty_y0, int *p_dirty_y1);
void LbHwSpriteAtlasClean(void);

/** The colour maps used by the recorded sprites, each 256 bytes. */
const ubyte *LbHwRemapTables(int *p_count);

/** Tells the recorder where the texture pages are; the pages are 256x256 bytes each,
 * and an entry may be NULL. Triangles using a texture from elsewhere are not recorded.
 */
void LbHwSetTexturePages(ubyte * const *pages, int count);
const ubyte *LbHwTexturePage(int index);

/** The texture page as it was when the drawing of the frame ended. */
const ubyte *LbHwTexturePageSnapshot(int index);
int LbHwTexturePagesCount(void);

/** Access to the recorded data, for the code which draws it. */
const struct HwVertex *LbHwRecordedVertices(long *p_count);

/** Whether anything was recorded since the last LbHwFrameBegin(). */
TbBool LbHwHasRecorded(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif // BFLIBRARY_BFHWREND_H_

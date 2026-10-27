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

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
#define HWREND_MODE_BLOOD 106        /**< A pool of blood, drawn by the shader from the shape of a sprite; blended over the screen. */
#define HWREND_MODE_BLOOD_DROP 107   /**< A flying drop of blood, a streak along its way; blended over the screen. */
#define HWREND_MODE_BLOOD_SPLAT 108  /**< A splatter of blood on the ground; blended over the screen. */
#define HWREND_MODE_SPARK 109        /**< A hot spark in flight, a glowing streak; added to the screen. */
#define HWREND_MODE_JET 112          /**< A tongue of the flame thrower, from the gun to a puff: fire which widens and cools along it; blended over the scene. */
#define HWREND_MODE_ARC 111          /**< A segment of an electric arc: a blue-white core with a soft blue glow; added to the screen. */
#define HWREND_MODE_BEAM 110        /**< A beam of a laser: a white-hot core in a halo of the colour of the beam, with an electric flicker; added to the screen. */

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
TbBool LbHwRecordSmokePuff(float x0, float y0, float x1, float y1, int seed, int kind, float alpha);
void LbHwNoteSmoke(float sx, float sy, float size, int seed, int kind, float alpha);
void LbHwFlushSmoke(void);
TbBool LbHwRecordFlame(float x0, float y0, float x1, float y1, int seed, float strength);

/** Records a pool of blood in place of the sprite which draws it. The GPU draws a glossy liquid with the
 * outline of the sprite, a bit wider, moving slowly. Works with the enhanced lighting only.
 * @param x0,y0,x1,y1 The area of the sprite on the screen.
 * @param seed Makes each pool a bit different.
 * @param bright Lighting of the place, 1.0 for the neutral one.
 * @param growth Size of the pool compared to the sprite, from 0 (a spot) to 1; it grows as the person bleeds.
 * @return False if the GPU does not draw it, and the sprite has to be drawn. */
TbBool LbHwRecordBloodPool(const struct TbSprite *spr, long x0, long y0, long x1, long y1, int seed, float bright,
  float growth);
/** Records a drop of blood in flight: a streak from the place where it was (tail) to where it is (head).
 * @param width Width of the streak in pixels. @param bright Lighting of the place, 1.0 for the neutral one.
 * @return False if the GPU does not draw it. */
TbBool LbHwRecordBloodStreak(float hx, float hy, float tx, float ty, float width, float alpha, int seed, float bright);
/** Records a hot spark in flight (from a ricochet): a glowing streak from the tail to the head, orange at the
 * tail and white at the head, which is added to the screen; the alpha is how hot it still is. */
TbBool LbHwRecordSparkStreak(float hx, float hy, float tx, float ty, float width, float alpha, int seed);
/** Records a splatter of blood on the ground, an ellipse around the centre.
 * @param age 0 for fresh and wet, 1 for dried. @param dirx,diry Direction of the spatter, in the plane of the
 *   ground (a circle before it is flattened to the ellipse); the splatter has spikes that way.
 * @return False if the GPU does not draw it. */
TbBool LbHwRecordBloodSplat(float cx, float cy, float rx, float ry, int seed, float age, float alpha,
  float bright, float dirx, float diry);
/** The same for a splatter on any surface, such as a wall: the circle of the splatter is a parallelogram on the
 * screen, with the half-sides (ux, uy) and (vx, vy); the direction of the spatter is in that circle, where
 * (0, 1) is along the second side. @param tongue How long the tongue and the rays in the direction of the spatter
 * are, 1.0 for a splatter on the ground, less for a stain on a wall. */
TbBool LbHwRecordBloodSplatOriented(float cx, float cy, float ux, float uy, float vx, float vy, int seed,
  float age, float alpha, float bright, float dirx, float diry, float tongue);
/** The same for the sprite drawn with the scaling set by SetAlphaScalingData(), like LbHwRecordSpriteDrawAlphaScaled(). */
TbBool LbHwRecordBloodPoolAlphaScaled(const struct TbSprite *spr, long posx, long posy, int seed, float bright,
  float growth);

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
/** Notes the light which an electric arc throws on the ground around it. */
void LbHwNoteArcLight(float sx1, float sy1, float sx2, float sy2, int colour);
/** Records one piece of a glowing laser beam (kind 0, strength 0..1), of a flame thrower jet (kind 2, param the age\r\n * in percents, strength the half width in pixels, colour a seed) or of a lightning (kind 1, param bit 0 = a
 * branch, bit 1 = white flash). Called when the line of the piece is drawn, so that the scene in front of it covers
 * it. Works with the enhanced lighting only; returns false when the caller has to draw the plain line. */
TbBool LbHwRecordGlowSeg(float sx1, float sy1, float sx2, float sy2, int kind, int param, float strength, int colour);
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

/** Replacement colours: sprite pixels which have one of the marker palette indices are shown by
 * the GPU in an exact colour, not from the palette. The pixel is lit as the original shade
 * (a palette index) would be, so the replaced colours follow the lighting of the scene.
 */
#define HW_CUSTOM_MAX 24
/** @param markers Palette indices which the sprites have in place of the original shades.
 * @param orig The original shade (palette index) of each marker.
 * @param rgb Three bytes for each marker: the colour to show, when the original shade is not darkened. */
void LbHwCustomColoursSet(int count, const ubyte *markers, const ubyte *orig, const ubyte *rgb);
/** Gives the replacement colours for upload to the GPU: `markers` has HW_CUSTOM_MAX floats; `data` has
 * four for each marker (red, green, blue in 0..1, and the original shade). Returns the count. */
int LbHwCustomColoursGet(const float **p_markers, const float **p_data);
/** Tells that the software screen, not only the sprites, has the marker indices to be replaced. */
void LbHwCustomLayerSet(TbBool on);
TbBool LbHwCustomLayer(void);

/** A palette index of the software screen which is shown as a translucent colour over what the
 * GPU drew; for the boxes of the pause screen. */
#define HWREND_OVERLAY_INDEX 254
/** Sets the palette colour of the translucent overlay, or -1 to have no such index. */
/** The colours of the interface (panels, and the lines and text of the accent) in the missions: the pixels of
 * the screen which have the colours of the interface are shown by the GPU in the chosen colour, with their own
 * shades kept. Part 0 is the panels, 1 the accent colour. */
#define HW_UI_TINT_PARTS 2
void LbHwUiTintSet(int part, TbBool on, const ubyte *rgb, ubyte alpha);
/** Gives for each part four numbers: the colour in 0..1 and 1 if it is on. */
const float *LbHwUiTintGet(void);
/** Gives for each part the opacity of its colour, 0..1: below 1 the scene shows through. */
const float *LbHwUiAlphaGet(void);
/** Opacity (0..1) of the textured triangles which are recorded from now on; 1 is solid. The buildings are seen
 * through when the player holds the key for it. */
void LbHwFaceAlphaSet(float alpha);
void LbHwOverlaySet(int colour);
int LbHwOverlay(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif // BFLIBRARY_BFHWREND_H_

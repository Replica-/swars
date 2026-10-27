/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file sglrend.c
 *     Drawing of the recorded 3D scene on the GPU.
 * @par Purpose:
 *     Takes the triangles recorded by ghwrend.c and draws them with OpenGL, in
 *     the order in which the software renderer would, using the same textures,
 *     palette and colour tables as the software renderer does.
 * @par Comment:
 *     None.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "bfgl.h"
#include "privgl.h"
#include "bfhwrend.h"
#include "bfgentab.h"
#include "bfpalette.h"
#include "privbflog.h"

#define TEXPAGE_SIZE 256
#define TEXPAGES_MAX 32

static GLuint scene_program = 0;
static GLuint scene_vao = 0;
static GLuint scene_vbo = 0;
static GLuint tex_pages = 0;
static GLuint tex_fade = 0;
static GLuint tex_atlas = 0;
static GLuint tex_remap = 0;
static TbBool atlas_needs_full_upload = true;
static TbBool scene_ready = false;
static TbBool scene_failed = false;
static GLint loc_bcount = -1, loc_bgain = -1, loc_ba = -1, loc_bb = -1;
static GLint loc_lm0 = -1, loc_lm1 = -1, loc_lcount = -1, loc_lgain = -1, loc_lights = -1;
static GLint loc_screen = -1, loc_pages = -1, loc_fade = -1, loc_pal = -1, loc_enh = -1, loc_atlas = -1, loc_remap = -1, loc_bias = -1, loc_skip = -1;


static const char *scene_vs_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec3 aUVS;\n"
    "layout(location = 2) in vec3 aMC;\n"
    "layout(location = 3) in vec2 aMax;\n"
    "layout(location = 4) in vec4 aTriA;\n"
    "layout(location = 5) in vec2 aTriB;\n"
    "uniform vec2 uScreen;\n"
    "out vec3 vUVS;\n"
    "flat out vec3 vMC;\n"
    "flat out vec2 vMax;\n"
    "flat out vec4 vTriA;\n"
    "flat out vec2 vTriB;\n"
    "out vec2 vPos;\n"
    "void main()\n"
    "{\n"
    "    // The software renderer takes the vertex positions as the pixel positions,\n"
    "    // while GL samples at pixel centres - shift to get the same coverage\n"
    "    // A little more than half a pixel, so that the pixel centres do not fall exactly on\n"
    "    // the vertices and the edges between whole pixel positions - there, a pixel at the\n"
    "    // corner of four tiles can be left out by all four\n"
    "    vec2 p = (aPos + vec2(0.5 + 1.0 / 64.0, 0.5 + 1.0 / 128.0)) / uScreen;\n"
    "    gl_Position = vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);\n"
    "    vUVS = aUVS;\n"
    "    vMC = aMC;\n"
    "    vMax = aMax;\n"
    "    vTriA = aTriA;\n"
    "    vTriB = aTriB;\n"
    "    vPos = aPos;\n"
    "}\n";

static const char *scene_fs_src =
    "#version 330 core\n"
    "in vec3 vUVS;\n"
    "flat in vec3 vMC;\n"
    "flat in vec2 vMax;\n"
    "flat in vec4 vTriA;\n"
    "flat in vec2 vTriB;\n"
    "in vec2 vPos;\n"
    "layout(location = 0) out vec4 outColor;\n"
    "layout(location = 1) out vec4 outGlow;\n"
    "uniform sampler2DArray uPages;\n"
    "uniform sampler2D uFade;\n"
    "uniform sampler2D uPal;\n"
    "uniform sampler2D uAtlas;\n"
    "uniform sampler2D uRemap;\n"
    "uniform int uEnhanced;\n"
    "uniform vec4 uLMat;\n"
    "uniform float uVComp;\n"
    "uniform int uLightCount;\n"
    "uniform float uLightGain;\n"
    "uniform vec4 uLights[64];\n"
    "uniform int uBeamCount;\n"
    "uniform float uBeamGain;\n"
    "uniform vec4 uBeamA[16];\n"
    "uniform vec4 uBeamB[16];\n"
    "uniform float uUvBias;\n"
    "uniform int uSkipMode;\n"
    "\n"
    "int texel(vec2 uv, int layer)\n"
    "{\n"
    "    ivec2 t = ivec2(floor(uv)) & 255;\n"
    "    return int(texelFetch(uPages, ivec3(t, layer), 0).r * 255.0 + 0.5);\n"
    "}\n"
    "int fade(int row, int idx)\n"
    "{\n"
    "    return int(texelFetch(uFade, ivec2(idx & 255, row & 255), 0).r * 255.0 + 0.5);\n"
    "}\n"
    "vec3 pal(int idx)\n"
    "{\n"
    "    return texelFetch(uPal, ivec2(idx & 255, 0), 0).rgb;\n"
    "}\n"
    "float luma(vec3 c)\n"
    "{\n"
    "    return dot(c, vec3(0.30, 0.59, 0.11));\n"
    "}\n"
    "// How much a colour looks like a light: bright and saturated colours are neon\n"
    "// The shade (fade table rows) which the lights near the pixel add; the pixel position on\n"
    "// the map comes from the screen position, as if the pixel was on the ground\n"
    "vec2 toGround(vec2 d)\n"
    "{\n"
    "    // A distance on the screen as a distance on the ground; going up the screen counts a bit less,\n"
    "    // so that the light reaches further up the walls which stand on the ground\n"
    "    d.y *= uVComp;\n"
    "    return vec2(uLMat.x * d.x + uLMat.y * d.y, uLMat.z * d.x + uLMat.w * d.y);\n"
    "}\n"
    "float lightDelta(vec2 p)\n"
    "{\n"
    "    if (uLightCount == 0)\n"
    "        return 0.0;\n"
    "    float s = 0.0;\n"
    "    for (int i = 0; i < uLightCount; i++) {\n"
    "        vec4 L = uLights[i];\n"
    "        float f = max(1.0 - length(toGround(p - L.xy)) / L.z, 0.0);\n"
    "        s += L.w * f * f;\n"
    "    }\n"
    "    return s * uLightGain;\n"
    "}\n"
    "// Coloured light which the beams (shots, lasers) throw on the ground around them, brighter near\n"
    "// the muzzle, where the beam starts\n"
    "vec3 beamGlow(vec2 p)\n"
    "{\n"
    "    if (uBeamCount == 0)\n"
    "        return vec3(0.0);\n"
    "    vec3 g = vec3(0.0);\n"
    "    for (int i = 0; i < uBeamCount; i++) {\n"
    "        vec4 A = uBeamA[i];\n"
    "        vec4 B = uBeamB[i];\n"
    "        vec2 tp = toGround(p - A.xy);\n"
    "        vec2 tb = toGround(A.zw - A.xy);\n"
    "        float t = clamp(dot(tp, tb) / max(dot(tb, tb), 1.0), 0.0, 1.0);\n"
    "        float f = max(1.0 - length(tp - tb * t) / B.z, 0.0);\n"
    "        f *= f;\n"
    "        float m = max(1.0 - length(tp) / (B.z * 1.6), 0.0);\n"
    "        f += m * m;\n"
    "        g += pal(int(B.x + 0.5)) * (B.y * f);\n"
    "    }\n"
    "    return g * uBeamGain;\n"
    "}\n"
    "float neonAmount(vec3 c)\n"
    "{\n"
    "    float mx = max(c.r, max(c.g, c.b));\n"
    "    float mn = min(c.r, min(c.g, c.b));\n"
    "    float sat = (mx - mn) / max(mx, 0.001);\n"
    "    return smoothstep(0.50, 0.80, mx) * smoothstep(0.35, 0.65, sat);\n"
    "}\n"
    "// Colour of a palette entry lit by the given shade (the fade table row, 32 is neutral)\n"
    "vec3 shaded(int idx, float shade)\n"
    "{\n"
    "    if (uEnhanced == 0)\n"
    "        return pal(fade(int(floor(shade)), idx));\n"
    "    // Between two rows of the fade table, mix their colours, instead of jumping\n"
    "    // from one to the other; the table itself says how the shade darkens each colour\n"
    "    // The software takes the whole part of the shade, so a row stands for the shade\n"
    "    // from r to r+1; the middle of that is what the smooth value has to give for r\n"
    "    float s = clamp(shade - 0.5, 0.0, 62.999);\n"
    "    int r = int(s);\n"
    "    vec3 c = mix(pal(fade(r, idx)), pal(fade(r + 1, idx)), s - float(r));\n"
    "    // Neon colours give light, so the shade of the place does not darken them,\n"
    "    // and they are a bit stronger than the palette has them\n"
    "    vec3 base = pal(idx);\n"
    "    c = mix(c, base * 1.25, neonAmount(base));\n"
    "    // Slightly under what the table gives: the software renderer is a little darker\n"
    "    // than the smooth shade comes out, as its colours are rounded to the palette\n"
    "    c *= 0.965;\n"
    "    // Soft shoulder, so that what is brighter than the screen can show\n"
    "    vec3 over = max(c - 0.8, 0.0);\n"
    "    return min(c, vec3(0.8)) + 0.2 * tanh(over / 0.2);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    int mode = int(vMC.x + 0.5);\n"
    "    if (mode == uSkipMode) discard;\n"
    "    int colour = int(floor(vMC.y + 0.5));\n"
    "    int layer = int(floor(vMC.z + 0.5));\n"
    "    int row = int(floor(vUVS.z)) & 255;\n"
    "    int tx = 0;\n"
    "    if (layer >= 0 && mode < 100)\n"
    "        tx = texel(vUVS.xy, layer);\n"
    "    vec3 rgb = vec3(0.0);\n"
    "    float alpha = 1.0;\n"
    "    float ld = (uEnhanced != 0) ? lightDelta(vPos) : 0.0;\n"
    "    float sh = vUVS.z + ld;\n"
    "    if (mode >= 100) {\n"
    "        if (mode >= 102) {\n"
    "            // flat colour\n"
    "            rgb = pal(colour);\n"
    "            alpha = (mode == 103) ? 0.5 : 1.0;\n"
    "        } else {\n"
    "            // a sprite from the atlas, where 0 is the transparent colour\n"
    "            int a = int(texelFetch(uAtlas, ivec2(floor(vUVS.xy)), 0).r * 255.0 + 0.5);\n"
    "            if (a == 0) discard;\n"
    "            int idx = a;\n"
    "            if (layer >= 0)\n"
    "                idx = int(texelFetch(uRemap, ivec2(a, layer), 0).r * 255.0 + 0.5);\n"
    "            if (colour >= 0)\n"
    "                idx = colour;\n"
    "            rgb = pal(idx);\n"
    "            alpha = (mode == 101) ? 0.5 : 1.0;\n"
    "        }\n"
    "    } else if (mode == 0) {\n"
    "        rgb = pal(colour);\n"
    "    } else if (mode == 1) {\n"
    "        rgb = pal(row);\n"
    "    } else if (mode == 2 || mode == 27) {\n"
    "        rgb = pal(tx);\n"
    "    } else if (mode == 3) {\n"
    "        if (tx == 0) discard;\n"
    "        rgb = pal(tx);\n"
    "    } else if (mode == 4) {\n"
    "        rgb = shaded(colour, sh);\n"
    "    } else if (mode == 5) {\n"
    "        rgb = shaded(tx, sh);\n"
    "    } else if (mode == 6) {\n"
    "        if (tx == 0) discard;\n"
    "        rgb = shaded(tx, sh);\n"
    "    } else if (mode == 7 || mode == 11) {\n"
    "        rgb = (colour == 32) ? pal(tx) : shaded(tx, float(colour) + 0.5 + ld);\n"
    "    } else if (mode == 8) {\n"
    "        if (tx == 0) discard;\n"
    "        rgb = shaded(tx, float(colour) + 0.5 + ld);\n"
    "    } else if (mode == 9 || mode == 10) {\n"
    "        // Darkens what is below, by a fade table row: the one of the texel (mode 9), or the\n"
    "        // one given in the colour (mode 10); a texel of 0 leaves the screen as it is\n"
    "        if (tx == 0) discard;\n"
    "        float frow = (mode == 9) ? float(tx) : float(colour);\n"
    "        outColor = vec4(0.0, 0.0, 0.0, clamp(1.0 - frow / 32.0, 0.0, 1.0));\n"
    "        outGlow = outColor;\n"
    "        return;\n"
    "    } else {\n"
    "        // Modes which mix with what is already on the screen; approximated\n"
    "        // by a blend of the colour which the mode would put there\n"
    "        alpha = 0.5;\n"
    "        if (mode == 14 || mode == 15) {\n"
    "            rgb = pal(colour);\n"
    "        } else if (mode == 16 || mode == 17) {\n"
    "            rgb = shaded(colour, sh);\n"
    "        } else if (mode == 12 || mode == 13) {\n"
    "            alpha = 1.0;\n"
    "            rgb = pal(tx);\n"
    "        } else if (mode == 18 || mode == 19) {\n"
    "            rgb = pal(tx);\n"
    "        } else if (mode == 20 || mode == 21) {\n"
    "            rgb = shaded(tx, sh);\n"
    "        } else if (mode == 22 || mode == 23) {\n"
    "            if (tx == 0) discard;\n"
    "            rgb = pal(tx);\n"
    "        } else if (mode == 24 || mode == 25) {\n"
    "            if (tx == 0) discard;\n"
    "            rgb = shaded(tx, sh);\n"
    "        } else {\n"
    "            rgb = shaded(tx, sh);\n"
    "        }\n"
    "    }\n"    "    if (mode < 100 && alpha >= 1.0 && uEnhanced != 0 && uBeamCount > 0)\n"
    "        rgb += beamGlow(vPos) * (0.35 + 1.3 * luma(rgb));\n"
    "    bool isSprite = (mode == 100 || mode == 101);\n"
    "    if (alpha < 1.0 && uEnhanced != 0 && !isSprite) {\n"
    "        // What is drawn half transparent and is bright - lights, glows, lasers - is added to\n"
    "        // the screen instead of mixed with it, so that it glows, as neon does\n"
    "        float w = smoothstep(0.30, 0.65, max(rgb.r, max(rgb.g, rgb.b)));\n"
    "        outColor = vec4(rgb * (w * 1.4 + (1.0 - w) * alpha), alpha * (1.0 - w));\n"
    "    } else {\n"
    "        outColor = vec4(rgb * alpha, alpha);\n"
    "    }\n"
    "    // Sprites (smoke, people, effects) do not glow, but they hide the glow behind them\n"
    "    outGlow = isSprite ? vec4(0.0, 0.0, 0.0, outColor.a) : outColor;\n"
    "}\n";

static GLuint post_vao = 0;
static GLuint bright_program = 0, blur_program = 0, compose_program = 0;
static GLuint scene_fbo = 0, scene_tex = 0, glow_tex = 0;
static GLuint bloom_fbo[2] = {0, 0}, bloom_tex[2] = {0, 0};
static long post_buf_w = 0, post_buf_h = 0;
static TbBool post_failed = false;

static void LbGlSceneFree(void)
{
    post_vao = 0;
    bright_program = blur_program = compose_program = 0;
    scene_fbo = scene_tex = glow_tex = 0;
    bloom_fbo[0] = bloom_fbo[1] = 0;
    bloom_tex[0] = bloom_tex[1] = 0;
    post_buf_w = post_buf_h = 0;
    post_failed = false;
    scene_ready = false;
    scene_program = 0;
    scene_vao = 0;
    scene_vbo = 0;
    tex_pages = 0;
    tex_fade = 0;
    tex_atlas = 0;
    tex_remap = 0;
}

static TbBool LbGlSceneInit(void)
{
    scene_program = LbGlBuildProgram("scene", scene_vs_src, scene_fs_src);
    if (scene_program == 0)
        return false;
    loc_screen = gl.GetUniformLocation(scene_program, "uScreen");
    loc_pages = gl.GetUniformLocation(scene_program, "uPages");
    loc_fade = gl.GetUniformLocation(scene_program, "uFade");
    loc_pal = gl.GetUniformLocation(scene_program, "uPal");
    loc_enh = gl.GetUniformLocation(scene_program, "uEnhanced");
    loc_lm0 = gl.GetUniformLocation(scene_program, "uLMat");
    loc_lm1 = gl.GetUniformLocation(scene_program, "uVComp");
    loc_lcount = gl.GetUniformLocation(scene_program, "uLightCount");
    loc_lgain = gl.GetUniformLocation(scene_program, "uLightGain");
    loc_lights = gl.GetUniformLocation(scene_program, "uLights");
    loc_bcount = gl.GetUniformLocation(scene_program, "uBeamCount");
    loc_bgain = gl.GetUniformLocation(scene_program, "uBeamGain");
    loc_ba = gl.GetUniformLocation(scene_program, "uBeamA");
    loc_bb = gl.GetUniformLocation(scene_program, "uBeamB");
    loc_atlas = gl.GetUniformLocation(scene_program, "uAtlas");
    loc_bias = gl.GetUniformLocation(scene_program, "uUvBias");
    loc_skip = gl.GetUniformLocation(scene_program, "uSkipMode");
    loc_remap = gl.GetUniformLocation(scene_program, "uRemap");

    gl.GenVertexArrays(1, &scene_vao);
    gl.BindVertexArray(scene_vao);
    gl.GenBuffers(1, &scene_vbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, scene_vbo);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, x));
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, u));
    gl.EnableVertexAttribArray(2);
    gl.VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, mode));
    gl.EnableVertexAttribArray(3);
    gl.VertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, umax));
    gl.EnableVertexAttribArray(4);
    gl.VertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, tx0));
    gl.EnableVertexAttribArray(5);
    gl.VertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(struct HwVertex),
      (const void *)offsetof(struct HwVertex, tx2));

    gl.GenTextures(1, &tex_pages);
    gl.BindTexture(GL_TEXTURE_2D_ARRAY, tex_pages);
    gl.TexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, TEXPAGE_SIZE, TEXPAGE_SIZE, TEXPAGES_MAX,
      0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    gl.GenTextures(1, &tex_atlas);
    gl.BindTexture(GL_TEXTURE_2D, tex_atlas);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 2048, 2048, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    atlas_needs_full_upload = true;

    gl.GenTextures(1, &tex_remap);
    gl.BindTexture(GL_TEXTURE_2D, tex_remap);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 256, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl.GenTextures(1, &tex_fade);
    gl.BindTexture(GL_TEXTURE_2D, tex_fade);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 256, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

/* Bloom: the scene goes to a buffer of floating point colours, bright colours are taken from it
 * into a quarter size buffer, blurred, and added back when the scene goes to the window. */
#define BLOOM_DIV 4

static const char *post_vs_src =
    "#version 330 core\n"
    "out vec2 uv;\n"
    "void main()\n"
    "{\n"
    "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "    uv = p;\n"
    "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char *bright_fs_src =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D uSrc;\n"
    "uniform vec2 uTexel;\n"
    "void main()\n"
    "{\n"
    "    // Average of the source pixels which this one covers, bright and colourful parts only\n"
    "    vec3 sum = vec3(0.0);\n"
    "    for (int y = 0; y < 4; y++) {\n"
    "        for (int x = 0; x < 4; x++) {\n"
    "            vec3 c = texture(uSrc, uv + (vec2(x, y) - 1.5) * uTexel).rgb;\n"
    "            float mn = min(c.r, min(c.g, c.b));\n"
    "            // Only what is close to white, so the yellows and greens do not glow\n"
    "            sum += c * smoothstep(0.65, 0.95, mn);\n"
    "        }\n"
    "    }\n"
    "    outColor = vec4(sum * (1.0 / 16.0), 1.0);\n"
    "}\n";

static const char *blur_fs_src =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D uSrc;\n"
    "uniform vec2 uDir;\n"
    "void main()\n"
    "{\n"
    "    // Gaussian with two samples per bilinear fetch\n"
    "    vec3 c = texture(uSrc, uv).rgb * 0.2270270270;\n"
    "    c += texture(uSrc, uv + uDir * 1.3846153846).rgb * 0.3162162162;\n"
    "    c += texture(uSrc, uv - uDir * 1.3846153846).rgb * 0.3162162162;\n"
    "    c += texture(uSrc, uv + uDir * 3.2307692308).rgb * 0.0702702703;\n"
    "    c += texture(uSrc, uv - uDir * 3.2307692308).rgb * 0.0702702703;\n"
    "    outColor = vec4(c, 1.0);\n"
    "}\n";

static const char *compose_fs_src =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D uScene;\n"
    "uniform sampler2D uBloom;\n"
    "uniform float uStrength;\n"
    "void main()\n"
    "{\n"
    "    vec3 c = texture(uScene, uv).rgb + texture(uBloom, uv).rgb * uStrength;\n"
    "    outColor = vec4(min(c, vec3(1.0)), 1.0);\n"
    "}\n";


static GLuint LbGlMakeTarget(GLuint *p_tex, long w, long h)
{
    GLuint fbo;

    gl.GenTextures(1, p_tex);
    gl.BindTexture(GL_TEXTURE_2D, *p_tex);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.GenFramebuffers(1, &fbo);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *p_tex, 0);
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 0;
    return fbo;
}

static TbBool LbGlPostPrepare(long buf_w, long buf_h)
{
    if (post_failed)
        return false;
    if (post_vao == 0)
    {
        bright_program = LbGlBuildProgram("bloom bright", post_vs_src, bright_fs_src);
        blur_program = LbGlBuildProgram("bloom blur", post_vs_src, blur_fs_src);
        compose_program = LbGlBuildProgram("bloom compose", post_vs_src, compose_fs_src);
        if ((bright_program == 0) || (blur_program == 0) || (compose_program == 0)) {
            post_failed = true;
            return false;
        }
        gl.GenVertexArrays(1, &post_vao);
    }
    if ((post_buf_w != buf_w) || (post_buf_h != buf_h))
    {
        if (scene_tex != 0) {
            gl.DeleteTextures(1, &scene_tex);
            gl.DeleteTextures(1, &glow_tex);
            gl.DeleteFramebuffers(1, &scene_fbo);
            gl.DeleteTextures(2, bloom_tex);
            gl.DeleteFramebuffers(2, bloom_fbo);
        }
        scene_fbo = LbGlMakeTarget(&scene_tex, buf_w, buf_h);
        if (scene_fbo != 0) {
            // The second target of the scene: what is allowed to glow
            GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            GLuint dummy;
            gl.GenTextures(1, &glow_tex);
            gl.BindTexture(GL_TEXTURE_2D, glow_tex);
            gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, buf_w, buf_h, 0, GL_RGBA, GL_FLOAT, NULL);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl.BindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
            gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, glow_tex, 0);
            gl.DrawBuffers(2, bufs);
            if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                scene_fbo = 0;
            (void)dummy;
        }
        bloom_fbo[0] = LbGlMakeTarget(&bloom_tex[0], buf_w / BLOOM_DIV, buf_h / BLOOM_DIV);
        bloom_fbo[1] = LbGlMakeTarget(&bloom_tex[1], buf_w / BLOOM_DIV, buf_h / BLOOM_DIV);
        post_buf_w = buf_w;
        post_buf_h = buf_h;
        if ((scene_fbo == 0) || (bloom_fbo[0] == 0) || (bloom_fbo[1] == 0)) {
            LOGERR("floating point buffers are not available, no bloom");
            post_failed = true;
            gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }
    return true;
}

TbBool LbGlSceneDrawWithBloom(GLuint pal_texture, long buf_width, long buf_height,
  int win_width, int win_height)
{
    long bw, bh;
    GLint loc;

    if (!LbGlSceneIsAvailable() || !LbGlPostPrepare(buf_width, buf_height))
        return false;
    bw = buf_width / BLOOM_DIV;
    bh = buf_height / BLOOM_DIV;

    // The scene into the floating point buffer
    gl.BindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
    gl.Viewport(0, 0, buf_width, buf_height);
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    LbGlSceneDraw(pal_texture, buf_width, buf_height);

    gl.Disable(GL_BLEND);
    gl.BindVertexArray(post_vao);

    // The bright parts, reduced
    gl.BindFramebuffer(GL_FRAMEBUFFER, bloom_fbo[0]);
    gl.Viewport(0, 0, bw, bh);
    gl.UseProgram(bright_program);
    gl.ActiveTexture(GL_TEXTURE0 + 6);
    gl.BindTexture(GL_TEXTURE_2D, glow_tex);
    gl.Uniform1i(gl.GetUniformLocation(bright_program, "uSrc"), 6);
    gl.Uniform2f(gl.GetUniformLocation(bright_program, "uTexel"), 1.0f / (float)buf_width, 1.0f / (float)buf_height);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);

    // Blurred, in two goes: horizontally, then vertically; and again for a wider glow
    gl.UseProgram(blur_program);
    loc = gl.GetUniformLocation(blur_program, "uDir");
    gl.Uniform1i(gl.GetUniformLocation(blur_program, "uSrc"), 6);
    {
        int pass;
        for (pass = 0; pass < 2; pass++)
        {
            gl.BindFramebuffer(GL_FRAMEBUFFER, bloom_fbo[1]);
            gl.BindTexture(GL_TEXTURE_2D, bloom_tex[0]);
            gl.Uniform2f(loc, 1.0f / (float)bw, 0.0f);
            gl.DrawArrays(GL_TRIANGLES, 0, 3);
            gl.BindFramebuffer(GL_FRAMEBUFFER, bloom_fbo[0]);
            gl.BindTexture(GL_TEXTURE_2D, bloom_tex[1]);
            gl.Uniform2f(loc, 0.0f, 1.0f / (float)bh);
            gl.DrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

    // Scene with the glow, to the window
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Viewport(0, 0, win_width, win_height);
    gl.UseProgram(compose_program);
    gl.ActiveTexture(GL_TEXTURE0 + 6);
    gl.BindTexture(GL_TEXTURE_2D, scene_tex);
    gl.ActiveTexture(GL_TEXTURE0 + 7);
    gl.BindTexture(GL_TEXTURE_2D, bloom_tex[0]);
    gl.Uniform1i(gl.GetUniformLocation(compose_program, "uScene"), 6);
    gl.Uniform1i(gl.GetUniformLocation(compose_program, "uBloom"), 7);
    gl.Uniform1f(gl.GetUniformLocation(compose_program, "uStrength"), 1.1f);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    return true;
}

void LbGlSceneDestroy(void)
{
    LbGlSceneFree();
    scene_failed = false;
}

TbBool LbGlSceneIsAvailable(void)
{
    if (scene_ready)
        return true;
    if (scene_failed || !LbGlIsActive())
        return false;
    if (!LbGlSceneInit()) {
        LOGERR("hardware scene rendering is not available");
        scene_failed = true;
        return false;
    }
    scene_ready = true;
    return true;
}

void LbGlSceneDraw(GLuint pal_texture, long buf_width, long buf_height)
{
    const struct HwVertex *verts;
    long count;
    int i, pages_num;

    if (!LbGlSceneIsAvailable())
        return;
    verts = LbHwRecordedVertices(&count);
    if ((verts == NULL) || (count <= 0))
        return;

    // Texture pages and the tables change from frame to frame
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.ActiveTexture(GL_TEXTURE0 + 2);
    gl.BindTexture(GL_TEXTURE_2D_ARRAY, tex_pages);
    pages_num = LbHwTexturePagesCount();
    if (pages_num > TEXPAGES_MAX)
        pages_num = TEXPAGES_MAX;
    for (i = 0; i < pages_num; i++)
    {
        const ubyte *page = LbHwTexturePageSnapshot(i);
        if (page == NULL)
            continue;
        gl.TexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, TEXPAGE_SIZE, TEXPAGE_SIZE, 1,
          GL_RED, GL_UNSIGNED_BYTE, page);
    }
    gl.ActiveTexture(GL_TEXTURE0 + 3);
    gl.BindTexture(GL_TEXTURE_2D, tex_fade);
    // The fade table is followed in memory by the ghost table. Software rendering
    // reads past the fade table for shade values over its size, so does this.
    gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RED, GL_UNSIGNED_BYTE, pixmap.fade_table);
    // Sprites, and the colour maps which they use
    {
        const ubyte *atlas;
        const ubyte *remaps;
        int aw, ah, dy0, dy1, remap_count;

        atlas = LbHwSpriteAtlas(&aw, &ah, &dy0, &dy1);
        gl.ActiveTexture(GL_TEXTURE0 + 4);
        gl.BindTexture(GL_TEXTURE_2D, tex_atlas);
        if (atlas != NULL) {
            if (atlas_needs_full_upload) {
                dy0 = 0;
                dy1 = ah;
                atlas_needs_full_upload = false;
            }
            if (dy1 > dy0)
                gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, dy0, aw, dy1 - dy0, GL_RED,
                  GL_UNSIGNED_BYTE, atlas + dy0 * aw);
            LbHwSpriteAtlasClean();
        }
        remaps = LbHwRemapTables(&remap_count);
        gl.ActiveTexture(GL_TEXTURE0 + 5);
        gl.BindTexture(GL_TEXTURE_2D, tex_remap);
        if (remap_count > 0)
            gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, remap_count, GL_RED,
              GL_UNSIGNED_BYTE, remaps);
    }
    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.BindTexture(GL_TEXTURE_2D, pal_texture);

    gl.BindVertexArray(scene_vao);
    gl.BindBuffer(GL_ARRAY_BUFFER, scene_vbo);
    gl.BufferData(GL_ARRAY_BUFFER, count * sizeof(struct HwVertex), verts, GL_STREAM_DRAW);

    gl.UseProgram(scene_program);
    gl.Uniform2f(loc_screen, (float)buf_width, (float)buf_height);
    gl.Uniform1i(loc_pages, 2);
    gl.Uniform1i(loc_fade, 3);
    gl.Uniform1i(loc_pal, 1);
    gl.Uniform1f(loc_bias, (getenv("SWFX_HW_UVBIAS") != NULL) ? (float)atof(getenv("SWFX_HW_UVBIAS")) : 0.0f);
    gl.Uniform1i(loc_skip, (getenv("SWFX_HW_SKIPMODE") != NULL) ? atoi(getenv("SWFX_HW_SKIPMODE")) : -1);
    gl.Uniform1i(loc_atlas, 4);
    gl.Uniform1i(loc_remap, 5);
    gl.Uniform1i(loc_enh, (LbHwLightingMode() == HwLight_Enhanced) ? 1 : 0);
    {
        const float *aff, *lts;
        int n;
        static float gain = -1.0f;

        if (gain < 0.0f)
            gain = (getenv("SWFX_PL_GAIN") != NULL) ? (float)atof(getenv("SWFX_PL_GAIN")) : 4.5f;
        n = LbHwGetPixelLights(&aff, &lts);
        gl.Uniform1i(loc_lcount, n);
        {
            static float vcomp = -1.0f;

            if (vcomp < 0.0f)
                vcomp = (getenv("SWFX_PL_VCOMP") != NULL) ? (float)atof(getenv("SWFX_PL_VCOMP")) : 0.65f;
            gl.Uniform4fv(loc_lm0, 1, aff);
            gl.Uniform1f(loc_lm1, vcomp);
        }
        if (n > 0) {
            gl.Uniform1f(loc_lgain, gain);
            gl.Uniform4fv(loc_lights, n, lts);
        }
        {
            const float *ba, *bb;
            static float bgain = -1.0f;
            int nb;

            if (bgain < 0.0f)
                bgain = (getenv("SWFX_BEAM_GAIN") != NULL) ? (float)atof(getenv("SWFX_BEAM_GAIN")) : 1.2f;
            nb = LbHwGetBeams(&ba, &bb);
            gl.Uniform1i(loc_bcount, nb);
            if (nb > 0) {
                gl.Uniform1f(loc_bgain, bgain);
                gl.Uniform4fv(loc_ba, nb, ba);
                gl.Uniform4fv(loc_bb, nb, bb);
            }
        }
    }
    gl.Enable(GL_BLEND);
    // The colours which the shader gives are already multiplied by their alpha
    gl.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl.DrawArrays(GL_TRIANGLES, 0, count);
    gl.Disable(GL_BLEND);
}

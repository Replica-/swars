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
#include <time.h>
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
static GLint loc_bc = -1;
static GLint loc_time = -1;
static GLint loc_bcount = -1, loc_bgain = -1, loc_ba = -1, loc_bb = -1;
static GLint loc_lm0 = -1, loc_lm1 = -1, loc_lcount = -1, loc_lgain = -1, loc_lights = -1;
static GLint loc_cn = -1, loc_cm = -1, loc_cdata = -1;
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
    "uniform vec4 uBeamA[32];\n"
    "uniform vec4 uBeamB[32];\n"
    "uniform vec4 uBeamC[32];\n"
    "uniform float uUvBias;\n"
    "uniform int uSkipMode;\n"
    "uniform float uTime;\n"
    "// Exact colours for the sprite pixels with the marker indices: (red, green, blue, original shade)\n"
    "uniform int uCustN;\n"
    "uniform vec4 uCustM[6];\n"
    "uniform vec4 uCust[24];\n"
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
    "        vec3 col = pal(int(B.x + 0.5));\n"
    "        if (B.w <= 0.0) {\n"
    "            // lightning throws a blue light, a laser a stronger and redder one than its own colour\n"
    "            if (uBeamC[i].z > 0.5)\n"
    "                col = vec3(0.25, 0.5, 1.0);\n"
    "            else\n"
    "                col = mix(col / max(max(col.r, max(col.g, col.b)), 0.25), vec3(1.0, 0.08, 0.03), 0.8);\n"
    "        }\n"
    "        vec2 tp = toGround(p - A.xy);\n"
    "        vec2 tb = toGround(A.zw - A.xy);\n"
    "        // The light of the beam on the ground along it\n"
    "        float t = clamp(dot(tp, tb) / max(dot(tb, tb), 1.0), 0.0, 1.0);\n"
    "        float f = max(1.0 - length(tp - tb * t) / B.z, 0.0);\n"
    "        g += col * (B.y * f * f);\n"
    "        // The light at the muzzle, thrown in the direction the gun points. It is measured on the\n"
    "        // screen, not on the ground, so that it also falls on a wall which stands next to the gun,\n"
    "        // and on the people there; a light with no direction goes all round\n"
    "        if (B.w > 0.0) {\n"
    "            float det = uLMat.x * uLMat.w - uLMat.y * uLMat.z;\n"
    "            float gpp = sqrt(max(abs(det), 1.0e-6));\n"
    "            float R = B.z * 1.4 / gpp;\n"
    "            vec2 gd = uBeamC[i].xy;\n"
    "            vec2 q = (p - A.xy) * vec2(1.0, 1.15);\n"
    "            vec3 fc = mix(col, vec3(1.0, 0.85, 0.55), 0.4);\n"
    "            float lobe;\n"
    "            if (dot(gd, gd) > 0.01) {\n"
    "                // a lobe of light which starts at the gun and goes the way it points: an ellipse\n"
    "                // along the direction, with its middle in front of the gun, and a small round light\n"
    "                // at the gun itself\n"
    "                vec2 sd = vec2(uLMat.w * gd.x - uLMat.y * gd.y, -uLMat.z * gd.x + uLMat.x * gd.y) / det;\n"
    "                sd = normalize(sd * vec2(1.0, 1.15) + vec2(1.0e-4));\n"
    "                float a = dot(q, sd);\n"
    "                float b = dot(q, vec2(-sd.y, sd.x));\n"
    "                float e = max(1.0 - length(vec2((a - 0.32 * R) / (0.95 * R), b / (0.5 * R))), 0.0);\n"
    "                float o = max(1.0 - length(q) / (0.4 * R), 0.0);\n"
    "                lobe = e * e + 0.3 * o * o;\n"
    "            } else {\n"
    "                float m = max(1.0 - length(q) / R, 0.0);\n"
    "                lobe = m * m;\n"
    "            }\n"
    "            g += fc * (B.w * 0.8 * lobe);\n"
    "        }\n"
    "    }\n"
    "    // The lights do not simply add up: many together approach full light and not beyond it\n"
    "    return vec3(1.0) - exp(-g * uBeamGain * 1.2);\n"
    "}\n"
    "// The light of a gun which fires, all round from the muzzle: it is not seen itself, only the surfaces\n"
    "// around get brighter, as in a dark room when a gun is fired and the shadows go back; the shade\n"
    "// of the surface is lifted, the way the lamps do it\n"
    "float muzzleBrightness(vec2 p)\n"
    "{\n"
    "    float s = 0.0;\n"
    "    float det = uLMat.x * uLMat.w - uLMat.y * uLMat.z;\n"
    "    float gpp = sqrt(max(abs(det), 1.0e-6));\n"
    "    for (int i = 0; i < uBeamCount; i++) {\n"
    "        vec4 B = uBeamB[i];\n"
    "        if (B.w <= 0.0)\n"
    "            continue;\n"
    "        vec2 q = (p - uBeamA[i].xy) * vec2(1.0, 1.15);\n"
    "        float m = max(1.0 - length(q) * gpp / (B.z * 3.4), 0.0);\n"
    "        s += B.w * m * m;\n"
    "    }\n"
    "    return 1.0 - exp(-s * 1.4);\n"
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
    "// The shade of a surface with the light of the explosions, shots and lamps added. The light adds to\n"
    "// the shade, so a surface which is lit already would go white; above a knee the shade rises slower\n"
    "// and cannot pass a ceiling - the light still brightens it, but what is lit gets less out of it\n"
    "float shadeKnee(float x)\n"
    "{\n"
    "    const float knee = 34.0;\n"
    "    const float range = 8.5;\n"
    "    return (x > knee) ? knee + range * tanh((x - knee) / range) : x;\n"
    "}\n"
    "float litShade(float base, float ld)\n"
    "{\n"
    "    if (ld <= 0.0)\n"
    "        return base + ld;\n"
    "    return base + shadeKnee(base + ld) - shadeKnee(base);\n"
    "}\n"
    "// Pools of blood: noise, and the shape of the sprite from the atlas, smoothed\n"
    "float bhash(vec2 p)\n"
    "{\n"
    "    p = fract(p * vec2(123.34, 456.21));\n"
    "    p += dot(p, p + 45.32);\n"
    "    return fract(p.x * p.y);\n"
    "}\n"
    "float bnoise(vec2 p)\n"
    "{\n"
    "    vec2 i = floor(p);\n"
    "    vec2 f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(bhash(i), bhash(i + vec2(1.0, 0.0)), f.x),\n"
    "               mix(bhash(i + vec2(0.0, 1.0)), bhash(i + vec2(1.0, 1.0)), f.x), f.y);\n"
    "}\n"
    "float bmask(ivec2 t, ivec2 sz)\n"
    "{\n"
    "    if (t.x < 0 || t.y < 0 || t.x >= sz.x || t.y >= sz.y)\n"
    "        return 0.0;\n"
    "    return (texelFetch(uAtlas, ivec2(vTriA.xy) + t, 0).r > 0.0) ? 1.0 : 0.0;\n"
    "}\n"
    "float bbil(vec2 p, ivec2 sz)\n"
    "{\n"
    "    vec2 f = p - 0.5;\n"
    "    ivec2 i = ivec2(floor(f));\n"
    "    vec2 w = f - vec2(i);\n"
    "    return mix(mix(bmask(i, sz), bmask(i + ivec2(1, 0), sz), w.x),\n"
    "               mix(bmask(i + ivec2(0, 1), sz), bmask(i + ivec2(1, 1), sz), w.x), w.y);\n"
    "}\n"
    "float bblur(vec2 p, ivec2 sz)\n"
    "{\n"
    "    return 0.25 * (bbil(p + vec2(0.7, 0.7), sz) + bbil(p + vec2(-0.7, 0.7), sz)\n"
    "                 + bbil(p + vec2(0.7, -0.7), sz) + bbil(p + vec2(-0.7, -0.7), sz));\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    int mode = int(vMC.x + 0.5);\n"
    "    if (mode == uSkipMode) discard;\n"
    "    int colour = int(floor(vMC.y + 0.5));\n"
    "    int layer = int(floor(vMC.z + 0.5));\n"
    "    if (mode == 105) {\n"
    "        // A puff of smoke: a soft, lumpy cloud in the colour of the gas, with lighter and darker\n"
    "        // parts, drifting a little; blended over what is behind\n"
    "        vec2 uv = vUVS.xy - 0.5;\n"
    "        float sd = vUVS.z;\n"
    "        if (colour >= 300) {\n"
    "            // A puff of the flame thrower: a tongue of fire which the noise bends and licks upwards, narrower\n"
    "            // and shorter as it ages; yellow-white in the heart, orange, then red at the rim; no smoke\n"
    "            float age = clamp(float(colour - 300) / 100.0, 0.0, 1.0);\n"
    "            float up = clamp(0.5 - uv.y, 0.0, 1.0);\n"
    "            float tm = uTime * 3.2 + sd;\n"
    "            float w1 = bnoise(vec2(uv.x * 4.0 + sd, up * 3.0 - tm * 1.6));\n"
    "            float w2 = bnoise(vec2(uv.x * 9.0 - sd, up * 6.5 - tm * 2.7));\n"
    "            float px = uv.x + ((w1 - 0.5) * 0.4 + (w2 - 0.5) * 0.18) * (0.3 + up);\n"
    "            float wid = mix(0.5, 0.1, pow(up, 1.1)) * (1.0 - 0.3 * age);\n"
    "            float dx = abs(px) / max(wid, 0.04);\n"
    "            float tip = smoothstep(1.0, 0.5, up * (1.0 + age * 0.35) + (w1 - 0.5) * 0.5);\n"
    "            float base = smoothstep(0.0, 0.05, up + 0.02);\n"
    "            float cov = smoothstep(1.0, 0.15, dx) * tip * base;\n"
    "            float heat = clamp(cov * (1.3 - up * 0.55 - age * 0.45) + (1.0 - dx) * 0.22 * (1.0 - age), 0.0, 1.0);\n"
    "            vec3 fc = mix(vec3(0.8, 0.2, 0.0), vec3(1.0, 0.5, 0.04), smoothstep(0.1, 0.35, heat));\n"
    "            fc = mix(fc, vec3(1.0, 0.85, 0.25), smoothstep(0.3, 0.6, heat));\n"
    "            fc = mix(fc, vec3(1.0, 0.96, 0.7), smoothstep(0.72, 1.0, heat));\n"
    "            float fl = 0.9 + 0.1 * sin(tm * 4.0 + sd * 3.0);\n"
    "            float fal = cov * vMC.z * (1.0 - smoothstep(0.7, 1.0, age));\n"
    "            outColor = vec4(fc * fal * fl, fal * 0.8);\n"
    "            outGlow = vec4(fc * fal * 0.25, fal * 0.5);\n"
    "            return;\n"
    "        }\n"
    "        if (colour >= 200) {\n"
    "            // Smoke of an explosion; the kind says how old it is, from 200 to 299: dark billowing smoke,\n"
    "            // which the fire still lights from below and warms at first, and which thins out as it ages\n"
    "            float age = clamp(float(colour - 200) / 99.0, 0.0, 1.0);\n"
    "            vec2 pp = uv * 3.2 + vec2(sd * 0.29, sd * 0.13);\n"
    "            float nz = bnoise(pp * 1.5 + vec2(uTime * 0.35, 0.0)) * 0.55 + bnoise(pp * 3.4 - vec2(0.0, uTime * 0.5)) * 0.3\n"
    "                     + bnoise(pp * 7.5 + vec2(sd, 0.0)) * 0.15;\n"
    "            float dd = length(uv) * 2.0;\n"
    "            float cov = smoothstep(1.0, 0.15, dd + (nz - 0.5) * 0.8);\n"
    "            vec3 smk = mix(vec3(0.05, 0.05, 0.05), vec3(0.36, 0.34, 0.32), nz);\n"
    "            float warm = (1.0 - age) * (1.0 - age) * (1.0 - dd * 0.6) * (0.4 + 0.6 * nz);\n"
    "            smk += vec3(0.55, 0.2, 0.03) * warm * 0.55;\n"
    "            float sal = cov * vMC.z * (1.0 - smoothstep(0.55, 1.0, age)) * 0.85;\n"
    "            outColor = vec4(smk * sal, sal);\n"
    "            outGlow = vec4(0.0, 0.0, 0.0, sal);\n"
    "            return;\n"
    "        }\n"
    "        if (colour >= 100) {\n"
    "            // A puff of an explosion; the kind says how old it is, from 100 to 200. Fire at first, hot\n"
    "            // white-yellow in the middle, which cools to orange, red and dark smoke\n"
    "            float age = float(colour - 100) / 100.0;\n"
    "            vec2 pp = uv * 3.0 + vec2(sd * 0.31, sd * 0.17);\n"
    "            float nz = bnoise(pp * 1.6 + vec2(uTime * 0.7, 0.0)) * 0.55 + bnoise(pp * 3.7 - vec2(0.0, uTime * 1.1)) * 0.3\n"
    "                     + bnoise(pp * 8.0 + vec2(sd, 0.0)) * 0.15;\n"
    "            float dd = length(uv) * 2.0;\n"
    "            float cov = smoothstep(1.0, 0.1, dd + (nz - 0.5) * 0.75);\n"
    "            float heat = clamp((1.05 - age * 1.15) * (1.2 - dd * 0.85) + (nz - 0.5) * 0.3, 0.0, 1.0);\n"
    "            vec3 smk = mix(vec3(0.05, 0.05, 0.05), vec3(0.30, 0.29, 0.28), nz);\n"
    "            vec3 fire = mix(vec3(0.45, 0.07, 0.0), vec3(0.95, 0.24, 0.02), smoothstep(0.2, 0.5, heat));\n"
    "            fire = mix(fire, vec3(1.0, 0.72, 0.22), smoothstep(0.5, 0.8, heat));\n"
    "            fire = mix(fire, vec3(1.0, 0.95, 0.75), smoothstep(0.8, 1.0, heat));\n"
    "            vec3 ec = mix(smk, fire, smoothstep(0.15, 0.45, heat));\n"
    "            float eal = cov * vMC.z * (1.0 - smoothstep(0.7, 1.0, age));\n"
    "            outColor = vec4(ec * eal, eal);\n"
    "            outGlow = vec4(fire * eal * smoothstep(0.4, 0.9, heat) * 0.3, eal);\n"
    "            return;\n"
    "        }\n"
    "        float n1 = sin(uv.x * 9.0 + sd * 1.7 + uTime * 0.8) * sin(uv.y * 8.0 - sd * 2.3 - uTime * 0.6);\n"
    "        float n2 = sin(uv.x * 17.0 - sd * 0.9 + uTime * 1.3) * sin(uv.y * 15.0 + sd * 1.1 + uTime * 1.1);\n"
    "        float n = 0.5 + 0.35 * n1 + 0.15 * n2;\n"
    "        float d = length(uv) * 2.0;\n"
    "        float a = smoothstep(1.0, 0.05, d + (n - 0.5) * 0.6);\n"
    "        vec3 dark = (colour == 0) ? vec3(0.12, 0.05, 0.26) : (colour == 2) ? vec3(0.0, 0.9, 0.0) : vec3(0.11, 0.11, 0.12);\n"
    "        vec3 light = (colour == 0) ? vec3(0.38, 0.16, 0.64) : (colour == 2) ? vec3(0.0, 1.0, 0.0) : vec3(0.40, 0.41, 0.43);\n"
    "        vec3 c = mix(dark, light, clamp(n * 1.1 + (0.5 - d) * 0.5, 0.0, 1.0));\n"
    "        float al = a * vMC.z;\n"
    "        outColor = vec4(c * al, al);\n"
    "        outGlow = vec4(0.0, 0.0, 0.0, al);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 104) {\n"
    "        // A flame: a billowing tongue of fire, which the noise carries upwards and tears at the top, hot\n"
    "        // white-yellow at the base and orange and red towards the tip, with embers rising from it; it is\n"
    "        // blended over what is behind, not added to it, so it never burns a bright place out\n"
    "        vec2 uv = vUVS.xy;\n"
    "        float seed = float(colour);\n"
    "        float t = clamp(1.0 - uv.y, 0.0, 1.0);\n"
    "        float x = (uv.x - 0.5) * 2.0;\n"
    "        float tm = uTime * 2.6 + seed * 1.7;\n"
    "        vec2 q = vec2(x * 1.3 + seed * 0.7, t * 2.0 - tm * 1.7);\n"
    "        float n1 = bnoise(q * 2.0);\n"
    "        float n2 = bnoise(q * 4.6 + vec2(7.1, 2.3));\n"
    "        x += ((n1 - 0.5) * 0.8 + (n2 - 0.5) * 0.35) * t + 0.14 * t * sin(tm * 1.8 + t * 4.0 + seed);\n"
    "        float w = pow(max(1.0 - t, 0.0), 0.7) * 0.8;\n"
    "        float d = abs(x) / max(w, 0.03);\n"
    "        float erode = bnoise(q * 3.0 + vec2(3.7, 1.3));\n"
    "        float life = (1.0 - t) * 1.25 - 0.1 + (erode - 0.5) * 0.9 * t;\n"
    "        float f = smoothstep(1.0, 0.25, d) * smoothstep(0.0, 0.35, life) * smoothstep(1.0, 0.86, uv.y);\n"
    "        float heat = clamp(f * (1.15 - t * 0.7) + (1.0 - d) * 0.25 * (1.0 - t), 0.0, 1.0);\n"
    "        vec3 fcol = mix(vec3(0.45, 0.04, 0.0), vec3(0.95, 0.22, 0.02), smoothstep(0.1, 0.4, heat));\n"
    "        fcol = mix(fcol, vec3(1.0, 0.62, 0.08), smoothstep(0.35, 0.65, heat));\n"
    "        fcol = mix(fcol, vec3(1.0, 0.95, 0.65), smoothstep(0.65, 0.95, heat));\n"
    "        float flick = 0.88 + 0.12 * sin(tm * 5.0 + seed * 5.0);\n"
    "        // embers: small bright specks which rise from the flame and drift\n"
    "        vec2 ep = vec2(uv.x * 7.0 + seed, t * 9.0 - tm * 2.2);\n"
    "        vec2 ci = floor(ep);\n"
    "        float hv = bhash(ci);\n"
    "        vec2 cf = fract(ep) - vec2(0.5 + 0.25 * sin(hv * 40.0 + tm * 2.0), 0.5);\n"
    "        float ember = step(0.88, hv) * smoothstep(0.16, 0.0, length(cf)) * smoothstep(1.0, 0.55, t) * smoothstep(0.0, 0.12, t);\n"
    "        float cover = clamp(f * 0.85 + ember, 0.0, 1.0);\n"
    "        vec3 c = fcol * f * flick + vec3(1.0, 0.7, 0.25) * ember;\n"
    "        c *= vMC.z;\n"
    "        outColor = vec4(c * 0.8, cover * 0.55 * vMC.z);\n"
    "        outGlow = vec4(c * 0.3, 0.0);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 106) {\n"
    "        // A pool of blood: a glossy liquid with the outline of the sprite, with a dark thick middle\n"
    "        // and a lighter rim; it does not move\n"
    "        vec2 p = vUVS.xy;\n"
    "        ivec2 sz = ivec2(vTriA.zw - vTriA.xy + 0.5);\n"
    "        float seed = float(colour);\n"
    "        // The pool grows from a spot in its middle as the person bleeds\n"
    "        float grow = clamp(vTriB.x, 0.06, 1.0);\n"
    "        p = 0.5 * vec2(sz) + (p - 0.5 * vec2(sz)) / grow;\n"
    "        vec2 w = vec2(bnoise(p * 0.45 + seed * 1.37), bnoise(p * 0.45 - seed * 0.91));\n"
    "        vec2 pp = p + (w - 0.5) * 1.6;\n"
    "        float d = bblur(pp, sz);\n"
    "        float n = bnoise(p * 0.8 + seed);\n"
    "        float body = d + (n - 0.5) * 0.28;\n"
    "        float a = smoothstep(0.28, 0.50, body);\n"
    "        if (a < 0.004) discard;\n"
    "        float gx = bblur(pp + vec2(0.8, 0.0), sz) - bblur(pp - vec2(0.8, 0.0), sz);\n"
    "        float gy = bblur(pp + vec2(0.0, 0.8), sz) - bblur(pp - vec2(0.0, 0.8), sz);\n"
    "        vec3 N = normalize(vec3(-gx * 0.35, -gy * 0.35, 1.0));\n"
    "        float thick = clamp((body - 0.3) / 0.55, 0.0, 1.0);\n"
    "        // The colour is the same all over the pool, with only a thin darker edge, and not deeper in the\n"
    "        // middle: so where two pools overlap, no edge of one shows in the other, and they look like one\n"
    "        vec3 base = mix(vec3(0.30, 0.012, 0.018), vec3(0.37, 0.014, 0.025), smoothstep(0.0, 0.1, thick));\n"
    "        vec3 L = normalize(vec3(-0.45, -0.55, 0.75));\n"
    "        float diff = clamp(dot(N, L), 0.0, 1.0);\n"
    "        vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));\n"
    "        float spec = pow(max(dot(N, H), 0.0), 36.0) * (0.35 + 0.65 * thick);\n"
    "        float lit = clamp(vMC.z, 0.3, 1.5);\n"
    "        vec3 c = base * (0.88 + 0.2 * diff) + vec3(1.0, 0.72, 0.72) * (spec * 0.16);\n"
    "        c *= lit;\n"
    "        float al = a * 0.97;\n"
    "        outColor = vec4(c * al, al);\n"
    "        outGlow = vec4(0.0, 0.0, 0.0, al);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 107) {\n"
    "        // A drop of blood in flight: a streak along its way with a round, glossy head and a thin tail\n"
    "        float u = vUVS.x;\n"
    "        float lq = vTriA.y;\n"
    "        float along = vUVS.y * lq;\n"
    "        float d = abs(u);\n"
    "        if (along > lq - 1.0)\n"
    "            d = length(vec2(u, along - (lq - 1.0)));\n"
    "        float r = mix(0.2, 1.0, smoothstep(0.0, lq * 0.8, along));\n"
    "        float a = (1.0 - smoothstep(r * 0.65, r, d)) * vMC.z;\n"
    "        if (a < 0.01) discard;\n"
    "        float head = smoothstep(lq - 2.0, lq, along);\n"
    "        vec3 c = mix(vec3(0.30, 0.01, 0.015), vec3(0.50, 0.02, 0.03), head);\n"
    "        float gl = max(1.0 - length(vec2(u + 0.35, along - (lq - 1.0) + 0.35)) * 1.6, 0.0);\n"
    "        c += vec3(1.0, 0.7, 0.7) * 0.5 * gl * gl;\n"
    "        c *= clamp(vTriA.x, 0.3, 1.5);\n"
    "        outColor = vec4(c * a, a);\n"
    "        outGlow = vec4(0.0, 0.0, 0.0, a);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 112) {\n"
    "        // The jet of a flame thrower: a tongue of fire from the gun to a puff, narrow at the gun and wider\n"
    "        // towards the puff, bent and licked by the noise, yellow-white in the heart and orange at the edge,\n"
    "        // flat where it leaves the gun; blended over what is behind\n"
    "        float u = vUVS.x;\n"
    "        float v = vUVS.y;\n"
    "        float lenpx = vTriA.x;\n"
    "        float age = clamp(vMC.z, 0.0, 1.0);\n"
    "        float seed = float(colour);\n"
    "        float t = uTime * 6.0 + seed;\n"
    "        // It flows like a liquid: a smooth stream which sways in slow waves that travel outwards, with bulges\n"
    "        // that run along it, and a soft edge; no tearing\n"
    "        float flow = v * lenpx * 0.045 - t * 1.3;\n"
    "        float lick = sin(flow) * 0.16 + (bnoise(vec2(flow * 0.7, 3.0)) - 0.5) * 0.3;\n"
    "        float cu = u + lick * v;\n"
    "        float bulge = 1.0 + 0.12 * sin(flow * 1.7);\n"
    "        float wid = mix(0.22, 1.0, pow(v, 0.65)) * bulge;\n"
    "        float d = abs(cu) / wid;\n"
    "        float cov = smoothstep(1.0, 0.1, d) * smoothstep(0.0, 0.05, v) * smoothstep(1.0, 0.55, v + age * 0.1);\n"
    "        float heat = clamp(cov * (1.35 - v * 0.6 - age * 0.4) + (1.0 - d) * 0.2 * (1.0 - v), 0.0, 1.0);\n"
    "        vec3 fc = mix(vec3(0.9, 0.3, 0.02), vec3(1.0, 0.62, 0.06), smoothstep(0.1, 0.4, heat));\n"
    "        fc = mix(fc, vec3(1.0, 0.86, 0.3), smoothstep(0.4, 0.7, heat));\n"
    "        fc = mix(fc, vec3(1.0, 0.98, 0.8), smoothstep(0.75, 1.0, heat));\n"
    "        float al = cov * 0.5 * (1.0 - smoothstep(0.85, 1.0, age));\n"
    "        outColor = vec4(fc * al, al * 0.85);\n"
    "        outGlow = vec4(fc * al * 0.2, al * 0.4);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 111) {\n"
    "        // One piece of a lightning: a thin white-blue core which is hot in the middle, and a blue bloom\n"
    "        // around it; the branches are thinner and dimmer; flickering with the time; added to the screen\n"
    "        float u = vUVS.x;\n"
    "        float lenpx = vTriA.x;\n"
    "        float halfpx = vTriA.y;\n"
    "        int pr = int(floor(vMC.z + 0.5));\n"
    "        bool br = (pr == 1) || (pr == 3);\n"
    "        bool fl = (pr >= 2);\n"
    "        // Where it is along the piece, from its start; each piece fades in and out over the length of its own\n"
    "        // width at both ends, so that the pieces next to each other add up to one whole line, with no bright\n"
    "        // spots or gaps at the joints\n"
    "        float along = (vUVS.y - 0.5) * (lenpx + 2.0 * halfpx) + lenpx * 0.5;\n"
    "        float wa = smoothstep(-1.0, 1.0, along / halfpx) - smoothstep(-1.0, 1.0, (along - lenpx) / halfpx);\n"
    "        float dist = abs(u * halfpx);\n"
    "        float d = dist / halfpx;\n"
    "        float fk = 0.85 + 0.15 * sin(uTime * 55.0);\n"
    "        float core = exp(-dist * dist * (br ? 2.2 : 1.1));\n"
    "        float halo = exp(-d * d * 8.0) * 0.6;\n"
    "        // The blue around it is see-through and bluish, not a grey veil over the screen; the thin line in the\n"
    "        // middle is solid and hot, white-blue\n"
    "        vec3 blue = fl ? vec3(0.6, 0.72, 1.0) : vec3(0.22, 0.5, 1.0);\n"
    "        float ha = halo * fk * (br ? 0.6 : 1.0) * clamp(wa, 0.0, 1.0);\n"
    "        float ca = clamp(core * 1.15, 0.0, 1.0) * (br ? 0.7 : 1.0) * clamp(wa, 0.0, 1.0);\n"
    "        vec3 corec = vec3(0.9, 0.95, 1.0);\n"
    "        outColor = vec4(blue * ha * 0.55 * (1.0 - ca) + corec * ca, ha * 0.3 * (1.0 - ca) + ca);\n"
    "        outGlow = vec4(0.0);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 110) {\n"
    "        // One piece of the beam of a laser: a white-hot thin core in a red halo, which flickers as\n"
    "        // electricity does, with bright pulses which run along it; added to the screen, and it glows\n"
    "        float u = vUVS.x;\n"
    "        float st = clamp(vMC.z, 0.0, 1.0);\n"
    "        vec3 col = pal(colour);\n"
    "        col = col / max(max(col.r, max(col.g, col.b)), 0.25);\n"
    "        col = mix(col, vec3(1.0, 0.1, 0.04), 0.55);\n"
    "        float t = uTime;\n"
    "        float along = vTriA.z + vUVS.y * vTriA.x;\n"
    "        float wob = (bnoise(vec2(along * 0.06 + t * 14.0, 3.1)) - 0.5) * 0.5\n"
    "                  + (bnoise(vec2(along * 0.17 - t * 23.0, 7.7)) - 0.5) * 0.25;\n"
    "        float d = abs(u + wob * 0.6);\n"
    "        float core = exp(-d * d * 30.0);\n"
    "        float halo = exp(-d * d * 3.4) * 0.5;\n"
    "        float pulse = 0.65 + 0.35 * sin(along * 0.12 - t * 30.0);\n"
    "        vec3 c = col * halo * pulse * 1.7 + vec3(1.0, 0.9, 0.85) * core * (0.75 + 0.25 * pulse) + col * core * 0.5;\n"
    "        c *= (0.35 + 0.65 * st);\n"
    "        // blended over the screen, not added to it, so a bright place does not burn out\n"
    "        c = vec3(1.0) - exp(-c);\n"
    "        outColor = vec4(c, max(c.r, max(c.g, c.b)));\n"
    "        outGlow = vec4(c * 0.6, 0.0);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 109) {\n"
    "        // A hot spark in flight: a thin glowing streak, orange in the tail and white-hot at the head;\n"
    "        // added to what is behind it, and it glows\n"
    "        float u = vUVS.x;\n"
    "        float lq = vTriA.y;\n"
    "        float along = vUVS.y * lq;\n"
    "        float d = abs(u);\n"
    "        if (along > lq - 1.0)\n"
    "            d = length(vec2(u, along - (lq - 1.0)));\n"
    "        float r = mix(0.15, 1.0, smoothstep(0.0, lq * 0.9, along));\n"
    "        float a = (1.0 - smoothstep(r * 0.5, r, d)) * vMC.z;\n"
    "        if (a < 0.01) discard;\n"
    "        float head = smoothstep(lq - 3.0, lq, along);\n"
    "        vec3 c = mix(vec3(1.0, 0.42, 0.06), vec3(1.0, 0.95, 0.7), head) * (1.0 + 0.7 * head);\n"
    "        outColor = vec4(c * a, a * 0.7);\n"
    "        outGlow = vec4(c * a, 0.0);\n"
    "        return;\n"
    "    }\n"
    "    if (mode == 108) {\n"
    "        // A splatter of blood on the ground: a blob with an uneven edge, a long tongue and rays in the\n"
    "        // direction it was spattered, and small drops around; wet and glossy at first, then dry and dark\n"
    "        vec2 p = vUVS.xy;\n"
    "        float seed = float(colour);\n"
    "        float age = clamp(vTriA.x, 0.0, 1.0);\n"
    "        float lit = clamp(vTriA.y, 0.3, 1.5);\n"
    "        vec2 dir = normalize(vTriA.zw + vec2(0.0001));\n"
    "        float r = length(p);\n"
    "        vec2 np = p / max(r, 0.0001);\n"
    "        float cd = dot(np, dir);\n"
    "        float n1 = bnoise(np * 2.3 + seed * 0.37);\n"
    "        float n2 = bnoise(np * 6.0 - seed * 0.71);\n"
    "        float rays = smoothstep(0.55, 1.0, bnoise(np * 7.0 + seed * 1.3 + 5.0)) * 0.16;\n"
    "        float tg = clamp(vTriB.x, 0.0, 1.0);\n"
    "        float Rr = 0.20 + 0.08 * n1 + 0.04 * n2\n"
    "                 + tg * (pow(max(cd, 0.0), 6.0) * (0.22 + 0.2 * bnoise(vec2(seed, 3.1)))\n"
    "                 + pow(max(cd, 0.0), 2.0) * 0.10 * bnoise(np * 9.0 + seed)\n"
    "                 + rays * (0.4 + 0.6 * max(cd, 0.0)));\n"
    "        float body = 1.0 - smoothstep(Rr * 0.8, Rr, r);\n"
    "        float sat = 0.0;\n"
    "        for (int i = 0; i < 14; i++) {\n"
    "            float fi = float(i);\n"
    "            vec2 hh = vec2(bhash(vec2(seed + fi, 1.7)), bhash(vec2(fi * 3.1, seed + 9.3)));\n"
    "            float ang = atan(dir.y, dir.x) + (hh.x - 0.5) * 2.6;\n"
    "            float dist = 0.30 + 0.55 * hh.y;\n"
    "            vec2 c = vec2(cos(ang), sin(ang)) * dist;\n"
    "            if (hh.x > 0.85)\n"
    "                c = (vec2(bhash(vec2(fi, seed)), bhash(vec2(seed, fi + 4.0))) * 2.0 - 1.0) * 0.8;\n"
    "            float rad = 0.018 + 0.05 * (1.0 - dist) * bhash(vec2(fi + 2.0, seed * 1.3));\n"
    "            sat = max(sat, 1.0 - smoothstep(rad * 0.6, rad, length(p - c)));\n"
    "        }\n"
    "        float m = max(body, sat) * (1.0 - smoothstep(0.85, 1.0, r));\n"
    "        float h = smoothstep(Rr, 0.0, r);\n"
    "        float hx = dFdx(h) * 9.0;\n"
    "        float hy = dFdy(h) * 9.0;\n"
    "        float a = m * vMC.z;\n"
    "        if (a < 0.004) discard;\n"
    "        vec3 N = normalize(vec3(-hx, -hy, 1.0));\n"
    "        vec3 base = mix(mix(vec3(0.36, 0.012, 0.02), vec3(0.52, 0.02, 0.04), h), vec3(0.17, 0.035, 0.03), age);\n"
    "        vec3 L = normalize(vec3(-0.45, -0.55, 0.75));\n"
    "        float diff = clamp(dot(N, L), 0.0, 1.0);\n"
    "        vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));\n"
    "        float spec = pow(max(dot(N, H), 0.0), 32.0) * (1.0 - age) * smoothstep(0.0, 0.4, h);\n"
    "        vec3 c = (base * (0.85 + 0.2 * diff) + vec3(1.0, 0.75, 0.75) * spec * 0.2) * lit;\n"
    "        a *= 0.95;\n"
    "        outColor = vec4(c * a, a);\n"
    "        outGlow = vec4(0.0, 0.0, 0.0, a);\n"
    "        return;\n"
    "    }\n"
    "    int row = int(floor(vUVS.z)) & 255;\n"
    "    int tx = 0;\n"
    "    if (layer >= 0 && mode < 100)\n"
    "        tx = texel(vUVS.xy, layer);\n"
    "    vec3 rgb = vec3(0.0);\n"
    "    float alpha = 1.0;\n"
    "    float ld = (uEnhanced != 0) ? lightDelta(vPos) : 0.0;\n"
    "    // The light of the weapons and explosions falls on the surfaces: the tile map, the walls and the\n"
    "    // people - not on the flat colours and the transparent effects. It brightens the surface the way\n"
    "    // the lamps do (through the shade), and tints it by multiplying its own colour, so the textures\n"
    "    // stay and are lit - it is not a glow put over them\n"
    "    bool surfMode = (mode == 2 || mode == 3 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 11 || mode == 27 || mode == 100);\n"
    "    vec3 bgl = vec3(0.0);\n"
    "    if (uEnhanced != 0 && uBeamCount > 0 && surfMode) {\n"
    "        bgl = beamGlow(vPos);\n"
    "        ld += dot(bgl, vec3(0.30, 0.59, 0.11)) * 10.0 + muzzleBrightness(vPos) * 16.0;\n"
    "    }\n"
    "    float sh = litShade(vUVS.z, ld);\n"
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
    "            if (colour < 0) {\n"
    "                for (int k = 0; k < uCustN; k++) {\n"
    "                    if (int(uCustM[k >> 2][k & 3] + 0.5) == a) {\n"
    "                        // The colour is lit as the original shade is by the colour map\n"
    "                        int o = int(uCust[k].a + 0.5);\n"
    "                        int lit = (layer >= 0) ? int(texelFetch(uRemap, ivec2(o, layer), 0).r * 255.0 + 0.5) : o;\n"
    "                        float lb = dot(pal(lit), vec3(0.30, 0.59, 0.11));\n"
    "                        float ob = max(dot(pal(o), vec3(0.30, 0.59, 0.11)), 0.02);\n"
    "                        rgb = clamp(uCust[k].rgb * (lb / ob), 0.0, 1.0);\n"
    "                    }\n"
    "                }\n"
    "            }\n"
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
    "        rgb = (colour == 32) ? pal(tx) : shaded(tx, litShade(float(colour) + 0.5, ld));\n"
    "    } else if (mode == 8) {\n"
    "        if (tx == 0) discard;\n"
    "        rgb = shaded(tx, litShade(float(colour) + 0.5, ld));\n"
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
    "    }\n"    "    if (uEnhanced != 0 && uBeamCount > 0 && surfMode)\n"
    "    {\n"
    "        // The coloured light of the shots multiplies the colour; with a soft shoulder, so what is lit\n"
    "        // already does not burn out\n"
    "        // The brighter the surface already is, the less the light does: it fills the room which is left\n"
    "        // up to full brightness, and does not add on top of what is lit\n"
    "        float room = 1.0 - smoothstep(0.25, 0.75, luma(rgb));\n"
    "        rgb *= vec3(1.0) + bgl * room;\n"
    "        vec3 over = max(rgb - 0.72, 0.0);\n"
    "        rgb = min(rgb, vec3(0.72)) + 0.18 * tanh(over / 0.18);\n"
    "    }\n"
    "    bool isSprite = (mode == 100 || mode == 101);\n"
    "    // The textured faces of the buildings seen through: the opacity is in the colour, 1000 and the percents\n"
    "    if (colour >= 1000 && (mode == 2 || mode == 3 || mode == 5 || mode == 6 || mode == 27)) {\n"
    "        float ga = float(colour - 1000) * 0.01;\n"
    "        outColor = vec4(rgb * ga, ga);\n"
    "        outGlow = vec4(0.0, 0.0, 0.0, ga);\n"
    "        return;\n"
    "    }\n"
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
    loc_bc = gl.GetUniformLocation(scene_program, "uBeamC");
    loc_atlas = gl.GetUniformLocation(scene_program, "uAtlas");
    loc_bias = gl.GetUniformLocation(scene_program, "uUvBias");
    loc_skip = gl.GetUniformLocation(scene_program, "uSkipMode");
    loc_time = gl.GetUniformLocation(scene_program, "uTime");
    loc_remap = gl.GetUniformLocation(scene_program, "uRemap");
    loc_cn = gl.GetUniformLocation(scene_program, "uCustN");
    loc_cm = gl.GetUniformLocation(scene_program, "uCustM");
    loc_cdata = gl.GetUniformLocation(scene_program, "uCust");

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
    "    vec3 sc = texture(uScene, uv).rgb;\n"
    "    // The glow does not pile up on what is bright already: the brighter the scene is, the less it adds\n"
    "    float room = 1.0 - 0.75 * smoothstep(0.35, 0.9, dot(sc, vec3(0.30, 0.59, 0.11)));\n"
    "    vec3 c = sc + texture(uBloom, uv).rgb * uStrength * room;\n"
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
        const float *cm, *cdata;
        int cn;

        cn = LbHwCustomColoursGet(&cm, &cdata);
        gl.Uniform1i(loc_cn, cn);
        if (cn > 0) {
            gl.Uniform4fv(loc_cm, (HW_CUSTOM_MAX + 3) / 4, cm);
            gl.Uniform4fv(loc_cdata, cn, cdata);
        }
    }
    gl.Uniform1f(loc_time, (float)((double)clock() / (double)CLOCKS_PER_SEC));
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
            const float *ba, *bb, *bc;
            static float bgain = -1.0f;
            int nb;

            if (bgain < 0.0f)
                bgain = (getenv("SWFX_BEAM_GAIN") != NULL) ? (float)atof(getenv("SWFX_BEAM_GAIN")) : 1.0f;
            nb = LbHwGetBeams(&ba, &bb, &bc);
            gl.Uniform1i(loc_bcount, nb);
            if (nb > 0) {
                gl.Uniform1f(loc_bgain, bgain);
                gl.Uniform4fv(loc_ba, nb, ba);
                gl.Uniform4fv(loc_bb, nb, bb);
                gl.Uniform4fv(loc_bc, nb, bc);
            }
        }
    }
    gl.Enable(GL_BLEND);
    // The colours which the shader gives are already multiplied by their alpha
    gl.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl.DrawArrays(GL_TRIANGLES, 0, count);
    gl.Disable(GL_BLEND);
}

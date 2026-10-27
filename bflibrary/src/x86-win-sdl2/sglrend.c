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
static TbBool scene_ready = false;
static TbBool scene_failed = false;
static GLint loc_screen = -1, loc_pages = -1, loc_fade = -1, loc_pal = -1, loc_enh = -1;


static const char *scene_vs_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec3 aUVS;\n"
    "layout(location = 2) in vec3 aMC;\n"
    "uniform vec2 uScreen;\n"
    "out vec3 vUVS;\n"
    "flat out vec3 vMC;\n"
    "void main()\n"
    "{\n"
    "    // The software renderer takes the vertex positions as the pixel positions,\n"
    "    // while GL samples at pixel centres - shift to get the same coverage\n"
    "    vec2 p = (aPos + vec2(0.5)) / uScreen;\n"
    "    gl_Position = vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);\n"
    "    vUVS = aUVS;\n"
    "    vMC = aMC;\n"
    "}\n";

static const char *scene_fs_src =
    "#version 330 core\n"
    "in vec3 vUVS;\n"
    "flat in vec3 vMC;\n"
    "out vec4 outColor;\n"
    "uniform sampler2DArray uPages;\n"
    "uniform sampler2D uFade;\n"
    "uniform sampler2D uPal;\n"
    "uniform int uEnhanced;\n"
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
    "    int colour = int(vMC.y + 0.5);\n"
    "    int layer = int(vMC.z + 0.5);\n"
    "    int row = int(floor(vUVS.z)) & 255;\n"
    "    int tx = 0;\n"
    "    if (layer >= 0)\n"
    "        tx = texel(vUVS.xy, layer);\n"
    "    vec3 rgb = vec3(0.0);\n"
    "    float alpha = 1.0;\n"
    "    float sh = vUVS.z;\n"
    "    if (mode == 0) {\n"
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
    "        rgb = (colour == 32) ? pal(tx) : shaded(tx, float(colour));\n"
    "    } else if (mode == 8) {\n"
    "        if (tx == 0) discard;\n"
    "        rgb = shaded(tx, float(colour));\n"
    "    } else if (mode == 9 || mode == 10) {\n"
    "        // Darkens what is below, by the fade table row given in the colour\n"
    "        if (tx == 0) discard;\n"
    "        outColor = vec4(0.0, 0.0, 0.0, clamp(1.0 - float(colour) / 32.0, 0.0, 1.0));\n"
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
    "    }\n"    "    outColor = vec4(rgb, alpha);\n"
    "}\n";

static void LbGlSceneFree(void)
{
    scene_ready = false;
    scene_program = 0;
    scene_vao = 0;
    scene_vbo = 0;
    tex_pages = 0;
    tex_fade = 0;
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

    gl.GenTextures(1, &tex_pages);
    gl.BindTexture(GL_TEXTURE_2D_ARRAY, tex_pages);
    gl.TexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, TEXPAGE_SIZE, TEXPAGE_SIZE, TEXPAGES_MAX,
      0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    gl.GenTextures(1, &tex_fade);
    gl.BindTexture(GL_TEXTURE_2D, tex_fade);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 256, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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
        const ubyte *page = LbHwTexturePage(i);
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
    gl.Uniform1i(loc_enh, (LbHwLightingMode() == HwLight_Enhanced) ? 1 : 0);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.DrawArrays(GL_TRIANGLES, 0, count);
    gl.Disable(GL_BLEND);
}

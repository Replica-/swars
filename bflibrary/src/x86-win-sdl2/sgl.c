/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file sgl.c
 *     OpenGL context and presenting of the game screen through the GPU.
 * @par Purpose:
 *     Creates the GL context of the game window, loads the GL functions, and
 *     shows the 8-bit game screen by uploading it as a texture and expanding
 *     the palette in a shader. The software renderer fills that screen as it
 *     always did; this only replaces the conversion and the copy to the window.
 * @par Comment:
 *     If anything here fails, the window falls back to the plain SDL window
 *     surface, which is how the game was presented before.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include "bfgl.h"
#include "bfhwrend.h"
#include "privgl.h"
#include "privbflog.h"

struct LbGlFuncs gl;

static SDL_Window *gl_window = NULL;
static SDL_GLContext gl_context = NULL;
static TbBool gl_active = false;

static GLuint present_program = 0;
static GLuint present_vao = 0;
static GLuint tex_screen = 0;
static GLuint tex_palette = 0;
static long tex_screen_w = 0, tex_screen_h = 0;
static GLint loc_idx = -1, loc_pal = -1, loc_keyed = -1;
static GLint loc_overlay = -1, loc_clayer = -1, loc_cn = -1, loc_cm = -1, loc_cdata = -1;

static ubyte palette_rgba[256 * 4];
static TbBool palette_dirty = true;
static unsigned long palette_version = 0;

static const char *present_vs_src =
    "#version 330 core\n"
    "out vec2 uv;\n"
    "void main()\n"
    "{\n"
    "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "    uv = vec2(p.x, 1.0 - p.y);\n"
    "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char *present_fs_src =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "out vec4 outColor;\n"
    "uniform sampler2D uIdx;\n"
    "uniform sampler2D uPal;\n"
    "uniform int uKeyed;\n"
    "// Translucent colour over the GPU image, shown in place of the index 254 (the pause boxes)\n"
    "uniform int uOverlay;\n"
    "// Exact colours shown in place of the marker indices, when the screen has them\n"
    "uniform int uCustLayer;\n"
    "uniform int uCustN;\n"
    "uniform vec4 uCustM[6];\n"
    "uniform vec4 uCust[24];\n"
    "void main()\n"
    "{\n"
    "    int i = int(texture(uIdx, uv).r * 255.0 + 0.5);\n"
    "    // Software screen of the frame which has the 3D scene drawn by GPU lets\n"
    "    // the GPU image through where the software has not drawn\n"
    "    if (uKeyed != 0 && i == 255)\n"
    "        discard;\n"
    "    if (uKeyed != 0 && uOverlay >= 0 && i == 254) {\n"
    "        outColor = vec4(texelFetch(uPal, ivec2(uOverlay, 0), 0).rgb, 0.6);\n"
    "        return;\n"
    "    }\n"
    "    vec3 rgb = texelFetch(uPal, ivec2(i, 0), 0).rgb;\n"
    "    if (uCustLayer != 0) {\n"
    "        for (int k = 0; k < uCustN; k++) {\n"
    "            if (int(uCustM[k >> 2][k & 3] + 0.5) == i)\n"
    "                rgb = uCust[k].rgb;\n"
    "        }\n"
    "    }\n"
    "    outColor = vec4(rgb, 1.0);\n"
    "}\n";

#define LOAD_GL(NAME) \
    do { \
        gl.NAME = (void *)SDL_GL_GetProcAddress("gl" #NAME); \
        if (gl.NAME == NULL) { \
            LOGERR("GL function gl%s is not available", #NAME); \
            ok = false; \
        } \
    } while (0)

static TbBool LbGlLoadFunctions(void)
{
    TbBool ok = true;

    memset(&gl, 0, sizeof(gl));
    LOAD_GL(Viewport);
    LOAD_GL(Scissor);
    LOAD_GL(Clear);
    LOAD_GL(ClearColor);
    LOAD_GL(Enable);
    LOAD_GL(Disable);
    LOAD_GL(BlendFunc);
    LOAD_GL(ColorMask);
    LOAD_GL(GenTextures);
    LOAD_GL(DeleteTextures);
    LOAD_GL(BindTexture);
    LOAD_GL(TexParameteri);
    LOAD_GL(TexImage2D);
    LOAD_GL(TexSubImage2D);
    LOAD_GL(PixelStorei);
    LOAD_GL(DrawArrays);
    LOAD_GL(GetError);
    LOAD_GL(GetString);
    LOAD_GL(GetIntegerv);
    LOAD_GL(ReadPixels);
    LOAD_GL(TexImage3D);
    LOAD_GL(TexSubImage3D);
    LOAD_GL(ActiveTexture);
    LOAD_GL(BlendFuncSeparate);
    LOAD_GL(GenBuffers);
    LOAD_GL(DeleteBuffers);
    LOAD_GL(BindBuffer);
    LOAD_GL(BufferData);
    LOAD_GL(BufferSubData);
    LOAD_GL(CreateShader);
    LOAD_GL(ShaderSource);
    LOAD_GL(CompileShader);
    LOAD_GL(GetShaderiv);
    LOAD_GL(GetShaderInfoLog);
    LOAD_GL(DeleteShader);
    LOAD_GL(CreateProgram);
    LOAD_GL(AttachShader);
    LOAD_GL(LinkProgram);
    LOAD_GL(GetProgramiv);
    LOAD_GL(GetProgramInfoLog);
    LOAD_GL(UseProgram);
    LOAD_GL(DeleteProgram);
    LOAD_GL(GetUniformLocation);
    LOAD_GL(Uniform1i);
    LOAD_GL(Uniform1f);
    LOAD_GL(Uniform2f);
    LOAD_GL(Uniform3f);
    LOAD_GL(Uniform4f);
    LOAD_GL(Uniform4fv);
    LOAD_GL(EnableVertexAttribArray);
    LOAD_GL(VertexAttribPointer);
    LOAD_GL(GenVertexArrays);
    LOAD_GL(BindVertexArray);
    LOAD_GL(DeleteVertexArrays);
    LOAD_GL(GenFramebuffers);
    LOAD_GL(DeleteFramebuffers);
    LOAD_GL(BindFramebuffer);
    LOAD_GL(FramebufferTexture2D);
    LOAD_GL(DrawBuffers);
    LOAD_GL(CheckFramebufferStatus);
    LOAD_GL(GenerateMipmap);
    return ok;
}

static GLuint LbGlCompileShader(const char *name, GLenum type, const char *src)
{
    GLuint sh;
    GLint ok;

    sh = gl.CreateShader(type);
    gl.ShaderSource(sh, 1, &src, NULL);
    gl.CompileShader(sh);
    gl.GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        log[0] = '\0';
        gl.GetShaderInfoLog(sh, sizeof(log), NULL, log);
        LOGERR("shader '%s' (%s) failed to compile: %s", name,
          (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", log);
        gl.DeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint LbGlBuildProgram(const char *name, const char *vs_src, const char *fs_src)
{
    GLuint vs, fs, prog;
    GLint ok;

    vs = LbGlCompileShader(name, GL_VERTEX_SHADER, vs_src);
    if (vs == 0)
        return 0;
    fs = LbGlCompileShader(name, GL_FRAGMENT_SHADER, fs_src);
    if (fs == 0) {
        gl.DeleteShader(vs);
        return 0;
    }
    prog = gl.CreateProgram();
    gl.AttachShader(prog, vs);
    gl.AttachShader(prog, fs);
    gl.LinkProgram(prog);
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    gl.GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        log[0] = '\0';
        gl.GetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOGERR("program '%s' failed to link: %s", name, log);
        gl.DeleteProgram(prog);
        return 0;
    }
    return prog;
}

/** Whether the use of GL was not blocked by the user. */
TbBool LbGlIsWanted(void)
{
    return (getenv("SWFX_NO_GL") == NULL);
}

void LbGlSetWindowAttributes(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
}

TbBool LbGlCreateContext(SDL_Window *window)
{
    gl_window = window;
    gl_context = SDL_GL_CreateContext(window);
    if (gl_context == NULL) {
        LOGWARN("GL context creation failed: %s", SDL_GetError());
        return false;
    }
    if (!LbGlLoadFunctions()) {
        LOGWARN("GL functions are missing, not using GL");
        LbGlDestroyContext();
        return false;
    }
    LOGSYNC("GL context created: %s, %s",
      (const char *)gl.GetString(GL_VERSION), (const char *)gl.GetString(GL_RENDERER));

    present_program = LbGlBuildProgram("present", present_vs_src, present_fs_src);
    if (present_program == 0) {
        LbGlDestroyContext();
        return false;
    }
    loc_idx = gl.GetUniformLocation(present_program, "uIdx");
    loc_pal = gl.GetUniformLocation(present_program, "uPal");
    loc_keyed = gl.GetUniformLocation(present_program, "uKeyed");
    loc_overlay = gl.GetUniformLocation(present_program, "uOverlay");
    loc_clayer = gl.GetUniformLocation(present_program, "uCustLayer");
    loc_cn = gl.GetUniformLocation(present_program, "uCustN");
    loc_cm = gl.GetUniformLocation(present_program, "uCustM");
    loc_cdata = gl.GetUniformLocation(present_program, "uCust");

    gl.GenVertexArrays(1, &present_vao);
    gl.GenTextures(1, &tex_screen);
    gl.GenTextures(1, &tex_palette);
    tex_screen_w = tex_screen_h = 0;
    palette_dirty = true;

    // No waiting for the display refresh; the game paces its frames on its own
    SDL_GL_SetSwapInterval(0);
    gl_active = true;
    return true;
}

void LbGlDestroyContext(void)
{
    LbGlSceneDestroy();
    gl_active = false;
    if (gl_context != NULL) {
        SDL_GL_DeleteContext(gl_context);
        gl_context = NULL;
    }
    present_program = 0;
    present_vao = 0;
    tex_screen = 0;
    tex_palette = 0;
    gl_window = NULL;
}

TbBool LbGlIsActive(void)
{
    return gl_active;
}

void LbGlSetPalette(const ubyte *colors_rgba)
{
    if (colors_rgba == NULL)
        return;
    memcpy(palette_rgba, colors_rgba, sizeof(palette_rgba));
    palette_dirty = true;
    palette_version++;
}

const ubyte *LbGlPaletteRGBA(unsigned long *p_version)
{
    *p_version = palette_version;
    return palette_rgba;
}

TbResult LbGlPresentIndexed(const ubyte *pixels, long width, long height, long pitch)
{
    int dw, dh;
    int keyed;

    if (!gl_active || (pixels == NULL))
        return Lb_FAIL;

    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);

    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, tex_screen);
    gl.PixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
    if ((tex_screen_w != width) || (tex_screen_h != height)) {
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED,
          GL_UNSIGNED_BYTE, pixels);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        tex_screen_w = width;
        tex_screen_h = height;
    } else {
        gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED,
          GL_UNSIGNED_BYTE, pixels);
    }
    gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.BindTexture(GL_TEXTURE_2D, tex_palette);
    if (palette_dirty) {
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA,
          GL_UNSIGNED_BYTE, palette_rgba);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        palette_dirty = false;
    }

    SDL_GL_GetDrawableSize(gl_window, &dw, &dh);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Viewport(0, 0, dw, dh);
    gl.Disable(GL_BLEND);
    keyed = 0;
    if (LbHwRenderEnabled() && LbHwHasRecorded() && LbGlSceneIsAvailable())
    {
        // The 3D scene goes first, the software screen is then put over it
        // For finding gaps between the triangles: SWFX_HW_MAGENTA shows the background
        if (getenv("SWFX_HW_MAGENTA") != NULL)
            gl.ClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        else
            gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        gl.Clear(GL_COLOR_BUFFER_BIT);
        if (!LbGlSceneDrawWithBloom(tex_palette, width, height, dw, dh))
        {
            gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
            gl.Viewport(0, 0, dw, dh);
            LbGlSceneDraw(tex_palette, width, height);
        }
        gl.Disable(GL_BLEND);
        keyed = 1;
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, tex_screen);
        gl.ActiveTexture(GL_TEXTURE0 + 1);
        gl.BindTexture(GL_TEXTURE_2D, tex_palette);
    }
    gl.UseProgram(present_program);
    gl.Uniform1i(loc_idx, 0);
    gl.Uniform1i(loc_pal, 1);
    gl.Uniform1i(loc_keyed, keyed);
    gl.Uniform1i(loc_overlay, LbHwOverlay());
    {
        const float *cm, *cdata;
        int cn;

        cn = LbHwCustomColoursGet(&cm, &cdata);
        gl.Uniform1i(loc_clayer, (LbHwCustomLayer() && (cn > 0)) ? 1 : 0);
        gl.Uniform1i(loc_cn, cn);
        if (cn > 0) {
            gl.Uniform4fv(loc_cm, (HW_CUSTOM_MAX + 3) / 4, cm);
            gl.Uniform4fv(loc_cdata, cn, cdata);
        }
    }
    gl.BindVertexArray(present_vao);
    if (keyed) {
        // The overlay colour is translucent over the scene
        gl.Enable(GL_BLEND);
        gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    gl.Disable(GL_BLEND);

    SDL_GL_SwapWindow(gl_window);
    return Lb_SUCCESS;
}

/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file privgl.h
 *     Internal header of the OpenGL support, shared by the GL code of the library.
 * @par Purpose:
 *     Table of the GL functions, which are loaded at run time. That way the
 *     game has no link time dependency on the GL library, and a machine without
 *     a usable GL just falls back to the software drawing.
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIBRARY_PRIVGL_H_
#define BFLIBRARY_PRIVGL_H_

#include <SDL.h>
#include <SDL_opengl.h>
#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

/* GL constants which SDL_opengl.h may not have, depending on its version. */
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif
#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

/** Table of the GL functions used by the game. Filled by LbGlLoadFunctions(). */
struct LbGlFuncs {
    /* GL 1.1 */
    void (APIENTRYP Viewport)(GLint x, GLint y, GLsizei w, GLsizei h);
    void (APIENTRYP Scissor)(GLint x, GLint y, GLsizei w, GLsizei h);
    void (APIENTRYP Clear)(GLbitfield mask);
    void (APIENTRYP ClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    void (APIENTRYP Enable)(GLenum cap);
    void (APIENTRYP Disable)(GLenum cap);
    void (APIENTRYP BlendFunc)(GLenum sfactor, GLenum dfactor);
    void (APIENTRYP ColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
    void (APIENTRYP GenTextures)(GLsizei n, GLuint *textures);
    void (APIENTRYP DeleteTextures)(GLsizei n, const GLuint *textures);
    void (APIENTRYP BindTexture)(GLenum target, GLuint texture);
    void (APIENTRYP TexParameteri)(GLenum target, GLenum pname, GLint param);
    void (APIENTRYP TexImage2D)(GLenum target, GLint level, GLint internalformat,
        GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
        const void *pixels);
    void (APIENTRYP TexSubImage2D)(GLenum target, GLint level, GLint xoffset,
        GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type,
        const void *pixels);
    void (APIENTRYP PixelStorei)(GLenum pname, GLint param);
    void (APIENTRYP DrawArrays)(GLenum mode, GLint first, GLsizei count);
    GLenum (APIENTRYP GetError)(void);
    const GLubyte *(APIENTRYP GetString)(GLenum name);
    void (APIENTRYP GetIntegerv)(GLenum pname, GLint *params);
    void (APIENTRYP ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height,
        GLenum format, GLenum type, void *pixels);
    /* GL 1.2 - 1.5 */
    void (APIENTRYP TexImage3D)(GLenum target, GLint level, GLint internalformat,
        GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format,
        GLenum type, const void *pixels);
    void (APIENTRYP TexSubImage3D)(GLenum target, GLint level, GLint xoffset,
        GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLenum type, const void *pixels);
    void (APIENTRYP ActiveTexture)(GLenum texture);
    void (APIENTRYP BlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
        GLenum dstAlpha);
    void (APIENTRYP GenBuffers)(GLsizei n, GLuint *buffers);
    void (APIENTRYP DeleteBuffers)(GLsizei n, const GLuint *buffers);
    void (APIENTRYP BindBuffer)(GLenum target, GLuint buffer);
    void (APIENTRYP BufferData)(GLenum target, GLsizeiptr size, const void *data,
        GLenum usage);
    void (APIENTRYP BufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
        const void *data);
    /* GL 2.0 */
    GLuint (APIENTRYP CreateShader)(GLenum type);
    void (APIENTRYP ShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string,
        const GLint *length);
    void (APIENTRYP CompileShader)(GLuint shader);
    void (APIENTRYP GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
    void (APIENTRYP GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length,
        GLchar *infoLog);
    void (APIENTRYP DeleteShader)(GLuint shader);
    GLuint (APIENTRYP CreateProgram)(void);
    void (APIENTRYP AttachShader)(GLuint program, GLuint shader);
    void (APIENTRYP LinkProgram)(GLuint program);
    void (APIENTRYP GetProgramiv)(GLuint program, GLenum pname, GLint *params);
    void (APIENTRYP GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length,
        GLchar *infoLog);
    void (APIENTRYP UseProgram)(GLuint program);
    void (APIENTRYP DeleteProgram)(GLuint program);
    GLint (APIENTRYP GetUniformLocation)(GLuint program, const GLchar *name);
    void (APIENTRYP Uniform1i)(GLint location, GLint v0);
    void (APIENTRYP Uniform1f)(GLint location, GLfloat v0);
    void (APIENTRYP Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
    void (APIENTRYP Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void (APIENTRYP Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
        GLfloat v3);
    void (APIENTRYP Uniform4fv)(GLint location, GLsizei count, const GLfloat *value);
    void (APIENTRYP EnableVertexAttribArray)(GLuint index);
    void (APIENTRYP VertexAttribPointer)(GLuint index, GLint size, GLenum type,
        GLboolean normalized, GLsizei stride, const void *pointer);
    /* GL 3.0 */
    void (APIENTRYP GenVertexArrays)(GLsizei n, GLuint *arrays);
    void (APIENTRYP BindVertexArray)(GLuint array);
    void (APIENTRYP DeleteVertexArrays)(GLsizei n, const GLuint *arrays);
    void (APIENTRYP GenFramebuffers)(GLsizei n, GLuint *framebuffers);
    void (APIENTRYP DeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
    void (APIENTRYP BindFramebuffer)(GLenum target, GLuint framebuffer);
    void (APIENTRYP FramebufferTexture2D)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
    GLenum (APIENTRYP CheckFramebufferStatus)(GLenum target);
    void (APIENTRYP GenerateMipmap)(GLenum target);
};

/** Functions of the context which the game window has; valid when LbGlIsActive(). */
extern struct LbGlFuncs gl;

/** The palette last given to LbGlSetPalette(), 4 bytes per colour; and a number which changes with it. */
const ubyte *LbGlPaletteRGBA(unsigned long *p_version);

/** Whether the use of GL is allowed; SWFX_NO_GL environment variable blocks it. */
TbBool LbGlIsWanted(void);

/** Sets the GL context attributes which have to be known before the window is created. */
void LbGlSetWindowAttributes(void);

/** Creates GL context for the window and prepares the presenting. Returns false on failure. */
TbBool LbGlCreateContext(SDL_Window *window);

void LbGlDestroyContext(void);

/** Draws the 3D scene recorded for the frame, with the palette from given texture.
 * The drawing goes to the currently bound framebuffer.
 */
void LbGlSceneDraw(GLuint pal_texture, long buf_width, long buf_height);

/** Whether the scene drawing is ready to use (the shaders were built). */
TbBool LbGlSceneIsAvailable(void);

void LbGlSceneDestroy(void);

/** Compiles and links a program from vertex and fragment shader sources.
 * On errors, writes the GL log into the game log and returns 0.
 */
GLuint LbGlBuildProgram(const char *name, const char *vs_src, const char *fs_src);

/******************************************************************************/
#ifdef __cplusplus
}
#endif

#endif // BFLIBRARY_PRIVGL_H_

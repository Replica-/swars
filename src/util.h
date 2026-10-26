/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file util.h
 *     Header file for util.c.
 * @par Purpose:
 *     Routines implementing small utilities.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     27 May 2022 - 12 Oct 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

#ifdef WIN32
#ifndef strcasecmp
# define strcasecmp stricmp
# define strncasecmp strnicmp
#endif
# define mkdir(path, mode) mkdir (path)
#endif

/** Converts character to be used with application specific fonts to upper case.
 *
 * Converts national characters in addition to normal alphabet.
 */
ubyte fontchrtoupper(ubyte ch);

/** Converts string to be used with application specific fonts to upper case.
 *
 * Converts national characters in addition to normal alphabet.
 */
void fontstrtoupper(char *s);

void strtolower (char *string);
void strtocapwords(char *s);

const char *extract_path_segment (const char *path,
				  char *buffer, size_t size);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

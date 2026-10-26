/******************************************************************************/
// Bullfrog Engine Smacker Playback Library - for use to remake classic games
// like Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file smailfile.c
 *     File access functions specific to AIL interface.
 * @par Purpose:
 *     Wrappers for file access functions which behave like Watcom API.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Nov 2011 - 13 Dec 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "smailfile.h"

#include <stddef.h>
/******************************************************************************/

extern int dos_low_level_open(const char *path, uint16_t data_segment, uint16_t mode);
extern size_t dos_low_level_read(int fd, void *buffer, uint16_t ds, size_t size,
  size_t *bytes_read);
extern size_t dos_low_level_seek_relative(int fd, int32_t pos);
size_t dos_low_level_seek(int fd, uint32_t pos);
void dos_low_level_close(int fd);

/******************************************************************************/

int AIL_lowfopen(const char *path, uint16_t path_ds, uint16_t mode)
{
    return dos_low_level_open(path, path_ds, mode);
}

size_t AIL_fread(int fd, void *buffer, uint16_t buf_ds, size_t size,
  size_t *bytes_read)
{
    return dos_low_level_read(fd, buffer, buf_ds, size, bytes_read);
}

size_t AIL_lowfseekcur(int fd, int32_t pos)
{
    return dos_low_level_seek_relative(fd, pos);
}

size_t AIL_lowfseekbegin(int fd, uint32_t pos)
{
    return dos_low_level_seek(fd, pos);
}

void AIL_lowfclose(int fd)
{
    dos_low_level_close(fd);
}

/******************************************************************************/

/******************************************************************************/
// Bullfrog Engine Smacker Playback Library - for use to remake classic games
// like Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file ailfile.h
 *     Header file for ailfile.c.
 * @par Purpose:
 *     File access functions specific to AIL interface.
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
#ifndef BFSMACKLIB_SMAILFILE_H_
#define BFSMACKLIB_SMAILFILE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/


/******************************************************************************/

int AIL_lowfopen(const char *path, uint16_t path_ds, uint16_t mode);
size_t AIL_fread(int fd, void *buffer, uint16_t buf_ds, size_t size,
  size_t *bytes_read);
size_t AIL_lowfseekcur(int fd, int32_t pos);
size_t AIL_lowfseekbegin(int fd, uint32_t pos);
void AIL_lowfclose(int fd);

/******************************************************************************/

#ifdef __cplusplus
};
#endif

#endif // BFSMACKLIB_AILFILE_H_
/******************************************************************************/

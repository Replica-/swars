/******************************************************************************/
// Bullfrog Sound Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file aildebug.h
 *     Header file for aildebug.c.
 * @par Purpose:
 *     OpenAL based reimplementation of MSS API.
 * @par Comment:
 *     Wrappers for MSS API functions, providing debug log capabilities.
 * @author   Tomasz Lis
 * @date     12 Jun 2022 - 05 Sep 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef AIL2OAL_AILDEBUG_H_
#define AIL2OAL_AILDEBUG_H_

#include "mssal.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Set one of internal MSS parameters.
 *
 * @param number Specifies index of the preference to set.
 * @param value The desired preference value.
 */
int32_t AIL_set_preference(uint32_t number, int32_t value);

/** Define a filename prefix for application's Global Timbre Library
 * files.
 *
 * Under MS-DOS, prefix may be up to 8 characters long and must not end
 * in a period.
 */
void AIL_set_GTL_filename_prefix(char const *prefix);

AIL_DRIVER *AIL_install_driver(const uint8_t *driver_image, uint32_t n_bytes);

/** Shut down and unload driver from memory; free driver descriptor
 *
 * Note: Not all DPMI hosts reclaim low DOS memory properly when freed.
 * This function never needs to be called by most applications.
 */
void AIL_uninstall_driver(AIL_DRIVER *drvr);

const SNDCARD_IO_PARMS *AIL_get_IO_environment(AIL_DRIVER *drvr);

void AIL_restore_USE16_ISR(int32_t irq);

/** Set a real-mode interrupt vector.
 */
void AIL_set_real_vect(uint32_t vectnum, void *real_ptr);

uint32_t AIL_sample_status(SNDSAMPLE *s);

void AIL_release_all_timers(void);

int32_t AIL_call_driver(AIL_DRIVER *drvr, int32_t fn,
        VDI_CALL *in, VDI_CALL *out);

MDI_DRIVER *AIL_install_MDI_driver_file(char *filename, SNDCARD_IO_PARMS *iop);

void AIL_uninstall_MDI_driver(MDI_DRIVER *mdidrv);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif // AIL2OAL_AILDEBUG_H_

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

/** Initialize MSS and prepares for the installation and use of sound drivers.
 */
int32_t AIL_startup(void);

/** Shuts down all active sound drivers, restore normal system timer operation,
 * and disconnect the API timer interrupt handler.
 */
void AIL_shutdown(void);

/** Set one of internal MSS parameters.
 *
 * @param number Specifies index of the preference to set.
 * @param value The desired preference value.
 */
int32_t AIL_set_preference(uint32_t number, int32_t value);

/** Returns time since first initialization of the library, in miliseconds.
 */
uint32_t AIL_ms_count(void);

/** Define a filename prefix for application's Global Timbre Library
 * files.
 *
 * Under MS-DOS, prefix may be up to 8 characters long and must not end
 * in a period.
 */
void AIL_set_GTL_filename_prefix(char const *prefix);

/** Set error text representing last error.
 */
void AIL_set_error(const char *error_msg);

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

/** Register an application function as a timer callback routine, and
 * obtain a timer handle which may be used to control the timer.
 */
HSNDTIMER AIL_register_timer(AILTIMERCB fn);

/** Set user data pointer for given timer.
 */
void *AIL_set_timer_user(HSNDTIMER timer, void *user_data);

/** Release (unlock) a locked physical channel.
 */
void AIL_release_channel(MDI_DRIVER *mdidrv, int32_t channel);

uint32_t AIL_sample_status(SNDSAMPLE *s);

/** Stop playback of sequence.
 *
 * Sequence playback may be resumed with AIL_resume_sequence(), or
 * restarted from the beginning with AIL_start_sequence().
 */
void AIL_stop_sequence(SNDSEQUENCE *seq);

/** Resume playback of previously stopped sequence.
 */
void AIL_resume_sequence(SNDSEQUENCE *seq);

/** Terminate playback of sequence, setting sequence status to SEQ_DONE.
 */
void AIL_end_sequence(SNDSEQUENCE *seq);

/** System-independent delay in 1/60 second intervals.
 *
 * Returns at once if called from background.
 */
void AIL_delay(int32_t intervals);

/** Returns TRUE if called from within timer handler or callback function.
 */
int32_t AIL_background(void);

/** Sets period between timer triggers, providing frequency.
 */
void AIL_set_timer_frequency(HSNDTIMER timer, uint32_t hertz);

/** Sets period between timer triggers, in microseconds.
 */
void AIL_set_timer_period(HSNDTIMER timer, uint32_t usec);

/** Start given timer.
 */
void AIL_start_timer(HSNDTIMER timer);

/** Start all configured timers.
 */
void AIL_start_all_timers(void);

void AIL_release_all_timers(void);

int32_t AIL_call_driver(AIL_DRIVER *drvr, int32_t fn,
        VDI_CALL *in, VDI_CALL *out);

/** Allocate a SEQUENCE structure for use with a given driver.
 */
SNDSEQUENCE *AIL_allocate_sequence_handle(MDI_DRIVER *mdidrv);

/** Repors the MIDI synthesis technology available with a specified driver.
 */
int32_t AIL_MDI_driver_type(MDI_DRIVER *mdidrv);

/** Load, install, and initialize MIDI audio driver according to
 *  contents of MDI_INI file.
 */
int32_t AIL_install_MDI_INI(MDI_DRIVER **mdidrv);

/** Read .INI file with sound hardware configuration.
 */
int32_t AIL_read_INI(AIL_INI *ini, char *fname);

MDI_DRIVER *AIL_install_MDI_driver_file(char *filename, SNDCARD_IO_PARMS *iop);

/** Uninstall XMIDI audio driver.
 */
void AIL_uninstall_MDI_driver(MDI_DRIVER *mdidrv);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif // AIL2OAL_AILDEBUG_H_
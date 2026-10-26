/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file privrdlog.h
 *     Private swrender logging macros.
 * @par Purpose:
 *     Macros to simplify logging within the SW Rendering Library.
 * @par Comment:
 *     Any other libraries and the application itself, should have its own
 *     collection of macros.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Jan 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SWRENDER_PRIVRDLOG_H_
#define SWRENDER_PRIVRDLOG_H_

#include "bflog.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Macro for logging messages of error severity within this module. */
#define LOGERR(format,args...) LbErrorLog("Rendr: %s: " format "\n", __func__ , ## args)

/** Macro for logging messages of warning severity within this module. */
#define LOGWARN(format,args...) LbWarnLog("Rendr: %s: " format "\n", __func__ , ## args)

#ifdef __DEBUG
#define LOGSYNC(format,args...) LbSyncLog("Rendr: %s: " format "\n", __func__ , ## args)
#define LOGDBG(format,args...) LbDbgLog("Rendr: %s: " format "\n", __func__ , ## args)
#else
#define LOGSYNC(format,args...)
#define LOGDBG(format,args...)
#endif

/** Macro for messages which are only for developers and disabled in normal builds. */
#define LOGNO(format,args...)

#ifdef __cplusplus
};
#endif

#endif // SWRENDER_PRIVRDLOG_H_
/******************************************************************************/

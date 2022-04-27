/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet or Dungeon Keeper.
/******************************************************************************/
/** @file bflib_fileio.c
 *     File handling routines wrapper.
 * @par Purpose:
 *     Buffer library for file i/o and directory manage routines.
 *     These should be used for all file access in the game.
 * @par Comment:
 *     Wraps standard c file handling routines. You could say this has no purpose,
 *     but here it is anyway.
 * @author   Tomasz Lis
 * @date     10 Feb 2008 - 30 Dec 2008
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bflib_fileio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "bflib_basics.h"
#include "dos.h"
/* #include "bflib_datetm.h" */

#if defined(WIN32)||defined(DOS)||defined(GO32)
#include <io.h>
#include <share.h>
#include <dos.h>
#include <direct.h>
#endif

#if defined(WIN32)
#ifdef __cplusplus
extern "C" {
#endif
//Selected declarations frow Win32 API - I don't want to use whole API
// since it influences everything
#ifndef WINBASEAPI
#ifdef __W32API_USE_DLLIMPORT__
#define WINBASEAPI DECLSPEC_IMPORT
#else
#define WINBASEAPI
#endif
#endif
#define WINAPI __stdcall
typedef char *PCHAR,*LPCH,*PCH,*NPSTR,*LPSTR,*PSTR;
typedef const char *LPCCH,*PCSTR,*LPCSTR;
typedef unsigned long DWORD;
typedef int WINBOOL,*PWINBOOL,*LPWINBOOL;
#define BOOL WINBOOL
typedef void *PVOID,*LPVOID;
typedef PVOID HANDLE;
#define DECLARE_HANDLE(n) typedef HANDLE n
typedef HANDLE *PHANDLE,*LPHANDLE;
WINBASEAPI DWORD WINAPI GetShortPathNameA(LPCSTR,LPSTR,DWORD);
#define GetShortPathName GetShortPathNameA
WINBASEAPI BOOL WINAPI FlushFileBuffers(HANDLE);
WINBASEAPI DWORD WINAPI GetLastError(void);
#ifdef __cplusplus
}
#endif
#endif
/******************************************************************************/
// Internal declarations
void convert_find_info(struct TbFileFind *ffind);
/******************************************************************************/

/** Changes the current directory on the specified drive to the specified path.
 *  If no drive is specified in path then the current drive is assumed.
 *  The path can be either relative to the current directory
 *  on the specified drive or it can be an absolute path name.
 *
 * @param path
 * @return
 */
int LbDirectoryChange(const char *path)
{
    int result;
    if (chdir(path))
        result = -1;
    else
        result = 1;
    return result;
}

//Converts file search information from platform-specific into independent form
//Yeah, right...
void convert_find_info(struct TbFileFind *ffind)
{
#if defined(WIN32)||defined(DOS)||defined(GO32)
  struct _finddata_t *fdata=&(ffind->Reserved);
  strncpy(ffind->Filename, fdata->name, sizeof(ffind->Filename));
  ffind->Filename[sizeof(ffind->Filename)-1] = '\0';
#if defined(WIN32)
  GetShortPathName(fdata->name, ffind->AlternateFilename, sizeof(ffind->AlternateFilename));
#else
  strncpy(ffind->AlternateFilename, fdata->name, sizeof(ffind->AlternateFilename));
#endif
  ffind->AlternateFilename[sizeof(ffind->AlternateFilename)-1] = '\0';
  if (fdata->size > ULONG_MAX)
    ffind->Length = ULONG_MAX;
  else
    ffind->Length = fdata->size;
  ffind->Attributes = fdata->attrib;
/* FIXME re-enable - this was temporarely disabled to remove datetime module dependence
  LbDateTimeDecode(&fdata->time_create,&ffind->CreationDate,&ffind->CreationTime);
  LbDateTimeDecode(&fdata->time_write,&ffind->LastWriteDate,&ffind->LastWriteTime);
*/
#else
  assert(!"not implemented");
#endif
}

char *LbGetCurrWorkDir(char *dest, const unsigned long maxlen)
{
  return getcwd(dest,maxlen);
}

int LbDirectoryCurrent(char *buf, unsigned long buflen)
{
    if ( getcwd(buf,buflen) != NULL )
    {
        if (buf[1] == ':')
            strcpy(buf, buf+2);
        int len = strlen(buf);
        if ( len>1 )
        {
            if (buf[len-2] == '\\')
                buf[len-2] = '\0';
        }
        return 1;
    }
    return -1;
}

TbResult LbFileMakeFullPath(const TbBool append_cur_dir,
  const char *directory, const char *filename, char *buf, const unsigned long len)
{
  if (filename == NULL) {
      buf[0] = '\0';
      return -1;
  }
  unsigned long namestart;
  if ( append_cur_dir )
  {
      if (LbDirectoryCurrent(buf, len-2) == -1) {
          buf[0] = '\0';
          return -1;
      }
      namestart = strlen(buf);
      if ( (namestart>0) && (buf[namestart-1]!='\\') && (buf[namestart-1]!='/')) {
          buf[namestart] = '/';
          namestart++;
      }
  } else
  {
    namestart = 0;
  }
  buf[namestart] = '\0';

  if (directory != NULL)
  {
      int copy_len;
      copy_len = strlen(directory);
      if ( len-2 <= namestart+copy_len-1 )
          return -1;
      memcpy(buf+namestart, directory, copy_len);
      namestart += copy_len-1;
      if ((namestart > 0) && (buf[namestart-1] != '\\') && (buf[namestart-1] != '/')) {
          buf[namestart] = '/';
          namestart++;
      }
      buf[namestart] = '\0';
  }
  if ( strlen(filename)+namestart-1 < len )
  {
      const char *ptr = filename;
      int invlen;
      for (invlen = -1; invlen != 0; invlen--)
      {
          if (*ptr++ == 0) {
              invlen--;
              break;
          }
      }
      int copy_len;
      const char *copy_src;
      char *copy_dst;
      copy_len = ~invlen;
      copy_src = &ptr[-copy_len];
      copy_dst = buf;
      for (invlen = -1; invlen != 0; invlen--)
      {
          if (*copy_dst++ == 0) {
              invlen--;
              break;
          }
      }
      memcpy(copy_dst-1, copy_src, copy_len);
      return 1;
  }
  return -1;
}

TbResult
LbDirectoryMake(const char *path, TbBool recursive)
{
    char buffer[FILENAME_MAX];
    char fname[DISKPATH_SIZE];
    char *p;
    size_t len;
    struct stat st;
    int err;
    mode_t __attribute__((unused)) mode = 0755;
    int num_levels = 0;

    // We need to transform the path here - if we did it later,
    // then we would skip base directories in recursion.
    // Also, the function expects file name, not path - make one
    if (lbFileNameTransform != NULL) {
        strncpy(fname, path, DISKPATH_SIZE-2);
        strcat(fname, "/a");
        lbFileNameTransform(buffer, fname);
        len = strlen(buffer) - 2;
        buffer[len] = '\0';
    } else {
        len = snprintf(buffer, sizeof(buffer), "%s", path);
    }

    /* First, find the longest existing path */
    do
    {
        err = stat(buffer, &st);
        if (err == 0)
        {
            if (!S_ISDIR(st.st_mode))
            {
                BFLIB_ERRORLOG("%s: Not a directory", buffer);
                return Lb_FAIL;
            }
        }
        else
        {
            if (errno != ENOENT)
            {
                BFLIB_ERRORLOG("%s: Cannot stat dir: %s", buffer, strerror(errno));
                return Lb_FAIL;
            }

            p = strrchr(buffer, FS_SEP);
            if (p == NULL)
                break;
            num_levels++;

            *p = 0;
        }
    }
    while (err != 0);

    if ((num_levels > 1) && (!recursive))
    {
        BFLIB_ERRORLOG("%s: Cannot create %d dirs - recursion disabled", buffer, num_levels);
        return Lb_FAIL;
    }
    /*
     * At this point, buffer contains the longest existing path.  Go forward
     * through the rest of the path and create the missing directories.
     */
    p = buffer;

    for (;;)
    {
        while (*p != '\0')
            p++;

        if (p >= buffer + len)
            break;

        *p = FS_SEP;

        BFLIB_DEBUGLOG(1,"%s: Creating directory", buffer);
#if defined(WIN32)
        err = mkdir(buffer);
#else
        err = mkdir(buffer, mode);
#endif
        if (err != 0)
        {
            BFLIB_ERRORLOG("%s: Cannot create dir: %s", buffer, strerror(errno));
            return Lb_FAIL;
        }
    }

    return Lb_SUCCESS;
}
/******************************************************************************/

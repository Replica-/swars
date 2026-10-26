/******************************************************************************/
// Bullfrog Engine Smacker Playback Library - for use to remake classic games
// like Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file smack.h
 *     Header file for Smacker API.
 * @par Purpose:
 *     Interface of the smacker library.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Nov 2011 - 13 Jun 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFSMACKLIB_SMACK_H_
#define BFSMACKLIB_SMACK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
struct SNDSAMPLE;
typedef struct SmackSndTrk SmackSndTrk;
typedef struct Smack Smack;

struct SmackSndTrk { // sizeof=112
    uint8_t *field_0;
    uint8_t *field_4;
    uint8_t *field_8;
    uint32_t field_C;
    uint32_t field_10;
    uint32_t field_14;
    uint32_t field_18[7];
    SmackSndTrk *next;
    struct SNDSAMPLE *smp;
    uint32_t field_3C;
    uint32_t field_40;
    uint32_t field_44;
    uint32_t field_48;
    uint32_t field_4C;
    uint32_t field_50;
    void *field_54[2];
    uint32_t field_5C;
    uint32_t field_60;
    uint32_t field_64;
    uint32_t field_68;
    uint32_t field_6C;
};

struct Smack { // sizeof=1196
    /* smacker file header starts, sizeof=0x68 */
    uint32_t Version;	/*< Magic value, "SMK2" */
    uint32_t Width;
    uint32_t Height;
    uint32_t Frames;
    uint32_t MSPerFrame;	/*< Milliseconds per frame, can be overriden during open */
    uint32_t SmackerType;
    uint32_t LargestInTrack[7];
    uint32_t tablesize;
    uint32_t codesize;
    uint32_t absize;
    uint32_t detailsize;
    uint32_t typesize;
    uint32_t TrackType[7];
    uint32_t extra;
    uint32_t NewPalette;
    /* smacker file header ends */
    uint8_t Palette[772];
    uint32_t CurrFrameNum;
    uint32_t FrameNum;
    uint32_t FrameSize;
    uint32_t SndSize;
    uint32_t LastRectxQ;
    uint32_t OpenFlags;
    uint32_t LastRectx;
    uint32_t LastRecty;
    uint32_t field_390;
    int FHandle;
    uint32_t IOBufferSize;
    void *UnkBuf39C;
    void *Highest1SecRate;
    uint32_t Highest1SecFrame;
    uint32_t ReadError;
    void *IOBuffer;
    void *field_3B0;
    uint32_t field_3B4;
    uint32_t *field_3B8;
    uint8_t *TrackTable;
    uint8_t field_3C0[32];
    uint32_t field_3E0;
    uint32_t field_3E4;
    uint32_t field_3E8;
    uint8_t field_3EC[16];
    uint32_t field_3FC;
    uint32_t MS100PerFrame;
    struct SmackSndTrk *Unkn404PerTrack[7];
    uint32_t field_420;
    uint8_t field_424[8];
    void *field_42C;
    void *field_430;
    void *field_434;
    void *field_438;
    uint32_t field_43C;
    uint32_t field_440;
    uint32_t field_444;
    uint32_t UnkReadTime1;
    uint32_t LastReadTime;
    uint32_t UnkReadTime2;
    uint32_t TotalBackReadTime;
    uint32_t field_458;
    uint32_t AllocdMemAmount;
    uint32_t FrameStartTime;
    uint32_t DecompStartTime;
    uint32_t BlitStartTime;
    uint32_t StartTime;
    uint32_t TotalOpenTime;
    uint32_t FirstReadTime;
    uint32_t TotalFrames;
    uint32_t SkippedFrames;
    uint32_t TotalBlitTime;
    uint32_t TotalDecompTime;
    uint32_t SlowestFrameTime;
    uint32_t Slowest2FrameTime;
    uint32_t SlowestFrameNum;
    uint32_t Slowest2FrameNum;
    uint32_t AverageFrameSize;
    uint32_t HighestMemAmount;
    uint32_t SimSpeed;
    uint32_t Highest1SecRate_2;
    uint32_t field_4A8;
};

/******************************************************************************/

#if defined(WIN32) && 0 // Disabled - `mkwrappers` expects cdecl on C side
#  define RADAPI __stdcall
#else
#  define RADAPI
#endif

/******************************************************************************/

/** Open .SMK video file for playback.
 *
 * Note, the order of parameters is reverted in later versions of Smacker API.
 * Well, these were clearly implemented assuming the correct order, just
 * the used calling convention was incorrect.
 *
 * @param extrabuf Extra space to be alocated as internal buffer referenced
 *   within Smack structure.
 * @param flags Open options flags.
 * @param name File name to be opened, or file handle depending on flags.
 * @return The Smack playback state structure, or NULL on error.
 */
struct Smack * RADAPI SMACKOPEN(uint32_t extrabuf, uint32_t flags, char *name);

/** Process the next frame for SMK file playback.
 */
uint32_t RADAPI SMACKDOFRAME(struct Smack *p_smk);

/** Signal the playback engine to advance SMK file playback to the next frame.
 */
void RADAPI SMACKNEXTFRAME(struct Smack *p_smk);

/** Wait the proper amount of time before each frame to ensure correct playback speed.
 */
uint32_t RADAPI SMACKWAIT(struct Smack *p_smk);

/** Gracefully close a SMK file and releases any resources used for playback.
 */
void RADAPI SMACKCLOSE(struct Smack *p_smk);

/** Decode a frame of SMK video into the specified memory buffer.
 */
void RADAPI SMACKTOBUFFER(uint32_t Flags, const void *buf,
 uint32_t destheight, uint32_t Pitch, uint32_t top, uint32_t left, struct Smack *p_smk);

/******************************************************************************/

#ifdef __cplusplus
};
#endif

#endif // BFSMACKLIB_SMACK_H_
/******************************************************************************/

/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file femail.h
 *     Header file for femail.c.
 * @par Purpose:
 *     Front-end desktop and menu system, emails handling.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 22 Oct 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef FEMAIL_H
#define FEMAIL_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

#define EMAIL_STORE_COUNT 20
#define BRIEF_STORE_COUNT 10
#define NEWMAIL_STORE_COUNT 29

enum MailType {
    MlTp_Email = 0,
    MlTp_Mission,
};

struct EmailItem { // sizeof=5
	ubyte RefNum;
	ubyte RecvDay;
	ubyte RecvMonth;
	ubyte RecvYear;
	ubyte Mission;
};

struct NewMailItem { // sizeof=5
	ubyte RecvDay;
	ubyte RecvMonth;
	ubyte RecvYear;
	ubyte Mission;
	ubyte Flag;
};

#pragma pack()
/******************************************************************************/
extern ubyte new_mail;
extern ushort next_email;
extern short next_ref;
extern struct EmailItem email_store[EMAIL_STORE_COUNT];
extern struct EmailItem brief_store[BRIEF_STORE_COUNT];
extern struct NewMailItem newmail_store[NEWMAIL_STORE_COUNT];

ushort activate_queued_mail(void);
void delete_mail(ushort mailnum, ubyte type);
void queue_up_new_mail(ubyte emtype, short missi);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

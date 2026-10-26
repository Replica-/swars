/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file plyr_net.h
 *     Header file for plyr_net.c.
 * @par Purpose:
 *     Players network session handling during mission gameplay.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     11 Dec 2024 - 01 Oct 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef PLYR_NET_H
#define PLYR_NET_H

#include "bftypes.h"
#include "game_bstype.h"
#include "network.h"
#include "weapon.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

#define NET_CHAT_MSG_LIMIT 5
#define NET_CHAT_MSG_LEN 25

struct NetPlayerChat {
  char Msg[NET_CHAT_MSG_LEN];
};

#pragma pack()
/******************************************************************************/

extern ubyte net_service_started;

extern ubyte net_serial_uses_modem;

extern ubyte net_host_player_no;

extern ubyte net_players_num;

extern struct NetPlayerChat net_player_chat[NET_CHAT_MSG_LIMIT];
extern ubyte net_player_chat_plyr[NET_CHAT_MSG_LIMIT];

//TODO maybe make it a part of larger struct
extern struct WeaponsFourPack net_agents__FourPacks[NET_PLAYERS_COUNT][4];

/******************************************************************************/
TbBool netgame_service_is_multi_client_capable(void);
void netgame_service_owned_link_reset(void);

void net_player_chat_init(void);
void net_player_chat_free_old_msg(void);
void net_player_chat_set_last(PlayerIdx plyr, const char *text);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif

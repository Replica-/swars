/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file plyr_net.c
 *     Players network session handling during mission gameplay.
 * @par Purpose:
 *     Implement functions for maintaining and switching states of the
 *     network session during a multiplayer game.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     11 Dec 2024 - 01 Oct 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "plyr_net.h"

#include <string.h>
#include "bfmemut.h"

#include "network.h"
#include "player.h"

/******************************************************************************/

ubyte net_service_started = 0;

ubyte net_serial_uses_modem = 0;

ubyte net_host_player_no = 0;

ubyte net_players_num = 1;

struct NetPlayerChat net_player_chat[NET_CHAT_MSG_LIMIT];
ubyte net_player_chat_plyr[NET_CHAT_MSG_LIMIT];

struct WeaponsFourPack net_agents__FourPacks[NET_PLAYERS_COUNT][AGENTS_SQUAD_MAX_COUNT];

/******************************************************************************/

TbBool netgame_service_is_multi_client_capable(void)
{
    return nsvc.I.Type == NetSvc_IPX;
}

void netgame_service_owned_link_reset(void)
{
    if (nsvc.I.Type != NetSvc_IPX)
    {
        if (net_serial_uses_modem)
            LbNetworkHangUp();
        LbNetworkReset();
        net_service_started = 0;
    }

}

void net_player_chat_init(void)
{
    struct NetPlayerChat *p_npchat;
    int i;

    for (i = 0; i < NET_CHAT_MSG_LIMIT; i++)
    {
        p_npchat = &net_player_chat[i];
        LbMemorySet(p_npchat, '\0', sizeof(struct NetPlayerChat));
    }
}

void net_player_chat_free_old_msg(void)
{
    struct NetPlayerChat *p_npchat1;
    struct NetPlayerChat *p_npchat2;
    int i;

    p_npchat2 = &net_player_chat[1];
    p_npchat1 = &net_player_chat[0];
    for (i = 0; i < NET_CHAT_MSG_LIMIT-2; i++)
    {
        net_player_chat_plyr[i] = net_player_chat_plyr[i+1];
        LbMemoryCopy(p_npchat1, p_npchat2, sizeof(struct NetPlayerChat));
        p_npchat1++;
        p_npchat2++;
    }
    p_npchat2->Msg[0] = '\0';
}

void net_player_chat_set_last(PlayerIdx plyr, const char *text)
{
    struct NetPlayerChat *p_npchat;

    net_player_chat_plyr[NET_CHAT_MSG_LIMIT - 1] = plyr;
    p_npchat = &net_player_chat[NET_CHAT_MSG_LIMIT - 1];
    strncpy(p_npchat->Msg, text, NET_CHAT_MSG_LEN - 1);
    p_npchat->Msg[NET_CHAT_MSG_LEN - 1] = '\0';
}

/******************************************************************************/

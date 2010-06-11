
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2008-2010 by Daniel Stenberg
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */


#include "ares_setup.h"

#include "ares.h"
#include "ares_data.h"
#include "ares_private.h"


int ares_get_servers(ares_channel channel,
                     struct ares_addr_node **servers)
{
  struct ares_addr_node *srvr_head = NULL;
  struct ares_addr_node *srvr_last = NULL;
  struct ares_addr_node *srvr_curr;
  int status = ARES_SUCCESS;
  int i;

  if (!channel)
    return ARES_ENODATA;

  for (i = 0; i < channel->nservers; i++)
    {
      /* Allocate storage for this server node appending it to the list */
      srvr_curr = ares_malloc_data(ARES_DATATYPE_ADDR_NODE);
      if (!srvr_curr)
        {
          status = ARES_ENOMEM;
          break;
        }
      if (srvr_last)
        {
          srvr_last->next = srvr_curr;
        }
      else
        {
          srvr_head = srvr_curr;
        }
      srvr_last = srvr_curr;

      /* Fill this server node data */
      srvr_curr->family = channel->servers[i].addr.family;
      if (srvr_curr->family == AF_INET)
        memcpy(&srvr_curr->addrV4, &channel->servers[i].addr.addrV4,
               sizeof(srvr_curr->addrV4));
      else
        memcpy(&srvr_curr->addrV6, &channel->servers[i].addr.addrV6,
               sizeof(srvr_curr->addrV6));
    }

  if (status != ARES_SUCCESS)
    {
      if (srvr_head)
        {
          ares_free_data(srvr_head);
          srvr_head = NULL;
        }
    }

  *servers = srvr_head;

  return status;
}


int ares_set_servers(ares_channel channel,
                     struct ares_addr_node *servers)
{
  struct ares_addr_node *srvr;
  int num_srvrs = 0;
  int i;

  if (ares_library_initialized() != ARES_SUCCESS)
    return ARES_ENOTINITIALIZED;

  if (!channel)
    return ARES_ENODATA;

  ares__destroy_servers_state(channel);

  for (srvr = servers; srvr; srvr = srvr->next)
    {
      num_srvrs++;
    }

  if (num_srvrs > 0)
    {
      /* Allocate storage for servers state */
      channel->servers = malloc(num_srvrs * sizeof(struct server_state));
      if (!channel->servers)
        {
          return ARES_ENOMEM;
        }
      channel->nservers = num_srvrs;
      /* Fill servers state address data */
      for (i = 0, srvr = servers; srvr; i++, srvr = srvr->next)
        {
          channel->servers[i].addr.family = srvr->family;
          if (srvr->family == AF_INET)
            memcpy(&channel->servers[i].addr.addrV4, &srvr->addrV4,
                   sizeof(srvr->addrV4));
          else
            memcpy(&channel->servers[i].addr.addrV6, &srvr->addrV6,
                   sizeof(srvr->addrV6));
        }
      /* Initialize servers state remaining data */
      ares__init_servers_state(channel);
    }

  return ARES_SUCCESS;
}

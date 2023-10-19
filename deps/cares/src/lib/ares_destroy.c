
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2004-2011 by Daniel Stenberg
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

#include <assert.h>

#include "ares.h"
#include "ares_private.h"

void ares_destroy_options(struct ares_options *options)
{
  int i;

  if(options->servers)
    ares_free(options->servers);
  for (i = 0; i < options->ndomains; i++)
    ares_free(options->domains[i]);
  if(options->domains)
    ares_free(options->domains);
  if(options->sortlist)
    ares_free(options->sortlist);
  if(options->lookups)
    ares_free(options->lookups);
  if(options->resolvconf_path)
    ares_free(options->resolvconf_path);
  if(options->hosts_path)
    ares_free(options->hosts_path);
}

void ares_destroy(ares_channel channel)
{
  int i;
  struct query *query;
  struct list_node* list_head;
  struct list_node* list_node;

  if (!channel)
    return;

  list_head = &(channel->all_queries);
  for (list_node = list_head->next; list_node != list_head; )
    {
      query = list_node->data;
      list_node = list_node->next;  /* since we're deleting the query */
      query->callback(query->arg, ARES_EDESTRUCTION, 0, NULL, 0);
      ares__free_query(query);
    }
#ifndef NDEBUG
  /* Freeing the query should remove it from all the lists in which it sits,
   * so all query lists should be empty now.
   */
  assert(ares__is_list_empty(&(channel->all_queries)));
  for (i = 0; i < ARES_QID_TABLE_SIZE; i++)
    {
      assert(ares__is_list_empty(&(channel->queries_by_qid[i])));
    }
  for (i = 0; i < ARES_TIMEOUT_TABLE_SIZE; i++)
    {
      assert(ares__is_list_empty(&(channel->queries_by_timeout[i])));
    }
#endif

  ares__destroy_servers_state(channel);

  if (channel->domains) {
    for (i = 0; i < channel->ndomains; i++)
      ares_free(channel->domains[i]);
    ares_free(channel->domains);
  }

  if(channel->sortlist)
    ares_free(channel->sortlist);

  if (channel->lookups)
    ares_free(channel->lookups);

  if (channel->resolvconf_path)
    ares_free(channel->resolvconf_path);

  if (channel->hosts_path)
    ares_free(channel->hosts_path);

  if (channel->rand_state)
    ares__destroy_rand_state(channel->rand_state);

  ares_free(channel);
}

void ares__destroy_servers_state(ares_channel channel)
{
  struct server_state *server;
  int i;

  if (channel->servers)
    {
      for (i = 0; i < channel->nservers; i++)
        {
          server = &channel->servers[i];
          ares__close_sockets(channel, server);
          assert(ares__is_list_empty(&server->queries_to_server));
        }
      ares_free(channel->servers);
      channel->servers = NULL;
    }
  channel->nservers = -1;
}

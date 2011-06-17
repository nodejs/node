
/* Copyright (C) 2004 by Daniel Stenberg et al
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include "ares_setup.h"
#include <assert.h>
#include <stdlib.h>
#include "ares.h"
#include "ares_private.h"

/*
 * ares_cancel() cancels all ongoing requests/resolves that might be going on
 * on the given channel. It does NOT kill the channel, use ares_destroy() for
 * that.
 */
void ares_cancel(ares_channel channel)
{
  struct query *query;
  struct list_node* list_head;
  struct list_node* list_node;
  int i;

  list_head = &(channel->all_queries);
  for (list_node = list_head->next; list_node != list_head; )
  {
    query = list_node->data;
    list_node = list_node->next;  /* since we're deleting the query */
    query->callback(query->arg, ARES_ECANCELLED, 0, NULL, 0);
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
  if (!(channel->flags & ARES_FLAG_STAYOPEN))
  {
    if (channel->servers)
    {
      for (i = 0; i < channel->nservers; i++)
        ares__close_sockets(channel, &channel->servers[i]);
    }
  }
}

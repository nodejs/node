
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
  struct list_node list_head_copy;
  struct list_node* list_head;
  struct list_node* list_node;
  int i;

  if (!ares__is_list_empty(&(channel->all_queries)))
  {
    /* Swap list heads, so that only those queries which were present on entry
     * into this function are cancelled. New queries added by callbacks of
     * queries being cancelled will not be cancelled themselves.
     */
    list_head = &(channel->all_queries);
    list_head_copy.prev = list_head->prev;
    list_head_copy.next = list_head->next;
    list_head_copy.prev->next = &list_head_copy;
    list_head_copy.next->prev = &list_head_copy;
    list_head->prev = list_head;
    list_head->next = list_head;
    for (list_node = list_head_copy.next; list_node != &list_head_copy; )
    {
      query = list_node->data;
      list_node = list_node->next;  /* since we're deleting the query */
      query->callback(query->arg, ARES_ECANCELLED, 0, NULL, 0);
      ares__free_query(query);
    }
  }
  if (!(channel->flags & ARES_FLAG_STAYOPEN) && ares__is_list_empty(&(channel->all_queries)))
  {
    if (channel->servers)
    {
      for (i = 0; i < channel->nservers; i++)
        ares__close_sockets(channel, &channel->servers[i]);
    }
  }
}

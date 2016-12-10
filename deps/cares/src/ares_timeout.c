
/* Copyright 1998 by the Massachusetts Institute of Technology.
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

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "ares.h"
#include "ares_private.h"

/* return time offset between now and (future) check, in milliseconds */
static long timeoffset(struct timeval *now, struct timeval *check)
{
  return (check->tv_sec - now->tv_sec)*1000 +
         (check->tv_usec - now->tv_usec)/1000;
}

/* WARNING: Beware that this is linear in the number of outstanding
 * requests! You are probably far better off just calling ares_process()
 * once per second, rather than calling ares_timeout() to figure out
 * when to next call ares_process().
 */
struct timeval *ares_timeout(ares_channel channel, struct timeval *maxtv,
                             struct timeval *tvbuf)
{
  struct query *query;
  struct list_node* list_head;
  struct list_node* list_node;
  struct timeval now;
  struct timeval nextstop;
  long offset, min_offset;

  /* No queries, no timeout (and no fetch of the current time). */
  if (ares__is_list_empty(&(channel->all_queries)))
    return maxtv;

  /* Find the minimum timeout for the current set of queries. */
  now = ares__tvnow();
  min_offset = -1;

  list_head = &(channel->all_queries);
  for (list_node = list_head->next; list_node != list_head;
       list_node = list_node->next)
    {
      query = list_node->data;
      if (query->timeout.tv_sec == 0)
        continue;
      offset = timeoffset(&now, &query->timeout);
      if (offset < 0)
        offset = 0;
      if (min_offset == -1 || offset < min_offset)
        min_offset = offset;
    }

  /* If we found a minimum timeout and it's sooner than the one specified in
   * maxtv (if any), return it.  Otherwise go with maxtv.
   */
  if (min_offset != -1)
    {
      int ioffset = (min_offset > (long)INT_MAX) ? INT_MAX : (int)min_offset;

      nextstop.tv_sec = ioffset/1000;
      nextstop.tv_usec = (ioffset%1000)*1000;

      if (!maxtv || ares__timedout(maxtv, &nextstop))
        {
          *tvbuf = nextstop;
          return tvbuf;
        }
    }

  return maxtv;
}

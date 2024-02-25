/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) The c-ares project and its contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ares_setup.h"

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#include "ares_nameser.h"

#include "ares.h"
#include "ares_dns.h"
#include "ares_private.h"

static unsigned short generate_unique_qid(ares_channel_t *channel)
{
  unsigned short id;

  do {
    id = ares__generate_new_id(channel->rand_state);
  } while (ares__htable_szvp_get(channel->queries_by_qid, id, NULL));

  return id;
}

ares_status_t ares_send_ex(ares_channel_t *channel, const unsigned char *qbuf,
                           size_t qlen, ares_callback callback, void *arg,
                           unsigned short *qid)
{
  struct query  *query;
  size_t         packetsz;
  struct timeval now = ares__tvnow();
  ares_status_t  status;
  unsigned short id   = generate_unique_qid(channel);
  unsigned char *abuf = NULL;
  size_t         alen = 0;

  /* Verify that the query is at least long enough to hold the header. */
  if (qlen < HFIXEDSZ || qlen >= (1 << 16)) {
    callback(arg, ARES_EBADQUERY, 0, NULL, 0);
    return ARES_EBADQUERY;
  }
  if (ares__slist_len(channel->servers) == 0) {
    callback(arg, ARES_ENOSERVER, 0, NULL, 0);
    return ARES_ENOSERVER;
  }

  /* Check query cache */
  status = ares_qcache_fetch(channel, &now, qbuf, qlen, &abuf, &alen);
  if (status != ARES_ENOTFOUND) {
    /* ARES_SUCCESS means we retrieved the cache, anything else is a critical
     * failure, all result in termination */
    callback(arg, (int)status, 0, abuf, (int)alen);
    ares_free(abuf);
    return status;
  }

  /* Allocate space for query and allocated fields. */
  query = ares_malloc(sizeof(struct query));
  if (!query) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    return ARES_ENOMEM;
  }
  memset(query, 0, sizeof(*query));

  query->channel = channel;
  query->qbuf    = ares_malloc(qlen);
  if (!query->qbuf) {
    ares_free(query);
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    return ARES_ENOMEM;
  }

  query->qid             = id;
  query->timeout.tv_sec  = 0;
  query->timeout.tv_usec = 0;

  /* Ignore first 2 bytes, assign our own query id */
  query->qbuf[0] = (unsigned char)((id >> 8) & 0xFF);
  query->qbuf[1] = (unsigned char)(id & 0xFF);
  memcpy(query->qbuf + 2, qbuf + 2, qlen - 2);
  query->qlen = qlen;

  /* Fill in query arguments. */
  query->callback = callback;
  query->arg      = arg;

  /* Initialize query status. */
  query->try_count = 0;

  packetsz = (channel->flags & ARES_FLAG_EDNS) ? channel->ednspsz : PACKETSZ;
  query->using_tcp = (channel->flags & ARES_FLAG_USEVC) || qlen > packetsz;

  query->error_status = ARES_SUCCESS;
  query->timeouts     = 0;

  /* Initialize our list nodes. */
  query->node_queries_by_timeout = NULL;
  query->node_queries_to_conn    = NULL;

  /* Chain the query into the list of all queries. */
  query->node_all_queries =
    ares__llist_insert_last(channel->all_queries, query);
  if (query->node_all_queries == NULL) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    ares__free_query(query);
    return ARES_ENOMEM;
  }

  /* Keep track of queries bucketed by qid, so we can process DNS
   * responses quickly.
   */
  if (!ares__htable_szvp_insert(channel->queries_by_qid, query->qid, query)) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    ares__free_query(query);
    return ARES_ENOMEM;
  }

  /* Perform the first query action. */

  status = ares__send_query(query, &now);
  if (status == ARES_SUCCESS && qid) {
    *qid = id;
  }
  return status;
}

void ares_send(ares_channel_t *channel, const unsigned char *qbuf, int qlen,
               ares_callback callback, void *arg)
{
  if (channel == NULL) {
    return;
  }

  ares__channel_lock(channel);

  ares_send_ex(channel, qbuf, (size_t)qlen, callback, arg, NULL);

  ares__channel_unlock(channel);
}

size_t ares_queue_active_queries(ares_channel_t *channel)
{
  size_t len;

  if (channel == NULL) {
    return 0;
  }

  ares__channel_lock(channel);

  len = ares__llist_len(channel->all_queries);

  ares__channel_unlock(channel);

  return len;
}

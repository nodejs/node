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

static ares_status_t ares_send_dnsrec_int(ares_channel_t          *channel,
                                          const ares_dns_record_t *dnsrec,
                                          ares_callback_dnsrec     callback,
                                          void *arg, unsigned short *qid)
{
  struct query            *query;
  size_t                   packetsz;
  struct timeval           now = ares__tvnow();
  ares_status_t            status;
  unsigned short           id          = generate_unique_qid(channel);
  const ares_dns_record_t *dnsrec_resp = NULL;

  if (ares__slist_len(channel->servers) == 0) {
    callback(arg, ARES_ENOSERVER, 0, NULL);
    return ARES_ENOSERVER;
  }

  /* Check query cache */
  status = ares_qcache_fetch(channel, &now, dnsrec, &dnsrec_resp);
  if (status != ARES_ENOTFOUND) {
    /* ARES_SUCCESS means we retrieved the cache, anything else is a critical
     * failure, all result in termination */
    callback(arg, status, 0, dnsrec_resp);
    return status;
  }

  /* Allocate space for query and allocated fields. */
  query = ares_malloc(sizeof(struct query));
  if (!query) {
    callback(arg, ARES_ENOMEM, 0, NULL);
    return ARES_ENOMEM;
  }
  memset(query, 0, sizeof(*query));

  query->channel = channel;

  status = ares_dns_write(dnsrec, &query->qbuf, &query->qlen);
  if (status != ARES_SUCCESS) {
    ares_free(query);
    callback(arg, status, 0, NULL);
    return status;
  }

  query->qid             = id;
  query->timeout.tv_sec  = 0;
  query->timeout.tv_usec = 0;

  /* Ignore first 2 bytes, assign our own query id */
  query->qbuf[0] = (unsigned char)((id >> 8) & 0xFF);
  query->qbuf[1] = (unsigned char)(id & 0xFF);

  /* Fill in query arguments. */
  query->callback = callback;
  query->arg      = arg;

  /* Initialize query status. */
  query->try_count = 0;

  packetsz = (channel->flags & ARES_FLAG_EDNS) ? channel->ednspsz : PACKETSZ;
  query->using_tcp =
    (channel->flags & ARES_FLAG_USEVC) || query->qlen > packetsz;

  query->error_status = ARES_SUCCESS;
  query->timeouts     = 0;

  /* Initialize our list nodes. */
  query->node_queries_by_timeout = NULL;
  query->node_queries_to_conn    = NULL;

  /* Chain the query into the list of all queries. */
  query->node_all_queries =
    ares__llist_insert_last(channel->all_queries, query);
  if (query->node_all_queries == NULL) {
    callback(arg, ARES_ENOMEM, 0, NULL);
    ares__free_query(query);
    return ARES_ENOMEM;
  }

  /* Keep track of queries bucketed by qid, so we can process DNS
   * responses quickly.
   */
  if (!ares__htable_szvp_insert(channel->queries_by_qid, query->qid, query)) {
    callback(arg, ARES_ENOMEM, 0, NULL);
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

ares_status_t ares_send_dnsrec(ares_channel_t          *channel,
                               const ares_dns_record_t *dnsrec,
                               ares_callback_dnsrec callback, void *arg,
                               unsigned short *qid)
{
  ares_status_t status;

  if (channel == NULL) {
    return ARES_EFORMERR;
  }

  ares__channel_lock(channel);

  status = ares_send_dnsrec_int(channel, dnsrec, callback, arg, qid);

  ares__channel_unlock(channel);

  return status;
}

void ares_send(ares_channel_t *channel, const unsigned char *qbuf, int qlen,
               ares_callback callback, void *arg)
{
  ares_dns_record_t *dnsrec = NULL;
  ares_status_t      status;
  void              *carg = NULL;

  if (channel == NULL) {
    return;
  }

  /* Verify that the query is at least long enough to hold the header. */
  if (qlen < HFIXEDSZ || qlen >= (1 << 16)) {
    callback(arg, ARES_EBADQUERY, 0, NULL, 0);
    return;
  }

  status = ares_dns_parse(qbuf, (size_t)qlen, 0, &dnsrec);
  if (status != ARES_SUCCESS) {
    callback(arg, (int)status, 0, NULL, 0);
    return;
  }

  carg = ares__dnsrec_convert_arg(callback, arg);
  if (carg == NULL) {
    status = ARES_ENOMEM;
    ares_dns_record_destroy(dnsrec);
    callback(arg, (int)status, 0, NULL, 0);
    return;
  }

  ares_send_dnsrec(channel, dnsrec, ares__dnsrec_convert_cb, carg, NULL);

  ares_dns_record_destroy(dnsrec);
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

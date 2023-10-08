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

int ares_send_ex(ares_channel channel, const unsigned char *qbuf, int qlen,
                 ares_callback callback, void *arg)
{
  struct query *query;
  int i, packetsz;
  struct timeval now;

  /* Verify that the query is at least long enough to hold the header. */
  if (qlen < HFIXEDSZ || qlen >= (1 << 16))
    {
      callback(arg, ARES_EBADQUERY, 0, NULL, 0);
      return ARES_EBADQUERY;
    }
  if (channel->nservers < 1)
    {
      callback(arg, ARES_ESERVFAIL, 0, NULL, 0);
      return ARES_ESERVFAIL;
    }
  /* Allocate space for query and allocated fields. */
  query = ares_malloc(sizeof(struct query));
  if (!query)
    {
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return ARES_ENOMEM;
    }
  memset(query, 0, sizeof(*query));
  query->channel = channel;
  query->tcpbuf = ares_malloc(qlen + 2);
  if (!query->tcpbuf)
    {
      ares_free(query);
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return ARES_ENOMEM;
    }
  query->server_info = ares_malloc(channel->nservers *
                                   sizeof(query->server_info[0]));
  if (!query->server_info)
    {
      ares_free(query->tcpbuf);
      ares_free(query);
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return ARES_ENOMEM;
    }

  /* Compute the query ID.  Start with no timeout. */
  query->qid = DNS_HEADER_QID(qbuf);
  query->timeout.tv_sec = 0;
  query->timeout.tv_usec = 0;

  /* Form the TCP query buffer by prepending qlen (as two
   * network-order bytes) to qbuf.
   */
  query->tcpbuf[0] = (unsigned char)((qlen >> 8) & 0xff);
  query->tcpbuf[1] = (unsigned char)(qlen & 0xff);
  memcpy(query->tcpbuf + 2, qbuf, qlen);
  query->tcplen = qlen + 2;

  /* Fill in query arguments. */
  query->qbuf = query->tcpbuf + 2;
  query->qlen = qlen;
  query->callback = callback;
  query->arg = arg;

  /* Initialize query status. */
  query->try_count = 0;

  /* Choose the server to send the query to. If rotation is enabled, keep track
   * of the next server we want to use. */
  query->server = channel->last_server;
  if (channel->rotate == 1)
    channel->last_server = (channel->last_server + 1) % channel->nservers;

  for (i = 0; i < channel->nservers; i++)
    {
      query->server_info[i].skip_server = 0;
      query->server_info[i].tcp_connection_generation = 0;
    }

  packetsz = (channel->flags & ARES_FLAG_EDNS) ? channel->ednspsz : PACKETSZ;
  query->using_tcp = (channel->flags & ARES_FLAG_USEVC) || qlen > packetsz;

  query->error_status = ARES_ECONNREFUSED;
  query->timeouts = 0;

  /* Initialize our list nodes. */
  query->node_queries_by_timeout = NULL;
  query->node_queries_to_conn    = NULL;

  /* Chain the query into the list of all queries. */
  query->node_all_queries = ares__llist_insert_last(channel->all_queries, query);
  if (query->node_all_queries == NULL) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    ares__free_query(query);
    return ARES_ENOMEM;
  }

  /* Keep track of queries bucketed by qid, so we can process DNS
   * responses quickly.
   */
  if (!ares__htable_stvp_insert(channel->queries_by_qid, query->qid, query)) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    ares__free_query(query);
    return ARES_ENOMEM;
  }

  /* Perform the first query action. */
  now = ares__tvnow();
  return ares__send_query(channel, query, &now);
}

void ares_send(ares_channel channel, const unsigned char *qbuf, int qlen,
               ares_callback callback, void *arg)
{
  ares_send_ex(channel, qbuf, qlen, callback, arg);
}

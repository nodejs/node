
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

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ares.h"
#include "ares_dns.h"
#include "ares_private.h"

void ares_send(ares_channel channel, const unsigned char *qbuf, int qlen,
               ares_callback callback, void *arg)
{
  struct query *query;
  int i;
  struct timeval now;

  /* Verify that the query is at least long enough to hold the header. */
  if (qlen < HFIXEDSZ || qlen >= (1 << 16))
    {
      callback(arg, ARES_EBADQUERY, 0, NULL, 0);
      return;
    }

  /* Allocate space for query and allocated fields. */
  query = malloc(sizeof(struct query));
  if (!query)
    {
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return;
    }
  query->tcpbuf = malloc(qlen + 2);
  if (!query->tcpbuf)
    {
      free(query);
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return;
    }
  query->server_info = malloc(channel->nservers *
                              sizeof(query->server_info[0]));
  if (!query->server_info)
    {
      free(query->tcpbuf);
      free(query);
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return;
    }

  /* Compute the query ID.  Start with no timeout. */
  query->qid = (unsigned short)DNS_HEADER_QID(qbuf);
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
  query->try = 0;

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
  query->using_tcp = (channel->flags & ARES_FLAG_USEVC) || qlen > PACKETSZ;
  query->error_status = ARES_ECONNREFUSED;
  query->timeouts = 0;

  /* Initialize our list nodes. */
  ares__init_list_node(&(query->queries_by_qid),     query);
  ares__init_list_node(&(query->queries_by_timeout), query);
  ares__init_list_node(&(query->queries_to_server),  query);
  ares__init_list_node(&(query->all_queries),        query);

  /* Chain the query into the list of all queries. */
  ares__insert_in_list(&(query->all_queries), &(channel->all_queries));
  /* Keep track of queries bucketed by qid, so we can process DNS
   * responses quickly.
   */
  ares__insert_in_list(
    &(query->queries_by_qid),
    &(channel->queries_by_qid[query->qid % ARES_QID_TABLE_SIZE]));

  /* Perform the first query action. */
  now = ares__tvnow();
  ares__send_query(channel, query, &now);
}

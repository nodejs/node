/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2010 Daniel Stenberg
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

#ifdef HAVE_SYS_UIO_H
#  include <sys/uio.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
#  include <netinet/tcp.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares_nameser.h"

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#ifdef NETWARE
#  include <sys/filio.h>
#endif

#include <assert.h>
#include <fcntl.h>
#include <limits.h>

#include "ares.h"
#include "ares_dns.h"
#include "ares_nowarn.h"
#include "ares_private.h"


static int try_again(int errnum);
static void write_tcp_data(ares_channel channel, fd_set *write_fds,
                           ares_socket_t write_fd, struct timeval *now);
static void read_packets(ares_channel channel, fd_set *read_fds,
                         ares_socket_t read_fd, struct timeval *now);
static void process_timeouts(ares_channel channel, struct timeval *now);
static void process_answer(ares_channel channel, const unsigned char *abuf,
                           int alen, struct server_connection *conn, int tcp,
                           struct timeval *now);
static void handle_error(struct server_connection *conn, struct timeval *now);
static void skip_server(ares_channel channel, struct query *query,
                        struct server_state *server);
static int next_server(ares_channel channel, struct query *query,
                        struct timeval *now);
static int open_socket(ares_channel channel, struct server_state *server,
                       int is_tcp);
static int same_questions(const unsigned char *qbuf, int qlen,
                          const unsigned char *abuf, int alen);
static int same_address(struct sockaddr *sa, struct ares_addr *aa);
static int has_opt_rr(const unsigned char *abuf, int alen);
static void end_query(ares_channel channel, struct query *query, int status,
                      const unsigned char *abuf, int alen);
static ares_ssize_t ares__socket_write(ares_channel channel, ares_socket_t s,
                                       const void * data, size_t len);

/* return true if now is exactly check time or later */
int ares__timedout(struct timeval *now,
                   struct timeval *check)
{
  long secs = (now->tv_sec - check->tv_sec);

  if(secs > 0)
    return 1; /* yes, timed out */
  if(secs < 0)
    return 0; /* nope, not timed out */

  /* if the full seconds were identical, check the sub second parts */
  return (now->tv_usec - check->tv_usec >= 0);
}

/* add the specific number of milliseconds to the time in the first argument */
static void timeadd(struct timeval *now, int millisecs)
{
  now->tv_sec += millisecs/1000;
  now->tv_usec += (millisecs%1000)*1000;

  if(now->tv_usec >= 1000000) {
    ++(now->tv_sec);
    now->tv_usec -= 1000000;
  }
}

/*
 * generic process function
 */
static void processfds(ares_channel channel,
                       fd_set *read_fds, ares_socket_t read_fd,
                       fd_set *write_fds, ares_socket_t write_fd)
{
  struct timeval now = ares__tvnow();

  write_tcp_data(channel, write_fds, write_fd, &now);
  read_packets(channel, read_fds, read_fd, &now);
  process_timeouts(channel, &now);
}

/* Something interesting happened on the wire, or there was a timeout.
 * See what's up and respond accordingly.
 */
void ares_process(ares_channel channel, fd_set *read_fds, fd_set *write_fds)
{
  processfds(channel, read_fds, ARES_SOCKET_BAD, write_fds, ARES_SOCKET_BAD);
}

/* Something interesting happened on the wire, or there was a timeout.
 * See what's up and respond accordingly.
 */
void ares_process_fd(ares_channel channel,
                     ares_socket_t read_fd, /* use ARES_SOCKET_BAD or valid
                                               file descriptors */
                     ares_socket_t write_fd)
{
  processfds(channel, NULL, read_fd, NULL, write_fd);
}


/* Return 1 if the specified error number describes a readiness error, or 0
 * otherwise. This is mostly for HP-UX, which could return EAGAIN or
 * EWOULDBLOCK. See this man page
 *
 * http://devrsrc1.external.hp.com/STKS/cgi-bin/man2html?
 *     manpage=/usr/share/man/man2.Z/send.2
 */
static int try_again(int errnum)
{
#if !defined EWOULDBLOCK && !defined EAGAIN
#error "Neither EWOULDBLOCK nor EAGAIN defined"
#endif
  switch (errnum)
    {
#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
      return 1;
#endif
#if defined EAGAIN && EAGAIN != EWOULDBLOCK
    case EAGAIN:
      return 1;
#endif
    }
  return 0;
}


/* If any TCP sockets select true for writing, write out queued data
 * we have for them.
 */
static void write_tcp_data(ares_channel channel,
                           fd_set *write_fds,
                           ares_socket_t write_fd,
                           struct timeval *now)
{
  struct server_state *server;
  int i;

  if(!write_fds && (write_fd == ARES_SOCKET_BAD))
    /* no possible action */
    return;

  for (i = 0; i < channel->nservers; i++) {
    const unsigned char *data;
    size_t               data_len;
    ares_ssize_t         count;

    /* Make sure server has data to send and is selected in write_fds or
       write_fd. */
    server = &channel->servers[i];
    if (ares__buf_len(server->tcp_send) == 0 || server->tcp_conn == NULL)
      continue;

    if (write_fds) {
      if (!FD_ISSET(server->tcp_conn->fd, write_fds))
        continue;
    } else {
      if (server->tcp_conn->fd != write_fd)
        continue;
    }

    if (write_fds) {
      /* If there's an error and we close this socket, then open
       * another with the same fd to talk to another server, then we
       * don't want to think that it was the new socket that was
       * ready. This is not disastrous, but is likely to result in
       * extra system calls and confusion. */
      FD_CLR(server->tcp_conn->fd, write_fds);
    }

    data  = ares__buf_peek(server->tcp_send, &data_len);
    count = ares__socket_write(channel, server->tcp_conn->fd, data, data_len);
    if (count <= 0) {
      if (!try_again(SOCKERRNO)) {
        handle_error(server->tcp_conn, now);
      }
      continue;
    }

    /* Strip data written from the buffer */
    ares__buf_consume(server->tcp_send, count);

    /* Notify state callback all data is written */
    if (ares__buf_len(server->tcp_send) == 0) {
      SOCK_STATE_CALLBACK(channel, server->tcp_conn->fd, 1, 0);
    }
  }
}


static ares_ssize_t socket_recvfrom(ares_channel channel,
   ares_socket_t s,
   void * data,
   size_t data_len,
   int flags,
   struct sockaddr *from,
   ares_socklen_t *from_len)
{
   if (channel->sock_funcs && channel->sock_funcs->arecvfrom)
      return channel->sock_funcs->arecvfrom(s, data, data_len,
	 flags, from, from_len,
	 channel->sock_func_cb_data);

#ifdef HAVE_RECVFROM
   return recvfrom(s, data, data_len, flags, from, from_len);
#else
   return sread(s, data, data_len);
#endif
}

static ares_ssize_t socket_recv(ares_channel channel,
   ares_socket_t s,
   void * data,
   size_t data_len)
{
   if (channel->sock_funcs && channel->sock_funcs->arecvfrom)
      return channel->sock_funcs->arecvfrom(s, data, data_len, 0, 0, 0,
	 channel->sock_func_cb_data);

   return sread(s, data, data_len);
}


/* If any TCP socket selects true for reading, read some data,
 * allocate a buffer if we finish reading the length word, and process
 * a packet if we finish reading one.
 */
static void read_tcp_data(ares_channel channel, struct server_connection *conn,
                          struct timeval *now)
{
  ares_ssize_t         count;
  struct server_state *server   = conn->server;

  /* Fetch buffer to store data we are reading */
  size_t               ptr_len  = 512;
  unsigned char       *ptr      = ares__buf_append_start(server->tcp_parser,
                                                         &ptr_len);

  if (ptr == NULL) {
    handle_error(conn, now);
    return; /* bail out on malloc failure. TODO: make this
               function return error codes */
  }

  /* Read from socket */
  count = socket_recv(channel, conn->fd, ptr, ptr_len);
  if (count <= 0) {
    ares__buf_append_finish(server->tcp_parser, 0);
    if (!(count == -1 && try_again(SOCKERRNO)))
      handle_error(conn, now);
    return;
  }

  /* Record amount of data read */
  ares__buf_append_finish(server->tcp_parser, count);

  /* Process all queued answers */
  while (1) {
     unsigned short       dns_len  = 0;
     const unsigned char *data     = NULL;
     size_t               data_len = 0;

    /* Tag so we can roll back */
    ares__buf_tag(server->tcp_parser);

    /* Read length indicator */
    if (ares__buf_fetch_be16(server->tcp_parser, &dns_len) != ARES_SUCCESS) {
      ares__buf_tag_rollback(server->tcp_parser);
      return;
    }

    /* Not enough data for a full response yet */
    if (ares__buf_consume(server->tcp_parser, dns_len) != ARES_SUCCESS) {
      ares__buf_tag_rollback(server->tcp_parser);
      return;
    }

    /* Can't fail except for misuse */
    data = ares__buf_tag_fetch(server->tcp_parser, &data_len);
    if (data == NULL) {
      ares__buf_tag_clear(server->tcp_parser);
      return;
    }

    /* Strip off 2 bytes length */
    data     += 2;
    data_len -= 2;

    /* We finished reading this answer; process it */
    process_answer(channel, data, (int)data_len, conn, 1, now);

    /* Since we processed the answer, clear the tag so space can be reclaimed */
    ares__buf_tag_clear(server->tcp_parser);
  }
}


static int socket_list_append(ares_socket_t **socketlist, ares_socket_t fd,
                              size_t *alloc_cnt, size_t *num)
{
  if (*num >= *alloc_cnt) {
    /* Grow by powers of 2 */
    size_t         new_alloc = (*alloc_cnt) << 1;
    ares_socket_t *new_list  = ares_realloc(socketlist,
                                            new_alloc * sizeof(*new_list));
    if (new_list == NULL)
      return 0;
    *alloc_cnt  = new_alloc;
    *socketlist = new_list;
  }

  (*socketlist)[(*num)++] = fd;
  return 1;
}


static ares_socket_t *channel_socket_list(ares_channel channel, size_t *num)
{
  size_t         alloc_cnt = 1 << 4;
  int            i;
  ares_socket_t *out       = ares_malloc(alloc_cnt * sizeof(*out));

  *num = 0;

  if (out == NULL)
    return NULL;

  for (i=0; i<channel->nservers; i++) {
    ares__llist_node_t *node;
    for (node = ares__llist_node_first(channel->servers[i].connections);
         node != NULL;
         node = ares__llist_node_next(node)) {
      struct server_connection *conn = ares__llist_node_val(node);

      if (conn->fd == ARES_SOCKET_BAD)
        continue;

      if (!socket_list_append(&out, conn->fd, &alloc_cnt, num))
        goto fail;
    }
  }

  return out;

fail:
  ares_free(out);
  *num = 0;
  return NULL;
}

/* If any UDP sockets select true for reading, process them. */
static void read_udp_packets_fd(ares_channel channel,
                                struct server_connection *conn,
                                struct timeval *now)
{
  ares_ssize_t read_len;
  unsigned char buf[MAXENDSSZ + 1];
  ares_socket_t fd = conn->fd; /* Cache for validation */

#ifdef HAVE_RECVFROM
  ares_socklen_t fromlen;
  union {
    struct sockaddr     sa;
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } from;
#endif

  /* To reduce event loop overhead, read and process as many
   * packets as we can. */
  do {
    if (conn->fd == ARES_SOCKET_BAD) {
      read_len = -1;
    } else {
      if (conn->server->addr.family == AF_INET) {
        fromlen = sizeof(from.sa4);
      } else {
        fromlen = sizeof(from.sa6);
      }
      read_len = socket_recvfrom(channel, conn->fd, (void *)buf,
                                 sizeof(buf), 0, &from.sa, &fromlen);
    }

    if (read_len == 0) {
      /* UDP is connectionless, so result code of 0 is a 0-length UDP
       * packet, and not an indication the connection is closed like on
       * tcp */
      continue;
    } else if (read_len < 0) {
      if (try_again(SOCKERRNO))
        continue;

      handle_error(conn, now);
      return;
#ifdef HAVE_RECVFROM
    } else if (!same_address(&from.sa, &conn->server->addr)) {
      /* The address the response comes from does not match the address we
       * sent the request to. Someone may be attempting to perform a cache
       * poisoning attack. */
      continue;
#endif

    } else {
      process_answer(channel, buf, (int)read_len, conn, 0, now);
    }
  /* process_answer may invalidate "conn" and close the file descriptor, so
   * check to see if file descriptor is still valid before looping! */
  } while (read_len >= 0 &&
           ares__htable_asvp_get_direct(channel->connnode_by_socket, fd) != NULL);

}


static void read_packets(ares_channel channel, fd_set *read_fds,
                        ares_socket_t read_fd, struct timeval *now)
{
  size_t                    i;
  ares_socket_t            *socketlist  = NULL;
  size_t                    num_sockets = 0;
  struct server_connection *conn        = NULL;
  ares__llist_node_t       *node        = NULL;

  if (!read_fds && (read_fd == ARES_SOCKET_BAD))
    /* no possible action */
    return;

  /* Single socket specified */
  if (!read_fds) {
    node = ares__htable_asvp_get_direct(channel->connnode_by_socket, read_fd);
    if (node == NULL)
      return;

    conn = ares__llist_node_val(node);

    if (conn->is_tcp) {
      read_tcp_data(channel, conn, now);
    } else {
      read_udp_packets_fd(channel, conn, now);
    }

    return;
  }

  /* There is no good way to iterate across an fd_set, instead we must pull a list
   * of all known fds, and iterate across that checking against the fd_set. */
  socketlist = channel_socket_list(channel, &num_sockets);

  for (i=0; i<num_sockets; i++) {
    if (!FD_ISSET(socketlist[i], read_fds))
      continue;

    /* If there's an error and we close this socket, then open
     * another with the same fd to talk to another server, then we
     * don't want to think that it was the new socket that was
     * ready. This is not disastrous, but is likely to result in
     * extra system calls and confusion. */
    FD_CLR(socketlist[i], read_fds);

    node = ares__htable_asvp_get_direct(channel->connnode_by_socket,
                                        socketlist[i]);
    if (node == NULL)
      return;

    conn = ares__llist_node_val(node);

    if (conn->is_tcp) {
      read_tcp_data(channel, conn, now);
    } else {
      read_udp_packets_fd(channel, conn, now);
    }
  }

  ares_free(socketlist);
}


/* If any queries have timed out, note the timeout and move them on. */
static void process_timeouts(ares_channel channel, struct timeval *now)
{
  ares__slist_node_t *node = ares__slist_node_first(channel->queries_by_timeout);
  while (node != NULL) {
    struct query       *query = ares__slist_node_val(node);
    /* Node might be removed, cache next */
    ares__slist_node_t *next  = ares__slist_node_next(node);
    ares_socket_t       fd;

    /* Since this is sorted, as soon as we hit a query that isn't timed out, break */
    if (!ares__timedout(now, &query->timeout)) {
      break;
    }

    query->error_status = ARES_ETIMEOUT;
    query->timeouts++;


    fd = query->conn->fd;
    next_server(channel, query, now);
    /* A timeout is a special case where we need to possibly cleanup a
     * a connection */
    ares__check_cleanup_conn(channel, fd);

    node = next;
  }
}


/* Handle an answer from a server. */
static void process_answer(ares_channel channel, const unsigned char *abuf,
                           int alen, struct server_connection *conn, int tcp,
                           struct timeval *now)
{
  int tc, rcode, packetsz;
  unsigned short id;
  struct query *query;
  /* Cache these as once ares__send_query() gets called, it may end up
   * invalidating the connection all-together */
  struct server_state *server = conn->server;
  ares_socket_t fd = conn->fd;

  /* If there's no room in the answer for a header, we can't do much
   * with it. */
  if (alen < HFIXEDSZ) {
    return;
  }

  /* Grab the query ID, truncate bit, and response code from the packet. */
  id = DNS_HEADER_QID(abuf); /* Converts to host byte order */
  tc = DNS_HEADER_TC(abuf);
  rcode = DNS_HEADER_RCODE(abuf);

  /* Find the query corresponding to this packet. The queries are
   * hashed/bucketed by query id, so this lookup should be quick.  
   */
  query = ares__htable_stvp_get_direct(channel->queries_by_qid, id);
  if (!query) {
    return;
  }

  /* Both the query id and the questions must be the same. We will drop any
   * replies that aren't for the same query as this is considered invalid. */
  if (!same_questions(query->qbuf, query->qlen, abuf, alen)) {
    return;
  }

  /* At this point we know we've received an answer for this query, so we should
   * remove it from the connection's queue so we can possibly invalidate the
   * connection. Delay cleaning up the connection though as we may enqueue
   * something new.  */
  ares__llist_node_destroy(query->node_queries_to_conn);
  query->node_queries_to_conn = NULL;

  packetsz = PACKETSZ;
  /* If we use EDNS and server answers with FORMERR without an OPT RR, the protocol
   * extension is not understood by the responder. We must retry the query
   * without EDNS enabled. */
  if (channel->flags & ARES_FLAG_EDNS)
  {
      packetsz = channel->ednspsz;
      if (rcode == FORMERR && has_opt_rr(abuf, alen) != 1)
      {
          int qlen = (query->tcplen - 2) - EDNSFIXEDSZ;
          channel->flags ^= ARES_FLAG_EDNS;
          query->tcplen -= EDNSFIXEDSZ;
          query->qlen -= EDNSFIXEDSZ;
          query->tcpbuf[0] = (unsigned char)((qlen >> 8) & 0xff);
          query->tcpbuf[1] = (unsigned char)(qlen & 0xff);
          DNS_HEADER_SET_ARCOUNT(query->tcpbuf + 2, 0);
          query->tcpbuf = ares_realloc(query->tcpbuf, query->tcplen);
          query->qbuf = query->tcpbuf + 2;
          ares__send_query(channel, query, now);
          ares__check_cleanup_conn(channel, fd);
          return;
      }
  }

  /* If we got a truncated UDP packet and are not ignoring truncation,
   * don't accept the packet, and switch the query to TCP if we hadn't
   * done so already.
   */
  if ((tc || alen > packetsz) && !tcp && !(channel->flags & ARES_FLAG_IGNTC))
    {
      if (!query->using_tcp)
        {
          query->using_tcp = 1;
          ares__send_query(channel, query, now);
        }
      ares__check_cleanup_conn(channel, fd);
      return;
    }

  /* Limit alen to PACKETSZ if we aren't using TCP (only relevant if we
   * are ignoring truncation.
   */
  if (alen > packetsz && !tcp)
      alen = packetsz;

  /* If we aren't passing through all error packets, discard packets
   * with SERVFAIL, NOTIMP, or REFUSED response codes.
   */
  if (!(channel->flags & ARES_FLAG_NOCHECKRESP))
    {
      if (rcode == SERVFAIL || rcode == NOTIMP || rcode == REFUSED)
        {
          switch (rcode) {
            case SERVFAIL:
              query->error_status = ARES_ESERVFAIL;
              break;
            case NOTIMP:
              query->error_status = ARES_ENOTIMP;
              break;
            case REFUSED:
              query->error_status = ARES_EREFUSED;
              break;
          }
          skip_server(channel, query, server);
          if (query->server == (int)server->idx) /* Is this ever not true? */
            next_server(channel, query, now);
          ares__check_cleanup_conn(channel, fd);
          return;
        }
    }

  end_query(channel, query, ARES_SUCCESS, abuf, alen);

  ares__check_cleanup_conn(channel, fd);
}


static void handle_error(struct server_connection *conn,
                         struct timeval *now)
{
  ares_channel         channel = conn->server->channel;
  struct server_state *server = conn->server;
  ares__llist_t       *list_copy;
  ares__llist_node_t  *node;

  /* We steal the list from the connection then close the connection, then
   * iterate across the list to requeue any inflight queries with the broken
   * connection.  Destroying the connection prior to requeuing ensures requests
   * won't go back to the broken connection */
  list_copy             = conn->queries_to_conn;
  conn->queries_to_conn = NULL;
  ares__close_connection(conn);

  while ((node = ares__llist_node_first(list_copy)) != NULL) {
    struct query       *query = ares__llist_node_val(node);

    assert(query->server == (int)server->idx);
    skip_server(channel, query, server);
    /* next_server will remove the current node from the list */
    next_server(channel, query, now);
  }

  ares__llist_destroy(list_copy);
}


static void skip_server(ares_channel channel, struct query *query,
                        struct server_state *server)
{
  /* The given server gave us problems with this query, so if we have the
   * luxury of using other servers, then let's skip the potentially broken
   * server and just use the others. If we only have one server and we need to
   * retry then we should just go ahead and re-use that server, since it's our
   * only hope; perhaps we just got unlucky, and retrying will work (eg, the
   * server timed out our TCP connection just as we were sending another
   * request).
   */
  if (channel->nservers > 1)
    {
      query->server_info[server->idx].skip_server = 1;
    }
}

static int next_server(ares_channel channel, struct query *query,
                        struct timeval *now)
{
  int status;
  /* We need to try each server channel->tries times. We have channel->nservers
   * servers to try. In total, we need to do channel->nservers * channel->tries
   * attempts. Use query->try to remember how many times we already attempted
   * this query. Use modular arithmetic to find the next server to try.
   * A query can be requested be terminated at the next interval by setting
   * query->no_retries */
  while (++(query->try_count) < (channel->nservers * channel->tries) &&
         !query->no_retries) {
    struct server_state *server;

    /* Move on to the next server. */
    query->server = (query->server + 1) % channel->nservers;
    server = &channel->servers[query->server];

    /* We don't want to use this server if (1) we've decided to skip this
     * server because of earlier errors we encountered, or (2) we already
     * sent this query over this exact connection.
     */
    if (!query->server_info[query->server].skip_server &&
        !(query->using_tcp &&
         (query->server_info[query->server].tcp_connection_generation ==
            server->tcp_connection_generation))) {
      return ares__send_query(channel, query, now);
    }

    /* You might think that with TCP we only need one try. However, even
     * when using TCP, servers can time-out our connection just as we're
     * sending a request, or close our connection because they die, or never
     * send us a reply because they get wedged or tickle a bug that drops
     * our request.
     */
  }

  /* If we are here, all attempts to perform query failed. */
  status = query->error_status;
  end_query(channel, query, query->error_status, NULL, 0);
  return status;
}

int ares__send_query(ares_channel channel, struct query *query,
                      struct timeval *now)
{
  struct server_state *server;
  struct server_connection *conn;
  int timeplus;
  int status;

  server = &channel->servers[query->server];
  if (query->using_tcp) {
    size_t prior_len = 0;
    /* Make sure the TCP socket for this server is set up and queue
     * a send request.
     */
    if (server->tcp_conn == NULL) {
      int err = open_socket(channel, server, 1);
      switch (err) {
        /* Good result, continue on */
        case ARES_SUCCESS:
          break;

        /* These conditions are retryable as they are server-specific
         * error codes */
        case ARES_ECONNREFUSED:
        case ARES_EBADFAMILY:
          skip_server(channel, query, server);
          return next_server(channel, query, now);

        /* Anything else is not retryable, likely ENOMEM */
        default:
          end_query(channel, query, err, NULL, 0);
          return err;
      }
    }

    conn = server->tcp_conn;

    prior_len = ares__buf_len(server->tcp_send);

    status = ares__buf_append(server->tcp_send, query->tcpbuf, query->tcplen);
    if (status != ARES_SUCCESS) {
      end_query(channel, query, status, NULL, 0);
      return ARES_ENOMEM;
    }

    if (prior_len == 0) {
      SOCK_STATE_CALLBACK(channel, conn->fd, 1, 1);
    }

    query->server_info[query->server].tcp_connection_generation =
      server->tcp_connection_generation;
  } else {
    ares__llist_node_t *node = ares__llist_node_first(server->connections);

    /* Don't use the found connection if we've gone over the maximum number
     * of queries. Also, skip over the TCP connection if it is the first in
     * the list */
    if (node != NULL) {
      conn = ares__llist_node_val(node);
      if (conn->is_tcp) {
        node = NULL;
      } else if (channel->udp_max_queries > 0 &&
                 conn->total_queries >= (size_t)channel->udp_max_queries) {
        node = NULL;
      }
    }

    if (node == NULL) {
      int err = open_socket(channel, server, 0);
      switch (err) {
        /* Good result, continue on */
        case ARES_SUCCESS:
          break;

        /* These conditions are retryable as they are server-specific
         * error codes */
        case ARES_ECONNREFUSED:
        case ARES_EBADFAMILY:
          skip_server(channel, query, server);
          return next_server(channel, query, now);

        /* Anything else is not retryable, likely ENOMEM */
        default:
          end_query(channel, query, err, NULL, 0);
          return err;
      }
      node = ares__llist_node_first(server->connections);
    }

    conn = ares__llist_node_val(node);
    if (ares__socket_write(channel, conn->fd, query->qbuf, query->qlen) == -1) {
      /* FIXME: Handle EAGAIN here since it likely can happen. */
      skip_server(channel, query, server);
      return next_server(channel, query, now);
    }
  }

  /* For each trip through the entire server list, double the channel's
   * assigned timeout, avoiding overflow.  If channel->timeout is negative,
   * leave it as-is, even though that should be impossible here.
   */
  timeplus = channel->timeout;
  {
    /* How many times do we want to double it?  Presume sane values here. */
    const int shift = query->try_count / channel->nservers;

    /* Is there enough room to shift timeplus left that many times?
     *
     * To find out, confirm that all of the bits we'll shift away are zero.
     * Stop considering a shift if we get to the point where we could shift
     * a 1 into the sign bit (i.e. when shift is within two of the bit
     * count).
     *
     * This has the side benefit of leaving negative numbers unchanged.
     */
    if(shift <= (int)(sizeof(int) * CHAR_BIT - 1)
       && (timeplus >> (sizeof(int) * CHAR_BIT - 1 - shift)) == 0)
    {
      timeplus <<= shift;
    }
  }

  /* Keep track of queries bucketed by timeout, so we can process
   * timeout events quickly.
   */
  ares__slist_node_destroy(query->node_queries_by_timeout);
  query->timeout = *now;
  timeadd(&query->timeout, timeplus);
  query->node_queries_by_timeout = ares__slist_insert(channel->queries_by_timeout, query);
  if (!query->node_queries_by_timeout) {
    end_query(channel, query, ARES_ENOMEM, NULL, 0);
    return ARES_ENOMEM;
  }

  /* Keep track of queries bucketed by connection, so we can process errors
   * quickly. */
  ares__llist_node_destroy(query->node_queries_to_conn);
  query->node_queries_to_conn =
    ares__llist_insert_last(conn->queries_to_conn, query);
  query->conn = conn;
  conn->total_queries++;
  return ARES_SUCCESS;
}

/*
 * setsocknonblock sets the given socket to either blocking or non-blocking
 * mode based on the 'nonblock' boolean argument. This function is highly
 * portable.
 */
static int setsocknonblock(ares_socket_t sockfd,    /* operate on this */
                           int nonblock   /* TRUE or FALSE */)
{
#if defined(USE_BLOCKING_SOCKETS)

  return 0; /* returns success */

#elif defined(HAVE_FCNTL_O_NONBLOCK)

  /* most recent unix versions */
  int flags;
  flags = fcntl(sockfd, F_GETFL, 0);
  if (FALSE != nonblock)
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
  else
    return fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK));  /* LCOV_EXCL_LINE */

#elif defined(HAVE_IOCTL_FIONBIO)

  /* older unix versions */
  int flags = nonblock ? 1 : 0;
  return ioctl(sockfd, FIONBIO, &flags);

#elif defined(HAVE_IOCTLSOCKET_FIONBIO)

#ifdef WATT32
  char flags = nonblock ? 1 : 0;
#else
  /* Windows */
  unsigned long flags = nonblock ? 1UL : 0UL;
#endif
  return ioctlsocket(sockfd, FIONBIO, &flags);

#elif defined(HAVE_IOCTLSOCKET_CAMEL_FIONBIO)

  /* Amiga */
  long flags = nonblock ? 1L : 0L;
  return IoctlSocket(sockfd, FIONBIO, flags);

#elif defined(HAVE_SETSOCKOPT_SO_NONBLOCK)

  /* BeOS */
  long b = nonblock ? 1L : 0L;
  return setsockopt(sockfd, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));

#else
#  error "no non-blocking method was found/used/set"
#endif
}

#if defined(IPV6_V6ONLY) && defined(WIN32)
/* It makes support for IPv4-mapped IPv6 addresses.
 * Linux kernel, NetBSD, FreeBSD and Darwin: default is off;
 * Windows Vista and later: default is on;
 * DragonFly BSD: acts like off, and dummy setting;
 * OpenBSD and earlier Windows: unsupported.
 * Linux: controlled by /proc/sys/net/ipv6/bindv6only.
 */
static void set_ipv6_v6only(ares_socket_t sockfd, int on)
{
  (void)setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, sizeof(on));
}
#else
#define set_ipv6_v6only(s,v)
#endif

static int configure_socket(ares_socket_t s, int family, ares_channel channel)
{
  union {
    struct sockaddr     sa;
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } local;

  /* do not set options for user-managed sockets */
  if (channel->sock_funcs && channel->sock_funcs->asocket)
    return 0;

  (void)setsocknonblock(s, TRUE);

#if defined(FD_CLOEXEC) && !defined(MSDOS)
  /* Configure the socket fd as close-on-exec. */
  if (fcntl(s, F_SETFD, FD_CLOEXEC) == -1)
    return -1;  /* LCOV_EXCL_LINE */
#endif

  /* Set the socket's send and receive buffer sizes. */
  if ((channel->socket_send_buffer_size > 0) &&
      setsockopt(s, SOL_SOCKET, SO_SNDBUF,
                 (void *)&channel->socket_send_buffer_size,
                 sizeof(channel->socket_send_buffer_size)) == -1)
    return -1;

  if ((channel->socket_receive_buffer_size > 0) &&
      setsockopt(s, SOL_SOCKET, SO_RCVBUF,
                 (void *)&channel->socket_receive_buffer_size,
                 sizeof(channel->socket_receive_buffer_size)) == -1)
    return -1;

#ifdef SO_BINDTODEVICE
  if (channel->local_dev_name[0]) {
    if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE,
                   channel->local_dev_name, sizeof(channel->local_dev_name))) {
      /* Only root can do this, and usually not fatal if it doesn't work, so */
      /* just continue on. */
    }
  }
#endif

  if (family == AF_INET) {
    if (channel->local_ip4) {
      memset(&local.sa4, 0, sizeof(local.sa4));
      local.sa4.sin_family = AF_INET;
      local.sa4.sin_addr.s_addr = htonl(channel->local_ip4);
      if (bind(s, &local.sa, sizeof(local.sa4)) < 0)
        return -1;
    }
  }
  else if (family == AF_INET6) {
    if (memcmp(channel->local_ip6, &ares_in6addr_any,
               sizeof(channel->local_ip6)) != 0) {
      memset(&local.sa6, 0, sizeof(local.sa6));
      local.sa6.sin6_family = AF_INET6;
      memcpy(&local.sa6.sin6_addr, channel->local_ip6,
             sizeof(channel->local_ip6));
      if (bind(s, &local.sa, sizeof(local.sa6)) < 0)
        return -1;
    }
    set_ipv6_v6only(s, 0);
  }

  return 0;
}

static int open_socket(ares_channel channel, struct server_state *server,
                       int is_tcp)
{
  ares_socket_t s;
  int opt;
  ares_socklen_t salen;
  union {
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } saddr;
  struct sockaddr *sa;
  unsigned short port;
  struct server_connection *conn;
  ares__llist_node_t *node;

  if (is_tcp) {
    port = aresx_sitous(server->addr.tcp_port?
                        server->addr.tcp_port:channel->tcp_port);
  } else {
    port = aresx_sitous(server->addr.udp_port?
                        server->addr.udp_port:channel->udp_port);
  }

  switch (server->addr.family) {
    case AF_INET:
      sa = (void *)&saddr.sa4;
      salen = sizeof(saddr.sa4);
      memset(sa, 0, salen);
      saddr.sa4.sin_family = AF_INET;
      saddr.sa4.sin_port   = port;
      memcpy(&saddr.sa4.sin_addr, &server->addr.addrV4,
             sizeof(server->addr.addrV4));
      break;
    case AF_INET6:
      sa = (void *)&saddr.sa6;
      salen = sizeof(saddr.sa6);
      memset(sa, 0, salen);
      saddr.sa6.sin6_family = AF_INET6;
      saddr.sa6.sin6_port   = port;
      memcpy(&saddr.sa6.sin6_addr, &server->addr.addrV6,
             sizeof(server->addr.addrV6));
      break;
    default:
      return ARES_EBADFAMILY;  /* LCOV_EXCL_LINE */
  }

  /* Acquire a socket. */
  s = ares__open_socket(channel, server->addr.family,
                        is_tcp?SOCK_STREAM:SOCK_DGRAM, 0);
  if (s == ARES_SOCKET_BAD)
    return ARES_ECONNREFUSED;

  /* Configure it. */
  if (configure_socket(s, server->addr.family, channel) < 0) {
    ares__close_socket(channel, s);
    return ARES_ECONNREFUSED;
  }

#ifdef TCP_NODELAY
  if (is_tcp) {
    /*
     * Disable the Nagle algorithm (only relevant for TCP sockets, and thus not
     * in configure_socket). In general, in DNS lookups we're pretty much
     * interested in firing off a single request and then waiting for a reply,
     * so batching isn't very interesting.
     */
    opt = 1;
    if (!channel->sock_funcs || !channel->sock_funcs->asocket) {
      if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void *)&opt, sizeof(opt))
          == -1) {
        ares__close_socket(channel, s);
        return ARES_ECONNREFUSED;
      }
    }
  }
#endif

  if (channel->sock_config_cb) {
    int err = channel->sock_config_cb(s, SOCK_STREAM,
                                      channel->sock_config_cb_data);
    if (err < 0) {
      ares__close_socket(channel, s);
      return ARES_ECONNREFUSED;
    }
  }

  /* Connect to the server. */
  if (ares__connect_socket(channel, s, sa, salen) == -1) {
    int err = SOCKERRNO;

    if (err != EINPROGRESS && err != EWOULDBLOCK) {
      ares__close_socket(channel, s);
      return ARES_ECONNREFUSED;
    }
  }

  if (channel->sock_create_cb) {
    int err = channel->sock_create_cb(s, SOCK_STREAM,
                                      channel->sock_create_cb_data);
    if (err < 0) {
      ares__close_socket(channel, s);
      return ARES_ECONNREFUSED;
    }
  }

  conn = ares_malloc(sizeof(*conn));
  if (conn == NULL) {
    ares__close_socket(channel, s);
    return ARES_ENOMEM;
  }
  memset(conn, 0, sizeof(*conn));
  conn->fd     = s;
  conn->server = server;
  conn->queries_to_conn = ares__llist_create(NULL);
  conn->is_tcp = is_tcp;
  if (conn->queries_to_conn == NULL) {
    ares__close_socket(channel, s);
    ares_free(conn);
    return ARES_ENOMEM;
  }

  /* TCP connections are thrown to the end as we don't spawn multiple TCP
   * connections. UDP connections are put on front where the newest connection
   * can be quickly pulled */
  if (is_tcp) {
    node = ares__llist_insert_last(server->connections, conn);
  } else {
    node = ares__llist_insert_first(server->connections, conn);
  }
  if (node == NULL) {
    ares__close_socket(channel, s);
    ares__llist_destroy(conn->queries_to_conn);
    ares_free(conn);
    return ARES_ENOMEM;
  }

  /* Register globally to quickly map event on file descriptor to connection
   * node object */
  if (!ares__htable_asvp_insert(channel->connnode_by_socket, s, node)) {
    ares__close_socket(channel, s);
    ares__llist_destroy(conn->queries_to_conn);
    ares__llist_node_claim(node);
    ares_free(conn);
    return ARES_ENOMEM;
  }

  SOCK_STATE_CALLBACK(channel, s, 1, 0);

  if (is_tcp) {
    server->tcp_connection_generation = ++channel->tcp_connection_generation;
    server->tcp_conn = conn;
  }

  return ARES_SUCCESS;
}


static int same_questions(const unsigned char *qbuf, int qlen,
                          const unsigned char *abuf, int alen)
{
  struct {
    const unsigned char *p;
    int qdcount;
    char *name;
    long namelen;
    int type;
    int dnsclass;
  } q, a;
  int i, j;

  if (qlen < HFIXEDSZ || alen < HFIXEDSZ)
    return 0;

  /* Extract qdcount from the request and reply buffers and compare them. */
  q.qdcount = DNS_HEADER_QDCOUNT(qbuf);
  a.qdcount = DNS_HEADER_QDCOUNT(abuf);
  if (q.qdcount != a.qdcount)
    return 0;

  /* For each question in qbuf, find it in abuf. */
  q.p = qbuf + HFIXEDSZ;
  for (i = 0; i < q.qdcount; i++)
    {
      /* Decode the question in the query. */
      if (ares_expand_name(q.p, qbuf, qlen, &q.name, &q.namelen)
          != ARES_SUCCESS)
        return 0;
      q.p += q.namelen;
      if (q.p + QFIXEDSZ > qbuf + qlen)
        {
          ares_free(q.name);
          return 0;
        }
      q.type = DNS_QUESTION_TYPE(q.p);
      q.dnsclass = DNS_QUESTION_CLASS(q.p);
      q.p += QFIXEDSZ;

      /* Search for this question in the answer. */
      a.p = abuf + HFIXEDSZ;
      for (j = 0; j < a.qdcount; j++)
        {
          /* Decode the question in the answer. */
          if (ares_expand_name(a.p, abuf, alen, &a.name, &a.namelen)
              != ARES_SUCCESS)
            {
              ares_free(q.name);
              return 0;
            }
          a.p += a.namelen;
          if (a.p + QFIXEDSZ > abuf + alen)
            {
              ares_free(q.name);
              ares_free(a.name);
              return 0;
            }
          a.type = DNS_QUESTION_TYPE(a.p);
          a.dnsclass = DNS_QUESTION_CLASS(a.p);
          a.p += QFIXEDSZ;

          /* Compare the decoded questions. */
          if (strcasecmp(q.name, a.name) == 0 && q.type == a.type
              && q.dnsclass == a.dnsclass)
            {
              ares_free(a.name);
              break;
            }
          ares_free(a.name);
        }

      ares_free(q.name);
      if (j == a.qdcount)
        return 0;
    }
  return 1;
}

static int same_address(struct sockaddr *sa, struct ares_addr *aa)
{
  void *addr1;
  void *addr2;

  if (sa->sa_family == aa->family)
    {
      switch (aa->family)
        {
          case AF_INET:
            addr1 = &aa->addrV4;
            addr2 = &(CARES_INADDR_CAST(struct sockaddr_in *, sa))->sin_addr;
            if (memcmp(addr1, addr2, sizeof(aa->addrV4)) == 0)
              return 1; /* match */
            break;
          case AF_INET6:
            addr1 = &aa->addrV6;
            addr2 = &(CARES_INADDR_CAST(struct sockaddr_in6 *, sa))->sin6_addr;
            if (memcmp(addr1, addr2, sizeof(aa->addrV6)) == 0)
              return 1; /* match */
            break;
          default:
            break;  /* LCOV_EXCL_LINE */
        }
    }
  return 0; /* different */
}

/* search for an OPT RR in the response */
static int has_opt_rr(const unsigned char *abuf, int alen)
{
  unsigned int qdcount, ancount, nscount, arcount, i;
  const unsigned char *aptr;
  int status;

  if (alen < HFIXEDSZ)
    return -1;

  /* Parse the answer header. */
  qdcount = DNS_HEADER_QDCOUNT(abuf);
  ancount = DNS_HEADER_ANCOUNT(abuf);
  nscount = DNS_HEADER_NSCOUNT(abuf);
  arcount = DNS_HEADER_ARCOUNT(abuf);

  aptr = abuf + HFIXEDSZ;

  /* skip the questions */
  for (i = 0; i < qdcount; i++)
    {
      char* name;
      long len;
      status = ares_expand_name(aptr, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
        return -1;
      ares_free_string(name);
      if (aptr + len + QFIXEDSZ > abuf + alen)
        return -1;
      aptr += len + QFIXEDSZ;
    }

  /* skip the ancount and nscount */
  for (i = 0; i < ancount + nscount; i++)
    {
      char* name;
      long len;
      int dlen;
      status = ares_expand_name(aptr, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
        return -1;
      ares_free_string(name);
      if (aptr + len + RRFIXEDSZ > abuf + alen)
        return -1;
      aptr += len;
      dlen = DNS_RR_LEN(aptr);
      aptr += RRFIXEDSZ;
      if (aptr + dlen > abuf + alen)
        return -1;
      aptr += dlen;
    }

  /* search for rr type (41) - opt */
  for (i = 0; i < arcount; i++)
    {
      char* name;
      long len;
      int dlen;
      status = ares_expand_name(aptr, abuf, alen, &name, &len);
      if (status != ARES_SUCCESS)
        return -1;
      ares_free_string(name);
      if (aptr + len + RRFIXEDSZ > abuf + alen)
        return -1;
      aptr += len;

      if (DNS_RR_TYPE(aptr) == T_OPT)
        return 1;

      dlen = DNS_RR_LEN(aptr);
      aptr += RRFIXEDSZ;
      if (aptr + dlen > abuf + alen)
        return -1;
      aptr += dlen;
    }

  return 0;
}

static void ares_detach_query(struct query *query)
{
  /* Remove the query from all the lists in which it is linked */
  ares__htable_stvp_remove(query->channel->queries_by_qid, query->qid);
  ares__slist_node_destroy(query->node_queries_by_timeout);
  ares__llist_node_destroy(query->node_queries_to_conn);
  ares__llist_node_destroy(query->node_all_queries);
  query->node_queries_by_timeout = NULL;
  query->node_queries_to_conn    = NULL;
  query->node_all_queries        = NULL;
}

static void end_query(ares_channel channel, struct query *query, int status,
                      const unsigned char *abuf, int alen)
{
  (void)channel;

  ares_detach_query(query);

  /* Invoke the callback. */
  query->callback(query->arg, status, query->timeouts,
                  /* due to prior design flaws, abuf isn't meant to be modified,
                   * but bad prototypes, ugh.  Lets cast off constfor compat. */
                  (unsigned char *)((void *)((size_t)abuf)),
                  alen);
  ares__free_query(query);
}

void ares__free_query(struct query *query)
{
  ares_detach_query(query);
  /* Zero out some important stuff, to help catch bugs */
  query->callback = NULL;
  query->arg = NULL;
  /* Deallocate the memory associated with the query */
  ares_free(query->tcpbuf);
  ares_free(query->server_info);
  ares_free(query);
}

ares_socket_t ares__open_socket(ares_channel channel,
                                int af, int type, int protocol)
{
  if (channel->sock_funcs && channel->sock_funcs->asocket) {
    return channel->sock_funcs->asocket(af,
                                        type,
                                        protocol,
                                        channel->sock_func_cb_data);
  }

  return socket(af, type, protocol);
}

int ares__connect_socket(ares_channel channel,
                         ares_socket_t sockfd,
                         const struct sockaddr *addr,
                         ares_socklen_t addrlen)
{
  if (channel->sock_funcs && channel->sock_funcs->aconnect) {
    return channel->sock_funcs->aconnect(sockfd,
                                         addr,
                                         addrlen,
                                         channel->sock_func_cb_data);
  }

  return connect(sockfd, addr, addrlen);
}

void ares__close_socket(ares_channel channel, ares_socket_t s)
{
  if (channel->sock_funcs && channel->sock_funcs->aclose) {
    channel->sock_funcs->aclose(s, channel->sock_func_cb_data);
  } else {
    sclose(s);
  }
}

#ifndef HAVE_WRITEV
/* Structure for scatter/gather I/O. */
struct iovec
{
  void *iov_base;  /* Pointer to data. */
  size_t iov_len;  /* Length of data.  */
};
#endif

static ares_ssize_t ares__socket_write(ares_channel channel, ares_socket_t s, const void * data, size_t len)
{
  if (channel->sock_funcs && channel->sock_funcs->asendv) {
    struct iovec vec;
    vec.iov_base = (void*)data;
    vec.iov_len = len;
    return channel->sock_funcs->asendv(s, &vec, 1, channel->sock_func_cb_data);
  }
  return swrite(s, data, len);
}

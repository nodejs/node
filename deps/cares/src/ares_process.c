
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2004-2017 by Daniel Stenberg
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
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

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
static void read_tcp_data(ares_channel channel, fd_set *read_fds,
                          ares_socket_t read_fd, struct timeval *now);
static void read_udp_packets(ares_channel channel, fd_set *read_fds,
                             ares_socket_t read_fd, struct timeval *now);
static void advance_tcp_send_queue(ares_channel channel, int whichserver,
                                   ares_ssize_t num_bytes);
static void process_timeouts(ares_channel channel, struct timeval *now);
static void process_broken_connections(ares_channel channel,
                                       struct timeval *now);
static void process_answer(ares_channel channel, unsigned char *abuf,
                           int alen, int whichserver, int tcp,
                           struct timeval *now);
static void handle_error(ares_channel channel, int whichserver,
                         struct timeval *now);
static void skip_server(ares_channel channel, struct query *query,
                        int whichserver);
static void next_server(ares_channel channel, struct query *query,
                        struct timeval *now);
static int open_tcp_socket(ares_channel channel, struct server_state *server);
static int open_udp_socket(ares_channel channel, struct server_state *server);
static int same_questions(const unsigned char *qbuf, int qlen,
                          const unsigned char *abuf, int alen);
static int same_address(struct sockaddr *sa, struct ares_addr *aa);
static void end_query(ares_channel channel, struct query *query, int status,
                      unsigned char *abuf, int alen);

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
  read_tcp_data(channel, read_fds, read_fd, &now);
  read_udp_packets(channel, read_fds, read_fd, &now);
  process_timeouts(channel, &now);
  process_broken_connections(channel, &now);
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

static ares_ssize_t socket_writev(ares_channel channel, ares_socket_t s, const struct iovec * vec, int len)
{
  if (channel->sock_funcs)
    return channel->sock_funcs->asendv(s, vec, len, channel->sock_func_cb_data);

  return writev(s, vec, len);
}

static ares_ssize_t socket_write(ares_channel channel, ares_socket_t s, const void * data, size_t len)
{
  if (channel->sock_funcs)
    {
      struct iovec vec;
      vec.iov_base = (void*)data;
      vec.iov_len = len;
      return channel->sock_funcs->asendv(s, &vec, 1, channel->sock_func_cb_data);
    }
  return swrite(s, data, len);
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
  struct send_request *sendreq;
  struct iovec *vec;
  int i;
  ares_ssize_t scount;
  ares_ssize_t wcount;
  size_t n;

  if(!write_fds && (write_fd == ARES_SOCKET_BAD))
    /* no possible action */
    return;

  for (i = 0; i < channel->nservers; i++)
    {
      /* Make sure server has data to send and is selected in write_fds or
         write_fd. */
      server = &channel->servers[i];
      if (!server->qhead || server->tcp_socket == ARES_SOCKET_BAD ||
          server->is_broken)
        continue;

      if(write_fds) {
        if(!FD_ISSET(server->tcp_socket, write_fds))
          continue;
      }
      else {
        if(server->tcp_socket != write_fd)
          continue;
      }

      if(write_fds)
        /* If there's an error and we close this socket, then open
         * another with the same fd to talk to another server, then we
         * don't want to think that it was the new socket that was
         * ready. This is not disastrous, but is likely to result in
         * extra system calls and confusion. */
        FD_CLR(server->tcp_socket, write_fds);

      /* Count the number of send queue items. */
      n = 0;
      for (sendreq = server->qhead; sendreq; sendreq = sendreq->next)
        n++;

      /* Allocate iovecs so we can send all our data at once. */
      vec = ares_malloc(n * sizeof(struct iovec));
      if (vec)
        {
          /* Fill in the iovecs and send. */
          n = 0;
          for (sendreq = server->qhead; sendreq; sendreq = sendreq->next)
            {
              vec[n].iov_base = (char *) sendreq->data;
              vec[n].iov_len = sendreq->len;
              n++;
            }
          wcount = socket_writev(channel, server->tcp_socket, vec, (int)n);
          ares_free(vec);
          if (wcount < 0)
            {
              if (!try_again(SOCKERRNO))
                handle_error(channel, i, now);
              continue;
            }

          /* Advance the send queue by as many bytes as we sent. */
          advance_tcp_send_queue(channel, i, wcount);
        }
      else
        {
          /* Can't allocate iovecs; just send the first request. */
          sendreq = server->qhead;

          scount = socket_write(channel, server->tcp_socket, sendreq->data, sendreq->len);
          if (scount < 0)
            {
              if (!try_again(SOCKERRNO))
                handle_error(channel, i, now);
              continue;
            }

          /* Advance the send queue by as many bytes as we sent. */
          advance_tcp_send_queue(channel, i, scount);
        }
    }
}

/* Consume the given number of bytes from the head of the TCP send queue. */
static void advance_tcp_send_queue(ares_channel channel, int whichserver,
                                   ares_ssize_t num_bytes)
{
  struct send_request *sendreq;
  struct server_state *server = &channel->servers[whichserver];
  while (num_bytes > 0) {
    sendreq = server->qhead;
    if ((size_t)num_bytes >= sendreq->len) {
      num_bytes -= sendreq->len;
      server->qhead = sendreq->next;
      if (sendreq->data_storage)
        ares_free(sendreq->data_storage);
      ares_free(sendreq);
      if (server->qhead == NULL) {
        SOCK_STATE_CALLBACK(channel, server->tcp_socket, 1, 0);
        server->qtail = NULL;

        /* qhead is NULL so we cannot continue this loop */
        break;
      }
    }
    else {
      sendreq->data += num_bytes;
      sendreq->len -= num_bytes;
      num_bytes = 0;
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
   if (channel->sock_funcs)
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
   if (channel->sock_funcs)
      return channel->sock_funcs->arecvfrom(s, data, data_len, 0, 0, 0,
	 channel->sock_func_cb_data);

   return sread(s, data, data_len);
}

/* If any TCP socket selects true for reading, read some data,
 * allocate a buffer if we finish reading the length word, and process
 * a packet if we finish reading one.
 */
static void read_tcp_data(ares_channel channel, fd_set *read_fds,
                          ares_socket_t read_fd, struct timeval *now)
{
  struct server_state *server;
  int i;
  ares_ssize_t count;

  if(!read_fds && (read_fd == ARES_SOCKET_BAD))
    /* no possible action */
    return;

  for (i = 0; i < channel->nservers; i++)
    {
      /* Make sure the server has a socket and is selected in read_fds. */
      server = &channel->servers[i];
      if (server->tcp_socket == ARES_SOCKET_BAD || server->is_broken)
        continue;

      if(read_fds) {
        if(!FD_ISSET(server->tcp_socket, read_fds))
          continue;
      }
      else {
        if(server->tcp_socket != read_fd)
          continue;
      }

      if(read_fds)
        /* If there's an error and we close this socket, then open another
         * with the same fd to talk to another server, then we don't want to
         * think that it was the new socket that was ready. This is not
         * disastrous, but is likely to result in extra system calls and
         * confusion. */
        FD_CLR(server->tcp_socket, read_fds);

      if (server->tcp_lenbuf_pos != 2)
        {
          /* We haven't yet read a length word, so read that (or
           * what's left to read of it).
           */
          count = socket_recv(channel, server->tcp_socket,
			      server->tcp_lenbuf + server->tcp_lenbuf_pos,
			      2 - server->tcp_lenbuf_pos);
          if (count <= 0)
            {
              if (!(count == -1 && try_again(SOCKERRNO)))
                handle_error(channel, i, now);
              continue;
            }

          server->tcp_lenbuf_pos += (int)count;
          if (server->tcp_lenbuf_pos == 2)
            {
              /* We finished reading the length word.  Decode the
               * length and allocate a buffer for the data.
               */
              server->tcp_length = server->tcp_lenbuf[0] << 8
                | server->tcp_lenbuf[1];
              server->tcp_buffer = ares_malloc(server->tcp_length);
              if (!server->tcp_buffer) {
                handle_error(channel, i, now);
                return; /* bail out on malloc failure. TODO: make this
                           function return error codes */
              }
              server->tcp_buffer_pos = 0;
            }
        }
      else
        {
          /* Read data into the allocated buffer. */
          count = socket_recv(channel, server->tcp_socket,
			      server->tcp_buffer + server->tcp_buffer_pos,
			      server->tcp_length - server->tcp_buffer_pos);
          if (count <= 0)
            {
              if (!(count == -1 && try_again(SOCKERRNO)))
                handle_error(channel, i, now);
              continue;
            }

          server->tcp_buffer_pos += (int)count;
          if (server->tcp_buffer_pos == server->tcp_length)
            {
              /* We finished reading this answer; process it and
               * prepare to read another length word.
               */
              process_answer(channel, server->tcp_buffer, server->tcp_length,
                             i, 1, now);
              ares_free(server->tcp_buffer);
              server->tcp_buffer = NULL;
              server->tcp_lenbuf_pos = 0;
              server->tcp_buffer_pos = 0;
            }
        }
    }
}

/* If any UDP sockets select true for reading, process them. */
static void read_udp_packets(ares_channel channel, fd_set *read_fds,
                             ares_socket_t read_fd, struct timeval *now)
{
  struct server_state *server;
  int i;
  ares_ssize_t count;
  unsigned char buf[MAXENDSSZ + 1];
#ifdef HAVE_RECVFROM
  ares_socklen_t fromlen;
  union {
    struct sockaddr     sa;
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } from;
#endif

  if(!read_fds && (read_fd == ARES_SOCKET_BAD))
    /* no possible action */
    return;

  for (i = 0; i < channel->nservers; i++)
    {
      /* Make sure the server has a socket and is selected in read_fds. */
      server = &channel->servers[i];

      if (server->udp_socket == ARES_SOCKET_BAD || server->is_broken)
        continue;

      if(read_fds) {
        if(!FD_ISSET(server->udp_socket, read_fds))
          continue;
      }
      else {
        if(server->udp_socket != read_fd)
          continue;
      }

      if(read_fds)
        /* If there's an error and we close this socket, then open
         * another with the same fd to talk to another server, then we
         * don't want to think that it was the new socket that was
         * ready. This is not disastrous, but is likely to result in
         * extra system calls and confusion. */
        FD_CLR(server->udp_socket, read_fds);

      /* To reduce event loop overhead, read and process as many
       * packets as we can. */
      do {
        if (server->udp_socket == ARES_SOCKET_BAD)
          count = 0;

        else {
          if (server->addr.family == AF_INET)
            fromlen = sizeof(from.sa4);
          else
            fromlen = sizeof(from.sa6);
          count = socket_recvfrom(channel, server->udp_socket, (void *)buf,
                                  sizeof(buf), 0, &from.sa, &fromlen);
        }

        if (count == -1 && try_again(SOCKERRNO))
          continue;
        else if (count <= 0)
          handle_error(channel, i, now);
#ifdef HAVE_RECVFROM
        else if (!same_address(&from.sa, &server->addr))
          /* The address the response comes from does not match the address we
           * sent the request to. Someone may be attempting to perform a cache
           * poisoning attack. */
          break;
#endif
        else
          process_answer(channel, buf, (int)count, i, 0, now);
       } while (count > 0);
    }
}

/* If any queries have timed out, note the timeout and move them on. */
static void process_timeouts(ares_channel channel, struct timeval *now)
{
  time_t t;  /* the time of the timeouts we're processing */
  struct query *query;
  struct list_node* list_head;
  struct list_node* list_node;

  /* Process all the timeouts that have fired since the last time we processed
   * timeouts. If things are going well, then we'll have hundreds/thousands of
   * queries that fall into future buckets, and only a handful of requests
   * that fall into the "now" bucket, so this should be quite quick.
   */
  for (t = channel->last_timeout_processed; t <= now->tv_sec; t++)
    {
      list_head = &(channel->queries_by_timeout[t % ARES_TIMEOUT_TABLE_SIZE]);
      for (list_node = list_head->next; list_node != list_head; )
        {
          query = list_node->data;
          list_node = list_node->next;  /* in case the query gets deleted */
          if (query->timeout.tv_sec && ares__timedout(now, &query->timeout))
            {
              query->error_status = ARES_ETIMEOUT;
              ++query->timeouts;
              next_server(channel, query, now);
            }
        }
     }
  channel->last_timeout_processed = now->tv_sec;
}

/* Handle an answer from a server. */
static void process_answer(ares_channel channel, unsigned char *abuf,
                           int alen, int whichserver, int tcp,
                           struct timeval *now)
{
  int tc, rcode, packetsz;
  unsigned short id;
  struct query *query;
  struct list_node* list_head;
  struct list_node* list_node;

  /* If there's no room in the answer for a header, we can't do much
   * with it. */
  if (alen < HFIXEDSZ)
    return;

  /* Grab the query ID, truncate bit, and response code from the packet. */
  id = DNS_HEADER_QID(abuf);
  tc = DNS_HEADER_TC(abuf);
  rcode = DNS_HEADER_RCODE(abuf);

  /* Find the query corresponding to this packet. The queries are
   * hashed/bucketed by query id, so this lookup should be quick.  Note that
   * both the query id and the questions must be the same; when the query id
   * wraps around we can have multiple outstanding queries with the same query
   * id, so we need to check both the id and question.
   */
  query = NULL;
  list_head = &(channel->queries_by_qid[id % ARES_QID_TABLE_SIZE]);
  for (list_node = list_head->next; list_node != list_head;
       list_node = list_node->next)
    {
      struct query *q = list_node->data;
      if ((q->qid == id) && same_questions(q->qbuf, q->qlen, abuf, alen))
        {
          query = q;
          break;
        }
    }
  if (!query)
    return;

  packetsz = PACKETSZ;
  /* If we use EDNS and server answers with one of these RCODES, the protocol
   * extension is not understood by the responder. We must retry the query
   * without EDNS enabled.
   */
  if (channel->flags & ARES_FLAG_EDNS)
  {
      packetsz = channel->ednspsz;
      if (rcode == NOTIMP || rcode == FORMERR || rcode == SERVFAIL)
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
          skip_server(channel, query, whichserver);
          if (query->server == whichserver)
            next_server(channel, query, now);
          return;
        }
    }

  end_query(channel, query, ARES_SUCCESS, abuf, alen);
}

/* Close all the connections that are no longer usable. */
static void process_broken_connections(ares_channel channel,
                                       struct timeval *now)
{
  int i;
  for (i = 0; i < channel->nservers; i++)
    {
      struct server_state *server = &channel->servers[i];
      if (server->is_broken)
        {
          handle_error(channel, i, now);
        }
    }
}

/* Swap the contents of two lists */
static void swap_lists(struct list_node* head_a,
                       struct list_node* head_b)
{
  int is_a_empty = ares__is_list_empty(head_a);
  int is_b_empty = ares__is_list_empty(head_b);
  struct list_node old_a = *head_a;
  struct list_node old_b = *head_b;

  if (is_a_empty) {
    ares__init_list_head(head_b);
  } else {
    *head_b = old_a;
    old_a.next->prev = head_b;
    old_a.prev->next = head_b;
  }
  if (is_b_empty) {
    ares__init_list_head(head_a);
  } else {
    *head_a = old_b;
    old_b.next->prev = head_a;
    old_b.prev->next = head_a;
  }
}

static void handle_error(ares_channel channel, int whichserver,
                         struct timeval *now)
{
  struct server_state *server;
  struct query *query;
  struct list_node list_head;
  struct list_node* list_node;

  server = &channel->servers[whichserver];

  /* Reset communications with this server. */
  ares__close_sockets(channel, server);

  /* Tell all queries talking to this server to move on and not try this
   * server again. We steal the current list of queries that were in-flight to
   * this server, since when we call next_server this can cause the queries to
   * be re-sent to this server, which will re-insert these queries in that
   * same server->queries_to_server list.
   */
  ares__init_list_head(&list_head);
  swap_lists(&list_head, &(server->queries_to_server));
  for (list_node = list_head.next; list_node != &list_head; )
    {
      query = list_node->data;
      list_node = list_node->next;  /* in case the query gets deleted */
      assert(query->server == whichserver);
      skip_server(channel, query, whichserver);
      next_server(channel, query, now);
    }
  /* Each query should have removed itself from our temporary list as
   * it re-sent itself or finished up...
   */
  assert(ares__is_list_empty(&list_head));
}

static void skip_server(ares_channel channel, struct query *query,
                        int whichserver)
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
      query->server_info[whichserver].skip_server = 1;
    }
}

static void next_server(ares_channel channel, struct query *query,
                        struct timeval *now)
{
  /* We need to try each server channel->tries times. We have channel->nservers
   * servers to try. In total, we need to do channel->nservers * channel->tries
   * attempts. Use query->try to remember how many times we already attempted
   * this query. Use modular arithmetic to find the next server to try. */
  while (++(query->try_count) < (channel->nservers * channel->tries))
    {
      struct server_state *server;

      /* Move on to the next server. */
      query->server = (query->server + 1) % channel->nservers;
      server = &channel->servers[query->server];

      /* We don't want to use this server if (1) we decided this connection is
       * broken, and thus about to be closed, (2) we've decided to skip this
       * server because of earlier errors we encountered, or (3) we already
       * sent this query over this exact connection.
       */
      if (!server->is_broken &&
           !query->server_info[query->server].skip_server &&
           !(query->using_tcp &&
             (query->server_info[query->server].tcp_connection_generation ==
              server->tcp_connection_generation)))
        {
           ares__send_query(channel, query, now);
           return;
        }

      /* You might think that with TCP we only need one try. However, even
       * when using TCP, servers can time-out our connection just as we're
       * sending a request, or close our connection because they die, or never
       * send us a reply because they get wedged or tickle a bug that drops
       * our request.
       */
    }

  /* If we are here, all attempts to perform query failed. */
  end_query(channel, query, query->error_status, NULL, 0);
}

void ares__send_query(ares_channel channel, struct query *query,
                      struct timeval *now)
{
  struct send_request *sendreq;
  struct server_state *server;
  int timeplus;

  server = &channel->servers[query->server];
  if (query->using_tcp)
    {
      /* Make sure the TCP socket for this server is set up and queue
       * a send request.
       */
      if (server->tcp_socket == ARES_SOCKET_BAD)
        {
          if (open_tcp_socket(channel, server) == -1)
            {
              skip_server(channel, query, query->server);
              next_server(channel, query, now);
              return;
            }
        }
      sendreq = ares_malloc(sizeof(struct send_request));
      if (!sendreq)
        {
        end_query(channel, query, ARES_ENOMEM, NULL, 0);
          return;
        }
      memset(sendreq, 0, sizeof(struct send_request));
      /* To make the common case fast, we avoid copies by using the query's
       * tcpbuf for as long as the query is alive. In the rare case where the
       * query ends while it's queued for transmission, then we give the
       * sendreq its own copy of the request packet and put it in
       * sendreq->data_storage.
       */
      sendreq->data_storage = NULL;
      sendreq->data = query->tcpbuf;
      sendreq->len = query->tcplen;
      sendreq->owner_query = query;
      sendreq->next = NULL;
      if (server->qtail)
        server->qtail->next = sendreq;
      else
        {
          SOCK_STATE_CALLBACK(channel, server->tcp_socket, 1, 1);
          server->qhead = sendreq;
        }
      server->qtail = sendreq;
      query->server_info[query->server].tcp_connection_generation =
        server->tcp_connection_generation;
    }
  else
    {
      if (server->udp_socket == ARES_SOCKET_BAD)
        {
          if (open_udp_socket(channel, server) == -1)
            {
              skip_server(channel, query, query->server);
              next_server(channel, query, now);
              return;
            }
        }
      if (socket_write(channel, server->udp_socket, query->qbuf, query->qlen) == -1)
        {
          /* FIXME: Handle EAGAIN here since it likely can happen. */
          skip_server(channel, query, query->server);
          next_server(channel, query, now);
          return;
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

    query->timeout = *now;
    timeadd(&query->timeout, timeplus);
    /* Keep track of queries bucketed by timeout, so we can process
     * timeout events quickly.
     */
    ares__remove_from_list(&(query->queries_by_timeout));
    ares__insert_in_list(
        &(query->queries_by_timeout),
        &(channel->queries_by_timeout[query->timeout.tv_sec %
                                      ARES_TIMEOUT_TABLE_SIZE]));

    /* Keep track of queries bucketed by server, so we can process server
     * errors quickly.
     */
    ares__remove_from_list(&(query->queries_to_server));
    ares__insert_in_list(&(query->queries_to_server),
                         &(server->queries_to_server));
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

static int configure_socket(ares_socket_t s, int family, ares_channel channel)
{
  union {
    struct sockaddr     sa;
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } local;

  /* do not set options for user-managed sockets */
  if (channel->sock_funcs)
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
  }

  return 0;
}

static int open_tcp_socket(ares_channel channel, struct server_state *server)
{
  ares_socket_t s;
  int opt;
  ares_socklen_t salen;
  union {
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } saddr;
  struct sockaddr *sa;

  switch (server->addr.family)
    {
      case AF_INET:
        sa = (void *)&saddr.sa4;
        salen = sizeof(saddr.sa4);
        memset(sa, 0, salen);
        saddr.sa4.sin_family = AF_INET;
        if (server->addr.tcp_port) {
          saddr.sa4.sin_port = aresx_sitous(server->addr.tcp_port);
        } else {
          saddr.sa4.sin_port = aresx_sitous(channel->tcp_port);
        }
        memcpy(&saddr.sa4.sin_addr, &server->addr.addrV4,
               sizeof(server->addr.addrV4));
        break;
      case AF_INET6:
        sa = (void *)&saddr.sa6;
        salen = sizeof(saddr.sa6);
        memset(sa, 0, salen);
        saddr.sa6.sin6_family = AF_INET6;
        if (server->addr.tcp_port) {
          saddr.sa6.sin6_port = aresx_sitous(server->addr.tcp_port);
        } else {
          saddr.sa6.sin6_port = aresx_sitous(channel->tcp_port);
        }
        memcpy(&saddr.sa6.sin6_addr, &server->addr.addrV6,
               sizeof(server->addr.addrV6));
        break;
      default:
        return -1;  /* LCOV_EXCL_LINE */
    }

  /* Acquire a socket. */
  s = ares__open_socket(channel, server->addr.family, SOCK_STREAM, 0);
  if (s == ARES_SOCKET_BAD)
    return -1;

  /* Configure it. */
  if (configure_socket(s, server->addr.family, channel) < 0)
    {
       ares__close_socket(channel, s);
       return -1;
    }

#ifdef TCP_NODELAY
  /*
   * Disable the Nagle algorithm (only relevant for TCP sockets, and thus not
   * in configure_socket). In general, in DNS lookups we're pretty much
   * interested in firing off a single request and then waiting for a reply,
   * so batching isn't very interesting.
   */
  opt = 1;
  if (channel->sock_funcs == 0
     &&
     setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
                (void *)&opt, sizeof(opt)) == -1)
    {
       ares__close_socket(channel, s);
       return -1;
    }
#endif

  if (channel->sock_config_cb)
    {
      int err = channel->sock_config_cb(s, SOCK_STREAM,
                                        channel->sock_config_cb_data);
      if (err < 0)
        {
          ares__close_socket(channel, s);
          return err;
        }
    }

  /* Connect to the server. */
  if (ares__connect_socket(channel, s, sa, salen) == -1)
    {
      int err = SOCKERRNO;

      if (err != EINPROGRESS && err != EWOULDBLOCK)
        {
          ares__close_socket(channel, s);
          return -1;
        }
    }

  if (channel->sock_create_cb)
    {
      int err = channel->sock_create_cb(s, SOCK_STREAM,
                                        channel->sock_create_cb_data);
      if (err < 0)
        {
          ares__close_socket(channel, s);
          return err;
        }
    }

  SOCK_STATE_CALLBACK(channel, s, 1, 0);
  server->tcp_buffer_pos = 0;
  server->tcp_socket = s;
  server->tcp_connection_generation = ++channel->tcp_connection_generation;
  return 0;
}

static int open_udp_socket(ares_channel channel, struct server_state *server)
{
  ares_socket_t s;
  ares_socklen_t salen;
  union {
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;
  } saddr;
  struct sockaddr *sa;

  switch (server->addr.family)
    {
      case AF_INET:
        sa = (void *)&saddr.sa4;
        salen = sizeof(saddr.sa4);
        memset(sa, 0, salen);
        saddr.sa4.sin_family = AF_INET;
        if (server->addr.udp_port) {
          saddr.sa4.sin_port = aresx_sitous(server->addr.udp_port);
        } else {
          saddr.sa4.sin_port = aresx_sitous(channel->udp_port);
        }
        memcpy(&saddr.sa4.sin_addr, &server->addr.addrV4,
               sizeof(server->addr.addrV4));
        break;
      case AF_INET6:
        sa = (void *)&saddr.sa6;
        salen = sizeof(saddr.sa6);
        memset(sa, 0, salen);
        saddr.sa6.sin6_family = AF_INET6;
        if (server->addr.udp_port) {
          saddr.sa6.sin6_port = aresx_sitous(server->addr.udp_port);
        } else {
          saddr.sa6.sin6_port = aresx_sitous(channel->udp_port);
        }
        memcpy(&saddr.sa6.sin6_addr, &server->addr.addrV6,
               sizeof(server->addr.addrV6));
        break;
      default:
        return -1;  /* LCOV_EXCL_LINE */
    }

  /* Acquire a socket. */
  s = ares__open_socket(channel, server->addr.family, SOCK_DGRAM, 0);
  if (s == ARES_SOCKET_BAD)
    return -1;

  /* Set the socket non-blocking. */
  if (configure_socket(s, server->addr.family, channel) < 0)
    {
       ares__close_socket(channel, s);
       return -1;
    }

  if (channel->sock_config_cb)
    {
      int err = channel->sock_config_cb(s, SOCK_DGRAM,
                                        channel->sock_config_cb_data);
      if (err < 0)
        {
          ares__close_socket(channel, s);
          return err;
        }
    }

  /* Connect to the server. */
  if (ares__connect_socket(channel, s, sa, salen) == -1)
    {
      int err = SOCKERRNO;

      if (err != EINPROGRESS && err != EWOULDBLOCK)
        {
          ares__close_socket(channel, s);
          return -1;
        }
    }

  if (channel->sock_create_cb)
    {
      int err = channel->sock_create_cb(s, SOCK_DGRAM,
                                        channel->sock_create_cb_data);
      if (err < 0)
        {
          ares__close_socket(channel, s);
          return err;
        }
    }

  SOCK_STATE_CALLBACK(channel, s, 1, 0);

  server->udp_socket = s;
  return 0;
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

static void end_query (ares_channel channel, struct query *query, int status,
                       unsigned char *abuf, int alen)
{
  int i;

  /* First we check to see if this query ended while one of our send
   * queues still has pointers to it.
   */
  for (i = 0; i < channel->nservers; i++)
    {
      struct server_state *server = &channel->servers[i];
      struct send_request *sendreq;
      for (sendreq = server->qhead; sendreq; sendreq = sendreq->next)
        if (sendreq->owner_query == query)
          {
            sendreq->owner_query = NULL;
            assert(sendreq->data_storage == NULL);
            if (status == ARES_SUCCESS)
              {
                /* We got a reply for this query, but this queued sendreq
                 * points into this soon-to-be-gone query's tcpbuf. Probably
                 * this means we timed out and queued the query for
                 * retransmission, then received a response before actually
                 * retransmitting. This is perfectly fine, so we want to keep
                 * the connection running smoothly if we can. But in the worst
                 * case we may have sent only some prefix of the query, with
                 * some suffix of the query left to send. Also, the buffer may
                 * be queued on multiple queues. To prevent dangling pointers
                 * to the query's tcpbuf and handle these cases, we just give
                 * such sendreqs their own copy of the query packet.
                 */
               sendreq->data_storage = ares_malloc(sendreq->len);
               if (sendreq->data_storage != NULL)
                 {
                   memcpy(sendreq->data_storage, sendreq->data, sendreq->len);
                   sendreq->data = sendreq->data_storage;
                 }
              }
            if ((status != ARES_SUCCESS) || (sendreq->data_storage == NULL))
              {
                /* We encountered an error (probably a timeout, suggesting the
                 * DNS server we're talking to is probably unreachable,
                 * wedged, or severely overloaded) or we couldn't copy the
                 * request, so mark the connection as broken. When we get to
                 * process_broken_connections() we'll close the connection and
                 * try to re-send requests to another server.
                 */
               server->is_broken = 1;
               /* Just to be paranoid, zero out this sendreq... */
               sendreq->data = NULL;
               sendreq->len = 0;
             }
          }
    }

  /* Invoke the callback */
  query->callback(query->arg, status, query->timeouts, abuf, alen);
  ares__free_query(query);

  /* Simple cleanup policy: if no queries are remaining, close all network
   * sockets unless STAYOPEN is set.
   */
  if (!(channel->flags & ARES_FLAG_STAYOPEN) &&
      ares__is_list_empty(&(channel->all_queries)))
    {
      for (i = 0; i < channel->nservers; i++)
        ares__close_sockets(channel, &channel->servers[i]);
    }
}

void ares__free_query(struct query *query)
{
  /* Remove the query from all the lists in which it is linked */
  ares__remove_from_list(&(query->queries_by_qid));
  ares__remove_from_list(&(query->queries_by_timeout));
  ares__remove_from_list(&(query->queries_to_server));
  ares__remove_from_list(&(query->all_queries));
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
  if (channel->sock_funcs)
    return channel->sock_funcs->asocket(af,
                                        type,
                                        protocol,
                                        channel->sock_func_cb_data);
  else
    return socket(af, type, protocol);
}

int ares__connect_socket(ares_channel channel,
                         ares_socket_t sockfd,
                         const struct sockaddr *addr,
                         ares_socklen_t addrlen)
{
  if (channel->sock_funcs)
    return channel->sock_funcs->aconnect(sockfd,
                                         addr,
                                         addrlen,
                                         channel->sock_func_cb_data);
  else
    return connect(sockfd, addr, addrlen);
}

void ares__close_socket(ares_channel channel, ares_socket_t s)
{
  if (channel->sock_funcs)
    channel->sock_funcs->aclose(s, channel->sock_func_cb_data);
  else
    sclose(s);
}

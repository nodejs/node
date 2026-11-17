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

#include "ares_private.h"
#include <assert.h>

static void ares_requeue_queries(ares_conn_t  *conn,
                                 ares_status_t requeue_status)
{
  ares_query_t  *query;
  ares_timeval_t now;

  ares_tvnow(&now);

  while ((query = ares_llist_first_val(conn->queries_to_conn)) != NULL) {
    ares_requeue_query(query, &now, requeue_status, ARES_TRUE, NULL, NULL);
  }
}

/*! Release a connection's memory.  Split from the unlink below so that the
 *  two can happen at different times: a caller further up the stack may still
 *  hold this pointer, and reading a detached connection is safe where reading
 *  a freed one is not. */
void ares_conn_reap(ares_conn_t *conn)
{
  if (conn == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  ares_buf_destroy(conn->in_buf);
  ares_buf_destroy(conn->out_buf);
  ares_llist_destroy(conn->queries_to_conn);
  ares_free(conn);
}

void ares_close_connection(ares_conn_t *conn, ares_status_t requeue_status)
{
  ares_server_t  *server;
  ares_channel_t *channel;

  if (conn == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  /* Already torn down and awaiting reclamation.  Reaching here twice is
   * ordinary once a connection can outlive its teardown: a caller holding it
   * from before -- handle_conn_error() after a failed write, say -- closes it
   * again without knowing.  Doing the work twice would requeue queries that
   * have already moved and would put this connection on the pending list a
   * second time, which is a double free. */
  if (conn->flags & ARES_CONN_FLAG_DEAD) {
    return;
  }

  server  = conn->server;
  channel = server->channel;

  /* Unlink from everything that can find this connection.  After this it is
   * unreachable by lookup, which is what stops anything new from picking it
   * up; anything that already holds it is handled by the deferral below. */
  ares_llist_node_claim(
    ares_htable_asvp_get_direct(channel->connnode_by_socket, conn->fd));
  ares_htable_asvp_remove(channel->connnode_by_socket, conn->fd);

  if (conn->flags & ARES_CONN_FLAG_TCP) {
    server->tcp_conn = NULL;
  }

  /* Marked dead as soon as it is unlinked, before anything below can run
   * application code.  ares_requeue_queries() ends queries terminally once
   * their retries are exhausted, which invokes the application's callback; if
   * the flag were set after that, a second ares_close_connection() reaching
   * this connection in the meantime would run the body again -- closing the
   * descriptor twice and listing the connection for reclamation twice, which
   * is a double free.  It also gives ares_close_sockets() its termination
   * condition: dead implies unlinked, so its re-read-the-head loop cannot see
   * a dead connection and spin. */
  conn->flags |= ARES_CONN_FLAG_DEAD;

  /* Requeue queries to other connections */
  ares_requeue_queries(conn, requeue_status);

  /* Hands the descriptor to the queued notification when one is due, so the
   * application is told to stop watching it before it is closed.  Otherwise
   * nothing will carry it and we close it here. */
  if (!ares_conn_sock_state_cb_close(conn)) {
    ares_socket_close(channel, conn->fd);
  }
  conn->fd = ARES_SOCKET_BAD;

  /* If a library call is in progress it may be holding this connection --
   * read_answers() and ares_send_query_int() both do, across code that can
   * reach here.  Keep it readable until that call unwinds.  The buffers and
   * the query list are freed by ares_conn_reap(), so a stale holder sees an
   * emptied connection rather than freed memory. */
  if (channel->deferred_cb_depth > 0) {
    conn->next_pending     = channel->pending_conns;
    channel->pending_conns = conn;
    return;
  }

  ares_conn_reap(conn);
}

void ares_close_sockets(ares_server_t *server)
{
  ares_llist_node_t *node;

  while ((node = ares_llist_node_first(server->connections)) != NULL) {
    ares_conn_t *conn = ares_llist_node_val(node);
    /* This loop re-reads the head each time and relies on the close below
     * unlinking it.  A dead connection returns early without unlinking, so
     * one on this list would spin forever holding the channel lock. */
    assert(!(conn->flags & ARES_CONN_FLAG_DEAD));
    ares_close_connection(conn, ARES_SUCCESS);
  }
}

void ares_check_cleanup_conns(const ares_channel_t *channel)
{
  ares_slist_node_t *snode;

  if (channel == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

restart:
  /* Iterate across each server */
  for (snode = ares_slist_node_first(channel->servers); snode != NULL;
       snode = ares_slist_node_next(snode)) {
    ares_server_t     *server = ares_slist_node_val(snode);
    ares_llist_node_t *cnode;

    /* Iterate across each connection */
    cnode = ares_llist_node_first(server->connections);
    while (cnode != NULL) {
      ares_llist_node_t *next       = ares_llist_node_next(cnode);
      ares_conn_t       *conn       = ares_llist_node_val(cnode);
      ares_bool_t        do_cleanup = ARES_FALSE;
      cnode                         = next;

      /* Has connections, not eligible */
      if (ares_llist_len(conn->queries_to_conn)) {
        continue;
      }

      /* If we are configured not to stay open, close it out */
      if (!(channel->flags & ARES_FLAG_STAYOPEN)) {
        do_cleanup = ARES_TRUE;
      }

      /* If the connection has been retired for new queries, close it out once
       * idle.  Resetting the connection (and specifically the source port
       * number) can help resolve situations where packets are being dropped.
       */
      if (conn->flags & ARES_CONN_FLAG_NONEW) {
        do_cleanup = ARES_TRUE;
      }

      /* If the udp connection hit its max queries, always close it */
      if (!(conn->flags & ARES_CONN_FLAG_TCP) && channel->udp_max_queries > 0 &&
          conn->total_queries >= channel->udp_max_queries) {
        do_cleanup = ARES_TRUE;
      }

      if (!do_cleanup) {
        continue;
      }

      /* Clean it up.  The close function is application code and may have
       * reconfigured the servers, freeing the list nodes this walk holds --
       * the server's, and the next connection's.  Start over; connections
       * already closed are no longer listed, so this ends. */
      ares_close_connection(conn, ARES_SUCCESS);
      goto restart;
    }
  }
}

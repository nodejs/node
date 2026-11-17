/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2004 Daniel Stenberg
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
#include "event/ares_event.h"
#include <assert.h>

void ares_destroy(ares_channel_t *channel)
{
  size_t             i;
  ares_llist_node_t *node = NULL;

  if (channel == NULL) {
    return;
  }

  /* Mark as being shutdown */
  ares_channel_lock(channel);
  channel->sys_up = ARES_FALSE;
  ares_channel_unlock(channel);

  /* Disable configuration change monitoring.  We can't hold a lock because
   * some cleanup routines, such as on Windows, are synchronous operations.
   * What we've observed is a system config change event was triggered right
   * at shutdown time and it tries to take the channel lock and the destruction
   * waits for that event to complete before it continues so we get a channel
   * lock deadlock at shutdown if we hold a lock during this process. */
  if (channel->optmask & ARES_OPT_EVENT_THREAD) {
    ares_event_thread_t *e = channel->sock_state_cb_data;
    if (e && e->configchg) {
      ares_event_configchg_destroy(e->configchg);
      e->configchg = NULL;
    }
  }

  /* Wait for reinit thread to exit if there was one pending, can't be
   * holding a lock as the thread may take locks. */
  if (channel->reinit_thread != NULL) {
    void *rv;
    ares_thread_join(channel->reinit_thread, &rv);
    channel->reinit_thread = NULL;
  }

  /* Lock because callbacks will be triggered, and any system-generated
   * callbacks need to hold a channel lock. */
  ares_channel_lock(channel);

  /* Destroy all queries.
   *
   * The queries are detached before their callbacks run.  Claiming the node
   * only unlinks a query from all_queries; it stays in queries_by_qid, on
   * conn->queries_to_conn and in queries_by_timeout.  A reentrant teardown
   * from inside the callback -- ares_set_servers_csv(), or a new query
   * hitting a connection error -- would otherwise still find the query, end
   * it a second time (invoking the application callback twice for one
   * lookup) and free it, and the ares_free_query() below would double-free.
   *
   * The loop re-reads the first node each iteration rather than caching the
   * next one, since a callback can cause other queries in this same list to
   * be ended and their nodes destroyed.  No list swap is needed: sys_up is
   * already cleared above, so ares_send_nolock() refuses new queries and
   * nothing can be added while we drain. */
  while ((node = ares_llist_node_first(channel->all_queries)) != NULL) {
    ares_query_t *query = ares_llist_node_claim(node);

    /* Must come first: node_claim() already destroyed this node, so
     * ares_detach_query() would otherwise destroy it a second time. */
    query->node_all_queries = NULL;
    ares_detach_query(query);

    query->callback(query->arg, ARES_EDESTRUCTION, 0, NULL);
    ares_free_query(query);
  }

  ares_queue_notify_empty(channel);

#ifndef NDEBUG
  /* Freeing the query should remove it from all the lists in which it sits,
   * so all query lists should be empty now.
   */
  assert(ares_llist_len(channel->all_queries) == 0);
  assert(ares_htable_szvp_num_keys(channel->queries_by_qid) == 0);
  assert(ares_slist_len(channel->queries_by_timeout) == 0);
#endif

  /* Tearing down the servers closes every remaining connection.  Applications
   * rely on being told about those closures so they can drop the descriptors
   * from their own event loop, so the notifications are still delivered here
   * rather than discarded -- just after the teardown instead of during it. */
  ares_deferred_cb_enter(channel);
  ares_destroy_servers_state(channel);
  ares_deferred_cb_leave(channel);

  /* The leave above delivered and destroyed both queues and reclaimed
   * everything the teardown unlinked, so these are normally no-ops.  They are
   * not redundant: if the first insert into a freshly created queue fails,
   * the array is left allocated and empty, the flush loop never runs because
   * both queues are empty, and nothing else would ever free it. */
  ares_array_destroy(channel->sock_state_queue);
  channel->sock_state_queue = NULL;
  ares_array_destroy(channel->server_state_queue);
  channel->server_state_queue = NULL;

#ifndef NDEBUG
  assert(channel->pending_servers == NULL);
  assert(channel->pending_conns == NULL);
#endif

#ifndef NDEBUG
  assert(ares_htable_asvp_num_keys(channel->connnode_by_socket) == 0);
#endif

  /* No more callbacks will be triggered after this point, unlock */
  ares_channel_unlock(channel);

  /* Shut down the event thread */
  if (channel->optmask & ARES_OPT_EVENT_THREAD) {
    ares_event_thread_destroy(channel);
  }

  if (channel->domains) {
    for (i = 0; i < channel->ndomains; i++) {
      ares_free(channel->domains[i]);
    }
    ares_free(channel->domains);
  }

  ares_llist_destroy(channel->all_queries);
  ares_slist_destroy(channel->queries_by_timeout);
  ares_htable_szvp_destroy(channel->queries_by_qid);
  ares_htable_asvp_destroy(channel->connnode_by_socket);

  ares_free(channel->sortlist);
  ares_free(channel->lookups);
  ares_free(channel->resolvconf_path);
  ares_free(channel->hosts_path);
  ares_destroy_rand_state(channel->rand_state);

  ares_hosts_file_destroy(channel->hf);

  ares_qcache_destroy(channel->qcache);

  ares_channel_threading_destroy(channel);

  ares_free(channel);
}

/*! Release a server's memory.  Split from ares_destroy_server() for the same
 *  reason ares_conn_reap() is split out: a caller further up the stack may
 *  still hold this pointer. */
void ares_server_reap(ares_server_t *server)
{
  if (server == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  ares_llist_destroy(server->connections);
  ares_free(server);
}

void ares_destroy_server(ares_server_t *server)
{
  ares_channel_t *channel;

  if (server == NULL) {
    return; /* LCOV_EXCL_LINE: DefensiveCoding */
  }

  channel = server->channel;

  /* Closes and unlinks every connection; each is deferred or reaped by
   * ares_close_connection() on the same rule applied below. */
  ares_close_sockets(server);

  /* The caller has already unlinked this server from channel->servers, so it
   * can no longer be found -- ares_server_in_channel() and
   * ares_slist_node_find() both report it gone, which is what callers check
   * before acting on one.  But a library call in progress may still be
   * holding the pointer, so keep it readable until that call unwinds. */
  if (channel != NULL && channel->deferred_cb_depth > 0) {
    server->next_pending     = channel->pending_servers;
    channel->pending_servers = server;
    return;
  }

  ares_server_reap(server);
}

void ares_destroy_servers_state(ares_channel_t *channel)
{
  ares_slist_node_t *node;

  while ((node = ares_slist_node_first(channel->servers)) != NULL) {
    ares_server_t *server = ares_slist_node_claim(node);
    ares_destroy_server(server);
  }

  ares_slist_destroy(channel->servers);
  channel->servers = NULL;
}

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

#include "ares_setup.h"

#include <assert.h>

#include "ares.h"
#include "ares_private.h"

void ares_destroy_options(struct ares_options *options)
{
  int i;

  if(options->servers)
    ares_free(options->servers);
  for (i = 0; i < options->ndomains; i++)
    ares_free(options->domains[i]);
  if(options->domains)
    ares_free(options->domains);
  if(options->sortlist)
    ares_free(options->sortlist);
  if(options->lookups)
    ares_free(options->lookups);
  if(options->resolvconf_path)
    ares_free(options->resolvconf_path);
  if(options->hosts_path)
    ares_free(options->hosts_path);
}

void ares_destroy(ares_channel channel)
{
  int                 i;
  ares__llist_node_t *node = NULL;

  if (!channel)
    return;

  /* Destroy all queries */
  node = ares__llist_node_first(channel->all_queries);
  while (node != NULL) {
    ares__llist_node_t *next  = ares__llist_node_next(node);
    struct query       *query = ares__llist_node_claim(node);

    query->node_all_queries = NULL;
    query->callback(query->arg, ARES_EDESTRUCTION, 0, NULL, 0);
    ares__free_query(query);

    node = next;
  }


#ifndef NDEBUG
  /* Freeing the query should remove it from all the lists in which it sits,
   * so all query lists should be empty now.
   */
  assert(ares__llist_len(channel->all_queries) == 0);
  assert(ares__htable_stvp_num_keys(channel->queries_by_qid) == 0);
  assert(ares__slist_len(channel->queries_by_timeout) == 0);
#endif

  ares__destroy_servers_state(channel);

#ifndef NDEBUG
  assert(ares__htable_asvp_num_keys(channel->connnode_by_socket) == 0);
#endif

  if (channel->domains) {
    for (i = 0; i < channel->ndomains; i++)
      ares_free(channel->domains[i]);
    ares_free(channel->domains);
  }

  ares__llist_destroy(channel->all_queries);
  ares__slist_destroy(channel->queries_by_timeout);
  ares__htable_stvp_destroy(channel->queries_by_qid);
  ares__htable_asvp_destroy(channel->connnode_by_socket);

  if(channel->sortlist)
    ares_free(channel->sortlist);

  if (channel->lookups)
    ares_free(channel->lookups);

  if (channel->resolvconf_path)
    ares_free(channel->resolvconf_path);

  if (channel->hosts_path)
    ares_free(channel->hosts_path);

  if (channel->rand_state)
    ares__destroy_rand_state(channel->rand_state);

  ares_free(channel);
}

void ares__destroy_servers_state(ares_channel channel)
{
  struct server_state *server;
  int i;

  if (channel->servers)
    {
      for (i = 0; i < channel->nservers; i++)
        {
          server = &channel->servers[i];
          ares__close_sockets(server);
          ares__llist_destroy(server->connections);
          ares__buf_destroy(server->tcp_parser);
          ares__buf_destroy(server->tcp_send);
        }
      ares_free(channel->servers);
      channel->servers = NULL;
    }
  channel->nservers = -1;
}

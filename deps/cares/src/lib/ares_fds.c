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

#include "ares.h"
#include "ares_private.h"

int ares_fds(ares_channel channel, fd_set *read_fds, fd_set *write_fds)
{
  struct server_state *server;
  ares_socket_t        nfds;
  size_t               i;

  /* Are there any active queries? */
  size_t               active_queries = ares__llist_len(channel->all_queries);

  nfds = 0;
  for (i = 0; i < channel->nservers; i++) {
    ares__llist_node_t *node;
    server = &channel->servers[i];

    for (node = ares__llist_node_first(server->connections); node != NULL;
         node = ares__llist_node_next(node)) {
      const struct server_connection *conn = ares__llist_node_val(node);

      /* We only need to register interest in UDP sockets if we have
       * outstanding queries.
       */
      if (active_queries || conn->is_tcp) {
        FD_SET(conn->fd, read_fds);
        if (conn->fd >= nfds) {
          nfds = conn->fd + 1;
        }
      }

      if (conn->is_tcp && ares__buf_len(server->tcp_send)) {
        FD_SET(conn->fd, write_fds);
      }
    }
  }

  return nfds;
}

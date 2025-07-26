/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2019 Andrew Selivanov
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

#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

void ares_freeaddrinfo_cnames(struct ares_addrinfo_cname *head)
{
  struct ares_addrinfo_cname *current;
  while (head) {
    current = head;
    head    = head->next;
    ares_free(current->alias);
    ares_free(current->name);
    ares_free(current);
  }
}

void ares_freeaddrinfo_nodes(struct ares_addrinfo_node *head)
{
  struct ares_addrinfo_node *current;
  while (head) {
    current = head;
    head    = head->ai_next;
    ares_free(current->ai_addr);
    ares_free(current);
  }
}

void ares_freeaddrinfo(struct ares_addrinfo *ai)
{
  if (ai == NULL) {
    return;
  }
  ares_freeaddrinfo_cnames(ai->cnames);
  ares_freeaddrinfo_nodes(ai->nodes);

  ares_free(ai->name);
  ares_free(ai);
}

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

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "ares.h"
#include "ares_private.h"

void ares__timeval_remaining(struct timeval       *remaining,
                             const struct timeval *now,
                             const struct timeval *tout)
{
  memset(remaining, 0, sizeof(*remaining));

  /* Expired! */
  if (tout->tv_sec < now->tv_sec ||
      (tout->tv_sec == now->tv_sec && tout->tv_usec < now->tv_usec)) {
    return;
  }

  remaining->tv_sec = tout->tv_sec - now->tv_sec;
  if (tout->tv_usec < now->tv_usec) {
    remaining->tv_sec  -= 1;
    remaining->tv_usec  = (tout->tv_usec + 1000000) - now->tv_usec;
  } else {
    remaining->tv_usec = tout->tv_usec - now->tv_usec;
  }
}

struct timeval *ares_timeout(ares_channel_t *channel, struct timeval *maxtv,
                             struct timeval *tvbuf)
{
  const struct query *query;
  ares__slist_node_t *node;
  struct timeval      now;

  /* The minimum timeout of all queries is always the first entry in
   * channel->queries_by_timeout */
  node = ares__slist_node_first(channel->queries_by_timeout);
  /* no queries/timeout */
  if (node == NULL) {
    return maxtv; /* <-- maxtv can be null though, hrm */
  }

  query = ares__slist_node_val(node);

  now = ares__tvnow();

  ares__timeval_remaining(tvbuf, &now, &query->timeout);

  if (maxtv == NULL) {
    return tvbuf;
  }

  /* Return the minimum time between maxtv and tvbuf */

  if (tvbuf->tv_sec > maxtv->tv_sec) {
    return maxtv;
  }
  if (tvbuf->tv_sec < maxtv->tv_sec) {
    return tvbuf;
  }

  if (tvbuf->tv_usec > maxtv->tv_usec) {
    return maxtv;
  }

  return tvbuf;
}

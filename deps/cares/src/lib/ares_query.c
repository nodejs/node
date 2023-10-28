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

struct qquery {
  ares_callback callback;
  void         *arg;
};

static void qcallback(void *arg, int status, int timeouts, unsigned char *abuf,
                      int alen);

/* a unique query id is generated using an rc4 key. Since the id may already
   be used by a running query (as infrequent as it may be), a lookup is
   performed per id generation. In practice this search should happen only
   once per newly generated id
*/
static unsigned short generate_unique_id(ares_channel channel)
{
  unsigned short id;

  do {
    id = ares__generate_new_id(channel->rand_state);
  } while (ares__htable_stvp_get(channel->queries_by_qid, id, NULL));

  return (unsigned short)id;
}

ares_status_t ares_query_qid(ares_channel channel, const char *name,
                             int dnsclass, int type, ares_callback callback,
                             void *arg, unsigned short *qid)
{
  struct qquery *qquery;
  unsigned char *qbuf;
  int            qlen;
  int            rd;
  ares_status_t  status;
  unsigned short id = generate_unique_id(channel);

  /* Compose the query. */
  rd     = !(channel->flags & ARES_FLAG_NORECURSE);
  status = (ares_status_t)ares_create_query(
    name, dnsclass, type, id, rd, &qbuf, &qlen,
    (channel->flags & ARES_FLAG_EDNS) ? (int)channel->ednspsz : 0);
  if (status != ARES_SUCCESS) {
    if (qbuf != NULL) {
      ares_free(qbuf);
    }
    callback(arg, (int)status, 0, NULL, 0);
    return status;
  }

  /* Allocate and fill in the query structure. */
  qquery = ares_malloc(sizeof(struct qquery));
  if (!qquery) {
    ares_free_string(qbuf);
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    return ARES_ENOMEM;
  }
  qquery->callback = callback;
  qquery->arg      = arg;

  /* Send it off.  qcallback will be called when we get an answer. */
  status = ares_send_ex(channel, qbuf, (size_t)qlen, qcallback, qquery);
  ares_free_string(qbuf);

  if (status == ARES_SUCCESS && qid) {
    *qid = id;
  }

  return status;
}

void ares_query(ares_channel channel, const char *name, int dnsclass, int type,
                ares_callback callback, void *arg)
{
  ares_query_qid(channel, name, dnsclass, type, callback, arg, NULL);
}

static void qcallback(void *arg, int status, int timeouts, unsigned char *abuf,
                      int alen)
{
  struct qquery *qquery = (struct qquery *)arg;
  size_t         ancount;
  int            rcode;

  if (status != ARES_SUCCESS) {
    qquery->callback(qquery->arg, status, timeouts, abuf, alen);
  } else {
    /* Pull the response code and answer count from the packet. */
    rcode   = DNS_HEADER_RCODE(abuf);
    ancount = DNS_HEADER_ANCOUNT(abuf);

    /* Convert errors. */
    switch (rcode) {
      case NOERROR:
        status = (ancount > 0) ? ARES_SUCCESS : ARES_ENODATA;
        break;
      case FORMERR:
        status = ARES_EFORMERR;
        break;
      case SERVFAIL:
        status = ARES_ESERVFAIL;
        break;
      case NXDOMAIN:
        status = ARES_ENOTFOUND;
        break;
      case NOTIMP:
        status = ARES_ENOTIMP;
        break;
      case REFUSED:
        status = ARES_EREFUSED;
        break;
    }
    qquery->callback(qquery->arg, status, timeouts, abuf, alen);
  }
  ares_free(qquery);
}

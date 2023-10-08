/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2009 Jakub Hrozek
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
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares_nameser.h"

#include "ares.h"
#include "ares_dns.h"
#include "ares_data.h"
#include "ares_private.h"

int
ares_parse_srv_reply (const unsigned char *abuf, int alen,
                      struct ares_srv_reply **srv_out)
{
  unsigned int qdcount, ancount, i;
  const unsigned char *aptr, *vptr;
  int status, rr_type, rr_class, rr_len;
  long len;
  char *hostname = NULL, *rr_name = NULL;
  struct ares_srv_reply *srv_head = NULL;
  struct ares_srv_reply *srv_last = NULL;
  struct ares_srv_reply *srv_curr;

  /* Set *srv_out to NULL for all failure cases. */
  *srv_out = NULL;

  /* Give up if abuf doesn't have room for a header. */
  if (alen < HFIXEDSZ)
    return ARES_EBADRESP;

  /* Fetch the question and answer count from the header. */
  qdcount = DNS_HEADER_QDCOUNT (abuf);
  ancount = DNS_HEADER_ANCOUNT (abuf);
  if (qdcount != 1)
    return ARES_EBADRESP;
  if (ancount == 0)
    return ARES_ENODATA;

  /* Expand the name from the question, and skip past the question. */
  aptr = abuf + HFIXEDSZ;
  status = ares_expand_name (aptr, abuf, alen, &hostname, &len);
  if (status != ARES_SUCCESS)
    return status;

  if (aptr + len + QFIXEDSZ > abuf + alen)
    {
      ares_free (hostname);
      return ARES_EBADRESP;
    }
  aptr += len + QFIXEDSZ;

  /* Examine each answer resource record (RR) in turn. */
  for (i = 0; i < ancount; i++)
    {
      /* Decode the RR up to the data field. */
      status = ares_expand_name (aptr, abuf, alen, &rr_name, &len);
      if (status != ARES_SUCCESS)
        {
          break;
        }
      aptr += len;
      if (aptr + RRFIXEDSZ > abuf + alen)
        {
          status = ARES_EBADRESP;
          break;
        }
      rr_type = DNS_RR_TYPE (aptr);
      rr_class = DNS_RR_CLASS (aptr);
      rr_len = DNS_RR_LEN (aptr);
      aptr += RRFIXEDSZ;
      if (aptr + rr_len > abuf + alen)
        {
          status = ARES_EBADRESP;
          break;
        }

      /* Check if we are really looking at a SRV record */
      if (rr_class == C_IN && rr_type == T_SRV)
        {
          /* parse the SRV record itself */
          if (rr_len < 6)
            {
              status = ARES_EBADRESP;
              break;
            }

          /* Allocate storage for this SRV answer appending it to the list */
          srv_curr = ares_malloc_data(ARES_DATATYPE_SRV_REPLY);
          if (!srv_curr)
            {
              status = ARES_ENOMEM;
              break;
            }
          if (srv_last)
            {
              srv_last->next = srv_curr;
            }
          else
            {
              srv_head = srv_curr;
            }
          srv_last = srv_curr;

          vptr = aptr;
          srv_curr->priority = DNS__16BIT(vptr);
          vptr += sizeof(unsigned short);
          srv_curr->weight = DNS__16BIT(vptr);
          vptr += sizeof(unsigned short);
          srv_curr->port = DNS__16BIT(vptr);
          vptr += sizeof(unsigned short);

          status = ares_expand_name (vptr, abuf, alen, &srv_curr->host, &len);
          if (status != ARES_SUCCESS)
            break;
        }

      /* Don't lose memory in the next iteration */
      ares_free (rr_name);
      rr_name = NULL;

      /* Move on to the next record */
      aptr += rr_len;
    }

  if (hostname)
    ares_free (hostname);
  if (rr_name)
    ares_free (rr_name);

  /* clean up on error */
  if (status != ARES_SUCCESS)
    {
      if (srv_head)
        ares_free_data (srv_head);
      return status;
    }

  /* everything looks fine, return the data */
  *srv_out = srv_head;

  return ARES_SUCCESS;
}

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

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include "ares.h"
#include "ares_dns.h"
#include "ares_data.h"
#include "ares_private.h"

static int
ares__parse_txt_reply (const unsigned char *abuf, int alen,
                       int ex, void **txt_out)
{
  size_t substr_len;
  unsigned int qdcount, ancount, i;
  const unsigned char *aptr;
  const unsigned char *strptr;
  int status, rr_type, rr_class, rr_len;
  long len;
  char *hostname = NULL, *rr_name = NULL;
  struct ares_txt_ext *txt_head = NULL;
  struct ares_txt_ext *txt_last = NULL;
  struct ares_txt_ext *txt_curr;

  /* Set *txt_out to NULL for all failure cases. */
  *txt_out = NULL;

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

      /* Check if we are really looking at a TXT record */
      if ((rr_class == C_IN || rr_class == C_CHAOS) && rr_type == T_TXT)
        {
          /*
           * There may be multiple substrings in a single TXT record. Each
           * substring may be up to 255 characters in length, with a
           * "length byte" indicating the size of the substring payload.
           * RDATA contains both the length-bytes and payloads of all
           * substrings contained therein.
           */

          strptr = aptr;
          while (strptr < (aptr + rr_len))
            {
              substr_len = (unsigned char)*strptr;
              if (strptr + substr_len + 1 > aptr + rr_len)
                {
                  status = ARES_EBADRESP;
                  break;
                }

              /* Allocate storage for this TXT answer appending it to the list */
              txt_curr = ares_malloc_data(ex ? ARES_DATATYPE_TXT_EXT :
                                               ARES_DATATYPE_TXT_REPLY);
              if (!txt_curr)
                {
                  status = ARES_ENOMEM;
                  break;
                }
              if (txt_last)
                {
                  txt_last->next = txt_curr;
                }
              else
                {
                  txt_head = txt_curr;
                }
              txt_last = txt_curr;

              if (ex)
                txt_curr->record_start = (strptr == aptr);
              txt_curr->length = substr_len;
              txt_curr->txt = ares_malloc (substr_len + 1/* Including null byte */);
              if (txt_curr->txt == NULL)
                {
                  status = ARES_ENOMEM;
                  break;
                }

              ++strptr;
              memcpy ((char *) txt_curr->txt, strptr, substr_len);

              /* Make sure we NULL-terminate */
              txt_curr->txt[substr_len] = 0;

              strptr += substr_len;
            }
        }

      /* Propagate any failures */
      if (status != ARES_SUCCESS)
        {
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
      if (txt_head)
        ares_free_data (txt_head);
      return status;
    }

  /* everything looks fine, return the data */
  *txt_out = txt_head;

  return ARES_SUCCESS;
}

int
ares_parse_txt_reply (const unsigned char *abuf, int alen,
                      struct ares_txt_reply **txt_out)
{
  return ares__parse_txt_reply(abuf, alen, 0, (void **) txt_out);
}


int
ares_parse_txt_reply_ext (const unsigned char *abuf, int alen,
                          struct ares_txt_ext **txt_out)
{
  return ares__parse_txt_reply(abuf, alen, 1, (void **) txt_out);
}

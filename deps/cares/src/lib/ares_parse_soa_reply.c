/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2012 Marko Kreen
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
ares_parse_soa_reply(const unsigned char *abuf, int alen,
		     struct ares_soa_reply **soa_out)
{
  const unsigned char *aptr;
  long len;
  char *qname = NULL, *rr_name = NULL;
  struct ares_soa_reply *soa = NULL;
  int qdcount, ancount, qclass;
  int status, i, rr_type, rr_class, rr_len;

  if (alen < HFIXEDSZ)
    return ARES_EBADRESP;

  /* parse message header */
  qdcount = DNS_HEADER_QDCOUNT(abuf);
  ancount = DNS_HEADER_ANCOUNT(abuf);

  if (qdcount != 1)
    return ARES_EBADRESP;
  if (ancount == 0)
    return ARES_EBADRESP;

  aptr = abuf + HFIXEDSZ;

  /* query name */
  status = ares__expand_name_for_response(aptr, abuf, alen, &qname, &len, 0);
  if (status != ARES_SUCCESS)
    goto failed_stat;

  if (alen <= len + HFIXEDSZ + 1)
    goto failed;
  aptr += len;

  qclass = DNS_QUESTION_TYPE(aptr);

  /* skip qtype & qclass */
  if (aptr + QFIXEDSZ > abuf + alen)
    goto failed;
  aptr += QFIXEDSZ;

  /* qclass of SOA with multiple answers */
  if (qclass == T_SOA && ancount > 1)
    goto failed;

  /* examine all the records, break and return if found soa */
  for (i = 0; i < ancount; i++)
  {
    rr_name = NULL;
    status  = ares__expand_name_for_response (aptr, abuf, alen, &rr_name, &len, 0);
    if (status != ARES_SUCCESS)
     {
      ares_free(rr_name);
      goto failed_stat;
     }

    aptr += len;
    if ( aptr + RRFIXEDSZ > abuf + alen )
    {
      ares_free(rr_name);
      status = ARES_EBADRESP;
      goto failed_stat;
    }
    rr_type = DNS_RR_TYPE( aptr );
    rr_class = DNS_RR_CLASS( aptr );
    rr_len = DNS_RR_LEN( aptr );
    aptr += RRFIXEDSZ;
    if (aptr + rr_len > abuf + alen)
      {
        ares_free(rr_name);
        status = ARES_EBADRESP;
        goto failed_stat;
      }
    if ( rr_class == C_IN && rr_type == T_SOA )
    {
      /* allocate result struct */
      soa = ares_malloc_data(ARES_DATATYPE_SOA_REPLY);
      if (!soa)
        {
          ares_free(rr_name);
          status = ARES_ENOMEM;
          goto failed_stat;
        }

      /* nsname */
      status = ares__expand_name_for_response(aptr, abuf, alen, &soa->nsname,
                                               &len, 0);
      if (status != ARES_SUCCESS)
       {
        ares_free(rr_name);
        goto failed_stat;
       }
      aptr += len;

      /* hostmaster */
      status = ares__expand_name_for_response(aptr, abuf, alen,
                                               &soa->hostmaster, &len, 0);
      if (status != ARES_SUCCESS)
       {
        ares_free(rr_name);
        goto failed_stat;
       }
      aptr += len;

      /* integer fields */
      if (aptr + 5 * 4 > abuf + alen)
       {
        ares_free(rr_name);
        goto failed;
       }
      soa->serial = DNS__32BIT(aptr + 0 * 4);
      soa->refresh = DNS__32BIT(aptr + 1 * 4);
      soa->retry = DNS__32BIT(aptr + 2 * 4);
      soa->expire = DNS__32BIT(aptr + 3 * 4);
      soa->minttl = DNS__32BIT(aptr + 4 * 4);

      ares_free(qname);
      ares_free(rr_name);

      *soa_out = soa;

      return ARES_SUCCESS;
    }
    aptr += rr_len;

    ares_free(rr_name);

    if (aptr > abuf + alen)
      goto failed_stat;
  }
  /* no SOA record found */
  status = ARES_EBADRESP;
  goto failed_stat;
failed:
  status = ARES_EBADRESP;

failed_stat:
  if (soa)
    ares_free_data(soa);
  if (qname)
    ares_free(qname);
  return status;
}

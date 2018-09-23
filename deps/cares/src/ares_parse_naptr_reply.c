
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2009 by Jakub Hrozek <jhrozek@redhat.com>
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
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
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

#include "ares.h"
#include "ares_dns.h"
#include "ares_data.h"
#include "ares_private.h"

/* AIX portability check */
#ifndef T_NAPTR
	#define T_NAPTR 35 /* naming authority pointer */
#endif

int
ares_parse_naptr_reply (const unsigned char *abuf, int alen,
                        struct ares_naptr_reply **naptr_out)
{
  unsigned int qdcount, ancount, i;
  const unsigned char *aptr, *vptr;
  int status, rr_type, rr_class, rr_len;
  long len;
  char *hostname = NULL, *rr_name = NULL;
  struct ares_naptr_reply *naptr_head = NULL;
  struct ares_naptr_reply *naptr_last = NULL;
  struct ares_naptr_reply *naptr_curr;

  /* Set *naptr_out to NULL for all failure cases. */
  *naptr_out = NULL;

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
      free (hostname);
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

      /* Check if we are really looking at a NAPTR record */
      if (rr_class == C_IN && rr_type == T_NAPTR)
        {
          /* parse the NAPTR record itself */

          /* Allocate storage for this NAPTR answer appending it to the list */
          naptr_curr = ares_malloc_data(ARES_DATATYPE_NAPTR_REPLY);
          if (!naptr_curr)
            {
              status = ARES_ENOMEM;
              break;
            }
          if (naptr_last)
            {
              naptr_last->next = naptr_curr;
            }
          else
            {
              naptr_head = naptr_curr;
            }
          naptr_last = naptr_curr;

          vptr = aptr;
          naptr_curr->order = DNS__16BIT(vptr);
          vptr += sizeof(unsigned short);
          naptr_curr->preference = DNS__16BIT(vptr);
          vptr += sizeof(unsigned short);

          status = ares_expand_string(vptr, abuf, alen, &naptr_curr->flags, &len);
          if (status != ARES_SUCCESS)
            break;
          vptr += len;

          status = ares_expand_string(vptr, abuf, alen, &naptr_curr->service, &len);
          if (status != ARES_SUCCESS)
            break;
          vptr += len;

          status = ares_expand_string(vptr, abuf, alen, &naptr_curr->regexp, &len);
          if (status != ARES_SUCCESS)
            break;
          vptr += len;

          status = ares_expand_name(vptr, abuf, alen, &naptr_curr->replacement, &len);
          if (status != ARES_SUCCESS)
            break;
        }

      /* Don't lose memory in the next iteration */
      free (rr_name);
      rr_name = NULL;

      /* Move on to the next record */
      aptr += rr_len;
    }

  if (hostname)
    free (hostname);
  if (rr_name)
    free (rr_name);

  /* clean up on error */
  if (status != ARES_SUCCESS)
    {
      if (naptr_head)
        ares_free_data (naptr_head);
      return status;
    }

  /* everything looks fine, return the data */
  *naptr_out = naptr_head;

  return ARES_SUCCESS;
}


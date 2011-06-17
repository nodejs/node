
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

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
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

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "ares.h"
#include "ares_dns.h"
#include "ares_data.h"
#include "ares_private.h"

int
ares_parse_txt_reply (const unsigned char *abuf, int alen,
                      struct ares_txt_reply **txt_out)
{
  size_t substr_len, str_len;
  unsigned int qdcount, ancount, i;
  const unsigned char *aptr;
  const unsigned char *strptr;
  int status, rr_type, rr_class, rr_len;
  long len;
  char *hostname = NULL, *rr_name = NULL;
  struct ares_txt_reply *txt_head = NULL;
  struct ares_txt_reply *txt_last = NULL;
  struct ares_txt_reply *txt_curr;

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

      /* Check if we are really looking at a TXT record */
      if (rr_class == C_IN && rr_type == T_TXT)
        {
          /* Allocate storage for this TXT answer appending it to the list */
          txt_curr = ares_malloc_data(ARES_DATATYPE_TXT_REPLY);
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

          /*
           * There may be multiple substrings in a single TXT record. Each
           * substring may be up to 255 characters in length, with a
           * "length byte" indicating the size of the substring payload.
           * RDATA contains both the length-bytes and payloads of all
           * substrings contained therein.
           */

          /* Compute total length to allow a single memory allocation */
          strptr = aptr;
          while (strptr < (aptr + rr_len))
            {
              substr_len = (unsigned char)*strptr;
              txt_curr->length += substr_len;
              strptr += substr_len + 1;
            }

          /* Including null byte */
          txt_curr->txt = malloc (txt_curr->length + 1);
          if (txt_curr->txt == NULL)
            {
              status = ARES_ENOMEM;
              break;
            }

          /* Step through the list of substrings, concatenating them */
          str_len = 0;
          strptr = aptr;
          while (strptr < (aptr + rr_len))
            {
              substr_len = (unsigned char)*strptr;
              strptr++;
              memcpy ((char *) txt_curr->txt + str_len, strptr, substr_len);
              str_len += substr_len;
              strptr += substr_len;
            }
          /* Make sure we NULL-terminate */
          *((char *) txt_curr->txt + txt_curr->length) = '\0';
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
      if (txt_head)
        ares_free_data (txt_head);
      return status;
    }

  /* everything looks fine, return the data */
  *txt_out = txt_head;

  return ARES_SUCCESS;
}

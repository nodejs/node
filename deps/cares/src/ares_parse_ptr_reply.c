
/* Copyright 1998 by the Massachusetts Institute of Technology.
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

#include "ares.h"
#include "ares_dns.h"
#include "ares_nowarn.h"
#include "ares_private.h"

int ares_parse_ptr_reply(const unsigned char *abuf, int alen, const void *addr,
                         int addrlen, int family, struct hostent **host)
{
  unsigned int qdcount, ancount;
  int status, i, rr_type, rr_class, rr_len;
  long len;
  const unsigned char *aptr;
  char *ptrname, *hostname, *rr_name, *rr_data;
  struct hostent *hostent;
  int aliascnt = 0;
  int alias_alloc = 8;
  char ** aliases;
  size_t rr_data_len;

  /* Set *host to NULL for all failure cases. */
  *host = NULL;

  /* Give up if abuf doesn't have room for a header. */
  if (alen < HFIXEDSZ)
    return ARES_EBADRESP;

  /* Fetch the question and answer count from the header. */
  qdcount = DNS_HEADER_QDCOUNT(abuf);
  ancount = DNS_HEADER_ANCOUNT(abuf);
  if (qdcount != 1)
    return ARES_EBADRESP;

  /* Expand the name from the question, and skip past the question. */
  aptr = abuf + HFIXEDSZ;
  status = ares__expand_name_for_response(aptr, abuf, alen, &ptrname, &len);
  if (status != ARES_SUCCESS)
    return status;
  if (aptr + len + QFIXEDSZ > abuf + alen)
    {
      ares_free(ptrname);
      return ARES_EBADRESP;
    }
  aptr += len + QFIXEDSZ;

  /* Examine each answer resource record (RR) in turn. */
  hostname = NULL;
  aliases = ares_malloc(alias_alloc * sizeof(char *));
  if (!aliases)
    {
      ares_free(ptrname);
      return ARES_ENOMEM;
    }
  for (i = 0; i < (int)ancount; i++)
    {
      /* Decode the RR up to the data field. */
      status = ares__expand_name_for_response(aptr, abuf, alen, &rr_name, &len);
      if (status != ARES_SUCCESS)
        break;
      aptr += len;
      if (aptr + RRFIXEDSZ > abuf + alen)
        {
          ares_free(rr_name);
          status = ARES_EBADRESP;
          break;
        }
      rr_type = DNS_RR_TYPE(aptr);
      rr_class = DNS_RR_CLASS(aptr);
      rr_len = DNS_RR_LEN(aptr);
      aptr += RRFIXEDSZ;
      if (aptr + rr_len > abuf + alen)
        {
          ares_free(rr_name);
          status = ARES_EBADRESP;
          break;
        }

      if (rr_class == C_IN && rr_type == T_PTR
          && strcasecmp(rr_name, ptrname) == 0)
        {
          /* Decode the RR data and set hostname to it. */
          status = ares__expand_name_for_response(aptr, abuf, alen, &rr_data,
                                                  &len);
          if (status != ARES_SUCCESS)
            {
              ares_free(rr_name);
              break;
            }
          if (hostname)
            ares_free(hostname);
          hostname = rr_data;
          rr_data_len = strlen(rr_data)+1;
          aliases[aliascnt] = ares_malloc(rr_data_len * sizeof(char));
          if (!aliases[aliascnt])
            {
              ares_free(rr_name);
              status = ARES_ENOMEM;
              break;
            }
          strncpy(aliases[aliascnt], rr_data, rr_data_len);
          aliascnt++;
          if (aliascnt >= alias_alloc) {
            char **ptr;
            alias_alloc *= 2;
            ptr = ares_realloc(aliases, alias_alloc * sizeof(char *));
            if(!ptr) {
              ares_free(rr_name);
              status = ARES_ENOMEM;
              break;
            }
            aliases = ptr;
          }
        }

      if (rr_class == C_IN && rr_type == T_CNAME)
        {
          /* Decode the RR data and replace ptrname with it. */
          status = ares__expand_name_for_response(aptr, abuf, alen, &rr_data,
                                                  &len);
          if (status != ARES_SUCCESS)
            {
              ares_free(rr_name);
              break;
            }
          ares_free(ptrname);
          ptrname = rr_data;
        }

      ares_free(rr_name);
      aptr += rr_len;
      if (aptr > abuf + alen)
        {  /* LCOV_EXCL_START: already checked above */
          status = ARES_EBADRESP;
          break;
        }  /* LCOV_EXCL_STOP */
    }

  if (status == ARES_SUCCESS && !hostname)
    status = ARES_ENODATA;
  if (status == ARES_SUCCESS)
    {
      /* We got our answer.  Allocate memory to build the host entry. */
      hostent = ares_malloc(sizeof(struct hostent));
      if (hostent)
        {
          hostent->h_addr_list = ares_malloc(2 * sizeof(char *));
          if (hostent->h_addr_list)
            {
              hostent->h_addr_list[0] = ares_malloc(addrlen);
              if (hostent->h_addr_list[0])
                {
                  hostent->h_aliases = ares_malloc((aliascnt+1) * sizeof (char *));
                  if (hostent->h_aliases)
                    {
                      /* Fill in the hostent and return successfully. */
                      hostent->h_name = hostname;
                      for (i=0 ; i<aliascnt ; i++)
                        hostent->h_aliases[i] = aliases[i];
                      hostent->h_aliases[aliascnt] = NULL;
                      hostent->h_addrtype = aresx_sitoss(family);
                      hostent->h_length = aresx_sitoss(addrlen);
                      memcpy(hostent->h_addr_list[0], addr, addrlen);
                      hostent->h_addr_list[1] = NULL;
                      *host = hostent;
                      ares_free(aliases);
                      ares_free(ptrname);
                      return ARES_SUCCESS;
                    }
                  ares_free(hostent->h_addr_list[0]);
                }
              ares_free(hostent->h_addr_list);
            }
          ares_free(hostent);
        }
      status = ARES_ENOMEM;
    }
  for (i=0 ; i<aliascnt ; i++)
    if (aliases[i])
      ares_free(aliases[i]);
  ares_free(aliases);
  if (hostname)
    ares_free(hostname);
  ares_free(ptrname);
  return status;
}

/* Copyright (C) 2019 by Andrew Selivanov
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

#include "ares_nameser.h"

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#ifdef HAVE_LIMITS_H
#  include <limits.h>
#endif

#include "ares.h"
#include "ares_dns.h"
#include "ares_private.h"

int ares__parse_into_addrinfo2(const unsigned char *abuf,
                               int alen,
                               char **question_hostname,
                               struct ares_addrinfo *ai)
{
  unsigned int qdcount, ancount;
  int status, i, rr_type, rr_class, rr_len, rr_ttl;
  int got_a = 0, got_aaaa = 0, got_cname = 0;
  long len;
  const unsigned char *aptr;
  char *hostname, *rr_name = NULL, *rr_data;
  struct ares_addrinfo_cname *cname, *cnames = NULL;
  struct ares_addrinfo_node *node, *nodes = NULL;
  struct sockaddr_in *sin;
  struct sockaddr_in6 *sin6;

  *question_hostname = NULL;

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
  status = ares__expand_name_for_response(aptr, abuf, alen, question_hostname, &len, 0);
  if (status != ARES_SUCCESS)
    return status;
  if (aptr + len + QFIXEDSZ > abuf + alen)
    {
      return ARES_EBADRESP;
    }

  hostname = *question_hostname;

  aptr += len + QFIXEDSZ;

  /* Examine each answer resource record (RR) in turn. */
  for (i = 0; i < (int)ancount; i++)
    {
      /* Decode the RR up to the data field. */
      status = ares__expand_name_for_response(aptr, abuf, alen, &rr_name, &len, 0);
      if (status != ARES_SUCCESS)
        {
          rr_name = NULL;
          goto failed_stat;
        }

      aptr += len;
      if (aptr + RRFIXEDSZ > abuf + alen)
        {
          status = ARES_EBADRESP;
          goto failed_stat;
        }
      rr_type = DNS_RR_TYPE(aptr);
      rr_class = DNS_RR_CLASS(aptr);
      rr_len = DNS_RR_LEN(aptr);
      rr_ttl = DNS_RR_TTL(aptr);
      aptr += RRFIXEDSZ;
      if (aptr + rr_len > abuf + alen)
        {
          status = ARES_EBADRESP;
          goto failed_stat;
        }

      if (rr_class == C_IN && rr_type == T_A
          && rr_len == sizeof(struct in_addr)
          && strcasecmp(rr_name, hostname) == 0)
        {
          got_a = 1;
          if (aptr + sizeof(struct in_addr) > abuf + alen)
          {  /* LCOV_EXCL_START: already checked above */
            status = ARES_EBADRESP;
            goto failed_stat;
          }  /* LCOV_EXCL_STOP */

          node = ares__append_addrinfo_node(&nodes);
          if (!node)
            {
              status = ARES_ENOMEM;
              goto failed_stat;
            }

          sin = ares_malloc(sizeof(struct sockaddr_in));
          if (!sin)
            {
              status = ARES_ENOMEM;
              goto failed_stat;
            }
          memset(sin, 0, sizeof(struct sockaddr_in));
          memcpy(&sin->sin_addr.s_addr, aptr, sizeof(struct in_addr));
          sin->sin_family = AF_INET;

          node->ai_addr = (struct sockaddr *)sin;
          node->ai_family = AF_INET;
          node->ai_addrlen = sizeof(struct sockaddr_in);

          node->ai_ttl = rr_ttl;

          status = ARES_SUCCESS;
        }
      else if (rr_class == C_IN && rr_type == T_AAAA
          && rr_len == sizeof(struct ares_in6_addr)
          && strcasecmp(rr_name, hostname) == 0)
        {
          got_aaaa = 1;
          if (aptr + sizeof(struct ares_in6_addr) > abuf + alen)
          {  /* LCOV_EXCL_START: already checked above */
            status = ARES_EBADRESP;
            goto failed_stat;
          }  /* LCOV_EXCL_STOP */

          node = ares__append_addrinfo_node(&nodes);
          if (!node)
            {
              status = ARES_ENOMEM;
              goto failed_stat;
            }

          sin6 = ares_malloc(sizeof(struct sockaddr_in6));
          if (!sin6)
            {
              status = ARES_ENOMEM;
              goto failed_stat;
            }

          memset(sin6, 0, sizeof(struct sockaddr_in6));
          memcpy(&sin6->sin6_addr.s6_addr, aptr, sizeof(struct ares_in6_addr));
          sin6->sin6_family = AF_INET6;

          node->ai_addr = (struct sockaddr *)sin6;
          node->ai_family = AF_INET6;
          node->ai_addrlen = sizeof(struct sockaddr_in6);

          node->ai_ttl = rr_ttl;

          status = ARES_SUCCESS;
        }

      if (rr_class == C_IN && rr_type == T_CNAME)
        {
          got_cname = 1;
          status = ares__expand_name_for_response(aptr, abuf, alen, &rr_data,
                                                  &len, 1);
          if (status != ARES_SUCCESS)
            {
              goto failed_stat;
            }

          /* Decode the RR data and replace the hostname with it. */
          /* SA: Seems wrong as it introduses order dependency. */
          hostname = rr_data;

          cname = ares__append_addrinfo_cname(&cnames);
          if (!cname)
            {
              status = ARES_ENOMEM;
              ares_free(rr_data);
              goto failed_stat;
            }
          cname->ttl = rr_ttl;
          cname->alias = rr_name;
          cname->name = rr_data;
        }
      else
        {
          ares_free(rr_name);
        }


      aptr += rr_len;
      if (aptr > abuf + alen)
        {  /* LCOV_EXCL_START: already checked above */
          status = ARES_EBADRESP;
          goto failed_stat;
        }  /* LCOV_EXCL_STOP */
    }

  if (status == ARES_SUCCESS)
    {
      ares__addrinfo_cat_nodes(&ai->nodes, nodes);
      if (got_cname)
        {
          ares__addrinfo_cat_cnames(&ai->cnames, cnames);
          return status;
        }
      else if (got_a == 0 && got_aaaa == 0)
        {
          /* the check for naliases to be zero is to make sure CNAME responses
             don't get caught here */
          status = ARES_ENODATA;
        }
    }

  return status;

failed_stat:
  ares_free(rr_name);
  ares__freeaddrinfo_cnames(cnames);
  ares__freeaddrinfo_nodes(nodes);
  return status;
}

int ares__parse_into_addrinfo(const unsigned char *abuf,
                              int alen,
                              struct ares_addrinfo *ai)
{
  int status;
  char *question_hostname;
  status = ares__parse_into_addrinfo2(abuf, alen, &question_hostname, ai);
  ares_free(question_hostname);
  return status;
}

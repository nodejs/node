
/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2019 by Andrew Selivanov
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

int ares_parse_a_reply(const unsigned char *abuf, int alen,
                       struct hostent **host, struct ares_addrttl *addrttls,
                       int *naddrttls)
{
  struct ares_addrinfo ai;
  char *question_hostname = NULL;
  int status;
  int req_naddrttls = 0;

  if (naddrttls)
    {
      req_naddrttls = *naddrttls;
      *naddrttls = 0;
    }

  memset(&ai, 0, sizeof(ai));

  status = ares__parse_into_addrinfo(abuf, alen, 0, 0, &ai);
  if (status != ARES_SUCCESS && status != ARES_ENODATA)
    {
      goto fail;
    }

  if (host != NULL)
    {
      status = ares__addrinfo2hostent(&ai, AF_INET, host);
      if (status != ARES_SUCCESS && status != ARES_ENODATA)
        {
          goto fail;
        }
    }

  if (addrttls != NULL && req_naddrttls)
   {
     ares__addrinfo2addrttl(&ai, AF_INET, req_naddrttls, addrttls,
                            NULL, naddrttls);
   }


fail:
  ares__freeaddrinfo_cnames(ai.cnames);
  ares__freeaddrinfo_nodes(ai.nodes);
  ares_free(ai.name);
  ares_free(question_hostname);

  return status;
}

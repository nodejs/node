/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2019 Andrew Selivanov
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

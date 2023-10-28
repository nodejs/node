/* MIT License
 *
 * Copyright (c) 1998, 2011 Massachusetts Institute of Technology
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
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

ares_status_t ares__get_hostent(FILE *fp, int family, struct hostent **host)
{
  char            *line = NULL;
  char            *p;
  char            *q;
  char           **alias;
  const char      *txtaddr;
  const char      *txthost;
  char            *txtalias;
  ares_status_t    status;
  size_t           addrlen;
  size_t           linesize;
  size_t           naliases;
  struct ares_addr addr;
  struct hostent  *hostent = NULL;

  *host = NULL; /* Assume failure */

  /* Validate family */
  switch (family) {
    case AF_INET:
    case AF_INET6:
    case AF_UNSPEC:
      break;
    default:
      return ARES_EBADFAMILY;
  }

  while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
    /* Trim line comment. */
    p = line;
    while (*p && (*p != '#')) {
      p++;
    }
    *p = '\0';

    /* Trim trailing whitespace. */
    q = p - 1;
    while ((q >= line) && ISSPACE(*q)) {
      q--;
    }
    *++q = '\0';

    /* Skip leading whitespace. */
    p = line;
    while (*p && ISSPACE(*p)) {
      p++;
    }
    if (!*p) {
      /* Ignore line if empty. */
      continue;
    }

    /* Pointer to start of IPv4 or IPv6 address part. */
    txtaddr = p;

    /* Advance past address part. */
    while (*p && !ISSPACE(*p)) {
      p++;
    }
    if (!*p) {
      /* Ignore line if reached end of line. */
      continue;
    }

    /* Null terminate address part. */
    *p = '\0';

    /* Advance to host name */
    p++;
    while (*p && ISSPACE(*p)) {
      p++;
    }
    if (!*p) {
      /* Ignore line if reached end of line. */
      continue; /* LCOV_EXCL_LINE: trailing whitespace already stripped */
    }

    /* Pointer to start of host name. */
    txthost = p;

    /* Advance past host name. */
    while (*p && !ISSPACE(*p)) {
      p++;
    }

    /* Pointer to start of first alias. */
    txtalias = NULL;
    if (*p) {
      q = p + 1;
      while (*q && ISSPACE(*q)) {
        q++;
      }
      if (*q) {
        txtalias = q;
      }
    }

    /* Null terminate host name. */
    *p = '\0';

    /* find out number of aliases. */
    naliases = 0;
    if (txtalias) {
      p = txtalias;
      while (*p) {
        while (*p && !ISSPACE(*p)) {
          p++;
        }
        while (*p && ISSPACE(*p)) {
          p++;
        }
        naliases++;
      }
    }

    /* Convert address string to network address for the requested family. */
    addrlen            = 0;
    addr.family        = AF_UNSPEC;
    addr.addrV4.s_addr = INADDR_NONE;
    if ((family == AF_INET || family == AF_UNSPEC) &&
        ares_inet_pton(AF_INET, txtaddr, &addr.addrV4) > 0) {
      /* Actual network address family and length. */
      addr.family = AF_INET;
      addrlen     = sizeof(addr.addrV4);
    }
    if ((family == AF_INET6 || (family == AF_UNSPEC && !addrlen)) &&
        ares_inet_pton(AF_INET6, txtaddr, &addr.addrV6) > 0) {
      /* Actual network address family and length. */
      addr.family = AF_INET6;
      addrlen     = sizeof(addr.addrV6);
    }
    if (!addrlen) {
      /* Ignore line if invalid address string for the requested family. */
      continue;
    }

    /*
    ** Actual address family possible values are AF_INET and AF_INET6 only.
    */

    /* Allocate memory for the hostent structure. */
    hostent = ares_malloc(sizeof(struct hostent));
    if (!hostent) {
      break;
    }

    /* Initialize fields for out of memory condition. */
    hostent->h_aliases   = NULL;
    hostent->h_addr_list = NULL;

    /* Copy official host name. */
    hostent->h_name = ares_strdup(txthost);
    if (!hostent->h_name) {
      break;
    }

    /* Copy network address. */
    hostent->h_addr_list = ares_malloc(2 * sizeof(char *));
    if (!hostent->h_addr_list) {
      break;
    }
    hostent->h_addr_list[1] = NULL;
    hostent->h_addr_list[0] = ares_malloc(addrlen);
    if (!hostent->h_addr_list[0]) {
      break;
    }
    if (addr.family == AF_INET) {
      memcpy(hostent->h_addr_list[0], &addr.addrV4, sizeof(addr.addrV4));
    } else {
      memcpy(hostent->h_addr_list[0], &addr.addrV6, sizeof(addr.addrV6));
    }

    /* Copy aliases. */
    hostent->h_aliases = ares_malloc((naliases + 1) * sizeof(char *));
    if (!hostent->h_aliases) {
      break;
    }
    alias = hostent->h_aliases;
    while (naliases) {
      *(alias + naliases--) = NULL;
    }
    *alias = NULL;
    while (txtalias) {
      p = txtalias;
      while (*p && !ISSPACE(*p)) {
        p++;
      }
      q = p;
      while (*q && ISSPACE(*q)) {
        q++;
      }
      *p = '\0';
      if ((*alias = ares_strdup(txtalias)) == NULL) {
        break;
      }
      alias++;
      txtalias = *q ? q : NULL;
    }
    if (txtalias) {
      /* Alias memory allocation failure. */
      break;
    }

    /* Copy actual network address family and length. */
    hostent->h_addrtype = addr.family;
    hostent->h_length   = (int)addrlen;

    /* Free line buffer. */
    ares_free(line);

    /* Return hostent successfully */
    *host = hostent;
    return ARES_SUCCESS;
  }

  /* If allocated, free line buffer. */
  if (line) {
    ares_free(line);
  }

  if (status == ARES_SUCCESS) {
    /* Memory allocation failure; clean up. */
    if (hostent) {
      if (hostent->h_name) {
        ares_free(hostent->h_name);
      }
      if (hostent->h_aliases) {
        for (alias = hostent->h_aliases; *alias; alias++) {
          ares_free(*alias);
        }
        ares_free(hostent->h_aliases);
      }
      if (hostent->h_addr_list) {
        if (hostent->h_addr_list[0]) {
          ares_free(hostent->h_addr_list[0]);
        }
        ares_free(hostent->h_addr_list);
      }
      ares_free(hostent);
    }
    return ARES_ENOMEM;
  }

  return status;
}

/* MIT License
 *
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

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_private.h"

#define MAX_ALIASES 40

ares_status_t ares__readaddrinfo(FILE *fp, const char *name,
                                 unsigned short                    port,
                                 const struct ares_addrinfo_hints *hints,
                                 struct ares_addrinfo             *ai)
{
  char                       *line = NULL;
  char                       *p;
  char                       *q;
  const char                 *txtaddr;
  const char                 *txthost;
  char                       *txtalias;
  char                       *aliases[MAX_ALIASES];
  size_t                      i;
  size_t                      alias_count;
  ares_status_t               status = ARES_SUCCESS;
  size_t                      linesize;
  struct ares_addrinfo_cname *cname  = NULL;
  struct ares_addrinfo_cname *cnames = NULL;
  struct ares_addrinfo_node  *nodes  = NULL;
  ares_bool_t                 match_with_alias;
  ares_bool_t                 match_with_canonical;
  ares_bool_t                 want_cname =
    (hints->ai_flags & ARES_AI_CANONNAME) ? ARES_TRUE : ARES_FALSE;

  /* Validate family */
  switch (hints->ai_family) {
    case AF_INET:
    case AF_INET6:
    case AF_UNSPEC:
      break;
    default:
      return ARES_EBADFAMILY;
  }

  ai->name = ares_strdup(name);
  if (!ai->name) {
    status = ARES_ENOMEM;
    goto fail;
  }

  while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
    match_with_alias     = ARES_FALSE;
    match_with_canonical = ARES_FALSE;
    alias_count          = 0;
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

    /* Find out if host name matches with canonical host name. */
    if (strcasecmp(txthost, name) == 0) {
      match_with_canonical = ARES_TRUE;
    }

    /* Find out if host name matches with one of the aliases. */
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
      if (strcasecmp(txtalias, name) == 0) {
        match_with_alias = ARES_TRUE;
        if (!want_cname) {
          break;
        }
      }
      if (alias_count < MAX_ALIASES) {
        aliases[alias_count++] = txtalias;
      }
      txtalias = *q ? q : NULL;
    }

    /* Try next line if host does not match. */
    if (!match_with_alias && !match_with_canonical) {
      continue;
    }

    /*
     * Convert address string to network address for the requested families.
     * Actual address family possible values are AF_INET and AF_INET6 only.
     */
    if ((hints->ai_family == AF_INET) || (hints->ai_family == AF_UNSPEC)) {
      struct in_addr addr4;
      if (ares_inet_pton(AF_INET, txtaddr, &addr4) == 1) {
        status = ares_append_ai_node(AF_INET, port, 0, &addr4, &nodes);
        if (status != ARES_SUCCESS) {
          goto fail;
        }
      }
    }
    if ((hints->ai_family == AF_INET6) || (hints->ai_family == AF_UNSPEC)) {
      struct ares_in6_addr addr6;
      if (ares_inet_pton(AF_INET6, txtaddr, &addr6) == 1) {
        status = ares_append_ai_node(AF_INET6, port, 0, &addr6, &nodes);
        if (status != ARES_SUCCESS) {
          goto fail;
        }
      }
    }

    if (want_cname) {
      for (i = 0; i < alias_count; ++i) {
        cname = ares__append_addrinfo_cname(&cnames);
        if (!cname) {
          status = ARES_ENOMEM;
          goto fail;
        }
        cname->alias = ares_strdup(aliases[i]);
        cname->name  = ares_strdup(txthost);
      }
      /* No aliases, cname only. */
      if (!alias_count) {
        cname = ares__append_addrinfo_cname(&cnames);
        if (!cname) {
          status = ARES_ENOMEM;
          goto fail;
        }
        cname->name = ares_strdup(txthost);
      }
    }
  }

  /* Last read failed. */
  if (status == ARES_ENOMEM) {
    goto fail;
  }

  /* If no results, its a failure */
  if (!nodes) {
    status = ARES_ENOTFOUND;
    goto fail;
  }

  /* Free line buffer. */
  ares_free(line);
  ares__addrinfo_cat_cnames(&ai->cnames, cnames);
  ares__addrinfo_cat_nodes(&ai->nodes, nodes);

  return ARES_SUCCESS;

fail:
  ares_free(line);
  ares__freeaddrinfo_cnames(cnames);
  ares__freeaddrinfo_nodes(nodes);
  ares_free(ai->name);
  ai->name = NULL;
  return status;
}

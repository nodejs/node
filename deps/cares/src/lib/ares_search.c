/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
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

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include "ares.h"
#include "ares_private.h"

struct search_query {
  /* Arguments passed to ares_search */
  ares_channel  channel;
  char         *name; /* copied into an allocated buffer */
  int           dnsclass;
  int           type;
  ares_callback callback;
  void         *arg;

  int           status_as_is;  /* error status from trying as-is */
  size_t        next_domain;   /* next search domain to try */
  ares_bool_t   trying_as_is;  /* current query is for name as-is */
  size_t        timeouts;      /* number of timeouts we saw for this request */
  ares_bool_t ever_got_nodata; /* did we ever get ARES_ENODATA along the way? */
};

static void search_callback(void *arg, int status, int timeouts,
                            unsigned char *abuf, int alen);
static void end_squery(struct search_query *squery, ares_status_t status,
                       unsigned char *abuf, size_t alen);

void ares_search(ares_channel channel, const char *name, int dnsclass, int type,
                 ares_callback callback, void *arg)
{
  struct search_query *squery;
  char                *s;
  const char          *p;
  ares_status_t        status;
  size_t               ndots;

  /* Per RFC 7686, reject queries for ".onion" domain names with NXDOMAIN. */
  if (ares__is_onion_domain(name)) {
    callback(arg, ARES_ENOTFOUND, 0, NULL, 0);
    return;
  }

  /* If name only yields one domain to search, then we don't have
   * to keep extra state, so just do an ares_query().
   */
  status = ares__single_domain(channel, name, &s);
  if (status != ARES_SUCCESS) {
    callback(arg, (int)status, 0, NULL, 0);
    return;
  }
  if (s) {
    ares_query(channel, s, dnsclass, type, callback, arg);
    ares_free(s);
    return;
  }

  /* Allocate a search_query structure to hold the state necessary for
   * doing multiple lookups.
   */
  squery = ares_malloc(sizeof(struct search_query));
  if (!squery) {
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    return;
  }
  squery->channel = channel;
  squery->name    = ares_strdup(name);
  if (!squery->name) {
    ares_free(squery);
    callback(arg, ARES_ENOMEM, 0, NULL, 0);
    return;
  }
  squery->dnsclass        = dnsclass;
  squery->type            = type;
  squery->status_as_is    = -1;
  squery->callback        = callback;
  squery->arg             = arg;
  squery->timeouts        = 0;
  squery->ever_got_nodata = ARES_FALSE;

  /* Count the number of dots in name. */
  ndots = 0;
  for (p = name; *p; p++) {
    if (*p == '.') {
      ndots++;
    }
  }

  /* If ndots is at least the channel ndots threshold (usually 1),
   * then we try the name as-is first.  Otherwise, we try the name
   * as-is last.
   */
  if (ndots >= (size_t)channel->ndots) {
    /* Try the name as-is first. */
    squery->next_domain  = 0;
    squery->trying_as_is = ARES_TRUE;
    ares_query(channel, name, dnsclass, type, search_callback, squery);
  } else {
    /* Try the name as-is last; start with the first search domain. */
    squery->next_domain  = 1;
    squery->trying_as_is = ARES_FALSE;
    status               = ares__cat_domain(name, channel->domains[0], &s);
    if (status == ARES_SUCCESS) {
      ares_query(channel, s, dnsclass, type, search_callback, squery);
      ares_free(s);
    } else {
      /* failed, free the malloc()ed memory */
      ares_free(squery->name);
      ares_free(squery);
      callback(arg, (int)status, 0, NULL, 0);
    }
  }
}

static void search_callback(void *arg, int status, int timeouts,
                            unsigned char *abuf, int alen)
{
  struct search_query *squery  = (struct search_query *)arg;
  ares_channel         channel = squery->channel;
  char                *s;

  squery->timeouts += (size_t)timeouts;

  /* Stop searching unless we got a non-fatal error. */
  if (status != ARES_ENODATA && status != ARES_ESERVFAIL &&
      status != ARES_ENOTFOUND) {
    end_squery(squery, (ares_status_t)status, abuf, (size_t)alen);
  } else {
    /* Save the status if we were trying as-is. */
    if (squery->trying_as_is) {
      squery->status_as_is = status;
    }

    /*
     * If we ever get ARES_ENODATA along the way, record that; if the search
     * should run to the very end and we got at least one ARES_ENODATA,
     * then callers like ares_gethostbyname() may want to try a T_A search
     * even if the last domain we queried for T_AAAA resource records
     * returned ARES_ENOTFOUND.
     */
    if (status == ARES_ENODATA) {
      squery->ever_got_nodata = ARES_TRUE;
    }

    if (squery->next_domain < (size_t)channel->ndomains) {
      ares_status_t mystatus;
      /* Try the next domain. */
      mystatus = ares__cat_domain(squery->name,
                                  channel->domains[squery->next_domain], &s);
      if (mystatus != ARES_SUCCESS) {
        end_squery(squery, mystatus, NULL, 0);
      } else {
        squery->trying_as_is = ARES_FALSE;
        squery->next_domain++;
        ares_query(channel, s, squery->dnsclass, squery->type, search_callback,
                   squery);
        ares_free(s);
      }
    } else if (squery->status_as_is == -1) {
      /* Try the name as-is at the end. */
      squery->trying_as_is = ARES_TRUE;
      ares_query(channel, squery->name, squery->dnsclass, squery->type,
                 search_callback, squery);
    } else {
      if (squery->status_as_is == ARES_ENOTFOUND && squery->ever_got_nodata) {
        end_squery(squery, ARES_ENODATA, NULL, 0);
      } else {
        end_squery(squery, (ares_status_t)squery->status_as_is, NULL, 0);
      }
    }
  }
}

static void end_squery(struct search_query *squery, ares_status_t status,
                       unsigned char *abuf, size_t alen)
{
  squery->callback(squery->arg, (int)status, (int)squery->timeouts, abuf,
                   (int)alen);
  ares_free(squery->name);
  ares_free(squery);
}

/* Concatenate two domains. */
ares_status_t ares__cat_domain(const char *name, const char *domain, char **s)
{
  size_t nlen = ares_strlen(name);
  size_t dlen = ares_strlen(domain);

  *s = ares_malloc(nlen + 1 + dlen + 1);
  if (!*s) {
    return ARES_ENOMEM;
  }
  memcpy(*s, name, nlen);
  (*s)[nlen] = '.';
  if (strcmp(domain, ".") == 0) {
    /* Avoid appending the root domain to the separator, which would set *s to
       an ill-formed value (ending in two consequtive dots). */
    dlen = 0;
  }
  memcpy(*s + nlen + 1, domain, dlen);
  (*s)[nlen + 1 + dlen] = 0;
  return ARES_SUCCESS;
}

/* Determine if this name only yields one query.  If it does, set *s to
 * the string we should query, in an allocated buffer.  If not, set *s
 * to NULL.
 */
ares_status_t ares__single_domain(ares_channel channel, const char *name,
                                  char **s)
{
  size_t        len = ares_strlen(name);
  const char   *hostaliases;
  FILE         *fp;
  char         *line = NULL;
  ares_status_t status;
  size_t        linesize;
  const char   *p;
  const char   *q;
  int           error;

  /* If the name contains a trailing dot, then the single query is the name
   * sans the trailing dot.
   */
  if ((len > 0) && (name[len - 1] == '.')) {
    *s = ares_strdup(name);
    return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
  }

  if (!(channel->flags & ARES_FLAG_NOALIASES) && !strchr(name, '.')) {
    /* The name might be a host alias. */
    hostaliases = getenv("HOSTALIASES");
    if (hostaliases) {
      fp = fopen(hostaliases, "r");
      if (fp) {
        while ((status = ares__read_line(fp, &line, &linesize)) ==
               ARES_SUCCESS) {
          if (strncasecmp(line, name, len) != 0 || !ISSPACE(line[len])) {
            continue;
          }
          p = line + len;
          while (ISSPACE(*p)) {
            p++;
          }
          if (*p) {
            q = p + 1;
            while (*q && !ISSPACE(*q)) {
              q++;
            }
            *s = ares_malloc((size_t)(q - p + 1));
            if (*s) {
              memcpy(*s, p, (size_t)(q - p));
              (*s)[q - p] = 0;
            }
            ares_free(line);
            fclose(fp);
            return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
          }
        }
        ares_free(line);
        fclose(fp);
        if (status != ARES_SUCCESS && status != ARES_EOF) {
          return status;
        }
      } else {
        error = ERRNO;
        switch (error) {
          case ENOENT:
          case ESRCH:
            break;
          default:
            DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n", error,
                           strerror(error)));
            DEBUGF(fprintf(stderr, "Error opening file: %s\n", hostaliases));
            *s = NULL;
            return ARES_EFILE;
        }
      }
    }
  }

  if (channel->flags & ARES_FLAG_NOSEARCH || channel->ndomains == 0) {
    /* No domain search to do; just try the name as-is. */
    *s = ares_strdup(name);
    return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
  }

  *s = NULL;
  return ARES_SUCCESS;
}


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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include "ares.h"
#include "ares_private.h"

struct search_query {
  /* Arguments passed to ares_search */
  ares_channel channel;
  char *name;                   /* copied into an allocated buffer */
  int dnsclass;
  int type;
  ares_callback callback;
  void *arg;

  int status_as_is;             /* error status from trying as-is */
  int next_domain;              /* next search domain to try */
  int trying_as_is;             /* current query is for name as-is */
  int timeouts;                 /* number of timeouts we saw for this request */
  int ever_got_nodata;          /* did we ever get ARES_ENODATA along the way? */
};

static void search_callback(void *arg, int status, int timeouts,
                            unsigned char *abuf, int alen);
static void end_squery(struct search_query *squery, int status,
                       unsigned char *abuf, int alen);
static int cat_domain(const char *name, const char *domain, char **s);
static int single_domain(ares_channel channel, const char *name, char **s);

void ares_search(ares_channel channel, const char *name, int dnsclass,
                 int type, ares_callback callback, void *arg)
{
  struct search_query *squery;
  char *s;
  const char *p;
  int status, ndots;

  /* If name only yields one domain to search, then we don't have
   * to keep extra state, so just do an ares_query().
   */
  status = single_domain(channel, name, &s);
  if (status != ARES_SUCCESS)
    {
      callback(arg, status, 0, NULL, 0);
      return;
    }
  if (s)
    {
      ares_query(channel, s, dnsclass, type, callback, arg);
      free(s);
      return;
    }

  /* Allocate a search_query structure to hold the state necessary for
   * doing multiple lookups.
   */
  squery = malloc(sizeof(struct search_query));
  if (!squery)
    {
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return;
    }
  squery->channel = channel;
  squery->name = strdup(name);
  if (!squery->name)
    {
      free(squery);
      callback(arg, ARES_ENOMEM, 0, NULL, 0);
      return;
    }
  squery->dnsclass = dnsclass;
  squery->type = type;
  squery->status_as_is = -1;
  squery->callback = callback;
  squery->arg = arg;
  squery->timeouts = 0;
  squery->ever_got_nodata = 0;

  /* Count the number of dots in name. */
  ndots = 0;
  for (p = name; *p; p++)
    {
      if (*p == '.')
        ndots++;
    }

  /* If ndots is at least the channel ndots threshold (usually 1),
   * then we try the name as-is first.  Otherwise, we try the name
   * as-is last.
   */
  if (ndots >= channel->ndots)
    {
      /* Try the name as-is first. */
      squery->next_domain = 0;
      squery->trying_as_is = 1;
      ares_query(channel, name, dnsclass, type, search_callback, squery);
    }
  else
    {
      /* Try the name as-is last; start with the first search domain. */
      squery->next_domain = 1;
      squery->trying_as_is = 0;
      status = cat_domain(name, channel->domains[0], &s);
      if (status == ARES_SUCCESS)
        {
          ares_query(channel, s, dnsclass, type, search_callback, squery);
          free(s);
        }
      else
      {
        /* failed, free the malloc()ed memory */
        free(squery->name);
        free(squery);
        callback(arg, status, 0, NULL, 0);
      }
    }
}

static void search_callback(void *arg, int status, int timeouts,
                            unsigned char *abuf, int alen)
{
  struct search_query *squery = (struct search_query *) arg;
  ares_channel channel = squery->channel;
  char *s;

  squery->timeouts += timeouts;

  /* Stop searching unless we got a non-fatal error. */
  if (status != ARES_ENODATA && status != ARES_ESERVFAIL
      && status != ARES_ENOTFOUND)
    end_squery(squery, status, abuf, alen);
  else
    {
      /* Save the status if we were trying as-is. */
      if (squery->trying_as_is)
        squery->status_as_is = status;

      /*
       * If we ever get ARES_ENODATA along the way, record that; if the search
       * should run to the very end and we got at least one ARES_ENODATA,
       * then callers like ares_gethostbyname() may want to try a T_A search
       * even if the last domain we queried for T_AAAA resource records
       * returned ARES_ENOTFOUND.
       */
      if (status == ARES_ENODATA)
        squery->ever_got_nodata = 1;

      if (squery->next_domain < channel->ndomains)
        {
          /* Try the next domain. */
          status = cat_domain(squery->name,
                              channel->domains[squery->next_domain], &s);
          if (status != ARES_SUCCESS)
            end_squery(squery, status, NULL, 0);
          else
            {
              squery->trying_as_is = 0;
              squery->next_domain++;
              ares_query(channel, s, squery->dnsclass, squery->type,
                         search_callback, squery);
              free(s);
            }
        }
      else if (squery->status_as_is == -1)
        {
          /* Try the name as-is at the end. */
          squery->trying_as_is = 1;
          ares_query(channel, squery->name, squery->dnsclass, squery->type,
                     search_callback, squery);
        }
      else {
        if (squery->status_as_is == ARES_ENOTFOUND && squery->ever_got_nodata) {
          end_squery(squery, ARES_ENODATA, NULL, 0);
        }
        else
          end_squery(squery, squery->status_as_is, NULL, 0);
      }
    }
}

static void end_squery(struct search_query *squery, int status,
                       unsigned char *abuf, int alen)
{
  squery->callback(squery->arg, status, squery->timeouts, abuf, alen);
  free(squery->name);
  free(squery);
}

/* Concatenate two domains. */
static int cat_domain(const char *name, const char *domain, char **s)
{
  size_t nlen = strlen(name);
  size_t dlen = strlen(domain);

  *s = malloc(nlen + 1 + dlen + 1);
  if (!*s)
    return ARES_ENOMEM;
  memcpy(*s, name, nlen);
  (*s)[nlen] = '.';
  memcpy(*s + nlen + 1, domain, dlen);
  (*s)[nlen + 1 + dlen] = 0;
  return ARES_SUCCESS;
}

/* Determine if this name only yields one query.  If it does, set *s to
 * the string we should query, in an allocated buffer.  If not, set *s
 * to NULL.
 */
static int single_domain(ares_channel channel, const char *name, char **s)
{
  size_t len = strlen(name);
  const char *hostaliases;
  FILE *fp;
  char *line = NULL;
  int status;
  size_t linesize;
  const char *p, *q;
  int error;

  /* If the name contains a trailing dot, then the single query is the name
   * sans the trailing dot.
   */
  if (name[len - 1] == '.')
    {
      *s = strdup(name);
      return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
    }

  if (!(channel->flags & ARES_FLAG_NOALIASES) && !strchr(name, '.'))
    {
      /* The name might be a host alias. */
      hostaliases = getenv("HOSTALIASES");
      if (hostaliases)
        {
          fp = fopen(hostaliases, "r");
          if (fp)
            {
              while ((status = ares__read_line(fp, &line, &linesize))
                     == ARES_SUCCESS)
                {
                  if (strncasecmp(line, name, len) != 0 ||
                      !ISSPACE(line[len]))
                    continue;
                  p = line + len;
                  while (ISSPACE(*p))
                    p++;
                  if (*p)
                    {
                      q = p + 1;
                      while (*q && !ISSPACE(*q))
                        q++;
                      *s = malloc(q - p + 1);
                      if (*s)
                        {
                          memcpy(*s, p, q - p);
                          (*s)[q - p] = 0;
                        }
                      free(line);
                      fclose(fp);
                      return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
                    }
                }
              free(line);
              fclose(fp);
              if (status != ARES_SUCCESS && status != ARES_EOF)
                return status;
            }
          else
            {
              error = ERRNO;
              switch(error)
                {
                case ENOENT:
                case ESRCH:
                  break;
                default:
                  DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                                 error, strerror(error)));
                  DEBUGF(fprintf(stderr, "Error opening file: %s\n",
                                 hostaliases));
                  *s = NULL;
                  return ARES_EFILE;
                }
            }
        }
    }

  if (channel->flags & ARES_FLAG_NOSEARCH || channel->ndomains == 0)
    {
      /* No domain search to do; just try the name as-is. */
      *s = strdup(name);
      return (*s) ? ARES_SUCCESS : ARES_ENOMEM;
    }

  *s = NULL;
  return ARES_SUCCESS;
}

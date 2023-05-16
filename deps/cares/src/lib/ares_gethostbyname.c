/* Copyright 1998, 2011, 2013 by the Massachusetts Institute of Technology.
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
#include <strings.h>
#endif

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "bitncmp.h"
#include "ares_platform.h"
#include "ares_nowarn.h"
#include "ares_private.h"

static void sort_addresses(struct hostent *host,
                           const struct apattern *sortlist, int nsort);
static void sort6_addresses(struct hostent *host,
                            const struct apattern *sortlist, int nsort);
static int get_address_index(const struct in_addr *addr,
                             const struct apattern *sortlist, int nsort);
static int get6_address_index(const struct ares_in6_addr *addr,
                              const struct apattern *sortlist, int nsort);

struct host_query {
  ares_host_callback callback;
  void *arg;
  ares_channel channel;
};

static void ares_gethostbyname_callback(void *arg, int status, int timeouts,
                                        struct ares_addrinfo *result)
{
  struct hostent *hostent = NULL;
  struct host_query *ghbn_arg = arg;

  if (status == ARES_SUCCESS)
    {
      status = ares__addrinfo2hostent(result, AF_UNSPEC, &hostent);
    }

  /* addrinfo2hostent will only return ENODATA if there are no addresses _and_
   * no cname/aliases.  However, gethostbyname will return ENODATA even if there
   * is cname/alias data */
  if (status == ARES_SUCCESS && hostent &&
      (!hostent->h_addr_list || !hostent->h_addr_list[0]))
    {
      status = ARES_ENODATA;
    }

  if (status == ARES_SUCCESS && ghbn_arg->channel->nsort && hostent)
    {
      if (hostent->h_addrtype == AF_INET6)
        sort6_addresses(hostent, ghbn_arg->channel->sortlist,
                        ghbn_arg->channel->nsort);
      if (hostent->h_addrtype == AF_INET)
        sort_addresses(hostent, ghbn_arg->channel->sortlist,
                       ghbn_arg->channel->nsort);
    }

  ghbn_arg->callback(ghbn_arg->arg, status, timeouts, hostent);

  ares_freeaddrinfo(result);
  ares_free(ghbn_arg);
  ares_free_hostent(hostent);
}

void ares_gethostbyname(ares_channel channel, const char *name, int family,
                        ares_host_callback callback, void *arg)
{
  const struct ares_addrinfo_hints hints = { ARES_AI_CANONNAME, family, 0, 0 };
  struct host_query *ghbn_arg;

  if (!callback)
    return;

  ghbn_arg = ares_malloc(sizeof(*ghbn_arg));
  if (!ghbn_arg)
    {
      callback(arg, ARES_ENOMEM, 0, NULL);
      return;
    }

  ghbn_arg->callback=callback;
  ghbn_arg->arg=arg;
  ghbn_arg->channel=channel;

  ares_getaddrinfo(channel, name, NULL, &hints, ares_gethostbyname_callback,
                   ghbn_arg);
}


static void sort_addresses(struct hostent *host,
                           const struct apattern *sortlist, int nsort)
{
  struct in_addr a1, a2;
  int i1, i2, ind1, ind2;

  /* This is a simple insertion sort, not optimized at all.  i1 walks
   * through the address list, with the loop invariant that everything
   * to the left of i1 is sorted.  In the loop body, the value at i1 is moved
   * back through the list (via i2) until it is in sorted order.
   */
  for (i1 = 0; host->h_addr_list[i1]; i1++)
    {
      memcpy(&a1, host->h_addr_list[i1], sizeof(struct in_addr));
      ind1 = get_address_index(&a1, sortlist, nsort);
      for (i2 = i1 - 1; i2 >= 0; i2--)
        {
          memcpy(&a2, host->h_addr_list[i2], sizeof(struct in_addr));
          ind2 = get_address_index(&a2, sortlist, nsort);
          if (ind2 <= ind1)
            break;
          memcpy(host->h_addr_list[i2 + 1], &a2, sizeof(struct in_addr));
        }
      memcpy(host->h_addr_list[i2 + 1], &a1, sizeof(struct in_addr));
    }
}

/* Find the first entry in sortlist which matches addr.  Return nsort
 * if none of them match.
 */
static int get_address_index(const struct in_addr *addr,
                             const struct apattern *sortlist,
                             int nsort)
{
  int i;

  for (i = 0; i < nsort; i++)
    {
      if (sortlist[i].family != AF_INET)
        continue;
      if (sortlist[i].type == PATTERN_MASK)
        {
          if ((addr->s_addr & sortlist[i].mask.addr4.s_addr)
              == sortlist[i].addrV4.s_addr)
            break;
        }
      else
        {
          if (!ares__bitncmp(&addr->s_addr, &sortlist[i].addrV4.s_addr,
                             sortlist[i].mask.bits))
            break;
        }
    }
  return i;
}

static void sort6_addresses(struct hostent *host,
                            const struct apattern *sortlist, int nsort)
{
  struct ares_in6_addr a1, a2;
  int i1, i2, ind1, ind2;

  /* This is a simple insertion sort, not optimized at all.  i1 walks
   * through the address list, with the loop invariant that everything
   * to the left of i1 is sorted.  In the loop body, the value at i1 is moved
   * back through the list (via i2) until it is in sorted order.
   */
  for (i1 = 0; host->h_addr_list[i1]; i1++)
    {
      memcpy(&a1, host->h_addr_list[i1], sizeof(struct ares_in6_addr));
      ind1 = get6_address_index(&a1, sortlist, nsort);
      for (i2 = i1 - 1; i2 >= 0; i2--)
        {
          memcpy(&a2, host->h_addr_list[i2], sizeof(struct ares_in6_addr));
          ind2 = get6_address_index(&a2, sortlist, nsort);
          if (ind2 <= ind1)
            break;
          memcpy(host->h_addr_list[i2 + 1], &a2, sizeof(struct ares_in6_addr));
        }
      memcpy(host->h_addr_list[i2 + 1], &a1, sizeof(struct ares_in6_addr));
    }
}

/* Find the first entry in sortlist which matches addr.  Return nsort
 * if none of them match.
 */
static int get6_address_index(const struct ares_in6_addr *addr,
                              const struct apattern *sortlist,
                              int nsort)
{
  int i;

  for (i = 0; i < nsort; i++)
    {
      if (sortlist[i].family != AF_INET6)
        continue;
      if (!ares__bitncmp(addr, &sortlist[i].addrV6, sortlist[i].mask.bits))
        break;
    }
  return i;
}



static int file_lookup(const char *name, int family, struct hostent **host);

/* I really have no idea why this is exposed as a public function, but since
 * it is, we can't kill this legacy function. */
int ares_gethostbyname_file(ares_channel channel, const char *name,
                            int family, struct hostent **host)
{
  int result;

  /* We only take the channel to ensure that ares_init() been called. */
  if(channel == NULL)
    {
      /* Anything will do, really.  This seems fine, and is consistent with
         other error cases. */
      *host = NULL;
      return ARES_ENOTFOUND;
    }

  /* Just chain to the internal implementation we use here; it's exactly
   * what we want.
   */
  result = file_lookup(name, family, host);
  if(result != ARES_SUCCESS)
    {
      /* We guarantee a NULL hostent on failure. */
      *host = NULL;
    }
  return result;
}

static int file_lookup(const char *name, int family, struct hostent **host)
{
  FILE *fp;
  char **alias;
  int status;
  int error;

#ifdef WIN32
  char PATH_HOSTS[MAX_PATH];
  win_platform platform;

  PATH_HOSTS[0] = '\0';

  platform = ares__getplatform();

  if (platform == WIN_NT) {
    char tmp[MAX_PATH];
    HKEY hkeyHosts;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0, KEY_READ,
                     &hkeyHosts) == ERROR_SUCCESS)
    {
      DWORD dwLength = MAX_PATH;
      RegQueryValueExA(hkeyHosts, DATABASEPATH, NULL, NULL, (LPBYTE)tmp,
                      &dwLength);
      ExpandEnvironmentStringsA(tmp, PATH_HOSTS, MAX_PATH);
      RegCloseKey(hkeyHosts);
    }
  }
  else if (platform == WIN_9X)
    GetWindowsDirectoryA(PATH_HOSTS, MAX_PATH);
  else
    return ARES_ENOTFOUND;

  strcat(PATH_HOSTS, WIN_PATH_HOSTS);

#elif defined(WATT32)
  const char *PATH_HOSTS = _w32_GetHostsFile();

  if (!PATH_HOSTS)
    return ARES_ENOTFOUND;
#endif

  /* Per RFC 7686, reject queries for ".onion" domain names with NXDOMAIN. */
  if (ares__is_onion_domain(name))
    return ARES_ENOTFOUND;


  fp = fopen(PATH_HOSTS, "r");
  if (!fp)
    {
      error = ERRNO;
      switch(error)
        {
        case ENOENT:
        case ESRCH:
          return ARES_ENOTFOUND;
        default:
          DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                         error, strerror(error)));
          DEBUGF(fprintf(stderr, "Error opening file: %s\n",
                         PATH_HOSTS));
          *host = NULL;
          return ARES_EFILE;
        }
    }
  while ((status = ares__get_hostent(fp, family, host)) == ARES_SUCCESS)
    {
      if (strcasecmp((*host)->h_name, name) == 0)
        break;
      for (alias = (*host)->h_aliases; *alias; alias++)
        {
          if (strcasecmp(*alias, name) == 0)
            break;
        }
      if (*alias)
        break;
      ares_free_hostent(*host);
    }
  fclose(fp);
  if (status == ARES_EOF)
    status = ARES_ENOTFOUND;
  if (status != ARES_SUCCESS)
    *host = NULL;
  return status;
}



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
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#include "ares_nameser.h"

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_platform.h"
#include "ares_private.h"

#ifdef WATT32
#undef WIN32
#endif

struct addr_query {
  /* Arguments passed to ares_gethostbyaddr() */
  ares_channel channel;
  struct ares_addr addr;
  ares_host_callback callback;
  void *arg;

  const char *remaining_lookups;
  int timeouts;
};

static void next_lookup(struct addr_query *aquery);
static void addr_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen);
static void end_aquery(struct addr_query *aquery, int status,
                       struct hostent *host);
static int file_lookup(struct ares_addr *addr, struct hostent **host);
static void ptr_rr_name(char *name, const struct ares_addr *addr);

void ares_gethostbyaddr(ares_channel channel, const void *addr, int addrlen,
                        int family, ares_host_callback callback, void *arg)
{
  struct addr_query *aquery;

  if (family != AF_INET && family != AF_INET6)
    {
      callback(arg, ARES_ENOTIMP, 0, NULL);
      return;
    }

  if ((family == AF_INET && addrlen != sizeof(aquery->addr.addrV4)) ||
      (family == AF_INET6 && addrlen != sizeof(aquery->addr.addrV6)))
    {
      callback(arg, ARES_ENOTIMP, 0, NULL);
      return;
    }

  aquery = ares_malloc(sizeof(struct addr_query));
  if (!aquery)
    {
      callback(arg, ARES_ENOMEM, 0, NULL);
      return;
    }
  aquery->channel = channel;
  if (family == AF_INET)
    memcpy(&aquery->addr.addrV4, addr, sizeof(aquery->addr.addrV4));
  else
    memcpy(&aquery->addr.addrV6, addr, sizeof(aquery->addr.addrV6));
  aquery->addr.family = family;
  aquery->callback = callback;
  aquery->arg = arg;
  aquery->remaining_lookups = channel->lookups;
  aquery->timeouts = 0;

  next_lookup(aquery);
}

static void next_lookup(struct addr_query *aquery)
{
  const char *p;
  char name[128];
  int status;
  struct hostent *host;

  for (p = aquery->remaining_lookups; *p; p++)
    {
      switch (*p)
        {
        case 'b':
          ptr_rr_name(name, &aquery->addr);
          aquery->remaining_lookups = p + 1;
          ares_query(aquery->channel, name, C_IN, T_PTR, addr_callback,
                     aquery);
          return;
        case 'f':
          status = file_lookup(&aquery->addr, &host);

          /* this status check below previously checked for !ARES_ENOTFOUND,
             but we should not assume that this single error code is the one
             that can occur, as that is in fact no longer the case */
          if (status == ARES_SUCCESS)
            {
              end_aquery(aquery, status, host);
              return;
            }
          break;
        }
    }
  end_aquery(aquery, ARES_ENOTFOUND, NULL);
}

static void addr_callback(void *arg, int status, int timeouts,
                          unsigned char *abuf, int alen)
{
  struct addr_query *aquery = (struct addr_query *) arg;
  struct hostent *host;
  size_t addrlen;

  aquery->timeouts += timeouts;
  if (status == ARES_SUCCESS)
    {
      if (aquery->addr.family == AF_INET)
        {
          addrlen = sizeof(aquery->addr.addrV4);
          status = ares_parse_ptr_reply(abuf, alen, &aquery->addr.addrV4,
                                        (int)addrlen, AF_INET, &host);
        }
      else
        {
          addrlen = sizeof(aquery->addr.addrV6);
          status = ares_parse_ptr_reply(abuf, alen, &aquery->addr.addrV6,
                                        (int)addrlen, AF_INET6, &host);
        }
      end_aquery(aquery, status, host);
    }
  else if (status == ARES_EDESTRUCTION || status == ARES_ECANCELLED)
    end_aquery(aquery, status, NULL);
  else
    next_lookup(aquery);
}

static void end_aquery(struct addr_query *aquery, int status,
                       struct hostent *host)
{
  aquery->callback(aquery->arg, status, aquery->timeouts, host);
  if (host)
    ares_free_hostent(host);
  ares_free(aquery);
}

static int file_lookup(struct ares_addr *addr, struct hostent **host)
{
  FILE *fp;
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
  while ((status = ares__get_hostent(fp, addr->family, host)) == ARES_SUCCESS)
    {
      if (addr->family != (*host)->h_addrtype)
        {
          ares_free_hostent(*host);
          continue;
        }
      if (addr->family == AF_INET)
        {
          if (memcmp((*host)->h_addr, &addr->addrV4,
                     sizeof(addr->addrV4)) == 0)
            break;
        }
      else if (addr->family == AF_INET6)
        {
          if (memcmp((*host)->h_addr, &addr->addrV6,
                     sizeof(addr->addrV6)) == 0)
            break;
        }
      ares_free_hostent(*host);
    }
  fclose(fp);
  if (status == ARES_EOF)
    status = ARES_ENOTFOUND;
  if (status != ARES_SUCCESS)
    *host = NULL;
  return status;
}

static void ptr_rr_name(char *name, const struct ares_addr *addr)
{
  if (addr->family == AF_INET)
    {
       unsigned long laddr = ntohl(addr->addrV4.s_addr);
       unsigned long a1 = (laddr >> 24UL) & 0xFFUL;
       unsigned long a2 = (laddr >> 16UL) & 0xFFUL;
       unsigned long a3 = (laddr >>  8UL) & 0xFFUL;
       unsigned long a4 = laddr & 0xFFUL;
       sprintf(name, "%lu.%lu.%lu.%lu.in-addr.arpa", a4, a3, a2, a1);
    }
  else
    {
       unsigned char *bytes = (unsigned char *)&addr->addrV6;
       /* There are too many arguments to do this in one line using
        * minimally C89-compliant compilers */
       sprintf(name,
                "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.",
                bytes[15]&0xf, bytes[15] >> 4, bytes[14]&0xf, bytes[14] >> 4,
                bytes[13]&0xf, bytes[13] >> 4, bytes[12]&0xf, bytes[12] >> 4,
                bytes[11]&0xf, bytes[11] >> 4, bytes[10]&0xf, bytes[10] >> 4,
                bytes[9]&0xf, bytes[9] >> 4, bytes[8]&0xf, bytes[8] >> 4);
       sprintf(name+strlen(name),
                "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.ip6.arpa",
                bytes[7]&0xf, bytes[7] >> 4, bytes[6]&0xf, bytes[6] >> 4,
                bytes[5]&0xf, bytes[5] >> 4, bytes[4]&0xf, bytes[4] >> 4,
                bytes[3]&0xf, bytes[3] >> 4, bytes[2]&0xf, bytes[2] >> 4,
                bytes[1]&0xf, bytes[1] >> 4, bytes[0]&0xf, bytes[0] >> 4);
    }
}


/* Copyright 1998 by the Massachusetts Institute of Technology.
 * Copyright (C) 2007-2013 by Daniel Stenberg
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

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#else
#  include "nameser.h"
#endif
#ifdef HAVE_ARPA_NAMESER_COMPAT_H
#  include <arpa/nameser_compat.h>
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#include <sys/system_properties.h>
/* From the Bionic sources */
#define DNS_PROP_NAME_PREFIX  "net.dns"
#define MAX_DNS_PROPERTIES    8
#endif

#if defined(CARES_USE_LIBRESOLV)
#include <resolv.h>
#endif

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_library_init.h"
#include "ares_nowarn.h"
#include "ares_platform.h"
#include "ares_private.h"

#ifdef WATT32
#undef WIN32  /* Redefined in MingW/MSVC headers */
#endif

static int init_by_options(ares_channel channel,
                           const struct ares_options *options,
                           int optmask);
static int init_by_environment(ares_channel channel);
static int init_by_resolv_conf(ares_channel channel);
static int init_by_defaults(ares_channel channel);

#ifndef WATT32
static int config_nameserver(struct server_state **servers, int *nservers,
                             char *str);
#endif
static int set_search(ares_channel channel, const char *str);
static int set_options(ares_channel channel, const char *str);
static const char *try_option(const char *p, const char *q, const char *opt);
static int init_id_key(rc4_key* key,int key_data_len);

static int config_sortlist(struct apattern **sortlist, int *nsort,
                           const char *str);
static int sortlist_alloc(struct apattern **sortlist, int *nsort,
                          struct apattern *pat);
static int ip_addr(const char *s, ares_ssize_t len, struct in_addr *addr);
static void natural_mask(struct apattern *pat);
#if !defined(WIN32) && !defined(WATT32) && \
    !defined(ANDROID) && !defined(__ANDROID__) && !defined(CARES_USE_LIBRESOLV)
static int config_domain(ares_channel channel, char *str);
static int config_lookup(ares_channel channel, const char *str,
                         const char *bindch, const char *altbindch,
                         const char *filech);
static char *try_config(char *s, const char *opt, char scc);
#endif

#define ARES_CONFIG_CHECK(x) (x->lookups && x->nsort > -1 && \
                             x->nservers > -1 && \
                             x->ndomains > -1 && \
                             x->ndots > -1 && x->timeout > -1 && \
                             x->tries > -1)

int ares_init(ares_channel *channelptr)
{
  return ares_init_options(channelptr, NULL, 0);
}

int ares_init_options(ares_channel *channelptr, struct ares_options *options,
                      int optmask)
{
  ares_channel channel;
  int i;
  int status = ARES_SUCCESS;
  struct timeval now;

#ifdef CURLDEBUG
  const char *env = getenv("CARES_MEMDEBUG");

  if (env)
    curl_memdebug(env);
  env = getenv("CARES_MEMLIMIT");
  if (env) {
    char *endptr;
    long num = strtol(env, &endptr, 10);
    if((endptr != env) && (endptr == env + strlen(env)) && (num > 0))
      curl_memlimit(num);
  }
#endif

  if (ares_library_initialized() != ARES_SUCCESS)
    return ARES_ENOTINITIALIZED;  /* LCOV_EXCL_LINE: n/a on non-WinSock */

  channel = ares_malloc(sizeof(struct ares_channeldata));
  if (!channel) {
    *channelptr = NULL;
    return ARES_ENOMEM;
  }

  now = ares__tvnow();

  /* Set everything to distinguished values so we know they haven't
   * been set yet.
   */
  channel->flags = -1;
  channel->timeout = -1;
  channel->tries = -1;
  channel->ndots = -1;
  channel->rotate = -1;
  channel->udp_port = -1;
  channel->tcp_port = -1;
  channel->ednspsz = -1;
  channel->socket_send_buffer_size = -1;
  channel->socket_receive_buffer_size = -1;
  channel->nservers = -1;
  channel->ndomains = -1;
  channel->nsort = -1;
  channel->tcp_connection_generation = 0;
  channel->lookups = NULL;
  channel->domains = NULL;
  channel->sortlist = NULL;
  channel->servers = NULL;
  channel->sock_state_cb = NULL;
  channel->sock_state_cb_data = NULL;
  channel->sock_create_cb = NULL;
  channel->sock_create_cb_data = NULL;
  channel->sock_config_cb = NULL;
  channel->sock_config_cb_data = NULL;
  channel->sock_funcs = NULL;
  channel->sock_func_cb_data = NULL;

  channel->last_server = 0;
  channel->last_timeout_processed = (time_t)now.tv_sec;

  memset(&channel->local_dev_name, 0, sizeof(channel->local_dev_name));
  channel->local_ip4 = 0;
  memset(&channel->local_ip6, 0, sizeof(channel->local_ip6));

  /* Initialize our lists of queries */
  ares__init_list_head(&(channel->all_queries));
  for (i = 0; i < ARES_QID_TABLE_SIZE; i++)
    {
      ares__init_list_head(&(channel->queries_by_qid[i]));
    }
  for (i = 0; i < ARES_TIMEOUT_TABLE_SIZE; i++)
    {
      ares__init_list_head(&(channel->queries_by_timeout[i]));
    }

  /* Initialize configuration by each of the four sources, from highest
   * precedence to lowest.
   */

  status = init_by_options(channel, options, optmask);
  if (status != ARES_SUCCESS) {
    DEBUGF(fprintf(stderr, "Error: init_by_options failed: %s\n",
                   ares_strerror(status)));
    /* If we fail to apply user-specified options, fail the whole init process */
    goto done;
  }
  status = init_by_environment(channel);
  if (status != ARES_SUCCESS)
    DEBUGF(fprintf(stderr, "Error: init_by_environment failed: %s\n",
                   ares_strerror(status)));
  if (status == ARES_SUCCESS) {
    status = init_by_resolv_conf(channel);
    if (status != ARES_SUCCESS)
      DEBUGF(fprintf(stderr, "Error: init_by_resolv_conf failed: %s\n",
                     ares_strerror(status)));
  }

  /*
   * No matter what failed or succeeded, seed defaults to provide
   * useful behavior for things that we missed.
   */
  status = init_by_defaults(channel);
  if (status != ARES_SUCCESS)
    DEBUGF(fprintf(stderr, "Error: init_by_defaults failed: %s\n",
                   ares_strerror(status)));

  /* Generate random key */

  if (status == ARES_SUCCESS) {
    status = init_id_key(&channel->id_key, ARES_ID_KEY_LEN);
    if (status == ARES_SUCCESS)
      channel->next_id = ares__generate_new_id(&channel->id_key);
    else
      DEBUGF(fprintf(stderr, "Error: init_id_key failed: %s\n",
                     ares_strerror(status)));
  }

done:
  if (status != ARES_SUCCESS)
    {
      /* Something failed; clean up memory we may have allocated. */
      if (channel->servers)
        ares_free(channel->servers);
      if (channel->domains)
        {
          for (i = 0; i < channel->ndomains; i++)
            ares_free(channel->domains[i]);
          ares_free(channel->domains);
        }
      if (channel->sortlist)
        ares_free(channel->sortlist);
      if(channel->lookups)
        ares_free(channel->lookups);
      ares_free(channel);
      return status;
    }

  /* Trim to one server if ARES_FLAG_PRIMARY is set. */
  if ((channel->flags & ARES_FLAG_PRIMARY) && channel->nservers > 1)
    channel->nservers = 1;

  ares__init_servers_state(channel);

  *channelptr = channel;
  return ARES_SUCCESS;
}

/* ares_dup() duplicates a channel handle with all its options and returns a
   new channel handle */
int ares_dup(ares_channel *dest, ares_channel src)
{
  struct ares_options opts;
  struct ares_addr_port_node *servers;
  int non_v4_default_port = 0;
  int i, rc;
  int optmask;

  *dest = NULL; /* in case of failure return NULL explicitly */

  /* First get the options supported by the old ares_save_options() function,
     which is most of them */
  rc = ares_save_options(src, &opts, &optmask);
  if(rc)
  {
    ares_destroy_options(&opts);
    return rc;
  }

  /* Then create the new channel with those options */
  rc = ares_init_options(dest, &opts, optmask);

  /* destroy the options copy to not leak any memory */
  ares_destroy_options(&opts);

  if(rc)
    return rc;

  /* Now clone the options that ares_save_options() doesn't support. */
  (*dest)->sock_create_cb      = src->sock_create_cb;
  (*dest)->sock_create_cb_data = src->sock_create_cb_data;
  (*dest)->sock_config_cb      = src->sock_config_cb;
  (*dest)->sock_config_cb_data = src->sock_config_cb_data;
  (*dest)->sock_funcs          = src->sock_funcs;
  (*dest)->sock_func_cb_data   = src->sock_func_cb_data;

  strncpy((*dest)->local_dev_name, src->local_dev_name,
          sizeof(src->local_dev_name));
  (*dest)->local_ip4 = src->local_ip4;
  memcpy((*dest)->local_ip6, src->local_ip6, sizeof(src->local_ip6));

  /* Full name server cloning required if there is a non-IPv4, or non-default port, nameserver */
  for (i = 0; i < src->nservers; i++)
    {
      if ((src->servers[i].addr.family != AF_INET) ||
          (src->servers[i].addr.udp_port != 0) ||
          (src->servers[i].addr.tcp_port != 0)) {
        non_v4_default_port++;
        break;
      }
    }
  if (non_v4_default_port) {
    rc = ares_get_servers_ports(src, &servers);
    if (rc != ARES_SUCCESS) {
      ares_destroy(*dest);
      *dest = NULL;
      return rc;
    }
    rc = ares_set_servers_ports(*dest, servers);
    ares_free_data(servers);
    if (rc != ARES_SUCCESS) {
      ares_destroy(*dest);
      *dest = NULL;
      return rc;
    }
  }

  return ARES_SUCCESS; /* everything went fine */
}

/* Save options from initialized channel */
int ares_save_options(ares_channel channel, struct ares_options *options,
                      int *optmask)
{
  int i, j;
  int ipv4_nservers = 0;

  /* Zero everything out */
  memset(options, 0, sizeof(struct ares_options));

  if (!ARES_CONFIG_CHECK(channel))
    return ARES_ENODATA;

  /* Traditionally the optmask wasn't saved in the channel struct so it was
     recreated here. ROTATE is the first option that has no struct field of
     its own in the public config struct */
  (*optmask) = (ARES_OPT_FLAGS|ARES_OPT_TRIES|ARES_OPT_NDOTS|
                ARES_OPT_UDP_PORT|ARES_OPT_TCP_PORT|ARES_OPT_SOCK_STATE_CB|
                ARES_OPT_SERVERS|ARES_OPT_DOMAINS|ARES_OPT_LOOKUPS|
                ARES_OPT_SORTLIST|ARES_OPT_TIMEOUTMS);
  (*optmask) |= (channel->rotate ? ARES_OPT_ROTATE : ARES_OPT_NOROTATE);

  /* Copy easy stuff */
  options->flags   = channel->flags;

  /* We return full millisecond resolution but that's only because we don't
     set the ARES_OPT_TIMEOUT anymore, only the new ARES_OPT_TIMEOUTMS */
  options->timeout = channel->timeout;
  options->tries   = channel->tries;
  options->ndots   = channel->ndots;
  options->udp_port = ntohs(aresx_sitous(channel->udp_port));
  options->tcp_port = ntohs(aresx_sitous(channel->tcp_port));
  options->sock_state_cb     = channel->sock_state_cb;
  options->sock_state_cb_data = channel->sock_state_cb_data;

  /* Copy IPv4 servers that use the default port */
  if (channel->nservers) {
    for (i = 0; i < channel->nservers; i++)
    {
      if ((channel->servers[i].addr.family == AF_INET) &&
          (channel->servers[i].addr.udp_port == 0) &&
          (channel->servers[i].addr.tcp_port == 0))
        ipv4_nservers++;
    }
    if (ipv4_nservers) {
      options->servers = ares_malloc(ipv4_nservers * sizeof(struct in_addr));
      if (!options->servers)
        return ARES_ENOMEM;
      for (i = j = 0; i < channel->nservers; i++)
      {
        if ((channel->servers[i].addr.family == AF_INET) &&
            (channel->servers[i].addr.udp_port == 0) &&
            (channel->servers[i].addr.tcp_port == 0))
          memcpy(&options->servers[j++],
                 &channel->servers[i].addr.addrV4,
                 sizeof(channel->servers[i].addr.addrV4));
      }
    }
  }
  options->nservers = ipv4_nservers;

  /* copy domains */
  if (channel->ndomains) {
    options->domains = ares_malloc(channel->ndomains * sizeof(char *));
    if (!options->domains)
      return ARES_ENOMEM;

    for (i = 0; i < channel->ndomains; i++)
    {
      options->ndomains = i;
      options->domains[i] = ares_strdup(channel->domains[i]);
      if (!options->domains[i])
        return ARES_ENOMEM;
    }
  }
  options->ndomains = channel->ndomains;

  /* copy lookups */
  if (channel->lookups) {
    options->lookups = ares_strdup(channel->lookups);
    if (!options->lookups && channel->lookups)
      return ARES_ENOMEM;
  }

  /* copy sortlist */
  if (channel->nsort) {
    options->sortlist = ares_malloc(channel->nsort * sizeof(struct apattern));
    if (!options->sortlist)
      return ARES_ENOMEM;
    for (i = 0; i < channel->nsort; i++)
      options->sortlist[i] = channel->sortlist[i];
  }
  options->nsort = channel->nsort;

  return ARES_SUCCESS;
}

static int init_by_options(ares_channel channel,
                           const struct ares_options *options,
                           int optmask)
{
  int i;

  /* Easy stuff. */
  if ((optmask & ARES_OPT_FLAGS) && channel->flags == -1)
    channel->flags = options->flags;
  if ((optmask & ARES_OPT_TIMEOUTMS) && channel->timeout == -1)
    channel->timeout = options->timeout;
  else if ((optmask & ARES_OPT_TIMEOUT) && channel->timeout == -1)
    channel->timeout = options->timeout * 1000;
  if ((optmask & ARES_OPT_TRIES) && channel->tries == -1)
    channel->tries = options->tries;
  if ((optmask & ARES_OPT_NDOTS) && channel->ndots == -1)
    channel->ndots = options->ndots;
  if ((optmask & ARES_OPT_ROTATE) && channel->rotate == -1)
    channel->rotate = 1;
  if ((optmask & ARES_OPT_NOROTATE) && channel->rotate == -1)
    channel->rotate = 0;
  if ((optmask & ARES_OPT_UDP_PORT) && channel->udp_port == -1)
    channel->udp_port = htons(options->udp_port);
  if ((optmask & ARES_OPT_TCP_PORT) && channel->tcp_port == -1)
    channel->tcp_port = htons(options->tcp_port);
  if ((optmask & ARES_OPT_SOCK_STATE_CB) && channel->sock_state_cb == NULL)
    {
      channel->sock_state_cb = options->sock_state_cb;
      channel->sock_state_cb_data = options->sock_state_cb_data;
    }
  if ((optmask & ARES_OPT_SOCK_SNDBUF)
      && channel->socket_send_buffer_size == -1)
    channel->socket_send_buffer_size = options->socket_send_buffer_size;
  if ((optmask & ARES_OPT_SOCK_RCVBUF)
      && channel->socket_receive_buffer_size == -1)
    channel->socket_receive_buffer_size = options->socket_receive_buffer_size;

  if ((optmask & ARES_OPT_EDNSPSZ) && channel->ednspsz == -1)
    channel->ednspsz = options->ednspsz;

  /* Copy the IPv4 servers, if given. */
  if ((optmask & ARES_OPT_SERVERS) && channel->nservers == -1)
    {
      /* Avoid zero size allocations at any cost */
      if (options->nservers > 0)
        {
          channel->servers =
            ares_malloc(options->nservers * sizeof(struct server_state));
          if (!channel->servers)
            return ARES_ENOMEM;
          for (i = 0; i < options->nservers; i++)
            {
              channel->servers[i].addr.family = AF_INET;
              channel->servers[i].addr.udp_port = 0;
              channel->servers[i].addr.tcp_port = 0;
              memcpy(&channel->servers[i].addr.addrV4,
                     &options->servers[i],
                     sizeof(channel->servers[i].addr.addrV4));
            }
        }
      channel->nservers = options->nservers;
    }

  /* Copy the domains, if given.  Keep channel->ndomains consistent so
   * we can clean up in case of error.
   */
  if ((optmask & ARES_OPT_DOMAINS) && channel->ndomains == -1)
    {
      /* Avoid zero size allocations at any cost */
      if (options->ndomains > 0)
      {
        channel->domains = ares_malloc(options->ndomains * sizeof(char *));
        if (!channel->domains)
          return ARES_ENOMEM;
        for (i = 0; i < options->ndomains; i++)
          {
            channel->ndomains = i;
            channel->domains[i] = ares_strdup(options->domains[i]);
            if (!channel->domains[i])
              return ARES_ENOMEM;
          }
      }
      channel->ndomains = options->ndomains;
    }

  /* Set lookups, if given. */
  if ((optmask & ARES_OPT_LOOKUPS) && !channel->lookups)
    {
      channel->lookups = ares_strdup(options->lookups);
      if (!channel->lookups)
        return ARES_ENOMEM;
    }

  /* copy sortlist */
  if ((optmask & ARES_OPT_SORTLIST) && (channel->nsort == -1)) {
    if (options->nsort > 0) {
      channel->sortlist = ares_malloc(options->nsort * sizeof(struct apattern));
      if (!channel->sortlist)
        return ARES_ENOMEM;
      for (i = 0; i < options->nsort; i++)
        channel->sortlist[i] = options->sortlist[i];
    }
    channel->nsort = options->nsort;
  }

  channel->optmask = optmask;

  return ARES_SUCCESS;
}

static int init_by_environment(ares_channel channel)
{
  const char *localdomain, *res_options;
  int status;

  localdomain = getenv("LOCALDOMAIN");
  if (localdomain && channel->ndomains == -1)
    {
      status = set_search(channel, localdomain);
      if (status != ARES_SUCCESS)
        return status;
    }

  res_options = getenv("RES_OPTIONS");
  if (res_options)
    {
      status = set_options(channel, res_options);
      if (status != ARES_SUCCESS)
        return status;  /* LCOV_EXCL_LINE: set_options() never fails */
    }

  return ARES_SUCCESS;
}

#ifdef WIN32
/*
 * get_REG_SZ()
 *
 * Given a 'hKey' handle to an open registry key and a 'leafKeyName' pointer
 * to the name of the registry leaf key to be queried, fetch it's string
 * value and return a pointer in *outptr to a newly allocated memory area
 * holding it as a null-terminated string.
 *
 * Returns 0 and nullifies *outptr upon inability to return a string value.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Supported on Windows NT 3.5 and newer.
 */
static int get_REG_SZ(HKEY hKey, const char *leafKeyName, char **outptr)
{
  DWORD size = 0;
  int   res;

  *outptr = NULL;

  /* Find out size of string stored in registry */
  res = RegQueryValueEx(hKey, leafKeyName, 0, NULL, NULL, &size);
  if ((res != ERROR_SUCCESS && res != ERROR_MORE_DATA) || !size)
    return 0;

  /* Allocate buffer of indicated size plus one given that string
     might have been stored without null termination */
  *outptr = ares_malloc(size+1);
  if (!*outptr)
    return 0;

  /* Get the value for real */
  res = RegQueryValueEx(hKey, leafKeyName, 0, NULL,
                        (unsigned char *)*outptr, &size);
  if ((res != ERROR_SUCCESS) || (size == 1))
  {
    ares_free(*outptr);
    *outptr = NULL;
    return 0;
  }

  /* Null terminate buffer allways */
  *(*outptr + size) = '\0';

  return 1;
}

/*
 * get_REG_SZ_9X()
 *
 * Functionally identical to get_REG_SZ()
 *
 * Supported on Windows 95, 98 and ME.
 */
static int get_REG_SZ_9X(HKEY hKey, const char *leafKeyName, char **outptr)
{
  DWORD dataType = 0;
  DWORD size = 0;
  int   res;

  *outptr = NULL;

  /* Find out size of string stored in registry */
  res = RegQueryValueEx(hKey, leafKeyName, 0, &dataType, NULL, &size);
  if ((res != ERROR_SUCCESS && res != ERROR_MORE_DATA) || !size)
    return 0;

  /* Allocate buffer of indicated size plus one given that string
     might have been stored without null termination */
  *outptr = ares_malloc(size+1);
  if (!*outptr)
    return 0;

  /* Get the value for real */
  res = RegQueryValueEx(hKey, leafKeyName, 0, &dataType,
                        (unsigned char *)*outptr, &size);
  if ((res != ERROR_SUCCESS) || (size == 1))
  {
    ares_free(*outptr);
    *outptr = NULL;
    return 0;
  }

  /* Null terminate buffer allways */
  *(*outptr + size) = '\0';

  return 1;
}

/*
 * get_enum_REG_SZ()
 *
 * Given a 'hKeyParent' handle to an open registry key and a 'leafKeyName'
 * pointer to the name of the registry leaf key to be queried, parent key
 * is enumerated searching in child keys for given leaf key name and its
 * associated string value. When located, this returns a pointer in *outptr
 * to a newly allocated memory area holding it as a null-terminated string.
 *
 * Returns 0 and nullifies *outptr upon inability to return a string value.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Supported on Windows NT 3.5 and newer.
 */
static int get_enum_REG_SZ(HKEY hKeyParent, const char *leafKeyName,
                           char **outptr)
{
  char  enumKeyName[256];
  DWORD enumKeyNameBuffSize;
  DWORD enumKeyIdx = 0;
  HKEY  hKeyEnum;
  int   gotString;
  int   res;

  *outptr = NULL;

  for(;;)
  {
    enumKeyNameBuffSize = sizeof(enumKeyName);
    res = RegEnumKeyEx(hKeyParent, enumKeyIdx++, enumKeyName,
                       &enumKeyNameBuffSize, 0, NULL, NULL, NULL);
    if (res != ERROR_SUCCESS)
      break;
    res = RegOpenKeyEx(hKeyParent, enumKeyName, 0, KEY_QUERY_VALUE,
                       &hKeyEnum);
    if (res != ERROR_SUCCESS)
      continue;
    gotString = get_REG_SZ(hKeyEnum, leafKeyName, outptr);
    RegCloseKey(hKeyEnum);
    if (gotString)
      break;
  }

  if (!*outptr)
    return 0;

  return 1;
}

/*
 * get_DNS_Registry_9X()
 *
 * Functionally identical to get_DNS_Registry()
 *
 * Implementation supports Windows 95, 98 and ME.
 */
static int get_DNS_Registry_9X(char **outptr)
{
  HKEY hKey_VxD_MStcp;
  int  gotString;
  int  res;

  *outptr = NULL;

  res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_9X, 0, KEY_READ,
                     &hKey_VxD_MStcp);
  if (res != ERROR_SUCCESS)
    return 0;

  gotString = get_REG_SZ_9X(hKey_VxD_MStcp, NAMESERVER, outptr);
  RegCloseKey(hKey_VxD_MStcp);

  if (!gotString || !*outptr)
    return 0;

  return 1;
}

/*
 * get_DNS_Registry_NT()
 *
 * Functionally identical to get_DNS_Registry()
 *
 * Refs: Microsoft Knowledge Base articles KB120642 and KB314053.
 *
 * Implementation supports Windows NT 3.5 and newer.
 */
static int get_DNS_Registry_NT(char **outptr)
{
  HKEY hKey_Interfaces = NULL;
  HKEY hKey_Tcpip_Parameters;
  int  gotString;
  int  res;

  *outptr = NULL;

  res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0, KEY_READ,
                     &hKey_Tcpip_Parameters);
  if (res != ERROR_SUCCESS)
    return 0;

  /*
  ** Global DNS settings override adapter specific parameters when both
  ** are set. Additionally static DNS settings override DHCP-configured
  ** parameters when both are set.
  */

  /* Global DNS static parameters */
  gotString = get_REG_SZ(hKey_Tcpip_Parameters, NAMESERVER, outptr);
  if (gotString)
    goto done;

  /* Global DNS DHCP-configured parameters */
  gotString = get_REG_SZ(hKey_Tcpip_Parameters, DHCPNAMESERVER, outptr);
  if (gotString)
    goto done;

  /* Try adapter specific parameters */
  res = RegOpenKeyEx(hKey_Tcpip_Parameters, "Interfaces", 0,
                     KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                     &hKey_Interfaces);
  if (res != ERROR_SUCCESS)
  {
    hKey_Interfaces = NULL;
    goto done;
  }

  /* Adapter specific DNS static parameters */
  gotString = get_enum_REG_SZ(hKey_Interfaces, NAMESERVER, outptr);
  if (gotString)
    goto done;

  /* Adapter specific DNS DHCP-configured parameters */
  gotString = get_enum_REG_SZ(hKey_Interfaces, DHCPNAMESERVER, outptr);

done:
  if (hKey_Interfaces)
    RegCloseKey(hKey_Interfaces);

  RegCloseKey(hKey_Tcpip_Parameters);

  if (!gotString || !*outptr)
    return 0;

  return 1;
}

/*
 * get_DNS_Registry()
 *
 * Locates DNS info in the registry. When located, this returns a pointer
 * in *outptr to a newly allocated memory area holding a null-terminated
 * string with a space or comma seperated list of DNS IP addresses.
 *
 * Returns 0 and nullifies *outptr upon inability to return DNSes string.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 */
static int get_DNS_Registry(char **outptr)
{
  win_platform platform;
  int gotString = 0;

  *outptr = NULL;

  platform = ares__getplatform();

  if (platform == WIN_NT)
    gotString = get_DNS_Registry_NT(outptr);
  else if (platform == WIN_9X)
    gotString = get_DNS_Registry_9X(outptr);

  if (!gotString)
    return 0;

  return 1;
}

static void commanjoin(char** dst, const char* const src, const size_t len)
{
  char *newbuf;
  size_t newsize;

  /* 1 for terminating 0 and 2 for , and terminating 0 */
  newsize = len + (*dst ? (strlen(*dst) + 2) : 1);
  newbuf = ares_realloc(*dst, newsize);
  if (!newbuf)
    return;
  if (*dst == NULL)
    *newbuf = '\0';
  *dst = newbuf;
  if (strlen(*dst) != 0)
    strcat(*dst, ",");
  strncat(*dst, src, len);
}

/*
 * commajoin()
 *
 * RTF code.
 */
static void commajoin(char **dst, const char *src)
{
  commanjoin(dst, src, strlen(src));
}

/*
 * get_DNS_NetworkParams()
 *
 * Locates DNS info using GetNetworkParams() function from the Internet
 * Protocol Helper (IP Helper) API. When located, this returns a pointer
 * in *outptr to a newly allocated memory area holding a null-terminated
 * string with a space or comma seperated list of DNS IP addresses.
 *
 * Returns 0 and nullifies *outptr upon inability to return DNSes string.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Implementation supports Windows 98 and newer.
 *
 * Note: Ancient PSDK required in order to build a W98 target.
 */
static int get_DNS_NetworkParams(char **outptr)
{
  FIXED_INFO       *fi, *newfi;
  struct ares_addr namesrvr;
  char             *txtaddr;
  IP_ADDR_STRING   *ipAddr;
  int              res;
  DWORD            size = sizeof (*fi);

  *outptr = NULL;

  /* Verify run-time availability of GetNetworkParams() */
  if (ares_fpGetNetworkParams == ZERO_NULL)
    return 0;

  fi = ares_malloc(size);
  if (!fi)
    return 0;

  res = (*ares_fpGetNetworkParams) (fi, &size);
  if ((res != ERROR_BUFFER_OVERFLOW) && (res != ERROR_SUCCESS))
    goto done;

  newfi = ares_realloc(fi, size);
  if (!newfi)
    goto done;

  fi = newfi;
  res = (*ares_fpGetNetworkParams) (fi, &size);
  if (res != ERROR_SUCCESS)
    goto done;

  for (ipAddr = &fi->DnsServerList; ipAddr; ipAddr = ipAddr->Next)
  {
    txtaddr = &ipAddr->IpAddress.String[0];

    /* Validate converting textual address to binary format. */
    if (ares_inet_pton(AF_INET, txtaddr, &namesrvr.addrV4) == 1)
    {
      if ((namesrvr.addrV4.S_un.S_addr == INADDR_ANY) ||
          (namesrvr.addrV4.S_un.S_addr == INADDR_NONE))
        continue;
    }
    else if (ares_inet_pton(AF_INET6, txtaddr, &namesrvr.addrV6) == 1)
    {
      if (memcmp(&namesrvr.addrV6, &ares_in6addr_any,
                 sizeof(namesrvr.addrV6)) == 0)
        continue;
    }
    else
      continue;

    commajoin(outptr, txtaddr);

    if (!*outptr)
      break;
  }

done:
  if (fi)
    ares_free(fi);

  if (!*outptr)
    return 0;

  return 1;
}

static BOOL ares_IsWindowsVistaOrGreater(void)
{
  OSVERSIONINFO vinfo;
  memset(&vinfo, 0, sizeof(vinfo));
  vinfo.dwOSVersionInfoSize = sizeof(vinfo);
  if (!GetVersionEx(&vinfo) || vinfo.dwMajorVersion < 6)
    return FALSE;
  return TRUE;
}

/* A structure to hold the string form of IPv4 and IPv6 addresses so we can
 * sort them by a metric.
 */
typedef struct
{
  /* The metric we sort them by. */
  ULONG metric;

  /* Room enough for the string form of any IPv4 or IPv6 address that
   * ares_inet_ntop() will create.  Based on the existing c-ares practice.
   */
  char text[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
} Address;

/* Sort Address values \a left and \a right by metric, returning the usual
 * indicators for qsort().
 */
static int compareAddresses(const void *arg1,
                            const void *arg2)
{
  const Address * const left = arg1;
  const Address * const right = arg2;
  if(left->metric < right->metric) return -1;
  if(left->metric > right->metric) return 1;
  return 0;
}

/* There can be multiple routes to "the Internet".  And there can be different
 * DNS servers associated with each of the interfaces that offer those routes.
 * We have to assume that any DNS server can serve any request.  But, some DNS
 * servers may only respond if requested over their associated interface.  But
 * we also want to use "the preferred route to the Internet" whenever possible
 * (and not use DNS servers on a non-preferred route even by forcing request
 * to go out on the associated non-preferred interface).  i.e. We want to use
 * the DNS servers associated with the same interface that we would use to
 * make a general request to anything else.
 *
 * But, Windows won't sort the DNS servers by the metrics associated with the
 * routes and interfaces _even_ though it obviously sends IP packets based on
 * those same routes and metrics.  So, we must do it ourselves.
 *
 * So, we sort the DNS servers by the same metric values used to determine how
 * an outgoing IP packet will go, thus effectively using the DNS servers
 * associated with the interface that the DNS requests themselves will
 * travel.  This gives us optimal routing and avoids issues where DNS servers
 * won't respond to requests that don't arrive via some specific subnetwork
 * (and thus some specific interface).
 *
 * This function computes the metric we use to sort.  On the interface
 * identified by \a luid, it determines the best route to \a dest and combines
 * that route's metric with \a interfaceMetric to compute a metric for the
 * destination address on that interface.  This metric can be used as a weight
 * to sort the DNS server addresses associated with each interface (lower is
 * better).
 *
 * Note that by restricting the route search to the specific interface with
 * which the DNS servers are associated, this function asks the question "What
 * is the metric for sending IP packets to this DNS server?" which allows us
 * to sort the DNS servers correctly.
 */
static ULONG getBestRouteMetric(IF_LUID * const luid, /* Can't be const :( */
                                const SOCKADDR_INET * const dest,
                                const ULONG interfaceMetric)
{
  /* On this interface, get the best route to that destination. */
  MIB_IPFORWARD_ROW2 row;
  SOCKADDR_INET ignored;
  if(!ares_fpGetBestRoute2 ||
     ares_fpGetBestRoute2(/* The interface to use.  The index is ignored since we are
                           * passing a LUID.
                           */
                           luid, 0,
                           /* No specific source address. */
                           NULL,
                           /* Our destination address. */
                           dest,
                           /* No options. */
                           0,
                           /* The route row. */
                           &row,
                           /* The best source address, which we don't need. */
                           &ignored) != NO_ERROR
     /* If the metric is "unused" (-1) or too large for us to add the two
      * metrics, use the worst possible, thus sorting this last.
      */
     || row.Metric == (ULONG)-1
     || row.Metric > ((ULONG)-1) - interfaceMetric) {
    /* Return the worst possible metric. */
    return (ULONG)-1;
  }

  /* Return the metric value from that row, plus the interface metric.
   *
   * See
   * http://msdn.microsoft.com/en-us/library/windows/desktop/aa814494(v=vs.85).aspx
   * which describes the combination as a "sum".
   */
  return row.Metric + interfaceMetric;
}

/*
 * get_DNS_AdaptersAddresses()
 *
 * Locates DNS info using GetAdaptersAddresses() function from the Internet
 * Protocol Helper (IP Helper) API. When located, this returns a pointer
 * in *outptr to a newly allocated memory area holding a null-terminated
 * string with a space or comma seperated list of DNS IP addresses.
 *
 * Returns 0 and nullifies *outptr upon inability to return DNSes string.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Implementation supports Windows XP and newer.
 */
#define IPAA_INITIAL_BUF_SZ 15 * 1024
#define IPAA_MAX_TRIES 3
static int get_DNS_AdaptersAddresses(char **outptr)
{
  IP_ADAPTER_DNS_SERVER_ADDRESS *ipaDNSAddr;
  IP_ADAPTER_ADDRESSES *ipaa, *newipaa, *ipaaEntry;
  ULONG ReqBufsz = IPAA_INITIAL_BUF_SZ;
  ULONG Bufsz = IPAA_INITIAL_BUF_SZ;
  ULONG AddrFlags = 0;
  int trying = IPAA_MAX_TRIES;
  int res;

  /* The capacity of addresses, in elements. */
  size_t addressesSize;
  /* The number of elements in addresses. */
  size_t addressesIndex = 0;
  /* The addresses we will sort. */
  Address *addresses;

  union {
    struct sockaddr     *sa;
    struct sockaddr_in  *sa4;
    struct sockaddr_in6 *sa6;
  } namesrvr;

  *outptr = NULL;

  /* Verify run-time availability of GetAdaptersAddresses() */
  if (ares_fpGetAdaptersAddresses == ZERO_NULL)
    return 0;

  ipaa = ares_malloc(Bufsz);
  if (!ipaa)
    return 0;

  /* Start with enough room for a few DNS server addresses and we'll grow it
   * as we encounter more.
   */
  addressesSize = 4;
  addresses = (Address*)ares_malloc(sizeof(Address) * addressesSize);
  if(addresses == NULL) {
    /* We need room for at least some addresses to function. */
    ares_free(ipaa);
    return 0;
  }

  /* Usually this call suceeds with initial buffer size */
  res = (*ares_fpGetAdaptersAddresses) (AF_UNSPEC, AddrFlags, NULL,
                                        ipaa, &ReqBufsz);
  if ((res != ERROR_BUFFER_OVERFLOW) && (res != ERROR_SUCCESS))
    goto done;

  while ((res == ERROR_BUFFER_OVERFLOW) && (--trying))
  {
    if (Bufsz < ReqBufsz)
    {
      newipaa = ares_realloc(ipaa, ReqBufsz);
      if (!newipaa)
        goto done;
      Bufsz = ReqBufsz;
      ipaa = newipaa;
    }
    res = (*ares_fpGetAdaptersAddresses) (AF_UNSPEC, AddrFlags, NULL,
                                          ipaa, &ReqBufsz);
    if (res == ERROR_SUCCESS)
      break;
  }
  if (res != ERROR_SUCCESS)
    goto done;

  for (ipaaEntry = ipaa; ipaaEntry; ipaaEntry = ipaaEntry->Next)
  {
    if(ipaaEntry->OperStatus != IfOperStatusUp)
        continue;

    /* For each interface, find any associated DNS servers as IPv4 or IPv6
     * addresses.  For each found address, find the best route to that DNS
     * server address _on_ _that_ _interface_ (at this moment in time) and
     * compute the resulting total metric, just as Windows routing will do.
     * Then, sort all the addresses found by the metric.
     */
    for (ipaDNSAddr = ipaaEntry->FirstDnsServerAddress;
         ipaDNSAddr;
         ipaDNSAddr = ipaDNSAddr->Next)
    {
      namesrvr.sa = ipaDNSAddr->Address.lpSockaddr;

      if (namesrvr.sa->sa_family == AF_INET)
      {
        if ((namesrvr.sa4->sin_addr.S_un.S_addr == INADDR_ANY) ||
            (namesrvr.sa4->sin_addr.S_un.S_addr == INADDR_NONE))
          continue;

        /* Allocate room for another address, if necessary, else skip. */
        if(addressesIndex == addressesSize) {
          const size_t newSize = addressesSize + 4;
          Address * const newMem =
            (Address*)ares_realloc(addresses, sizeof(Address) * newSize);
          if(newMem == NULL) {
            continue;
          }
          addresses = newMem;
          addressesSize = newSize;
        }

        /* Vista required for Luid or Ipv4Metric */
        if (ares_IsWindowsVistaOrGreater())
        {
          /* Save the address as the next element in addresses. */
          addresses[addressesIndex].metric =
            getBestRouteMetric(&ipaaEntry->Luid,
                               (SOCKADDR_INET*)(namesrvr.sa),
                               ipaaEntry->Ipv4Metric);
        }
        else
        {
          addresses[addressesIndex].metric = -1;
        }

        if (! ares_inet_ntop(AF_INET, &namesrvr.sa4->sin_addr,
                             addresses[addressesIndex].text,
                             sizeof(addresses[0].text))) {
          continue;
        }
        ++addressesIndex;
      }
      else if (namesrvr.sa->sa_family == AF_INET6)
      {
        /* Windows apparently always reports some IPv6 DNS servers that
         * prefixed with fec0:0:0:ffff. These ususally do not point to
         * working DNS servers, so we ignore them. */
        if (strncmp(addresses[addressesIndex].text, "fec0:0:0:ffff:", 14) == 0)
          continue;
        if (memcmp(&namesrvr.sa6->sin6_addr, &ares_in6addr_any,
                   sizeof(namesrvr.sa6->sin6_addr)) == 0)
          continue;

        /* Allocate room for another address, if necessary, else skip. */
        if(addressesIndex == addressesSize) {
          const size_t newSize = addressesSize + 4;
          Address * const newMem =
            (Address*)ares_realloc(addresses, sizeof(Address) * newSize);
          if(newMem == NULL) {
            continue;
          }
          addresses = newMem;
          addressesSize = newSize;
        }

        /* Vista required for Luid or Ipv4Metric */
        if (ares_IsWindowsVistaOrGreater())
        {
          /* Save the address as the next element in addresses. */
          addresses[addressesIndex].metric =
            getBestRouteMetric(&ipaaEntry->Luid,
                               (SOCKADDR_INET*)(namesrvr.sa),
                               ipaaEntry->Ipv6Metric);
        }
        else
        {
          addresses[addressesIndex].metric = -1;
        }

        if (! ares_inet_ntop(AF_INET6, &namesrvr.sa6->sin6_addr,
                             addresses[addressesIndex].text,
                             sizeof(addresses[0].text))) {
          continue;
        }
        ++addressesIndex;
      }
      else {
        /* Skip non-IPv4/IPv6 addresses completely. */
        continue;
      }
    }
  }

  /* Sort all of the textual addresses by their metric. */
  qsort(addresses, addressesIndex, sizeof(*addresses), compareAddresses);

  /* Join them all into a single string, removing duplicates. */
  {
    size_t i;
    for(i = 0; i < addressesIndex; ++i) {
      size_t j;
      /* Look for this address text appearing previously in the results. */
      for(j = 0; j < i; ++j) {
        if(strcmp(addresses[j].text, addresses[i].text) == 0) {
          break;
        }
      }
      /* Iff we didn't emit this address already, emit it now. */
      if(j == i) {
        /* Add that to outptr (if we can). */
        commajoin(outptr, addresses[i].text);
      }
    }
  }

done:
  ares_free(addresses);

  if (ipaa)
    ares_free(ipaa);

  if (!*outptr) {
    return 0;
  }

  return 1;
}

/*
 * get_DNS_Windows()
 *
 * Locates DNS info from Windows employing most suitable methods available at
 * run-time no matter which Windows version it is. When located, this returns
 * a pointer in *outptr to a newly allocated memory area holding a string with
 * a space or comma seperated list of DNS IP addresses, null-terminated.
 *
 * Returns 0 and nullifies *outptr upon inability to return DNSes string.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Implementation supports Windows 95 and newer.
 */
static int get_DNS_Windows(char **outptr)
{
  /* Try using IP helper API GetAdaptersAddresses(). IPv4 + IPv6, also sorts
   * DNS servers by interface route metrics to try to use the best DNS server. */
  if (get_DNS_AdaptersAddresses(outptr))
    return 1;

  /* Try using IP helper API GetNetworkParams(). IPv4 only. */
  if (get_DNS_NetworkParams(outptr))
    return 1;

  /* Fall-back to registry information */
  return get_DNS_Registry(outptr);
}

static void replace_comma_by_space(char* str)
{
  /* replace ',' by ' ' to coincide with resolv.conf search parameter */
  char *p;
  for (p = str; *p != '\0'; p++)
  {
    if (*p == ',')
      *p = ' ';
  }
}

/* Search if 'suffix' is containted in the 'searchlist'. Returns true if yes,
 * otherwise false. 'searchlist' is a comma separated list of domain suffixes,
 * 'suffix' is one domain suffix, 'len' is the length of 'suffix'.
 * The search ignores case. E.g.:
 * contains_suffix("abc.def,ghi.jkl", "ghi.JKL") returns true  */
static bool contains_suffix(const char* const searchlist,
                            const char* const suffix, const size_t len)
{
  const char* beg = searchlist;
  const char* end;
  if (!*suffix)
    return true;
  for (;;)
  {
    while (*beg && (ISSPACE(*beg) || (*beg == ',')))
      ++beg;
    if (!*beg)
      return false;
    end = beg;
    while (*end && !ISSPACE(*end) && (*end != ','))
      ++end;
    if (len == (end - beg) && !strnicmp(beg, suffix, len))
      return true;
    beg = end;
  }
}

/* advances list to the next suffix within a comma separated search list.
 * len is the length of the next suffix. */
static size_t next_suffix(const char** list, const size_t advance)
{
  const char* beg = *list + advance;
  const char* end;
  while (*beg && (ISSPACE(*beg) || (*beg == ',')))
    ++beg;
  end = beg;
  while (*end && !ISSPACE(*end) && (*end != ','))
    ++end;
  *list = beg;
  return end - beg;
}

/*
 * get_SuffixList_Windows()
 *
 * Reads the "DNS Suffix Search List" from registry and writes the list items
 * whitespace separated to outptr. If the Search List is empty, the
 * "Primary Dns Suffix" is written to outptr.
 *
 * Returns 0 and nullifies *outptr upon inability to return the suffix list.
 *
 * Returns 1 and sets *outptr when returning a dynamically allocated string.
 *
 * Implementation supports Windows Server 2003 and newer
 */
static int get_SuffixList_Windows(char **outptr)
{
  HKEY hKey, hKeyEnum;
  char  keyName[256];
  DWORD keyNameBuffSize;
  DWORD keyIdx = 0;
  char *p = NULL;
  char *pp;
  size_t len = 0;

  *outptr = NULL;

  if (ares__getplatform() != WIN_NT)
    return 0;

  /* 1. Global DNS Suffix Search List */
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY, 0,
      KEY_READ, &hKey) == ERROR_SUCCESS)
  {
    if (get_REG_SZ(hKey, SEARCHLIST_KEY, outptr))
      replace_comma_by_space(*outptr);
    RegCloseKey(hKey);
    if (*outptr)
      return 1;
  }

  /* 2. Connection Specific Search List composed of:
   *  a. Primary DNS Suffix */
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_DNSCLIENT, 0,
      KEY_READ, &hKey) == ERROR_SUCCESS)
  {
    get_REG_SZ(hKey, PRIMARYDNSSUFFIX_KEY, outptr);
    RegCloseKey(hKey);
  }
  if (!*outptr)
    return 0;

  /*  b. Interface SearchList, Domain, DhcpDomain */
  if (!RegOpenKeyEx(HKEY_LOCAL_MACHINE, WIN_NS_NT_KEY "\\" INTERFACES_KEY, 0,
      KEY_READ, &hKey) == ERROR_SUCCESS)
    return 0;
  for(;;)
  {
    keyNameBuffSize = sizeof(keyName);
    if (RegEnumKeyEx(hKey, keyIdx++, keyName, &keyNameBuffSize,
        0, NULL, NULL, NULL)
        != ERROR_SUCCESS)
      break;
    if (RegOpenKeyEx(hKey, keyName, 0, KEY_QUERY_VALUE, &hKeyEnum)
        != ERROR_SUCCESS)
      continue;
    if (get_REG_SZ(hKeyEnum, SEARCHLIST_KEY, &p) ||
        get_REG_SZ(hKeyEnum, DOMAIN_KEY, &p) ||
        get_REG_SZ(hKeyEnum, DHCPDOMAIN_KEY, &p))
    {
      /* p can be comma separated (SearchList) */
      pp = p;
      while (len = next_suffix(&pp, len))
      {
        if (!contains_suffix(*outptr, pp, len))
          commanjoin(outptr, pp, len);
      }
      ares_free(p);
      p = NULL;
    }
    RegCloseKey(hKeyEnum);
  }
  RegCloseKey(hKey);
  if (*outptr)
    replace_comma_by_space(*outptr);
  return *outptr != NULL;
}

#endif

static int init_by_resolv_conf(ares_channel channel)
{
#if !defined(ANDROID) && !defined(__ANDROID__) && !defined(WATT32) && \
    !defined(CARES_USE_LIBRESOLV)
  char *line = NULL;
#endif
  int status = -1, nservers = 0, nsort = 0;
  struct server_state *servers = NULL;
  struct apattern *sortlist = NULL;

#ifdef WIN32

  if (channel->nservers > -1)  /* don't override ARES_OPT_SERVER */
     return ARES_SUCCESS;

  if (get_DNS_Windows(&line))
  {
    status = config_nameserver(&servers, &nservers, line);
    ares_free(line);
  }

  if (channel->ndomains == -1 && get_SuffixList_Windows(&line))
  {
      status = set_search(channel, line);
      ares_free(line);
  }

  if (status == ARES_SUCCESS)
    status = ARES_EOF;
  else
    /* Catch the case when all the above checks fail (which happens when there
       is no network card or the cable is unplugged) */
    status = ARES_EFILE;

#elif defined(__riscos__)

  /* Under RISC OS, name servers are listed in the
     system variable Inet$Resolvers, space separated. */

  line = getenv("Inet$Resolvers");
  status = ARES_EOF;
  if (line) {
    char *resolvers = ares_strdup(line), *pos, *space;

    if (!resolvers)
      return ARES_ENOMEM;

    pos = resolvers;
    do {
      space = strchr(pos, ' ');
      if (space)
        *space = '\0';
      status = config_nameserver(&servers, &nservers, pos);
      if (status != ARES_SUCCESS)
        break;
      pos = space + 1;
    } while (space);

    if (status == ARES_SUCCESS)
      status = ARES_EOF;

    ares_free(resolvers);
  }

#elif defined(WATT32)
  int i;

  sock_init();
  for (i = 0; def_nameservers[i]; i++)
      ;
  if (i == 0)
    return ARES_SUCCESS; /* use localhost DNS server */

  nservers = i;
  servers = ares_malloc(sizeof(struct server_state));
  if (!servers)
     return ARES_ENOMEM;
  memset(servers, 0, sizeof(struct server_state));

  for (i = 0; def_nameservers[i]; i++)
  {
    servers[i].addr.addrV4.s_addr = htonl(def_nameservers[i]);
    servers[i].addr.family = AF_INET;
    servers[i].addr.udp_port = 0;
    servers[i].addr.tcp_port = 0;
  }
  status = ARES_EOF;

#elif defined(ANDROID) || defined(__ANDROID__)
  unsigned int i;
  char propname[PROP_NAME_MAX];
  char propvalue[PROP_VALUE_MAX]="";

  for (i = 1; i <= MAX_DNS_PROPERTIES; i++) {
    snprintf(propname, sizeof(propname), "%s%u", DNS_PROP_NAME_PREFIX, i);
    if (__system_property_get(propname, propvalue) < 1) {
      status = ARES_EOF;
      break;
    }
    status = config_nameserver(&servers, &nservers, propvalue);
    if (status != ARES_SUCCESS)
      break;
    status = ARES_EOF;
  }
#elif defined(CARES_USE_LIBRESOLV)
  struct __res_state res;
  memset(&res, 0, sizeof(res));
  int result = res_ninit(&res);
  if (result == 0 && (res.options & RES_INIT)) {
    status = ARES_EOF;

    if (channel->nservers == -1) {
      union res_sockaddr_union addr[MAXNS];
      int nscount = res_getservers(&res, addr, MAXNS);
      for (int i = 0; i < nscount; ++i) {
        char str[INET6_ADDRSTRLEN];
        int config_status;
        sa_family_t family = addr[i].sin.sin_family;
        if (family == AF_INET) {
          ares_inet_ntop(family, &addr[i].sin.sin_addr, str, sizeof(str));
        } else if (family == AF_INET6) {
          ares_inet_ntop(family, &addr[i].sin6.sin6_addr, str, sizeof(str));
        } else {
          continue;
        }

        config_status = config_nameserver(&servers, &nservers, str);
        if (config_status != ARES_SUCCESS) {
          status = config_status;
          break;
        }
      }
    }
    if (channel->ndomains == -1) {
      int entries = 0;
      while ((entries < MAXDNSRCH) && res.dnsrch[entries])
        entries++;

      channel->domains = ares_malloc(entries * sizeof(char *));
      if (!channel->domains) {
        status = ARES_ENOMEM;
      } else {
        channel->ndomains = entries;
        for (int i = 0; i < channel->ndomains; ++i) {
          channel->domains[i] = ares_strdup(res.dnsrch[i]);
          if (!channel->domains[i])
            status = ARES_ENOMEM;
        }
      }
    }
    if (channel->ndots == -1)
      channel->ndots = res.ndots;
    if (channel->tries == -1)
      channel->tries = res.retry;
    if (channel->rotate == -1)
      channel->rotate = res.options & RES_ROTATE;
    if (channel->timeout == -1)
      channel->timeout = res.retrans * 1000;

    res_ndestroy(&res);
  }
#else
  {
    char *p;
    FILE *fp;
    size_t linesize;
    int error;
    int update_domains;

    /* Don't read resolv.conf and friends if we don't have to */
    if (ARES_CONFIG_CHECK(channel))
        return ARES_SUCCESS;

    /* Only update search domains if they're not already specified */
    update_domains = (channel->ndomains == -1);

    fp = fopen(PATH_RESOLV_CONF, "r");
    if (fp) {
      while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS)
      {
        if ((p = try_config(line, "domain", ';')) && update_domains)
          status = config_domain(channel, p);
        else if ((p = try_config(line, "lookup", ';')) && !channel->lookups)
          status = config_lookup(channel, p, "bind", NULL, "file");
        else if ((p = try_config(line, "search", ';')) && update_domains)
          status = set_search(channel, p);
        else if ((p = try_config(line, "nameserver", ';')) &&
                 channel->nservers == -1)
          status = config_nameserver(&servers, &nservers, p);
        else if ((p = try_config(line, "sortlist", ';')) &&
                 channel->nsort == -1)
          status = config_sortlist(&sortlist, &nsort, p);
        else if ((p = try_config(line, "options", ';')))
          status = set_options(channel, p);
        else
          status = ARES_SUCCESS;
        if (status != ARES_SUCCESS)
          break;
      }
      fclose(fp);
    }
    else {
      error = ERRNO;
      switch(error) {
      case ENOENT:
      case ESRCH:
        status = ARES_EOF;
        break;
      default:
        DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                       error, strerror(error)));
        DEBUGF(fprintf(stderr, "Error opening file: %s\n", PATH_RESOLV_CONF));
        status = ARES_EFILE;
      }
    }

    if ((status == ARES_EOF) && (!channel->lookups)) {
      /* Many systems (Solaris, Linux, BSD's) use nsswitch.conf */
      fp = fopen("/etc/nsswitch.conf", "r");
      if (fp) {
        while ((status = ares__read_line(fp, &line, &linesize)) ==
               ARES_SUCCESS)
        {
          if ((p = try_config(line, "hosts:", '\0')) && !channel->lookups)
            (void)config_lookup(channel, p, "dns", "resolve", "files");
        }
        fclose(fp);
      }
      else {
        error = ERRNO;
        switch(error) {
        case ENOENT:
        case ESRCH:
          break;
        default:
          DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                         error, strerror(error)));
          DEBUGF(fprintf(stderr, "Error opening file: %s\n",
                         "/etc/nsswitch.conf"));
        }

        /* ignore error, maybe we will get luck in next if clause */
        status = ARES_EOF;
      }
    }

    if ((status == ARES_EOF) && (!channel->lookups)) {
      /* Linux / GNU libc 2.x and possibly others have host.conf */
      fp = fopen("/etc/host.conf", "r");
      if (fp) {
        while ((status = ares__read_line(fp, &line, &linesize)) ==
               ARES_SUCCESS)
        {
          if ((p = try_config(line, "order", '\0')) && !channel->lookups)
            /* ignore errors */
            (void)config_lookup(channel, p, "bind", NULL, "hosts");
        }
        fclose(fp);
      }
      else {
        error = ERRNO;
        switch(error) {
        case ENOENT:
        case ESRCH:
          break;
        default:
          DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                         error, strerror(error)));
          DEBUGF(fprintf(stderr, "Error opening file: %s\n",
                         "/etc/host.conf"));
        }

        /* ignore error, maybe we will get luck in next if clause */
        status = ARES_EOF;
      }
    }

    if ((status == ARES_EOF) && (!channel->lookups)) {
      /* Tru64 uses /etc/svc.conf */
      fp = fopen("/etc/svc.conf", "r");
      if (fp) {
        while ((status = ares__read_line(fp, &line, &linesize)) ==
               ARES_SUCCESS)
        {
          if ((p = try_config(line, "hosts=", '\0')) && !channel->lookups)
            /* ignore errors */
            (void)config_lookup(channel, p, "bind", NULL, "local");
        }
        fclose(fp);
      }
      else {
        error = ERRNO;
        switch(error) {
        case ENOENT:
        case ESRCH:
          break;
        default:
          DEBUGF(fprintf(stderr, "fopen() failed with error: %d %s\n",
                         error, strerror(error)));
          DEBUGF(fprintf(stderr, "Error opening file: %s\n", "/etc/svc.conf"));
        }

        /* ignore error, default value will be chosen for `channel->lookups` */
        status = ARES_EOF;
      }
    }

    if(line)
      ares_free(line);
  }

#endif

  /* Handle errors. */
  if (status != ARES_EOF)
    {
      if (servers != NULL)
        ares_free(servers);
      if (sortlist != NULL)
        ares_free(sortlist);
      return status;
    }

  /* If we got any name server entries, fill them in. */
  if (servers)
    {
      channel->servers = servers;
      channel->nservers = nservers;
    }

  /* If we got any sortlist entries, fill them in. */
  if (sortlist)
    {
      channel->sortlist = sortlist;
      channel->nsort = nsort;
    }

  return ARES_SUCCESS;
}

static int init_by_defaults(ares_channel channel)
{
  char *hostname = NULL;
  int rc = ARES_SUCCESS;
#ifdef HAVE_GETHOSTNAME
  char *dot;
#endif

  if (channel->flags == -1)
    channel->flags = 0;
  if (channel->timeout == -1)
    channel->timeout = DEFAULT_TIMEOUT;
  if (channel->tries == -1)
    channel->tries = DEFAULT_TRIES;
  if (channel->ndots == -1)
    channel->ndots = 1;
  if (channel->rotate == -1)
    channel->rotate = 0;
  if (channel->udp_port == -1)
    channel->udp_port = htons(NAMESERVER_PORT);
  if (channel->tcp_port == -1)
    channel->tcp_port = htons(NAMESERVER_PORT);

  if (channel->ednspsz == -1)
    channel->ednspsz = EDNSPACKETSZ;

  if (channel->nservers == -1) {
    /* If nobody specified servers, try a local named. */
    channel->servers = ares_malloc(sizeof(struct server_state));
    if (!channel->servers) {
      rc = ARES_ENOMEM;
      goto error;
    }
    channel->servers[0].addr.family = AF_INET;
    channel->servers[0].addr.addrV4.s_addr = htonl(INADDR_LOOPBACK);
    channel->servers[0].addr.udp_port = 0;
    channel->servers[0].addr.tcp_port = 0;
    channel->nservers = 1;
  }

#if defined(USE_WINSOCK)
#define toolong(x) (x == -1) &&  (SOCKERRNO == WSAEFAULT)
#elif defined(ENAMETOOLONG)
#define toolong(x) (x == -1) && ((SOCKERRNO == ENAMETOOLONG) || \
                                 (SOCKERRNO == EINVAL))
#else
#define toolong(x) (x == -1) &&  (SOCKERRNO == EINVAL)
#endif

  if (channel->ndomains == -1) {
    /* Derive a default domain search list from the kernel hostname,
     * or set it to empty if the hostname isn't helpful.
     */
#ifndef HAVE_GETHOSTNAME
    channel->ndomains = 0; /* default to none */
#else
    GETHOSTNAME_TYPE_ARG2 lenv = 64;
    size_t len = 64;
    int res;
    channel->ndomains = 0; /* default to none */

    hostname = ares_malloc(len);
    if(!hostname) {
      rc = ARES_ENOMEM;
      goto error;
    }

    do {
      res = gethostname(hostname, lenv);

      if(toolong(res)) {
        char *p;
        len *= 2;
        lenv *= 2;
        p = ares_realloc(hostname, len);
        if(!p) {
          rc = ARES_ENOMEM;
          goto error;
        }
        hostname = p;
        continue;
      }
      else if(res) {
        rc = ARES_EBADNAME;
        goto error;
      }

    } while (res != 0);

    dot = strchr(hostname, '.');
    if (dot) {
      /* a dot was found */
      channel->domains = ares_malloc(sizeof(char *));
      if (!channel->domains) {
        rc = ARES_ENOMEM;
        goto error;
      }
      channel->domains[0] = ares_strdup(dot + 1);
      if (!channel->domains[0]) {
        rc = ARES_ENOMEM;
        goto error;
      }
      channel->ndomains = 1;
    }
#endif
  }

  if (channel->nsort == -1) {
    channel->sortlist = NULL;
    channel->nsort = 0;
  }

  if (!channel->lookups) {
    channel->lookups = ares_strdup("fb");
    if (!channel->lookups)
      rc = ARES_ENOMEM;
  }

  error:
  if(rc) {
    if(channel->servers) {
      ares_free(channel->servers);
      channel->servers = NULL;
    }

    if(channel->domains && channel->domains[0])
      ares_free(channel->domains[0]);
    if(channel->domains) {
      ares_free(channel->domains);
      channel->domains = NULL;
    }

    if(channel->lookups) {
      ares_free(channel->lookups);
      channel->lookups = NULL;
    }
  }

  if(hostname)
    ares_free(hostname);

  return rc;
}

#if !defined(WIN32) && !defined(WATT32) && \
    !defined(ANDROID) && !defined(__ANDROID__) && !defined(CARES_USE_LIBRESOLV)
static int config_domain(ares_channel channel, char *str)
{
  char *q;

  /* Set a single search domain. */
  q = str;
  while (*q && !ISSPACE(*q))
    q++;
  *q = '\0';
  return set_search(channel, str);
}

#if defined(__INTEL_COMPILER) && (__INTEL_COMPILER == 910) && \
    defined(__OPTIMIZE__) && defined(__unix__) &&  defined(__i386__)
  /* workaround icc 9.1 optimizer issue */
# define vqualifier volatile
#else
# define vqualifier
#endif

static int config_lookup(ares_channel channel, const char *str,
                         const char *bindch, const char *altbindch,
                         const char *filech)
{
  char lookups[3], *l;
  const char *vqualifier p;

  if (altbindch == NULL)
    altbindch = bindch;

  /* Set the lookup order.  Only the first letter of each work
   * is relevant, and it has to be "b" for DNS or "f" for the
   * host file.  Ignore everything else.
   */
  l = lookups;
  p = str;
  while (*p)
    {
      if ((*p == *bindch || *p == *altbindch || *p == *filech) && l < lookups + 2) {
        if (*p == *bindch || *p == *altbindch) *l++ = 'b';
        else *l++ = 'f';
      }
      while (*p && !ISSPACE(*p) && (*p != ','))
        p++;
      while (*p && (ISSPACE(*p) || (*p == ',')))
        p++;
    }
  *l = '\0';
  channel->lookups = ares_strdup(lookups);
  return (channel->lookups) ? ARES_SUCCESS : ARES_ENOMEM;
}
#endif  /* !WIN32 & !WATT32 & !ANDROID & !__ANDROID__ & !CARES_USE_LIBRESOLV */

#ifndef WATT32
static int config_nameserver(struct server_state **servers, int *nservers,
                             char *str)
{
  struct ares_addr host;
  struct server_state *newserv;
  char *p, *txtaddr;
  /* On Windows, there may be more than one nameserver specified in the same
   * registry key, so we parse input as a space or comma seperated list.
   */
  for (p = str; p;)
    {
      /* Skip whitespace and commas. */
      while (*p && (ISSPACE(*p) || (*p == ',')))
        p++;
      if (!*p)
        /* No more input, done. */
        break;

      /* Pointer to start of IPv4 or IPv6 address part. */
      txtaddr = p;

      /* Advance past this address. */
      while (*p && !ISSPACE(*p) && (*p != ','))
        p++;
      if (*p)
        /* Null terminate this address. */
        *p++ = '\0';
      else
        /* Reached end of input, done when this address is processed. */
        p = NULL;

      /* Convert textual address to binary format. */
      if (ares_inet_pton(AF_INET, txtaddr, &host.addrV4) == 1)
        host.family = AF_INET;
      else if (ares_inet_pton(AF_INET6, txtaddr, &host.addrV6) == 1)
        host.family = AF_INET6;
      else
        continue;

      /* Resize servers state array. */
      newserv = ares_realloc(*servers, (*nservers + 1) *
                             sizeof(struct server_state));
      if (!newserv)
        return ARES_ENOMEM;

      /* Store address data. */
      newserv[*nservers].addr.family = host.family;
      newserv[*nservers].addr.udp_port = 0;
      newserv[*nservers].addr.tcp_port = 0;
      if (host.family == AF_INET)
        memcpy(&newserv[*nservers].addr.addrV4, &host.addrV4,
               sizeof(host.addrV4));
      else
        memcpy(&newserv[*nservers].addr.addrV6, &host.addrV6,
               sizeof(host.addrV6));

      /* Update arguments. */
      *servers = newserv;
      *nservers += 1;
    }

  return ARES_SUCCESS;
}
#endif  /* !WATT32 */

static int config_sortlist(struct apattern **sortlist, int *nsort,
                           const char *str)
{
  struct apattern pat;
  const char *q;

  /* Add sortlist entries. */
  while (*str && *str != ';')
    {
      int bits;
      char ipbuf[16], ipbufpfx[32];
      /* Find just the IP */
      q = str;
      while (*q && *q != '/' && *q != ';' && !ISSPACE(*q))
        q++;
      memcpy(ipbuf, str, q-str);
      ipbuf[q-str] = '\0';
      /* Find the prefix */
      if (*q == '/')
        {
          const char *str2 = q+1;
          while (*q && *q != ';' && !ISSPACE(*q))
            q++;
          memcpy(ipbufpfx, str, q-str);
          ipbufpfx[q-str] = '\0';
          str = str2;
        }
      else
        ipbufpfx[0] = '\0';
      /* Lets see if it is CIDR */
      /* First we'll try IPv6 */
      if ((bits = ares_inet_net_pton(AF_INET6, ipbufpfx[0] ? ipbufpfx : ipbuf,
                                     &pat.addrV6,
                                     sizeof(pat.addrV6))) > 0)
        {
          pat.type = PATTERN_CIDR;
          pat.mask.bits = (unsigned short)bits;
          pat.family = AF_INET6;
          if (!sortlist_alloc(sortlist, nsort, &pat)) {
            ares_free(*sortlist);
            *sortlist = NULL;
            return ARES_ENOMEM;
          }
        }
      else if (ipbufpfx[0] &&
               (bits = ares_inet_net_pton(AF_INET, ipbufpfx, &pat.addrV4,
                                          sizeof(pat.addrV4))) > 0)
        {
          pat.type = PATTERN_CIDR;
          pat.mask.bits = (unsigned short)bits;
          pat.family = AF_INET;
          if (!sortlist_alloc(sortlist, nsort, &pat)) {
            ares_free(*sortlist);
            *sortlist = NULL;
            return ARES_ENOMEM;
          }
        }
      /* See if it is just a regular IP */
      else if (ip_addr(ipbuf, q-str, &pat.addrV4) == 0)
        {
          if (ipbufpfx[0])
            {
              memcpy(ipbuf, str, q-str);
              ipbuf[q-str] = '\0';
              if (ip_addr(ipbuf, q-str, &pat.mask.addr4) != 0)
                natural_mask(&pat);
            }
          else
            natural_mask(&pat);
          pat.family = AF_INET;
          pat.type = PATTERN_MASK;
          if (!sortlist_alloc(sortlist, nsort, &pat)) {
            ares_free(*sortlist);
            *sortlist = NULL;
            return ARES_ENOMEM;
          }
        }
      else
        {
          while (*q && *q != ';' && !ISSPACE(*q))
            q++;
        }
      str = q;
      while (ISSPACE(*str))
        str++;
    }

  return ARES_SUCCESS;
}

static int set_search(ares_channel channel, const char *str)
{
  int n;
  const char *p, *q;

  if(channel->ndomains != -1) {
    /* LCOV_EXCL_START: all callers check ndomains == -1 */
    /* if we already have some domains present, free them first */
    for(n=0; n < channel->ndomains; n++)
      ares_free(channel->domains[n]);
    ares_free(channel->domains);
    channel->domains = NULL;
    channel->ndomains = -1;
  } /* LCOV_EXCL_STOP */

  /* Count the domains given. */
  n = 0;
  p = str;
  while (*p)
    {
      while (*p && !ISSPACE(*p))
        p++;
      while (ISSPACE(*p))
        p++;
      n++;
    }

  if (!n)
    {
      channel->ndomains = 0;
      return ARES_SUCCESS;
    }

  channel->domains = ares_malloc(n * sizeof(char *));
  if (!channel->domains)
    return ARES_ENOMEM;

  /* Now copy the domains. */
  n = 0;
  p = str;
  while (*p)
    {
      channel->ndomains = n;
      q = p;
      while (*q && !ISSPACE(*q))
        q++;
      channel->domains[n] = ares_malloc(q - p + 1);
      if (!channel->domains[n])
        return ARES_ENOMEM;
      memcpy(channel->domains[n], p, q - p);
      channel->domains[n][q - p] = 0;
      p = q;
      while (ISSPACE(*p))
        p++;
      n++;
    }
  channel->ndomains = n;

  return ARES_SUCCESS;
}

static int set_options(ares_channel channel, const char *str)
{
  const char *p, *q, *val;

  p = str;
  while (*p)
    {
      q = p;
      while (*q && !ISSPACE(*q))
        q++;
      val = try_option(p, q, "ndots:");
      if (val && channel->ndots == -1)
        channel->ndots = aresx_sltosi(strtol(val, NULL, 10));
      val = try_option(p, q, "retrans:");
      if (val && channel->timeout == -1)
        channel->timeout = aresx_sltosi(strtol(val, NULL, 10));
      val = try_option(p, q, "retry:");
      if (val && channel->tries == -1)
        channel->tries = aresx_sltosi(strtol(val, NULL, 10));
      val = try_option(p, q, "rotate");
      if (val && channel->rotate == -1)
        channel->rotate = 1;
      p = q;
      while (ISSPACE(*p))
        p++;
    }

  return ARES_SUCCESS;
}

static const char *try_option(const char *p, const char *q, const char *opt)
{
  size_t len = strlen(opt);
  return ((size_t)(q - p) >= len && !strncmp(p, opt, len)) ? &p[len] : NULL;
}

#if !defined(WIN32) && !defined(WATT32) && \
    !defined(ANDROID) && !defined(__ANDROID__) && !defined(CARES_USE_LIBRESOLV)
static char *try_config(char *s, const char *opt, char scc)
{
  size_t len;
  char *p;
  char *q;

  if (!s || !opt)
    /* no line or no option */
    return NULL;  /* LCOV_EXCL_LINE */

  /* Hash '#' character is always used as primary comment char, additionally
     a not-NUL secondary comment char will be considered when specified. */

  /* trim line comment */
  p = s;
  if(scc)
    while (*p && (*p != '#') && (*p != scc))
      p++;
  else
    while (*p && (*p != '#'))
      p++;
  *p = '\0';

  /* trim trailing whitespace */
  q = p - 1;
  while ((q >= s) && ISSPACE(*q))
    q--;
  *++q = '\0';

  /* skip leading whitespace */
  p = s;
  while (*p && ISSPACE(*p))
    p++;

  if (!*p)
    /* empty line */
    return NULL;

  if ((len = strlen(opt)) == 0)
    /* empty option */
    return NULL;  /* LCOV_EXCL_LINE */

  if (strncmp(p, opt, len) != 0)
    /* line and option do not match */
    return NULL;

  /* skip over given option name */
  p += len;

  if (!*p)
    /* no option value */
    return NULL;  /* LCOV_EXCL_LINE */

  if ((opt[len-1] != ':') && (opt[len-1] != '=') && !ISSPACE(*p))
    /* whitespace between option name and value is mandatory
       for given option names which do not end with ':' or '=' */
    return NULL;

  /* skip over whitespace */
  while (*p && ISSPACE(*p))
    p++;

  if (!*p)
    /* no option value */
    return NULL;

  /* return pointer to option value */
  return p;
}
#endif  /* !WIN32 & !WATT32 & !ANDROID & !__ANDROID__ */

static int ip_addr(const char *ipbuf, ares_ssize_t len, struct in_addr *addr)
{

  /* Four octets and three periods yields at most 15 characters. */
  if (len > 15)
    return -1;

  addr->s_addr = inet_addr(ipbuf);
  if (addr->s_addr == INADDR_NONE && strcmp(ipbuf, "255.255.255.255") != 0)
    return -1;
  return 0;
}

static void natural_mask(struct apattern *pat)
{
  struct in_addr addr;

  /* Store a host-byte-order copy of pat in a struct in_addr.  Icky,
   * but portable.
   */
  addr.s_addr = ntohl(pat->addrV4.s_addr);

  /* This is out of date in the CIDR world, but some people might
   * still rely on it.
   */
  if (IN_CLASSA(addr.s_addr))
    pat->mask.addr4.s_addr = htonl(IN_CLASSA_NET);
  else if (IN_CLASSB(addr.s_addr))
    pat->mask.addr4.s_addr = htonl(IN_CLASSB_NET);
  else
    pat->mask.addr4.s_addr = htonl(IN_CLASSC_NET);
}

static int sortlist_alloc(struct apattern **sortlist, int *nsort,
                          struct apattern *pat)
{
  struct apattern *newsort;
  newsort = ares_realloc(*sortlist, (*nsort + 1) * sizeof(struct apattern));
  if (!newsort)
    return 0;
  newsort[*nsort] = *pat;
  *sortlist = newsort;
  (*nsort)++;
  return 1;
}

/* initialize an rc4 key. If possible a cryptographically secure random key
   is generated using a suitable function (for example win32's RtlGenRandom as
   described in
   http://blogs.msdn.com/michael_howard/archive/2005/01/14/353379.aspx
   otherwise the code defaults to cross-platform albeit less secure mechanism
   using rand
*/
static void randomize_key(unsigned char* key,int key_data_len)
{
  int randomized = 0;
  int counter=0;
#ifdef WIN32
  BOOLEAN res;
  if (ares_fpSystemFunction036)
    {
      res = (*ares_fpSystemFunction036) (key, key_data_len);
      if (res)
        randomized = 1;
    }
#else /* !WIN32 */
#ifdef RANDOM_FILE
  FILE *f = fopen(RANDOM_FILE, "rb");
  if(f) {
    counter = aresx_uztosi(fread(key, 1, key_data_len, f));
    fclose(f);
  }
#endif
#endif /* WIN32 */

  if (!randomized) {
    for (;counter<key_data_len;counter++)
      key[counter]=(unsigned char)(rand() % 256);  /* LCOV_EXCL_LINE */
  }
}

static int init_id_key(rc4_key* key,int key_data_len)
{
  unsigned char index1;
  unsigned char index2;
  unsigned char* state;
  short counter;
  unsigned char *key_data_ptr = 0;

  key_data_ptr = ares_malloc(key_data_len);
  if (!key_data_ptr)
    return ARES_ENOMEM;
  memset(key_data_ptr, 0, key_data_len);

  state = &key->state[0];
  for(counter = 0; counter < 256; counter++)
    /* unnecessary AND but it keeps some compilers happier */
    state[counter] = (unsigned char)(counter & 0xff);
  randomize_key(key->state,key_data_len);
  key->x = 0;
  key->y = 0;
  index1 = 0;
  index2 = 0;
  for(counter = 0; counter < 256; counter++)
  {
    index2 = (unsigned char)((key_data_ptr[index1] + state[counter] +
                              index2) % 256);
    ARES_SWAP_BYTE(&state[counter], &state[index2]);

    index1 = (unsigned char)((index1 + 1) % key_data_len);
  }
  ares_free(key_data_ptr);
  return ARES_SUCCESS;
}

void ares_set_local_ip4(ares_channel channel, unsigned int local_ip)
{
  channel->local_ip4 = local_ip;
}

/* local_ip6 should be 16 bytes in length */
void ares_set_local_ip6(ares_channel channel,
                        const unsigned char* local_ip6)
{
  memcpy(&channel->local_ip6, local_ip6, sizeof(channel->local_ip6));
}

/* local_dev_name should be null terminated. */
void ares_set_local_dev(ares_channel channel,
                        const char* local_dev_name)
{
  strncpy(channel->local_dev_name, local_dev_name,
          sizeof(channel->local_dev_name));
  channel->local_dev_name[sizeof(channel->local_dev_name) - 1] = 0;
}


void ares_set_socket_callback(ares_channel channel,
                              ares_sock_create_callback cb,
                              void *data)
{
  channel->sock_create_cb = cb;
  channel->sock_create_cb_data = data;
}

void ares_set_socket_configure_callback(ares_channel channel,
                                        ares_sock_config_callback cb,
                                        void *data)
{
  channel->sock_config_cb = cb;
  channel->sock_config_cb_data = data;
}

void ares_set_socket_functions(ares_channel channel,
                               const struct ares_socket_functions * funcs,
                               void *data)
{
  channel->sock_funcs = funcs;
  channel->sock_func_cb_data = data;
}

int ares_set_sortlist(ares_channel channel, const char *sortstr)
{
  int nsort = 0;
  struct apattern *sortlist = NULL;
  int status;

  if (!channel)
    return ARES_ENODATA;

  status = config_sortlist(&sortlist, &nsort, sortstr);
  if (status == ARES_SUCCESS && sortlist) {
    if (channel->sortlist)
      ares_free(channel->sortlist);
    channel->sortlist = sortlist;
    channel->nsort = nsort;
  }
  return status;
}

void ares__init_servers_state(ares_channel channel)
{
  struct server_state *server;
  int i;

  for (i = 0; i < channel->nservers; i++)
    {
      server = &channel->servers[i];
      server->udp_socket = ARES_SOCKET_BAD;
      server->tcp_socket = ARES_SOCKET_BAD;
      server->tcp_connection_generation = ++channel->tcp_connection_generation;
      server->tcp_lenbuf_pos = 0;
      server->tcp_buffer_pos = 0;
      server->tcp_buffer = NULL;
      server->tcp_length = 0;
      server->qhead = NULL;
      server->qtail = NULL;
      ares__init_list_head(&server->queries_to_server);
      server->channel = channel;
      server->is_broken = 0;
    }
}

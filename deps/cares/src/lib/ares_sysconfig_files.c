/* MIT License
 *
 * Copyright (c) 1998 Massachusetts Institute of Technology
 * Copyright (c) 2007 Daniel Stenberg
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

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

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

#if defined(ANDROID) || defined(__ANDROID__)
#  include <sys/system_properties.h>
#  include "ares_android.h"
/* From the Bionic sources */
#  define DNS_PROP_NAME_PREFIX "net.dns"
#  define MAX_DNS_PROPERTIES   8
#endif

#if defined(CARES_USE_LIBRESOLV)
#  include <resolv.h>
#endif

#if defined(USE_WINSOCK) && defined(HAVE_IPHLPAPI_H)
#  include <iphlpapi.h>
#endif

#include "ares.h"
#include "ares_inet_net_pton.h"
#include "ares_platform.h"
#include "ares_private.h"

static int ip_addr(const char *ipbuf, ares_ssize_t len, struct in_addr *addr)
{
  /* Four octets and three periods yields at most 15 characters. */
  if (len > 15) {
    return -1;
  }

  if (ares_inet_pton(AF_INET, ipbuf, addr) < 1) {
    return -1;
  }

  return 0;
}

static void natural_mask(struct apattern *pat)
{
  struct in_addr addr;

  /* Store a host-byte-order copy of pat in a struct in_addr.  Icky,
   * but portable.
   */
  addr.s_addr = ntohl(pat->addr.addr4.s_addr);

  /* This is out of date in the CIDR world, but some people might
   * still rely on it.
   */
  if (IN_CLASSA(addr.s_addr)) {
    pat->mask.addr4.s_addr = htonl(IN_CLASSA_NET);
  } else if (IN_CLASSB(addr.s_addr)) {
    pat->mask.addr4.s_addr = htonl(IN_CLASSB_NET);
  } else {
    pat->mask.addr4.s_addr = htonl(IN_CLASSC_NET);
  }
}

static ares_bool_t sortlist_alloc(struct apattern **sortlist, size_t *nsort,
                                  const struct apattern *pat)
{
  struct apattern *newsort;
  newsort = ares_realloc(*sortlist, (*nsort + 1) * sizeof(struct apattern));
  if (!newsort) {
    return ARES_FALSE;
  }
  newsort[*nsort] = *pat;
  *sortlist       = newsort;
  (*nsort)++;
  return ARES_TRUE;
}

ares_status_t ares__parse_sortlist(struct apattern **sortlist, size_t *nsort,
                                   const char *str)
{
  struct apattern pat;
  const char     *q;

  if (*sortlist != NULL) {
    ares_free(*sortlist);
  }

  *sortlist = NULL;
  *nsort    = 0;

  /* Add sortlist entries. */
  while (*str && *str != ';') {
    int    bits;
    char   ipbuf[17];
    char   ipbufpfx[32];
    size_t len;

    /* Find just the IP */
    q = str;
    while (*q && *q != '/' && *q != ';' && !ISSPACE(*q)) {
      q++;
    }

    len = (size_t)(q - str);
    if (len >= sizeof(ipbuf) - 1) {
      ares_free(*sortlist);
      *sortlist = NULL;
      return ARES_EBADSTR;
    }
    memcpy(ipbuf, str, len);
    ipbuf[len] = '\0';

    /* Find the prefix */
    if (*q == '/') {
      const char *str2 = q + 1;
      while (*q && *q != ';' && !ISSPACE(*q)) {
        q++;
      }
      if (q - str >= 32) {
        ares_free(*sortlist);
        *sortlist = NULL;
        return ARES_EBADSTR;
      }
      memcpy(ipbufpfx, str, (size_t)(q - str));
      ipbufpfx[q - str] = '\0';
      str               = str2;
    } else {
      ipbufpfx[0] = '\0';
    }
    /* Lets see if it is CIDR */
    /* First we'll try IPv6 */
    if ((bits = ares_inet_net_pton(AF_INET6, ipbufpfx[0] ? ipbufpfx : ipbuf,
                                   &pat.addr.addr6, sizeof(pat.addr.addr6))) >
        0) {
      pat.type      = PATTERN_CIDR;
      pat.mask.bits = (unsigned short)bits;
      pat.family    = AF_INET6;
      if (!sortlist_alloc(sortlist, nsort, &pat)) {
        ares_free(*sortlist);
        *sortlist = NULL;
        return ARES_ENOMEM;
      }
    } else if (ipbufpfx[0] &&
               (bits = ares_inet_net_pton(AF_INET, ipbufpfx, &pat.addr.addr4,
                                          sizeof(pat.addr.addr4))) > 0) {
      pat.type      = PATTERN_CIDR;
      pat.mask.bits = (unsigned short)bits;
      pat.family    = AF_INET;
      if (!sortlist_alloc(sortlist, nsort, &pat)) {
        ares_free(*sortlist);
        *sortlist = NULL;
        return ARES_ENOMEM;
      }
    }
    /* See if it is just a regular IP */
    else if (ip_addr(ipbuf, q - str, &pat.addr.addr4) == 0) {
      if (ipbufpfx[0]) {
        len = (size_t)(q - str);
        if (len >= sizeof(ipbuf) - 1) {
          ares_free(*sortlist);
          *sortlist = NULL;
          return ARES_EBADSTR;
        }
        memcpy(ipbuf, str, len);
        ipbuf[len] = '\0';

        if (ip_addr(ipbuf, q - str, &pat.mask.addr4) != 0) {
          natural_mask(&pat);
        }
      } else {
        natural_mask(&pat);
      }
      pat.family = AF_INET;
      pat.type   = PATTERN_MASK;
      if (!sortlist_alloc(sortlist, nsort, &pat)) {
        ares_free(*sortlist);
        *sortlist = NULL;
        return ARES_ENOMEM;
      }
    } else {
      while (*q && *q != ';' && !ISSPACE(*q)) {
        q++;
      }
    }
    str = q;
    while (ISSPACE(*str)) {
      str++;
    }
  }

  return ARES_SUCCESS;
}

static ares_status_t config_search(ares_sysconfig_t *sysconfig, const char *str)
{
  if (sysconfig->domains && sysconfig->ndomains > 0) {
    /* if we already have some domains present, free them first */
    ares__strsplit_free(sysconfig->domains, sysconfig->ndomains);
    sysconfig->domains  = NULL;
    sysconfig->ndomains = 0;
  }

  sysconfig->domains = ares__strsplit(str, ", ", &sysconfig->ndomains);
  if (sysconfig->domains == NULL) {
    return ARES_ENOMEM;
  }

  return ARES_SUCCESS;
}

static ares_status_t config_domain(ares_sysconfig_t *sysconfig, char *str)
{
  char *q;

  /* Set a single search domain. */
  q = str;
  while (*q && !ISSPACE(*q)) {
    q++;
  }
  *q = '\0';

  return config_search(sysconfig, str);
}

static ares_status_t config_lookup(ares_sysconfig_t *sysconfig, const char *str,
                                   const char *bindch, const char *altbindch,
                                   const char *filech)
{
  char        lookups[3];
  char       *l;
  const char *p;
  ares_bool_t found;

  if (altbindch == NULL) {
    altbindch = bindch;
  }

  /* Set the lookup order.  Only the first letter of each work
   * is relevant, and it has to be "b" for DNS or "f" for the
   * host file.  Ignore everything else.
   */
  l     = lookups;
  p     = str;
  found = ARES_FALSE;
  while (*p) {
    if ((*p == *bindch || *p == *altbindch || *p == *filech) &&
        l < lookups + 2) {
      if (*p == *bindch || *p == *altbindch) {
        *l++ = 'b';
      } else {
        *l++ = 'f';
      }
      found = ARES_TRUE;
    }
    while (*p && !ISSPACE(*p) && (*p != ',')) {
      p++;
    }
    while (*p && (ISSPACE(*p) || (*p == ','))) {
      p++;
    }
  }
  if (!found) {
    return ARES_ENOTINITIALIZED;
  }
  *l = '\0';

  ares_free(sysconfig->lookups);
  sysconfig->lookups = ares_strdup(lookups);
  if (sysconfig->lookups == NULL) {
    return ARES_ENOMEM;
  }
  return ARES_SUCCESS;
}

static const char *try_option(const char *p, const char *q, const char *opt)
{
  size_t len = ares_strlen(opt);
  return ((size_t)(q - p) >= len && !strncmp(p, opt, len)) ? &p[len] : NULL;
}

static ares_status_t set_options(ares_sysconfig_t *sysconfig, const char *str)
{
  const char *p;
  const char *q;
  const char *val;

  if (str == NULL) {
    return ARES_SUCCESS;
  }

  p = str;
  while (*p) {
    q = p;
    while (*q && !ISSPACE(*q)) {
      q++;
    }
    val = try_option(p, q, "ndots:");
    if (val) {
      sysconfig->ndots = strtoul(val, NULL, 10);
    }

    // Outdated option.
    val = try_option(p, q, "retrans:");
    if (val) {
      sysconfig->timeout_ms = strtoul(val, NULL, 10);
    }

    val = try_option(p, q, "timeout:");
    if (val) {
      sysconfig->timeout_ms = strtoul(val, NULL, 10) * 1000;
    }

    // Outdated option.
    val = try_option(p, q, "retry:");
    if (val) {
      sysconfig->tries = strtoul(val, NULL, 10);
    }

    val = try_option(p, q, "attempts:");
    if (val) {
      sysconfig->tries = strtoul(val, NULL, 10);
    }

    val = try_option(p, q, "rotate");
    if (val) {
      sysconfig->rotate = ARES_TRUE;
    }

    p = q;
    while (ISSPACE(*p)) {
      p++;
    }
  }

  return ARES_SUCCESS;
}

static char *try_config(char *s, const char *opt, char scc)
{
  size_t len;
  char  *p;
  char  *q;

  if (!s || !opt) {
    /* no line or no option */
    return NULL; /* LCOV_EXCL_LINE */
  }

  /* Hash '#' character is always used as primary comment char, additionally
     a not-NUL secondary comment char will be considered when specified. */

  /* trim line comment */
  p = s;
  if (scc) {
    while (*p && (*p != '#') && (*p != scc)) {
      p++;
    }
  } else {
    while (*p && (*p != '#')) {
      p++;
    }
  }
  *p = '\0';

  /* trim trailing whitespace */
  q = p - 1;
  while ((q >= s) && ISSPACE(*q)) {
    q--;
  }
  *++q = '\0';

  /* skip leading whitespace */
  p = s;
  while (*p && ISSPACE(*p)) {
    p++;
  }

  if (!*p) {
    /* empty line */
    return NULL;
  }

  if ((len = ares_strlen(opt)) == 0) {
    /* empty option */
    return NULL; /* LCOV_EXCL_LINE */
  }

  if (strncmp(p, opt, len) != 0) {
    /* line and option do not match */
    return NULL;
  }

  /* skip over given option name */
  p += len;

  if (!*p) {
    /* no option value */
    return NULL; /* LCOV_EXCL_LINE */
  }

  if ((opt[len - 1] != ':') && (opt[len - 1] != '=') && !ISSPACE(*p)) {
    /* whitespace between option name and value is mandatory
       for given option names which do not end with ':' or '=' */
    return NULL;
  }

  /* skip over whitespace */
  while (*p && ISSPACE(*p)) {
    p++;
  }

  if (!*p) {
    /* no option value */
    return NULL;
  }

  /* return pointer to option value */
  return p;
}

ares_status_t ares__init_by_environment(ares_sysconfig_t *sysconfig)
{
  const char   *localdomain;
  const char   *res_options;
  ares_status_t status;

  localdomain = getenv("LOCALDOMAIN");
  if (localdomain) {
    char *temp = ares_strdup(localdomain);
    if (temp == NULL) {
      return ARES_ENOMEM;
    }
    status = config_domain(sysconfig, temp);
    ares_free(temp);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  res_options = getenv("RES_OPTIONS");
  if (res_options) {
    status = set_options(sysconfig, res_options);
    if (status != ARES_SUCCESS) {
      return status;
    }
  }

  return ARES_SUCCESS;
}

ares_status_t ares__init_sysconfig_files(const ares_channel_t *channel,
                                         ares_sysconfig_t     *sysconfig)
{
  char         *p;
  FILE         *fp       = NULL;
  char         *line     = NULL;
  size_t        linesize = 0;
  int           error;
  const char   *resolvconf_path;
  ares_status_t status = ARES_SUCCESS;

  /* Support path for resolvconf filename set by ares_init_options */
  if (channel->resolvconf_path) {
    resolvconf_path = channel->resolvconf_path;
  } else {
    resolvconf_path = PATH_RESOLV_CONF;
  }

  fp = fopen(resolvconf_path, "r");
  if (fp) {
    while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
      if ((p = try_config(line, "domain", ';'))) {
        status = config_domain(sysconfig, p);
      } else if ((p = try_config(line, "lookup", ';'))) {
        status = config_lookup(sysconfig, p, "bind", NULL, "file");
      } else if ((p = try_config(line, "search", ';'))) {
        status = config_search(sysconfig, p);
      } else if ((p = try_config(line, "nameserver", ';'))) {
        status = ares__sconfig_append_fromstr(&sysconfig->sconfig, p);
      } else if ((p = try_config(line, "sortlist", ';'))) {
        status =
          ares__parse_sortlist(&sysconfig->sortlist, &sysconfig->nsortlist, p);
      } else if ((p = try_config(line, "options", ';'))) {
        status = set_options(sysconfig, p);
      } else {
        status = ARES_SUCCESS;
      }
      if (status != ARES_SUCCESS) {
        fclose(fp);
        goto done;
      }
    }
    fclose(fp);

    if (status != ARES_EOF) {
      goto done;
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
        DEBUGF(fprintf(stderr, "Error opening file: %s\n", PATH_RESOLV_CONF));
        status = ARES_EFILE;
        goto done;
    }
  }


  /* Many systems (Solaris, Linux, BSD's) use nsswitch.conf */
  fp = fopen("/etc/nsswitch.conf", "r");
  if (fp) {
    while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
      if ((p = try_config(line, "hosts:", '\0'))) {
        (void)config_lookup(sysconfig, p, "dns", "resolve", "files");
      }
    }
    fclose(fp);
    if (status != ARES_EOF) {
      goto done;
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
        DEBUGF(
          fprintf(stderr, "Error opening file: %s\n", "/etc/nsswitch.conf"));
        break;
    }
    /* ignore error, maybe we will get luck in next if clause */
  }


  /* Linux / GNU libc 2.x and possibly others have host.conf */
  fp = fopen("/etc/host.conf", "r");
  if (fp) {
    while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
      if ((p = try_config(line, "order", '\0'))) {
        /* ignore errors */
        (void)config_lookup(sysconfig, p, "bind", NULL, "hosts");
      }
    }
    fclose(fp);
    if (status != ARES_EOF) {
      goto done;
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
        DEBUGF(fprintf(stderr, "Error opening file: %s\n", "/etc/host.conf"));
        break;
    }

    /* ignore error, maybe we will get luck in next if clause */
  }


  /* Tru64 uses /etc/svc.conf */
  fp = fopen("/etc/svc.conf", "r");
  if (fp) {
    while ((status = ares__read_line(fp, &line, &linesize)) == ARES_SUCCESS) {
      if ((p = try_config(line, "hosts=", '\0'))) {
        /* ignore errors */
        (void)config_lookup(sysconfig, p, "bind", NULL, "local");
      }
    }
    fclose(fp);
    if (status != ARES_EOF) {
      goto done;
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
        DEBUGF(fprintf(stderr, "Error opening file: %s\n", "/etc/svc.conf"));
        break;
    }
    /* ignore error */
  }

  status = ARES_SUCCESS;

done:
  ares_free(line);

  return status;
}

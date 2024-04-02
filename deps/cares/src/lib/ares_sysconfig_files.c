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

static unsigned char ip_natural_mask(const struct ares_addr *addr)
{
  const unsigned char *ptr = NULL;
  /* This is an odd one.  If a raw ipv4 address is specified, then we take
   * what is called a natural mask, which means we look at the first octet
   * of the ip address and for values 0-127 we assume it is a class A (/8),
   * for values 128-191 we assume it is a class B (/16), and for 192-223
   * we assume it is a class C (/24).  223-239 is Class D which and 240-255 is
   * Class E, however, there is no pre-defined mask for this, so we'll use
   * /24 as well as that's what the old code did.
   *
   * For IPv6, we'll use /64.
   */

  if (addr->family == AF_INET6) {
    return 64;
  }

  ptr = (const unsigned char *)&addr->addr.addr4;
  if (*ptr < 128) {
    return 8;
  }

  if (*ptr < 192) {
    return 16;
  }

  return 24;
}

static ares_bool_t sortlist_append(struct apattern **sortlist, size_t *nsort,
                                   const struct apattern *pat)
{
  struct apattern *newsort;

  newsort = ares_realloc(*sortlist, (*nsort + 1) * sizeof(*newsort));
  if (newsort == NULL) {
    return ARES_FALSE;
  }

  *sortlist = newsort;

  memcpy(&(*sortlist)[*nsort], pat, sizeof(**sortlist));
  (*nsort)++;

  return ARES_TRUE;
}

static ares_status_t parse_sort(ares__buf_t *buf, struct apattern *pat)
{
  ares_status_t       status;
  const unsigned char ip_charset[]             = "ABCDEFabcdef0123456789.:";
  char                ipaddr[INET6_ADDRSTRLEN] = "";
  size_t              addrlen;

  memset(pat, 0, sizeof(*pat));

  /* Consume any leading whitespace */
  ares__buf_consume_whitespace(buf, ARES_TRUE);

  /* If no length, just ignore, return ENOTFOUND as an indicator */
  if (ares__buf_len(buf) == 0) {
    return ARES_ENOTFOUND;
  }

  ares__buf_tag(buf);

  /* Consume ip address */
  if (ares__buf_consume_charset(buf, ip_charset, sizeof(ip_charset)) == 0) {
    return ARES_EBADSTR;
  }

  /* Fetch ip address */
  status = ares__buf_tag_fetch_string(buf, ipaddr, sizeof(ipaddr));
  if (status != ARES_SUCCESS) {
    return status;
  }

  /* Parse it to make sure its valid */
  pat->addr.family = AF_UNSPEC;
  if (ares_dns_pton(ipaddr, &pat->addr, &addrlen) == NULL) {
    return ARES_EBADSTR;
  }

  /* See if there is a subnet mask */
  if (ares__buf_begins_with(buf, (const unsigned char *)"/", 1)) {
    char                maskstr[16];
    const unsigned char ipv4_charset[] = "0123456789.";


    /* Consume / */
    ares__buf_consume(buf, 1);

    ares__buf_tag(buf);

    /* Consume mask */
    if (ares__buf_consume_charset(buf, ipv4_charset, sizeof(ipv4_charset)) ==
        0) {
      return ARES_EBADSTR;
    }

    /* Fetch mask */
    status = ares__buf_tag_fetch_string(buf, maskstr, sizeof(maskstr));
    if (status != ARES_SUCCESS) {
      return status;
    }

    if (ares_str_isnum(maskstr)) {
      /* Numeric mask */
      int mask = atoi(maskstr);
      if (mask < 0 || mask > 128) {
        return ARES_EBADSTR;
      }
      if (pat->addr.family == AF_INET && mask > 32) {
        return ARES_EBADSTR;
      }
      pat->mask = (unsigned char)mask;
    } else {
      /* Ipv4 subnet style mask */
      struct ares_addr     maskaddr;
      const unsigned char *ptr;

      memset(&maskaddr, 0, sizeof(maskaddr));
      maskaddr.family = AF_INET;
      if (ares_dns_pton(maskstr, &maskaddr, &addrlen) == NULL) {
        return ARES_EBADSTR;
      }
      ptr       = (const unsigned char *)&maskaddr.addr.addr4;
      pat->mask = (unsigned char)(ares__count_bits_u8(ptr[0]) +
                                  ares__count_bits_u8(ptr[1]) +
                                  ares__count_bits_u8(ptr[2]) +
                                  ares__count_bits_u8(ptr[3]));
    }
  } else {
    pat->mask = ip_natural_mask(&pat->addr);
  }

  /* Consume any trailing whitespace */
  ares__buf_consume_whitespace(buf, ARES_TRUE);

  /* If we have any trailing bytes other than whitespace, its a parse failure */
  if (ares__buf_len(buf) != 0) {
    return ARES_EBADSTR;
  }

  return ARES_SUCCESS;
}

ares_status_t ares__parse_sortlist(struct apattern **sortlist, size_t *nsort,
                                   const char *str)
{
  ares__buf_t        *buf    = NULL;
  ares__llist_t      *list   = NULL;
  ares_status_t       status = ARES_SUCCESS;
  ares__llist_node_t *node   = NULL;

  if (sortlist == NULL || nsort == NULL || str == NULL) {
    return ARES_EFORMERR;
  }

  if (*sortlist != NULL) {
    ares_free(*sortlist);
  }

  *sortlist = NULL;
  *nsort    = 0;

  buf = ares__buf_create_const((const unsigned char *)str, ares_strlen(str));
  if (buf == NULL) {
    status = ARES_ENOMEM;
    goto done;
  }

  /* Split on space or semicolon */
  status = ares__buf_split(buf, (const unsigned char *)" ;", 2,
                           ARES_BUF_SPLIT_NONE, &list);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  for (node = ares__llist_node_first(list); node != NULL;
       node = ares__llist_node_next(node)) {
    ares__buf_t    *entry = ares__llist_node_val(node);

    struct apattern pat;

    status = parse_sort(entry, &pat);
    if (status != ARES_SUCCESS && status != ARES_ENOTFOUND) {
      goto done;
    }

    if (status != ARES_SUCCESS) {
      continue;
    }

    if (!sortlist_append(sortlist, nsort, &pat)) {
      status = ARES_ENOMEM;
      goto done;
    }
  }

  status = ARES_SUCCESS;

done:
  ares__buf_destroy(buf);
  ares__llist_destroy(list);

  if (status != ARES_SUCCESS) {
    ares_free(*sortlist);
    *sortlist = NULL;
    *nsort    = 0;
  }

  return status;
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
        status =
          ares__sconfig_append_fromstr(&sysconfig->sconfig, p, ARES_TRUE);
      } else if ((p = try_config(line, "sortlist", ';'))) {
        /* Ignore all failures except ENOMEM.  If the sysadmin set a bad
         * sortlist, just ignore the sortlist, don't cause an inoperable
         * channel */
        status =
          ares__parse_sortlist(&sysconfig->sortlist, &sysconfig->nsortlist, p);
        if (status != ARES_ENOMEM) {
          status = ARES_SUCCESS;
        }
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

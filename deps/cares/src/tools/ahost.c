/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
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

#if !defined(WIN32) || defined(WATT32)
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "ares.h"
#include "ares_dns.h"
#include "ares_getopt.h"
#include "ares_ipv6.h"
#include "ares_nowarn.h"

#ifndef HAVE_STRDUP
#  include "ares_strdup.h"
#  define strdup(ptr) ares_strdup(ptr)
#endif

#ifndef HAVE_STRCASECMP
#  include "ares_strcasecmp.h"
#  define strcasecmp(p1,p2) ares_strcasecmp(p1,p2)
#endif

#ifndef HAVE_STRNCASECMP
#  include "ares_strcasecmp.h"
#  define strncasecmp(p1,p2,n) ares_strncasecmp(p1,p2,n)
#endif

static void callback(void *arg, int status, int timeouts, struct hostent *host);
static void usage(void);
static void print_help_info_ahost(void);

int main(int argc, char **argv)
{
  struct ares_options options;
  int optmask = 0;
  ares_channel channel;
  int status, nfds, c, addr_family = AF_INET;
  fd_set read_fds, write_fds;
  struct timeval *tvp, tv;
  struct in_addr addr4;
  struct ares_in6_addr addr6;

#ifdef USE_WINSOCK
  WORD wVersionRequested = MAKEWORD(USE_WINSOCK,USE_WINSOCK);
  WSADATA wsaData;
  WSAStartup(wVersionRequested, &wsaData);
#endif

  memset(&options, 0, sizeof(options));

  status = ares_library_init(ARES_LIB_INIT_ALL);
  if (status != ARES_SUCCESS)
    {
      fprintf(stderr, "ares_library_init: %s\n", ares_strerror(status));
      return 1;
    }

  while ((c = ares_getopt(argc,argv,"dt:h?s:")) != -1)
    {
      switch (c)
        {
        case 'd':
#ifdef WATT32
          dbug_init();
#endif
          break;
        case 's':
          optmask |= ARES_OPT_DOMAINS;
          options.ndomains++;
          options.domains = (char **)realloc(options.domains,
                                             options.ndomains * sizeof(char *));
          options.domains[options.ndomains - 1] = strdup(optarg);
          break;
        case 't':
          if (!strcasecmp(optarg,"a"))
            addr_family = AF_INET;
          else if (!strcasecmp(optarg,"aaaa"))
            addr_family = AF_INET6;
          else if (!strcasecmp(optarg,"u"))
            addr_family = AF_UNSPEC;
          else
            usage();
          break;
        case 'h':
          print_help_info_ahost();
          break;
        case '?':
          print_help_info_ahost();
          break;
        default:
          usage();
          break;
        }
    }

  argc -= optind;
  argv += optind;
  if (argc < 1)
    usage();

  status = ares_init_options(&channel, &options, optmask);
  if (status != ARES_SUCCESS)
    {
      fprintf(stderr, "ares_init: %s\n", ares_strerror(status));
      return 1;
    }

  /* Initiate the queries, one per command-line argument. */
  for ( ; *argv; argv++)
    {
      if (ares_inet_pton(AF_INET, *argv, &addr4) == 1)
        {
          ares_gethostbyaddr(channel, &addr4, sizeof(addr4), AF_INET, callback,
                             *argv);
        }
      else if (ares_inet_pton(AF_INET6, *argv, &addr6) == 1)
        {
          ares_gethostbyaddr(channel, &addr6, sizeof(addr6), AF_INET6, callback,
                             *argv);
        }
      else
        {
          ares_gethostbyname(channel, *argv, addr_family, callback, *argv);
        }
    }

  /* Wait for all queries to complete. */
  for (;;)
    {
      int res;
      FD_ZERO(&read_fds);
      FD_ZERO(&write_fds);
      nfds = ares_fds(channel, &read_fds, &write_fds);
      if (nfds == 0)
        break;
      tvp = ares_timeout(channel, NULL, &tv);
      res = select(nfds, &read_fds, &write_fds, NULL, tvp);
      if (-1 == res)
        break;
      ares_process(channel, &read_fds, &write_fds);
    }

  ares_destroy(channel);

  ares_library_cleanup();

#ifdef USE_WINSOCK
  WSACleanup();
#endif

  return 0;
}

static void callback(void *arg, int status, int timeouts, struct hostent *host)
{
  char **p;

  (void)timeouts;

  if (status != ARES_SUCCESS)
    {
      fprintf(stderr, "%s: %s\n", (char *) arg, ares_strerror(status));
      return;
    }

  for (p = host->h_addr_list; *p; p++)
    {
      char addr_buf[46] = "??";

      ares_inet_ntop(host->h_addrtype, *p, addr_buf, sizeof(addr_buf));
      printf("%-32s\t%s", host->h_name, addr_buf);
#if 0
      if (host->h_aliases[0])
        {
           int i;

           printf (", Aliases: ");
           for (i = 0; host->h_aliases[i]; i++)
               printf("%s ", host->h_aliases[i]);
        }
#endif
      puts("");
    }
}

static void usage(void)
{
  fprintf(stderr, "usage: ahost [-h] [-d] [-s {domain}] [-t {a|aaaa|u}] {host|addr} ...\n");
  exit(1);
}

/* Information from the man page. Formatting taken from man -h */
static void print_help_info_ahost(void) {
    printf("ahost, version %s\n\n", ARES_VERSION_STR);
    printf("usage: ahost [-h] [-d] [[-s domain] ...] [-t a|aaaa|u] host|addr ...\n\n"
    "  h : Display this help and exit.\n"
    "  d : Print some extra debugging output.\n\n"
    "  s domain : Specify the domain to search instead of using the default values\n"
    "               from /etc/resolv.conf. This option only has an effect on\n"
    "               platforms that use /etc/resolv.conf for DNS configuration;\n"
    "               it has no effect on other platforms (such as Win32 or Android).\n\n"
    "  t type   : If type is \"a\", print the A record (default).\n"
    "               If type is \"aaaa\", print the AAAA record.\n"
    "               If type is \"u\", look for either AAAA or A record (in that order).\n\n");
    exit(0);
} 

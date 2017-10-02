
/* Copyright (C) 2005 by Dominick Meglio
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

#ifndef ARES_IPV6_H
#define ARES_IPV6_H

#ifndef HAVE_PF_INET6
#define PF_INET6 AF_INET6
#endif

#ifndef HAVE_STRUCT_SOCKADDR_IN6
struct sockaddr_in6
{
  unsigned short       sin6_family;
  unsigned short       sin6_port;
  unsigned long        sin6_flowinfo;
  struct ares_in6_addr sin6_addr;
  unsigned int         sin6_scope_id;
};
#endif

#ifndef HAVE_STRUCT_ADDRINFO
struct addrinfo
{
  int              ai_flags;
  int              ai_family;
  int              ai_socktype;
  int              ai_protocol;
  ares_socklen_t   ai_addrlen;   /* Follow rfc3493 struct addrinfo */
  char            *ai_canonname;
  struct sockaddr *ai_addr;
  struct addrinfo *ai_next;
};
#endif

#ifndef NS_IN6ADDRSZ
#ifndef HAVE_STRUCT_IN6_ADDR
/* We cannot have it set to zero, so we pick a fixed value here */
#define NS_IN6ADDRSZ 16
#else
#define NS_IN6ADDRSZ sizeof(struct in6_addr)
#endif
#endif

#ifndef NS_INADDRSZ
#define NS_INADDRSZ sizeof(struct in_addr)
#endif

#ifndef NS_INT16SZ
#define NS_INT16SZ 2
#endif

#ifndef IF_NAMESIZE
#ifdef IFNAMSIZ
#define IF_NAMESIZE IFNAMSIZ
#else
#define IF_NAMESIZE 256
#endif
#endif

/* Defined in inet_net_pton.c for no particular reason. */
extern const struct ares_in6_addr ares_in6addr_any; /* :: */


#endif /* ARES_IPV6_H */

#ifndef HEADER_CARES_INET_NET_PTON_H
#define HEADER_CARES_INET_NET_PTON_H

/* Copyright (C) 2005-2013 by Daniel Stenberg et al
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

#ifdef HAVE_INET_NET_PTON
#define ares_inet_net_pton(w,x,y,z) inet_net_pton(w,x,y,z)
#else
int ares_inet_net_pton(int af, const char *src, void *dst, size_t size);
#endif

#endif /* HEADER_CARES_INET_NET_PTON_H */

/* $Id: udns_XtoX.c,v 1.1 2007/01/07 22:20:39 mjt Exp $
   udns_ntop() and udns_pton() routines, which are either
     - wrappers for inet_ntop() and inet_pton() or
     - reimplementations of those routines.

   Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
   This file is part of UDNS library, an async DNS stub resolver.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library, in file named COPYING.LGPL; if not,
   write to the Free Software Foundation, Inc., 59 Temple Place,
   Suite 330, Boston, MA  02111-1307  USA

 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "udns.h"

#ifdef HAVE_INET_PTON_NTOP

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

const char *dns_ntop(int af, const void *src, char *dst, int size) {
  return inet_ntop(af, src, dst, size);
}

int dns_pton(int af, const char *src, void *dst) {
  return inet_pton(af, src, dst);
}

#else

#define inet_XtoX_prefix udns_
#include "inet_XtoX.c"

#endif

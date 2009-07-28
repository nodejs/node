/* $Id: udns_dntosp.c,v 1.5 2005/04/19 21:48:09 mjt Exp $
   dns_dntosp() = convert DN to asciiz string using static buffer

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

#include "udns.h"

static char name[DNS_MAXNAME];

const char *dns_dntosp(dnscc_t *dn) {
  return dns_dntop(dn, name, sizeof(name)) > 0 ? name : 0;
}

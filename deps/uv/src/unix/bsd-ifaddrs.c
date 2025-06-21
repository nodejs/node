/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <stddef.h>

#include <ifaddrs.h>
#include <net/if.h>
#if !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(__GNU__)
#include <net/if_dl.h>
#endif

#if defined(__HAIKU__)
#define IFF_RUNNING IFF_LINK
#endif

static int uv__ifaddr_exclude(struct ifaddrs *ent, int exclude_type) {
  if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)))
    return 1;
  if (ent->ifa_addr == NULL)
    return 1;
#if !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(__GNU__)
  /*
   * If `exclude_type` is `UV__EXCLUDE_IFPHYS`, return whether `sa_family`
   * equals `AF_LINK`. Otherwise, the result depends on the operating
   * system with `AF_LINK` or `PF_INET`.
   */
  if (exclude_type == UV__EXCLUDE_IFPHYS)
    return (ent->ifa_addr->sa_family != AF_LINK);
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFly__) || \
    defined(__HAIKU__)
  /*
   * On BSD getifaddrs returns information related to the raw underlying
   * devices. We're not interested in this information.
   */
  if (ent->ifa_addr->sa_family == AF_LINK)
    return 1;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
  if (ent->ifa_addr->sa_family != PF_INET &&
      ent->ifa_addr->sa_family != PF_INET6)
    return 1;
#endif
  return 0;
}

/* TODO(bnoordhuis) share with linux.c */
int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  uv_interface_address_t* address;
  struct ifaddrs* addrs;
  struct ifaddrs* ent;
  size_t namelen;
  char* name;

  *count = 0;
  *addresses = NULL;

  if (getifaddrs(&addrs) != 0)
    return UV__ERR(errno);

  /* Count the number of interfaces */
  namelen = 0;
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFADDR))
      continue;
    namelen += strlen(ent->ifa_name) + 1;
    (*count)++;
  }

  if (*count == 0) {
    freeifaddrs(addrs);
    return 0;
  }

  /* Make sure the memory is initiallized to zero using calloc() */
  *addresses = uv__calloc(1, *count * sizeof(**addresses) + namelen);
  if (*addresses == NULL) {
    freeifaddrs(addrs);
    return UV_ENOMEM;
  }

  name = (char*) &(*addresses)[*count];
  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFADDR))
      continue;

    namelen = strlen(ent->ifa_name) + 1;
    address->name = memcpy(name, ent->ifa_name, namelen);
    name += namelen;

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6*) ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) ent->ifa_addr);
    }

    if (ent->ifa_netmask == NULL) {
      memset(&address->netmask, 0, sizeof(address->netmask));
    } else if (ent->ifa_netmask->sa_family == AF_INET6) {
      address->netmask.netmask6 = *((struct sockaddr_in6*) ent->ifa_netmask);
    } else {
      address->netmask.netmask4 = *((struct sockaddr_in*) ent->ifa_netmask);
    }

    address->is_internal = !!(ent->ifa_flags & IFF_LOOPBACK);

    address++;
  }

#if !(defined(__CYGWIN__) || defined(__MSYS__)) && !defined(__GNU__)
  /* Fill in physical addresses for each interface */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    int i;

    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFPHYS))
      continue;

    address = *addresses;

    for (i = 0; i < *count; i++) {
      if (strcmp(address->name, ent->ifa_name) == 0) {
        struct sockaddr_dl* sa_addr;
        sa_addr = (struct sockaddr_dl*)(ent->ifa_addr);
        memcpy(address->phys_addr, LLADDR(sa_addr), sizeof(address->phys_addr));
      }
      address++;
    }
  }
#endif

  freeifaddrs(addrs);

  return 0;
}


/* TODO(bnoordhuis) share with linux.c */
void uv_free_interface_addresses(uv_interface_address_t* addresses,
                                 int count) {
  uv__free(addresses);
}

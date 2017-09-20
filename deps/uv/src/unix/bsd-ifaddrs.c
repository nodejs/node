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
#if !defined(__CYGWIN__) && !defined(__MSYS__)
#include <net/if_dl.h>
#endif

static int uv__ifaddr_exclude(struct ifaddrs *ent, int exclude_type) {
  if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)))
    return 1;
  if (ent->ifa_addr == NULL)
    return 1;
  /*
   * If `exclude_type` is `UV__EXCLUDE_IFPHYS`, just see whether `sa_family`
   * equals to `AF_LINK` or not. Otherwise, the result depends on the operation
   * system with `AF_LINK` or `PF_INET`.
   */
  if (exclude_type == UV__EXCLUDE_IFPHYS)
    return (ent->ifa_addr->sa_family != AF_LINK);
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFly__)
  /*
   * On BSD getifaddrs returns information related to the raw underlying
   * devices.  We're not interested in this information.
   */
  if (ent->ifa_addr->sa_family == AF_LINK)
    return 1;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
  if (ent->ifa_addr->sa_family != PF_INET)
    return 1;
#endif
  return 0;
}

int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  struct ifaddrs* addrs;
  struct ifaddrs* ent;
  uv_interface_address_t* address;
  int i;

  if (getifaddrs(&addrs) != 0)
    return -errno;

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFADDR))
      continue;
    (*count)++;
  }

  *addresses = uv__malloc(*count * sizeof(**addresses));

  if (*addresses == NULL) {
    freeifaddrs(addrs);
    return -ENOMEM;
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFADDR))
      continue;

    address->name = uv__strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6*) ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) ent->ifa_addr);
    }

    if (ent->ifa_netmask->sa_family == AF_INET6) {
      address->netmask.netmask6 = *((struct sockaddr_in6*) ent->ifa_netmask);
    } else {
      address->netmask.netmask4 = *((struct sockaddr_in*) ent->ifa_netmask);
    }

    address->is_internal = !!(ent->ifa_flags & IFF_LOOPBACK);

    address++;
  }

  /* Fill in physical addresses for each interface */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (uv__ifaddr_exclude(ent, UV__EXCLUDE_IFPHYS))
      continue;

    address = *addresses;

    for (i = 0; i < *count; i++) {
      if (strcmp(address->name, ent->ifa_name) == 0) {
#if defined(__CYGWIN__) || defined(__MSYS__)
        memset(address->phys_addr, 0, sizeof(address->phys_addr));
#else
        struct sockaddr_dl* sa_addr;
        sa_addr = (struct sockaddr_dl*)(ent->ifa_addr);
        memcpy(address->phys_addr, LLADDR(sa_addr), sizeof(address->phys_addr));
#endif
      }
      address++;
    }
  }

  freeifaddrs(addrs);

  return 0;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
                                 int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}

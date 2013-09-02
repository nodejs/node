/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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
#include "task.h"

#include <stdio.h>
#include <string.h>

#ifdef __linux__
# include <sys/socket.h>
# include <net/if.h>
#endif


TEST_IMPL(ip6_addr_link_local) {
#ifdef UV_PLATFORM_HAS_IP6_LINK_LOCAL_ADDRESS
  char string_address[INET6_ADDRSTRLEN];
  uv_interface_address_t* addresses;
  uv_interface_address_t* address;
  struct sockaddr_in6 addr;
  unsigned int iface_index;
  const char* device_name;
  /* 40 bytes address, 16 bytes device name, plus reserve. */
  char scoped_addr[128];
  int count;
  int ix;

  ASSERT(0 == uv_interface_addresses(&addresses, &count));

  for (ix = 0; ix < count; ix++) {
    address = addresses + ix;

    if (address->address.address6.sin6_family != AF_INET6)
      continue;

    ASSERT(0 == uv_inet_ntop(AF_INET6,
                             &address->address.address6.sin6_addr,
                             string_address,
                             sizeof(string_address)));

    /* Skip addresses that are not link-local. */
    if (strncmp(string_address, "fe80::", 6) != 0)
      continue;

    iface_index = address->address.address6.sin6_scope_id;
    device_name = address->name;

#ifdef _WIN32
    snprintf(scoped_addr,
             sizeof(scoped_addr),
             "%s%%%d",
             string_address,
             iface_index);
#else
    snprintf(scoped_addr,
             sizeof(scoped_addr),
             "%s%%%s",
             string_address,
             device_name);
#endif

    LOGF("Testing link-local address %s "
         "(iface_index: 0x%02x, device_name: %s)\n",
         scoped_addr,
         iface_index,
         device_name);

    ASSERT(0 == uv_ip6_addr(scoped_addr, TEST_PORT, &addr));
    LOGF("Got scope_id 0x%02x\n", addr.sin6_scope_id);
    ASSERT(iface_index == addr.sin6_scope_id);
  }

  uv_free_interface_addresses(addresses, count);

  MAKE_VALGRIND_HAPPY();
  return 0;
#else
  RETURN_SKIP("Qualified link-local addresses are not supported.");
#endif
}

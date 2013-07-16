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

typedef void (*iface_info_cb)(const char* ip6_addr, const char* device_name,
    unsigned iface_index);

void call_iface_info_cb(iface_info_cb iface_cb,
                        char const* iface_name,
                        struct sockaddr_in6 const* address) {
  char string_address[INET6_ADDRSTRLEN];

  ASSERT(0 == uv_inet_ntop(AF_INET6,
                           &address->sin6_addr,
                           string_address,
                           INET6_ADDRSTRLEN));
  iface_cb(string_address, iface_name, address->sin6_scope_id);
}


void foreach_ip6_interface(iface_info_cb iface_cb) {
  int count, ix;
  uv_interface_address_t* addresses;

  ASSERT(0 == uv_interface_addresses(&addresses, &count));

  for (ix = 0; ix < count; ix++) {
    if (addresses[ix].address.address4.sin_family != AF_INET6)
      continue;

    call_iface_info_cb(iface_cb,
                       addresses[ix].name,
                       &addresses[ix].address.address6);
  }

  uv_free_interface_addresses(addresses, count);
}


void test_ip6_addr_scope(const char* ip6_addr,
                         const char* device_name,
                         unsigned iface_index) {
   /* 40 bytes address, 16 bytes device name, plus reserve */
  char scoped_addr[128];
  struct sockaddr_in6 addr;

  /* skip addresses that are not link-local */
  if (strncmp(ip6_addr, "fe80::", 6) != 0) return;

#ifdef _WIN32
  sprintf(scoped_addr, "%s%%%d", ip6_addr, iface_index);
#else
  sprintf(scoped_addr, "%s%%%s", ip6_addr, device_name);
#endif

  LOGF("Testing link-local address %s (iface_index: 0x%02x, device_name: %s)\n",
       scoped_addr,
       iface_index,
       device_name);

  addr = uv_ip6_addr(scoped_addr, TEST_PORT);

  LOGF("Got scope_id 0x%02x\n", addr.sin6_scope_id);
  ASSERT(iface_index == addr.sin6_scope_id);
}


TEST_IMPL(ip6_addr_link_local) {
#ifdef UV_PLATFORM_HAS_IP6_LINK_LOCAL_ADDRESS
  foreach_ip6_interface(&test_ip6_addr_scope);
  return 0;
#else
  RETURN_SKIP("Qualified link-local addresses are not supported.");
#endif
}

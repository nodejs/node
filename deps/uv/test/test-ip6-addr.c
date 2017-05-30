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
#if defined(__CYGWIN__) || defined(__MSYS__)
  /* FIXME: Does Cygwin support this?  */
  RETURN_SKIP("FIXME: This test needs more investigation on Cygwin");
#endif
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

    fprintf(stderr, "Testing link-local address %s "
            "(iface_index: 0x%02x, device_name: %s)\n",
            scoped_addr,
            iface_index,
            device_name);
    fflush(stderr);

    ASSERT(0 == uv_ip6_addr(scoped_addr, TEST_PORT, &addr));
    fprintf(stderr, "Got scope_id 0x%02x\n", addr.sin6_scope_id);
    fflush(stderr);
    ASSERT(iface_index == addr.sin6_scope_id);
  }

  uv_free_interface_addresses(addresses, count);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


#define GOOD_ADDR_LIST(X)                                                     \
    X("::")                                                                   \
    X("::1")                                                                  \
    X("fe80::1")                                                              \
    X("fe80::")                                                               \
    X("fe80::2acf:daff:fedd:342a")                                            \
    X("fe80:0:0:0:2acf:daff:fedd:342a")                                       \
    X("fe80:0:0:0:2acf:daff:1.2.3.4")                                         \
    X("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")                        \

#define BAD_ADDR_LIST(X)                                                      \
    X(":::1")                                                                 \
    X("abcde::1")                                                             \
    X("fe80:0:0:0:2acf:daff:fedd:342a:5678")                                  \
    X("fe80:0:0:0:2acf:daff:abcd:1.2.3.4")                                    \
    X("fe80:0:0:2acf:daff:1.2.3.4.5")                                         \
    X("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255.255")                    \

#define TEST_GOOD(ADDR)                                                       \
    ASSERT(0 == uv_inet_pton(AF_INET6, ADDR, &addr));                         \
    ASSERT(0 == uv_inet_pton(AF_INET6, ADDR "%en1", &addr));                  \
    ASSERT(0 == uv_inet_pton(AF_INET6, ADDR "%%%%", &addr));                  \
    ASSERT(0 == uv_inet_pton(AF_INET6, ADDR "%en1:1.2.3.4", &addr));          \

#define TEST_BAD(ADDR)                                                        \
    ASSERT(0 != uv_inet_pton(AF_INET6, ADDR, &addr));                         \
    ASSERT(0 != uv_inet_pton(AF_INET6, ADDR "%en1", &addr));                  \
    ASSERT(0 != uv_inet_pton(AF_INET6, ADDR "%%%%", &addr));                  \
    ASSERT(0 != uv_inet_pton(AF_INET6, ADDR "%en1:1.2.3.4", &addr));          \

TEST_IMPL(ip6_pton) {
  struct in6_addr addr;

  GOOD_ADDR_LIST(TEST_GOOD)
  BAD_ADDR_LIST(TEST_BAD)

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#undef GOOD_ADDR_LIST
#undef BAD_ADDR_LIST

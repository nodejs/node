//===-- Definition of macros from netinet/in.h ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_NETINET_IN_MACROS_H
#define LLVM_LIBC_MACROS_NETINET_IN_MACROS_H

#include "../llvm-libc-types/in_addr_t.h"
#include "__llvm-libc-common.h"

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_RAW 255

#define IPV6_UNICAST_HOPS 16
#define IPV6_MULTICAST_IF 17
#define IPV6_MULTICAST_HOPS 18
#define IPV6_MULTICAST_LOOP 19
#define IPV6_JOIN_GROUP 20
#define IPV6_LEAVE_GROUP 21
#define IPV6_V6ONLY 26

#define INADDR_ANY __LLVM_LIBC_CAST(static_cast, in_addr_t, 0x00000000)
#define INADDR_BROADCAST __LLVM_LIBC_CAST(static_cast, in_addr_t, 0xffffffff)
#define INADDR_NONE __LLVM_LIBC_CAST(static_cast, in_addr_t, 0xffffffff)

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

// The following macros test for special IPv6 addresses. Each macro is of type
// int and takes a single argument of type const struct in6_addr *:
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/netinet_in.h.html

#define IN6_IS_ADDR_UNSPECIFIED(a)                                             \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[0]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[1]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[2]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[3]) == 0)

#define IN6_IS_ADDR_LOOPBACK(a)                                                \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[0]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[1]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[2]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[12]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[13]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[14]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[15]) == 1)

#define IN6_IS_ADDR_MULTICAST(a)                                               \
  (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[0]) == 0xff

#define IN6_IS_ADDR_LINKLOCAL(a)                                               \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[0]) == 0xfe &&            \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xc0) == 0x80)

#define IN6_IS_ADDR_SITELOCAL(a)                                               \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[0]) == 0xfe &&            \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xc0) == 0xc0)

#define IN6_IS_ADDR_V4MAPPED(a)                                                \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[0]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[1]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[8]) == 0 &&               \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[9]) == 0 &&               \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[10]) == 0xff &&           \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[11]) == 0xff)

#define IN6_IS_ADDR_V4COMPAT(a)                                                \
  ((__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[0]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[1]) == 0 &&              \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint32_t *, a)[2]) == 0 &&              \
   !IN6_IS_ADDR_UNSPECIFIED(a) && !IN6_IS_ADDR_LOOPBACK(a))

#define IN6_IS_ADDR_MC_NODELOCAL(a)                                            \
  (IN6_IS_ADDR_MULTICAST(a) &&                                                 \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xf) == 0x1)

#define IN6_IS_ADDR_MC_LINKLOCAL(a)                                            \
  (IN6_IS_ADDR_MULTICAST(a) &&                                                 \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xf) == 0x2)

#define IN6_IS_ADDR_MC_SITELOCAL(a)                                            \
  (IN6_IS_ADDR_MULTICAST(a) &&                                                 \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xf) == 0x5)

#define IN6_IS_ADDR_MC_ORGLOCAL(a)                                             \
  (IN6_IS_ADDR_MULTICAST(a) &&                                                 \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xf) == 0x8)

#define IN6_IS_ADDR_MC_GLOBAL(a)                                               \
  (IN6_IS_ADDR_MULTICAST(a) &&                                                 \
   (__LLVM_LIBC_CAST(reinterpret_cast, uint8_t *, a)[1] & 0xf) == 0xe)

#endif // LLVM_LIBC_MACROS_NETINET_IN_MACROS_H

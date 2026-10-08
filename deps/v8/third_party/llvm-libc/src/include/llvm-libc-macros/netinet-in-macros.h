//===-- Definition of macros from netinet/in.h ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_NETINET_IN_MACROS_H
#define LLVM_LIBC_MACROS_NETINET_IN_MACROS_H

#include "../__llvm-libc-common.h"
#include "../llvm-libc-types/in_addr_t.h"
#include "../llvm-libc-types/struct_in6_addr.h"

#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_RAW 255

#define INADDR_ANY __LLVM_LIBC_CAST(static_cast, in_addr_t, 0x00000000)
#define INADDR_BROADCAST __LLVM_LIBC_CAST(static_cast, in_addr_t, 0xffffffff)
#define INADDR_NONE __LLVM_LIBC_CAST(static_cast, in_addr_t, 0xffffffff)
// Not specified by POSIX, added in SVR4
#define INADDR_LOOPBACK __LLVM_LIBC_CAST(static_cast, in_addr_t, 0x7f000001)

#define IN6ADDR_ANY_INIT                                                       \
  {                                                                            \
    {                                                                          \
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }                       \
    }                                                                          \
  }
#define IN6ADDR_LOOPBACK_INIT                                                  \
  {                                                                            \
    {                                                                          \
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }                       \
    }                                                                          \
  }

// The following macros test for special IPv6 addresses. Each macro is of type
// int and takes a single argument of type const struct in6_addr *:
// https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/netinet_in.h.html

#define __IN6_IS_ADDR_UNSPECIFIED(a)                                           \
  ((a)->s6_addr32[0] == 0 && (a)->s6_addr32[1] == 0 &&                         \
   (a)->s6_addr32[2] == 0 && (a)->s6_addr32[3] == 0)

#define __IN6_IS_ADDR_LOOPBACK(a)                                              \
  ((a)->s6_addr32[0] == 0 && (a)->s6_addr32[1] == 0 &&                         \
   (a)->s6_addr32[2] == 0 && (a)->s6_addr[12] == 0 && (a)->s6_addr[13] == 0 && \
   (a)->s6_addr[14] == 0 && (a)->s6_addr[15] == 1)

#define IN6_IS_ADDR_UNSPECIFIED(a)                                             \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __IN6_IS_ADDR_UNSPECIFIED(__a);                                            \
  }))

#define IN6_IS_ADDR_LOOPBACK(a)                                                \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __IN6_IS_ADDR_LOOPBACK(__a);                                               \
  }))

#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xff)

#define IN6_IS_ADDR_LINKLOCAL(a)                                               \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __a->s6_addr[0] == 0xfe && (__a->s6_addr[1] & 0xc0) == 0x80;               \
  }))

#define IN6_IS_ADDR_SITELOCAL(a)                                               \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __a->s6_addr[0] == 0xfe && (__a->s6_addr[1] & 0xc0) == 0xc0;               \
  }))

#define IN6_IS_ADDR_V4MAPPED(a)                                                \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __a->s6_addr32[0] == 0 && __a->s6_addr32[1] == 0 &&                        \
        __a->s6_addr[8] == 0 && __a->s6_addr[9] == 0 &&                        \
        __a->s6_addr[10] == 0xff && __a->s6_addr[11] == 0xff;                  \
  }))

#define IN6_IS_ADDR_V4COMPAT(a)                                                \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    __a->s6_addr32[0] == 0 && __a->s6_addr32[1] == 0 &&                        \
        __a->s6_addr32[2] == 0 && !__IN6_IS_ADDR_UNSPECIFIED(__a) &&           \
        !__IN6_IS_ADDR_LOOPBACK(__a);                                          \
  }))

#define IN6_IS_ADDR_MC_NODELOCAL(a)                                            \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    IN6_IS_ADDR_MULTICAST(__a) && (__a->s6_addr[1] & 0xf) == 0x1;              \
  }))

#define IN6_IS_ADDR_MC_LINKLOCAL(a)                                            \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    IN6_IS_ADDR_MULTICAST(__a) && (__a->s6_addr[1] & 0xf) == 0x2;              \
  }))

#define IN6_IS_ADDR_MC_SITELOCAL(a)                                            \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    IN6_IS_ADDR_MULTICAST(__a) && (__a->s6_addr[1] & 0xf) == 0x5;              \
  }))

#define IN6_IS_ADDR_MC_ORGLOCAL(a)                                             \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    IN6_IS_ADDR_MULTICAST(__a) && (__a->s6_addr[1] & 0xf) == 0x8;              \
  }))

#define IN6_IS_ADDR_MC_GLOBAL(a)                                               \
  (__extension__({                                                             \
    const struct in6_addr *__a = (a);                                          \
    IN6_IS_ADDR_MULTICAST(__a) && (__a->s6_addr[1] & 0xf) == 0xe;              \
  }))

#endif // LLVM_LIBC_MACROS_NETINET_IN_MACROS_H

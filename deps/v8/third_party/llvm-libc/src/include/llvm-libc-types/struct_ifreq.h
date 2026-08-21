//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ifreq.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_IFREQ_H
#define LLVM_LIBC_TYPES_STRUCT_IFREQ_H

#include "../llvm-libc-macros/net-if-macros.h"
#include "struct_sockaddr.h"

struct ifreq {
  char ifr_name[IF_NAMESIZE];
  __extension__ union {
    struct sockaddr ifr_hwaddr;
    struct sockaddr ifr_addr;
    struct sockaddr ifr_dstaddr;
    struct sockaddr ifr_broadaddr;
    struct sockaddr ifr_netmask;
    short int ifr_flags;
    int ifr_metric;
    int ifr_mtu;
    int ifr_ifindex;
    int ifr_bandwidth;
    int ifr_qlen;
    char ifr_newname[IF_NAMESIZE];
    char ifr_slave[IF_NAMESIZE];
    char *ifr_data;
  };
};

#endif // LLVM_LIBC_TYPES_STRUCT_IFREQ_H

//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ifreq for Linux.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LINUX_STRUCT_IFREQ_H
#define LLVM_LIBC_TYPES_LINUX_STRUCT_IFREQ_H

#include "../../llvm-libc-macros/net-if-macros.h"
#include "../struct_sockaddr.h"
#include "struct_ifmap.h"

// Prevent the linux headers from defining this type.
#define __UAPI_DEF_IF_IFREQ 0

struct ifreq {
  union {
    char ifrn_name[IF_NAMESIZE];
  } ifr_ifrn;

  union {
    struct sockaddr ifru_addr;
    struct sockaddr ifru_dstaddr;
    struct sockaddr ifru_broadaddr;
    struct sockaddr ifru_netmask;
    struct sockaddr ifru_hwaddr;
    short int ifru_flags;
    int ifru_ivalue;
    int ifru_mtu;
    struct ifmap ifru_map;
    char ifru_slave[IF_NAMESIZE];
    char ifru_newname[IF_NAMESIZE];
    char *ifru_data;
  } ifr_ifru;
};

#define ifr_name ifr_ifrn.ifrn_name
#define ifr_hwaddr ifr_ifru.ifru_hwaddr
#define ifr_addr ifr_ifru.ifru_addr
#define ifr_dstaddr ifr_ifru.ifru_dstaddr
#define ifr_broadaddr ifr_ifru.ifru_broadaddr
#define ifr_netmask ifr_ifru.ifru_netmask
#define ifr_flags ifr_ifru.ifru_flags
#define ifr_metric ifr_ifru.ifru_ivalue
#define ifr_mtu ifr_ifru.ifru_mtu
#define ifr_map ifr_ifru.ifru_map
#define ifr_slave ifr_ifru.ifru_slave
#define ifr_data ifr_ifru.ifru_data
#define ifr_ifindex ifr_ifru.ifru_ivalue
#define ifr_bandwidth ifr_ifru.ifru_ivalue
#define ifr_qlen ifr_ifru.ifru_ivalue
#define ifr_newname ifr_ifru.ifru_newname

#endif // LLVM_LIBC_TYPES_LINUX_STRUCT_IFREQ_H

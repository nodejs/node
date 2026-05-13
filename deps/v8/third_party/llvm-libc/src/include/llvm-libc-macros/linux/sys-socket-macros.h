//===-- Definition of macros from sys/socket.h ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_SOCKET_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_SOCKET_MACROS_H

// IEEE Std 1003.1-2017 - basedefs/sys_socket.h.html
// Macro values come from the Linux syscall interface.

#define AF_UNSPEC 0 // Unspecified
#define AF_UNIX 1   // Unix domain sockets
#define AF_LOCAL 1  // POSIX name for AF_UNIX
#define AF_INET 2   // Internet IPv4 Protocol
#define AF_INET6 10 // IP version 6

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3
#define SOCK_RDM 4
#define SOCK_SEQPACKET 5
#define SOCK_PACKET 10

#define SOCK_CLOEXEC 0x80000
#define SOCK_NONBLOCK 0x800

#define SOL_SOCKET 1

#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_NO_CHECK 11
#define SO_PRIORITY 12
#define SO_LINGER 13
#define SO_BSDCOMPAT 14
#define SO_REUSEPORT 15

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SCM_RIGHTS 1

#define MSG_OOB 0x01
#define MSG_PEEK 0x02
#define MSG_DONTROUTE 0x04
#define MSG_CTRUNC 0x08
#define MSG_PROXY 0x10
#define MSG_TRUNC 0x20
#define MSG_DONTWAIT 0x40
#define MSG_EOR 0x80
#define MSG_WAITALL 0x100
#define MSG_FIN 0x200
#define MSG_SYN 0x400
#define MSG_CONFIRM 0x800
#define MSG_RST 0x1000
#define MSG_ERRQUEUE 0x2000
#define MSG_NOSIGNAL 0x4000
#define MSG_MORE 0x8000
#define MSG_WAITFORONE 0x10000
#define MSG_BATCH 0x40000
#define MSG_SOCK_DEVMEM 0x2000000
#define MSG_ZEROCOPY 0x4000000
#define MSG_FASTOPEN 0x20000000
#define MSG_CMSG_CLOEXEC 0x40000000

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_LEN(len) (sizeof(struct cmsghdr) + (len))
#define CMSG_SPACE(len) (sizeof(struct cmsghdr) + CMSG_ALIGN(len))

#define CMSG_FIRSTHDR(msg)                                                     \
  ((msg)->msg_controllen >= sizeof(struct cmsghdr)                             \
       ? (struct cmsghdr *)(msg)->msg_control                                  \
       : 0)
#define __CMSG_NXTHDR_CANDIDATE(cmsg)                                          \
  ((struct cmsghdr *)((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len)))
#define CMSG_NXTHDR(msg, cmsg)                                                 \
  ((char *)(__CMSG_NXTHDR_CANDIDATE(cmsg) + 1) <=                              \
           ((char *)((msg)->msg_control) + (msg)->msg_controllen)              \
       ? __CMSG_NXTHDR_CANDIDATE(cmsg)                                         \
       : 0)
#define CMSG_DATA(cmsg) ((unsigned char *)((struct cmsghdr *)(cmsg) + 1))

#endif // LLVM_LIBC_MACROS_LINUX_SYS_SOCKET_MACROS_H

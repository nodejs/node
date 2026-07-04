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

// The values of the following constants differ on some architectures
#if defined(__parisc__) || defined(__alpha__) || defined(__sparc__) ||         \
    defined(__powerpc__) || defined(__mips__)
#error "Please check SO_ constant values"
#endif

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
#define SO_PASSCRED 16
#define SO_PEERCRED 17
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19

// The "old" options use a 32-bit time_t on 32-bit systems.
// #define SO_RCVTIMEO_OLD 20
// #define SO_SNDTIMEO_OLD 21

#define SO_SECURITY_AUTHENTICATION 22
#define SO_SECURITY_ENCRYPTION_TRANSPORT 23
#define SO_SECURITY_ENCRYPTION_NETWORK 24
#define SO_BINDTODEVICE 25
#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#define SO_GET_FILTER SO_ATTACH_FILTER
#define SO_PEERNAME 28
// #define SO_TIMESTAMP_OLD 29
#define SO_ACCEPTCONN 30
#define SO_PEERSEC 31
#define SO_SNDBUFFORCE 32
#define SO_RCVBUFFORCE 33
#define SO_PASSSEC 34
// #define SO_TIMESTAMPNS_OLD 35
#define SO_MARK 36
// #define SO_TIMESTAMPING_OLD 37
#define SO_PROTOCOL 38
#define SO_DOMAIN 39
#define SO_RXQ_OVFL 40
#define SO_WIFI_STATUS 41
#define SO_PEEK_OFF 42
#define SO_NOFCS 43
#define SO_LOCK_FILTER 44
#define SO_SELECT_ERR_QUEUE 45
#define SO_BUSY_POLL 46
#define SO_MAX_PACING_RATE 47
#define SO_BPF_EXTENSIONS 48
#define SO_INCOMING_CPU 49
#define SO_ATTACH_BPF 50
#define SO_DETACH_BPF SO_DETACH_FILTER
#define SO_ATTACH_REUSEPORT_CBPF 51
#define SO_ATTACH_REUSEPORT_EBPF 52
#define SO_CNX_ADVICE 53
#define SCM_TIMESTAMPING_OPT_STATS 54
#define SO_MEMINFO 55
#define SO_INCOMING_NAPI_ID 56
#define SO_COOKIE 57
#define SCM_TIMESTAMPING_PKTINFO 58
#define SO_PEERGROUPS 59
#define SO_ZEROCOPY 60
#define SO_TXTIME 61
#define SO_BINDTOIFINDEX 62

// These are the "new" options, which assume a 64-bit time_t, regardless of the
// pointer size.
#define SO_TIMESTAMP 63
#define SO_TIMESTAMPNS 64
#define SO_TIMESTAMPING 65
#define SO_RCVTIMEO 66
#define SO_SNDTIMEO 67

#define SO_DETACH_REUSEPORT_BPF 68
#define SO_PREFER_BUSY_POLL 69
#define SO_BUSY_POLL_BUDGET 70
#define SO_NETNS_COOKIE 71
#define SO_BUF_LOCK 72
#define SO_RESERVE_MEM 73
#define SO_TXREHASH 74
#define SO_RCVMARK 75
#define SO_PASSPIDFD 76
#define SO_PEERPIDFD 77
#define SO_DEVMEM_LINEAR 78
#define SO_DEVMEM_DMABUF 79
#define SO_DEVMEM_DONTNEED 80
#define SO_RCVPRIORITY 82
#define SO_PASSRIGHTS 83
#define SO_INQ 84

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

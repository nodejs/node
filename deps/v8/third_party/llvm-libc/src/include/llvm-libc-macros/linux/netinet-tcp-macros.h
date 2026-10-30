//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros defined in netinet/tcp.h header file. Values come from the Linux
/// syscall interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_NETINET_TCP_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_NETINET_TCP_MACROS_H

#define SOL_TCP 6

// TCP level socket options. The option type is int unless noted otherwise.
#define TCP_NODELAY 1
#define TCP_MAXSEG 2
#define TCP_CORK 3
#define TCP_KEEPIDLE 4
#define TCP_KEEPINTVL 5
#define TCP_KEEPCNT 6
#define TCP_SYNCNT 7
#define TCP_LINGER2 8
#define TCP_DEFER_ACCEPT 9
#define TCP_WINDOW_CLAMP 10
#define TCP_QUICKACK 12
#define TCP_CONGESTION 13 // char[]
#define TCP_THIN_LINEAR_TIMEOUTS 16
#define TCP_THIN_DUPACK 17
#define TCP_USER_TIMEOUT 18
#define TCP_REPAIR 19
#define TCP_REPAIR_QUEUE 20
#define TCP_QUEUE_SEQ 21
#define TCP_FASTOPEN 23
#define TCP_TIMESTAMP 24
#define TCP_NOTSENT_LOWAT 25
#define TCP_SAVE_SYN 27
#define TCP_SAVED_SYN 28 // unsigned char[]
#define TCP_FASTOPEN_CONNECT 30
#define TCP_ULP 31          // char[]
#define TCP_FASTOPEN_KEY 33 // unsigned char[]
#define TCP_FASTOPEN_NO_COOKIE 34
#define TCP_INQ 36
#define TCP_TX_DELAY 37
#define TCP_IS_MPTCP 43
#define TCP_RTO_MAX_MS 44
#define TCP_RTO_MIN_US 45
#define TCP_DELACK_MAX_US 46

// Values for TCP_REPAIR.
#define TCP_REPAIR_ON 1
#define TCP_REPAIR_OFF 0
#define TCP_REPAIR_OFF_NO_WP -1

// Values for TCP_REPAIR_QUEUE.
#define TCP_NO_QUEUE 0
#define TCP_RECV_QUEUE 1
#define TCP_SEND_QUEUE 2
#define TCP_QUEUES_NR 3 // Number of queues, not a valid value.

#endif // LLVM_LIBC_MACROS_LINUX_NETINET_TCP_MACROS_H

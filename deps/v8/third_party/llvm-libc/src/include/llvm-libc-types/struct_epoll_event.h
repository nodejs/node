//===-- Definition of epoll_event type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_EPOLL_EVENT_H
#define LLVM_LIBC_TYPES_STRUCT_EPOLL_EVENT_H

#include "struct_epoll_data.h"

typedef struct
#ifdef __x86_64__
    [[gnu::packed]] // Necessary for compatibility.
#endif
    epoll_event {
  __UINT32_TYPE__ events;
  epoll_data_t data;
} epoll_event;

#endif // LLVM_LIBC_TYPES_STRUCT_EPOLL_EVENT_H

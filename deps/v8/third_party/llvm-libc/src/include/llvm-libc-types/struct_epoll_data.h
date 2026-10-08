//===-- Definition of epoll_data type -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_EPOLL_DATA_H
#define LLVM_LIBC_TYPES_STRUCT_EPOLL_DATA_H

union epoll_data {
  void *ptr;
  int fd;
  __UINT32_TYPE__ u32;
  __UINT64_TYPE__ u64;
};

typedef union epoll_data epoll_data_t;

#endif // LLVM_LIBC_TYPES_STRUCT_EPOLL_DATA_H

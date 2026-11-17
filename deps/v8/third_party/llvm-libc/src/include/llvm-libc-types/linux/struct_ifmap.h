//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct ifmap for Linux.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LINUX_STRUCT_IFMAP_H
#define LLVM_LIBC_TYPES_LINUX_STRUCT_IFMAP_H

// Prevent the linux headers from defining this type.
#define __UAPI_DEF_IF_IFMAP 0

struct ifmap {
  unsigned long mem_start;
  unsigned long mem_end;
  unsigned short base_addr;
  unsigned char irq;
  unsigned char dma;
  unsigned char port;
};

#endif // LLVM_LIBC_TYPES_LINUX_STRUCT_IFMAP_H

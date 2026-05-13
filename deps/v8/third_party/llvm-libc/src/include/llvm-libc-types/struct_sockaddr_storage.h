//===-- Definition of struct sockaddr_storage -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_STRUCT_SOCKADDR_STORAGE_H
#define LLVM_LIBC_TYPES_STRUCT_SOCKADDR_STORAGE_H

#include "sa_family_t.h"

// A struct large (and aligned) enough to accomodate all supported
// protocol-specific address structures.
struct __attribute__((may_alias)) sockaddr_storage {
  sa_family_t ss_family;
  union {
    char __ss_padding[128 - sizeof(sa_family_t)]; // Ensures size.
    long __ss_align;                              // Ensures alignment.
  };
};

#endif // LLVM_LIBC_TYPES_STRUCT_SOCKADDR_STORAGE_H

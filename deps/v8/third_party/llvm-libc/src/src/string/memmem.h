//===-- Implementation header for memmem ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMMEM_H
#define LLVM_LIBC_SRC_STRING_MEMMEM_H

#include "src/__support/macros/config.h"
#include <stddef.h> // For size_t

namespace LIBC_NAMESPACE_DECL {

void *memmem(const void *haystack, size_t haystack_len, const void *needle,
             size_t needle_len);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMMEM_H

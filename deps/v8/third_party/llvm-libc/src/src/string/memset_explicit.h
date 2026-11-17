//===-- Implementation header for memset_explicit ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMSET_EXPLICIT_H
#define LLVM_LIBC_SRC_STRING_MEMSET_EXPLICIT_H

#include "src/__support/macros/config.h"
#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[gnu::noinline]] void *memset_explicit(void *ptr, int value, size_t count);

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMSET_EXPLICIT_H

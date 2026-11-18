//===-- Implementation of bzero -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_BZERO_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_BZERO_H

#include "src/__support/common.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/inline_memset.h"

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

LIBC_FLATTEN LIBC_INLINE static void inline_bzero(Ptr dst, size_t count) {
  inline_memset(dst, 0, count);
}

LIBC_FLATTEN LIBC_INLINE static void inline_bzero(void *dst, size_t count) {
  inline_bzero(reinterpret_cast<Ptr>(dst), count);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_BZERO_H

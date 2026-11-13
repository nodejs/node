//===-- Implementation of memcmp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCMP_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCMP_H

#include "src/__support/macros/attributes.h"               // LIBC_INLINE
#include "src/__support/macros/properties/architectures.h" // LIBC_TARGET_ARCH_IS_
#include "src/string/memory_utils/utils.h"                 // Ptr, CPtr

#include <stddef.h> // size_t

#if defined(LIBC_COPT_USE_MEM_BUILTINS)
#include "src/string/memory_utils/generic/builtin.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP inline_memcmp_builtin
#elif defined(LIBC_TARGET_ARCH_IS_X86)
#include "src/string/memory_utils/x86_64/inline_memcmp.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP inline_memcmp_x86
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "src/string/memory_utils/aarch64/inline_memcmp.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP inline_memcmp_aarch64_dispatch
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "src/string/memory_utils/riscv/inline_memcmp.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP inline_memcmp_riscv
#else
#include "src/string/memory_utils/generic/byte_per_byte.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP inline_memcmp_byte_per_byte
#endif

namespace LIBC_NAMESPACE_DECL {

LIBC_FLATTEN LIBC_INLINE int inline_memcmp(const void *p1, const void *p2,
                                           size_t count) {
  return static_cast<int>(LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP(
      reinterpret_cast<CPtr>(p1), reinterpret_cast<CPtr>(p2), count));
}

} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_SRC_STRING_MEMORY_UTILS_MEMCMP

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCMP_H

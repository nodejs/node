//===-- Memset implementation for riscv -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMSET_H
#define LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMSET_H

#include "src/__support/macros/attributes.h"               // LIBC_INLINE
#include "src/__support/macros/config.h" // LIBC_NAMESPACE_DECL
#include "src/__support/macros/properties/architectures.h" // LIBC_TARGET_ARCH_IS_RISCV64
#include "src/string/memory_utils/generic/aligned_access.h"
#include "src/string/memory_utils/utils.h" // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE static void inline_memset_riscv(Ptr dst, uint8_t value,
                                            size_t count) {
#if defined(LIBC_TARGET_ARCH_IS_RISCV64)
  return inline_memset_aligned_access_64bit(dst, value, count);
#elif defined(LIBC_TARGET_ARCH_IS_RISCV32)
  return inline_memset_aligned_access_32bit(dst, value, count);
#else
#error "Unimplemented"
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMSET_H

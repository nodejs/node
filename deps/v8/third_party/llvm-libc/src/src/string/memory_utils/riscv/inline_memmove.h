//===-- Memmove implementation for riscv ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMMOVE_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMMOVE_H

#include "src/__support/macros/attributes.h"               // LIBC_INLINE
#include "src/__support/macros/config.h" // LIBC_NAMESPACE_DECL
#include "src/__support/macros/properties/architectures.h" // LIBC_TARGET_ARCH_IS_RISCV64
#include "src/string/memory_utils/generic/byte_per_byte.h"
#include "src/string/memory_utils/utils.h" // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE void
inline_memmove_riscv(Ptr __restrict dst, CPtr __restrict src, size_t count) {
  return inline_memmove_byte_per_byte(dst, src, count);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_RISCV_INLINE_MEMMOVE_H

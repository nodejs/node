//===-- Memcpy implementation -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCPY_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCPY_H

#include "src/__support/macros/attributes.h"               // LIBC_INLINE
#include "src/__support/macros/properties/architectures.h" // LIBC_TARGET_ARCH_IS_
#include "src/string/memory_utils/utils.h"                 // Ptr, CPtr

#include <stddef.h> // size_t

#if defined(LIBC_COPT_USE_MEM_BUILTINS)
#include "src/string/memory_utils/generic/builtin.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_builtin
#elif defined(LIBC_COPT_MEMCPY_USE_EMBEDDED_TINY)
#include "src/string/memory_utils/generic/byte_per_byte.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_byte_per_byte
#elif defined(LIBC_TARGET_ARCH_IS_X86)
#include "src/string/memory_utils/x86_64/inline_memcpy.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY                                    \
  inline_memcpy_x86_maybe_interpose_repmovsb
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
#include "src/string/memory_utils/arm/inline_memcpy.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_arm
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "src/string/memory_utils/aarch64/inline_memcpy.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_aarch64
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "src/string/memory_utils/riscv/inline_memcpy.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_riscv
#elif defined(LIBC_TARGET_ARCH_IS_GPU) || defined(LIBC_TARGET_ARCH_IS_WASM)
#include "src/string/memory_utils/generic/builtin.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_builtin
#else
#include "src/string/memory_utils/generic/byte_per_byte.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY inline_memcpy_byte_per_byte
#endif

namespace LIBC_NAMESPACE_DECL {

LIBC_FLATTEN LIBC_INLINE void
inline_memcpy(void *__restrict dst, const void *__restrict src, size_t count) {
  LIBC_SRC_STRING_MEMORY_UTILS_MEMCPY(reinterpret_cast<Ptr>(dst),
                                      reinterpret_cast<CPtr>(src), count);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMCPY_H

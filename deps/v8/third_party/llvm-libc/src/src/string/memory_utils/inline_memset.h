//===-- Implementation of memset and bzero --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMSET_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMSET_H

#include "src/__support/macros/attributes.h"               // LIBC_INLINE
#include "src/__support/macros/properties/architectures.h" // LIBC_TARGET_ARCH_IS_
#include "src/string/memory_utils/utils.h"                 // Ptr, CPtr

#include <stddef.h> // size_t

#if defined(LIBC_COPT_USE_MEM_BUILTINS)
#include "src/string/memory_utils/generic/builtin.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_builtin
#elif defined(LIBC_TARGET_ARCH_IS_X86)
#include "src/string/memory_utils/x86_64/inline_memset.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_x86
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
#include "src/string/memory_utils/arm/inline_memset.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_arm_dispatch
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
#include "src/string/memory_utils/aarch64/inline_memset.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_aarch64_dispatch
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#include "src/string/memory_utils/riscv/inline_memset.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_riscv
#elif defined(LIBC_TARGET_ARCH_IS_GPU) || defined(LIBC_TARGET_ARCH_IS_WASM)
#include "src/string/memory_utils/generic/builtin.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_builtin
#else
#include "src/string/memory_utils/generic/byte_per_byte.h"
#define LIBC_SRC_STRING_MEMORY_UTILS_MEMSET inline_memset_byte_per_byte
#endif

namespace LIBC_NAMESPACE_DECL {

LIBC_FLATTEN LIBC_INLINE void inline_memset(void *dst, uint8_t value,
                                            size_t count) {
  LIBC_SRC_STRING_MEMORY_UTILS_MEMSET(reinterpret_cast<Ptr>(dst), value, count);
}

} // namespace LIBC_NAMESPACE_DECL

#undef LIBC_SRC_STRING_MEMORY_UTILS_MEMSET

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_MEMSET_H

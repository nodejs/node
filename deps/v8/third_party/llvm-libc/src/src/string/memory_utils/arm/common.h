//===-- Common constants and defines for arm --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_COMMON_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_COMMON_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE_VAR
#include "src/string/memory_utils/utils.h"   // CPtr, Ptr, distance_to_align

#include <stddef.h> // size_t

// Our minimum supported compiler version does not recognize the standard
// [[likely]] / [[unlikely]] attributes so we use the preprocessor.

// https://libc.llvm.org/compiler_support.html
// Support for [[likely]] / [[unlikely]]
//  [X] GCC 12.2
//  [X] Clang 12
//  [ ] Clang 11
#define LIBC_ATTR_LIKELY [[likely]]
#define LIBC_ATTR_UNLIKELY [[unlikely]]

#if defined(LIBC_COMPILER_IS_CLANG)
#if LIBC_COMPILER_CLANG_VER < 1200
#undef LIBC_ATTR_LIKELY
#undef LIBC_ATTR_UNLIKELY
#define LIBC_ATTR_LIKELY
#define LIBC_ATTR_UNLIKELY
#endif
#endif

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE_VAR constexpr size_t kWordSize = sizeof(uint32_t);

enum class AssumeAccess { kUnknown, kAligned };
enum class BlockOp { kFull, kByWord };

LIBC_INLINE auto misaligned(CPtr ptr) {
  return distance_to_align_down<kWordSize>(ptr);
}

LIBC_INLINE CPtr bitwise_or(CPtr a, CPtr b) {
  return cpp::bit_cast<CPtr>(cpp::bit_cast<uintptr_t>(a) |
                             cpp::bit_cast<uintptr_t>(b));
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_COMMON_H

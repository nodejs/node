//===-- Implementation using the __builtin_XXX_inline ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides generic C++ building blocks to compose memory functions.
// They rely on the compiler to generate the best possible code through the use
// of the `__builtin_XXX_inline` builtins. These builtins are currently only
// available in Clang.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_BUILTIN_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_BUILTIN_H

#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/utils.h"

namespace LIBC_NAMESPACE_DECL {
namespace builtin {

///////////////////////////////////////////////////////////////////////////////
// Memcpy
template <size_t Size> struct Memcpy {
  static constexpr size_t SIZE = Size;
  LIBC_INLINE static void block_offset(Ptr __restrict dst, CPtr __restrict src,
                                       size_t offset) {
    memcpy_inline<Size>(dst + offset, src + offset);
  }

  LIBC_INLINE static void block(Ptr __restrict dst, CPtr __restrict src) {
    block_offset(dst, src, 0);
  }

  LIBC_INLINE static void tail(Ptr __restrict dst, CPtr __restrict src,
                               size_t count) {
    block_offset(dst, src, count - SIZE);
  }

  LIBC_INLINE static void head_tail(Ptr __restrict dst, CPtr __restrict src,
                                    size_t count) {
    block(dst, src);
    tail(dst, src, count);
  }

  LIBC_INLINE static void loop_and_tail_offset(Ptr __restrict dst,
                                               CPtr __restrict src,
                                               size_t count, size_t offset) {
    static_assert(Size > 1, "a loop of size 1 does not need tail");
    do {
      block_offset(dst, src, offset);
      offset += SIZE;
    } while (offset < count - SIZE);
    tail(dst, src, count);
  }

  LIBC_INLINE static void loop_and_tail(Ptr __restrict dst, CPtr __restrict src,
                                        size_t count) {
    return loop_and_tail_offset(dst, src, count, 0);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Memset
template <size_t Size> struct Memset {
  using ME = Memset;
  static constexpr size_t SIZE = Size;
  LIBC_INLINE static void block(Ptr dst, uint8_t value) {
#ifdef LLVM_LIBC_HAS_BUILTIN_MEMSET_INLINE
    __builtin_memset_inline(dst, value, Size);
#else
    static_assert(cpp::always_false<decltype(Size)>,
                  "Missing __builtin_memset_inline");
    (void)dst;
    (void)value;
#endif
  }

  LIBC_INLINE static void tail(Ptr dst, uint8_t value, size_t count) {
    block(dst + count - SIZE, value);
  }

  LIBC_INLINE static void head_tail(Ptr dst, uint8_t value, size_t count) {
    block(dst, value);
    tail(dst, value, count);
  }

  LIBC_INLINE static void loop_and_tail(Ptr dst, uint8_t value, size_t count) {
    static_assert(Size > 1, "a loop of size 1 does not need tail");
    size_t offset = 0;
    do {
      block(dst + offset, value);
      offset += SIZE;
    } while (offset < count - SIZE);
    tail(dst, value, count);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Bcmp
template <size_t Size> struct Bcmp {
  using ME = Bcmp;
  static constexpr size_t SIZE = Size;
  LIBC_INLINE static BcmpReturnType block(CPtr, CPtr) {
    static_assert(cpp::always_false<decltype(Size)>,
                  "Missing __builtin_memcmp_inline");
    return BcmpReturnType::zero();
  }

  LIBC_INLINE static BcmpReturnType tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return BcmpReturnType::zero();
  }

  LIBC_INLINE static BcmpReturnType head_tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return BcmpReturnType::zero();
  }

  LIBC_INLINE static BcmpReturnType loop_and_tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return BcmpReturnType::zero();
  }
};

///////////////////////////////////////////////////////////////////////////////
// Memcmp
template <size_t Size> struct Memcmp {
  using ME = Memcmp;
  static constexpr size_t SIZE = Size;
  LIBC_INLINE static MemcmpReturnType block(CPtr, CPtr) {
    static_assert(cpp::always_false<decltype(Size)>,
                  "Missing __builtin_memcmp_inline");
    return MemcmpReturnType::zero();
  }

  LIBC_INLINE static MemcmpReturnType tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return MemcmpReturnType::zero();
  }

  LIBC_INLINE static MemcmpReturnType head_tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return MemcmpReturnType::zero();
  }

  LIBC_INLINE static MemcmpReturnType loop_and_tail(CPtr, CPtr, size_t) {
    static_assert(cpp::always_false<decltype(Size)>, "Not implemented");
    return MemcmpReturnType::zero();
  }
};

} // namespace builtin
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_BUILTIN_H

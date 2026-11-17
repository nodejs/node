//===-- Memmove implementation for x86_64 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMMOVE_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMMOVE_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/op_x86.h"
#include "src/string/memory_utils/utils.h"

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

LIBC_INLINE bool inline_memmove_small_size_x86(Ptr dst, CPtr src,
                                               size_t count) {
#if defined(__AVX512F__)
  constexpr size_t vector_size = 64;
  using uint128_t = generic_v128;
  using uint256_t = generic_v256;
  using uint512_t = generic_v512;
#elif defined(__AVX__)
  constexpr size_t vector_size = 32;
  using uint128_t = generic_v128;
  using uint256_t = generic_v256;
  using uint512_t = cpp::array<generic_v256, 2>;
#elif defined(__SSE2__)
  constexpr size_t vector_size = 16;
  using uint128_t = generic_v128;
  using uint256_t = cpp::array<generic_v128, 2>;
  using uint512_t = cpp::array<generic_v128, 4>;
#else
  constexpr size_t vector_size = 8;
  using uint128_t = cpp::array<uint64_t, 2>;
  using uint256_t = cpp::array<uint64_t, 4>;
  using uint512_t = cpp::array<uint64_t, 8>;
#endif
  (void)vector_size;
  if (count == 0)
    return true;
  if (count == 1) {
    generic::Memmove<uint8_t>::block(dst, src);
    return true;
  }
  if (count == 2) {
    generic::Memmove<uint16_t>::block(dst, src);
    return true;
  }
  if (count == 3) {
    generic::Memmove<cpp::array<uint8_t, 3>>::block(dst, src);
    return true;
  }
  if (count == 4) {
    generic::Memmove<uint32_t>::block(dst, src);
    return true;
  }
  if (count < 8) {
    generic::Memmove<uint32_t>::head_tail(dst, src, count);
    return true;
  }
  // If count is equal to a power of 2, we can handle it as head-tail
  // of both smaller size and larger size (head-tail are either
  // non-overlapping for smaller size, or completely collapsed
  // for larger size). It seems to be more profitable to do the copy
  // with the larger size, if it's natively supported (e.g. doing
  // 2 collapsed 32-byte moves for count=64 if AVX2 is supported).
  // But it's not profitable to use larger size if it's not natively
  // supported: we will both use more instructions and handle fewer
  // sizes in earlier branches.
  if (vector_size >= 16 ? count < 16 : count <= 16) {
    generic::Memmove<uint64_t>::head_tail(dst, src, count);
    return true;
  }
  if (vector_size >= 32 ? count < 32 : count <= 32) {
    generic::Memmove<uint128_t>::head_tail(dst, src, count);
    return true;
  }
  if (vector_size >= 64 ? count < 64 : count <= 64) {
    generic::Memmove<uint256_t>::head_tail(dst, src, count);
    return true;
  }
  if (count <= 128) {
    generic::Memmove<uint512_t>::head_tail(dst, src, count);
    return true;
  }
  return false;
}

LIBC_INLINE void inline_memmove_follow_up_x86(Ptr dst, CPtr src, size_t count) {
#if defined(__AVX512F__)
  using uint256_t = generic_v256;
  using uint512_t = generic_v512;
#elif defined(__AVX__)
  using uint256_t = generic_v256;
  using uint512_t = cpp::array<generic_v256, 2>;
#elif defined(__SSE2__)
  using uint256_t = cpp::array<generic_v128, 2>;
  using uint512_t = cpp::array<generic_v128, 4>;
#else
  using uint256_t = cpp::array<uint64_t, 4>;
  using uint512_t = cpp::array<uint64_t, 8>;
#endif
  if (dst < src) {
    generic::Memmove<uint256_t>::align_forward<Arg::Src>(dst, src, count);
    return generic::Memmove<uint512_t>::loop_and_tail_forward(dst, src, count);
  } else {
    generic::Memmove<uint256_t>::align_backward<Arg::Src>(dst, src, count);
    return generic::Memmove<uint512_t>::loop_and_tail_backward(dst, src, count);
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMMOVE_H

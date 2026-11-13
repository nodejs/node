//===-- Memset implementation for aarch64 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMSET_H
#define LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMSET_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/op_aarch64.h"
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/utils.h" // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

using uint128_t = generic_v128;
using uint256_t = generic_v256;
using uint512_t = generic_v512;

[[maybe_unused]] LIBC_INLINE static void
inline_memset_aarch64_no_fp(Ptr dst, uint8_t value, size_t count) {
  if (count == 0)
    return;
  if (count <= 3) {
    generic::Memset<uint8_t>::block(dst, value);
    if (count > 1)
      generic::Memset<uint16_t>::tail(dst, value, count);
    return;
  }
  if (count <= 8)
    return generic::Memset<uint32_t>::head_tail(dst, value, count);
  if (count <= 16)
    return generic::Memset<uint64_t>::head_tail(dst, value, count);
  if (count <= 32)
    return generic::Memset<uint128_t>::head_tail(dst, value, count);
  if (count <= (32 + 64)) {
    generic::Memset<uint256_t>::block(dst, value);
    if (count <= 64)
      return generic::Memset<uint256_t>::tail(dst, value, count);
    generic::Memset<uint256_t>::block(dst + 32, value);
    generic::Memset<uint256_t>::tail(dst, value, count);
    return;
  }

  generic::Memset<uint128_t>::block(dst, value);
  align_to_next_boundary<16>(dst, count);
  return generic::Memset<uint512_t>::loop_and_tail(dst, value, count);
}

#if defined(__ARM_NEON)
[[maybe_unused]] LIBC_INLINE static void
inline_memset_aarch64_with_fp(Ptr dst, uint8_t value, size_t count) {
  if (count == 0)
    return;
  if (count <= 3) {
    generic::Memset<uint8_t>::block(dst, value);
    if (count > 1)
      generic::Memset<uint16_t>::tail(dst, value, count);
    return;
  }
  if (count <= 8)
    return generic::Memset<uint32_t>::head_tail(dst, value, count);
  if (count <= 16)
    return generic::Memset<uint64_t>::head_tail(dst, value, count);
  if (count <= 32)
    return generic::Memset<uint128_t>::head_tail(dst, value, count);
  if (count <= (32 + 64)) {
    generic::Memset<uint256_t>::block(dst, value);
    if (count <= 64)
      return generic::Memset<uint256_t>::tail(dst, value, count);
    generic::Memset<uint256_t>::block(dst + 32, value);
    generic::Memset<uint256_t>::tail(dst, value, count);
    return;
  }

  if (count >= 448 && value == 0 && aarch64::neon::hasZva()) {
    generic::Memset<uint512_t>::block(dst, 0);
    align_to_next_boundary<64>(dst, count);
    return aarch64::neon::BzeroCacheLine::loop_and_tail(dst, 0, count);
  }

  generic::Memset<uint128_t>::block(dst, value);
  align_to_next_boundary<16>(dst, count);
  return generic::Memset<uint512_t>::loop_and_tail(dst, value, count);
}
#endif

LIBC_UNUSED LIBC_FLATTEN LIBC_INLINE static void
inline_memset_aarch64_dispatch(Ptr dst, uint8_t value, size_t count) {
#if defined(__ARM_NEON)
  return inline_memset_aarch64_with_fp(dst, value, count);
#else
  return inline_memset_aarch64_no_fp(dst, value, count);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMSET_H

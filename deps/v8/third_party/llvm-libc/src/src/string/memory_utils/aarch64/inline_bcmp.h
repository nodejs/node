//===-- Bcmp implementation for aarch64 -------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_BCMP_H
#define LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_BCMP_H

#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/config.h"       // LIBC_NAMESPACE_DECL
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/string/memory_utils/op_aarch64.h"
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/utils.h" // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_aarch64_no_fp(CPtr p1, CPtr p2, size_t count) {
  if (LIBC_LIKELY(count < 16)) {
    switch (count) {
    case 0:
      return BcmpReturnType::zero();
    case 1:
      return generic::Bcmp<uint8_t>::block(p1, p2);
    case 2:
      return generic::Bcmp<uint16_t>::block(p1, p2);
    case 3:
      return generic::Bcmp<uint16_t>::head_tail(p1, p2, count);
    case 4:
      return generic::Bcmp<uint32_t>::block(p1, p2);
    case 5:
    case 6:
    case 7:
      return generic::Bcmp<uint32_t>::head_tail(p1, p2, count);
    case 8:
      return generic::Bcmp<uint64_t>::block(p1, p2);
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      return generic::Bcmp<uint64_t>::head_tail(p1, p2, count);
    }
  }

  return generic::Bcmp<uint64_t>::loop_and_tail_align_above(256, p1, p2, count);
}

#ifdef __ARM_NEON
[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_aarch64_with_fp(CPtr p1, CPtr p2, size_t count) {
  if (LIBC_LIKELY(count <= 32)) {
    if (LIBC_UNLIKELY(count >= 16)) {
      return aarch64::Bcmp<16>::head_tail(p1, p2, count);
    }
    switch (count) {
    case 0:
      return BcmpReturnType::zero();
    case 1:
      return generic::Bcmp<uint8_t>::block(p1, p2);
    case 2:
      return generic::Bcmp<uint16_t>::block(p1, p2);
    case 3:
      return generic::Bcmp<uint16_t>::head_tail(p1, p2, count);
    case 4:
      return generic::Bcmp<uint32_t>::block(p1, p2);
    case 5:
    case 6:
    case 7:
      return generic::Bcmp<uint32_t>::head_tail(p1, p2, count);
    case 8:
      return generic::Bcmp<uint64_t>::block(p1, p2);
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      return generic::Bcmp<uint64_t>::head_tail(p1, p2, count);
    }
  }

  if (count <= 64)
    return aarch64::Bcmp<32>::head_tail(p1, p2, count);

  // Aligned loop if > 256, otherwise normal loop
  if (LIBC_UNLIKELY(count > 256)) {
    if (auto value = aarch64::Bcmp<32>::block(p1, p2))
      return value;
    align_to_next_boundary<16, Arg::P1>(p1, p2, count);
  }
  return aarch64::Bcmp<32>::loop_and_tail(p1, p2, count);
}
#endif

LIBC_FLATTEN LIBC_INLINE BcmpReturnType
inline_bcmp_aarch64_dispatch(CPtr p1, CPtr p2, size_t count) {
#if defined(__ARM_NEON)
  return inline_bcmp_aarch64_with_fp(p1, p2, count);
#else
  return inline_bcmp_aarch64_no_fp(p1, p2, count);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_BCMP_H

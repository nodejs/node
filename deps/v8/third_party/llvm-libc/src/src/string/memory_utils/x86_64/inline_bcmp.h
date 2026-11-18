//===-- Bcmp implementation for x86_64 --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_BCMP_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_BCMP_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/op_x86.h"
#include "src/string/memory_utils/utils.h" // Ptr, CPtr

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_generic_gt16(CPtr p1, CPtr p2, size_t count) {
  return generic::Bcmp<uint64_t>::loop_and_tail_align_above(256, p1, p2, count);
}

#if defined(__SSE4_1__)
[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_x86_sse41_gt16(CPtr p1, CPtr p2, size_t count) {
  if (count <= 32)
    return generic::Bcmp<__m128i>::head_tail(p1, p2, count);
  return generic::Bcmp<__m128i>::loop_and_tail_align_above(256, p1, p2, count);
}
#endif // __SSE4_1__

#if defined(__AVX__)
[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_x86_avx_gt16(CPtr p1, CPtr p2, size_t count) {
  if (count <= 32)
    return generic::Bcmp<__m128i>::head_tail(p1, p2, count);
  if (count <= 64)
    return generic::Bcmp<__m256i>::head_tail(p1, p2, count);
  return generic::Bcmp<__m256i>::loop_and_tail_align_above(256, p1, p2, count);
}
#endif // __AVX__

#if defined(__AVX512BW__)
[[maybe_unused]] LIBC_INLINE BcmpReturnType
inline_bcmp_x86_avx512bw_gt16(CPtr p1, CPtr p2, size_t count) {
  if (count <= 32)
    return generic::Bcmp<__m128i>::head_tail(p1, p2, count);
  if (count <= 64)
    return generic::Bcmp<__m256i>::head_tail(p1, p2, count);
  if (count <= 128)
    return generic::Bcmp<__m512i>::head_tail(p1, p2, count);
  return generic::Bcmp<__m512i>::loop_and_tail_align_above(256, p1, p2, count);
}
#endif // __AVX512BW__

[[maybe_unused]] LIBC_INLINE BcmpReturnType inline_bcmp_x86(CPtr p1, CPtr p2,
                                                            size_t count) {
  if (count == 0)
    return BcmpReturnType::zero();
  if (count == 1)
    return generic::Bcmp<uint8_t>::block(p1, p2);
  if (count == 2)
    return generic::Bcmp<uint16_t>::block(p1, p2);
  if (count == 3)
    return generic::BcmpSequence<uint16_t, uint8_t>::block(p1, p2);
  if (count == 4)
    return generic::Bcmp<uint32_t>::block(p1, p2);
  if (count == 5)
    return generic::BcmpSequence<uint32_t, uint8_t>::block(p1, p2);
  if (count == 6)
    return generic::BcmpSequence<uint32_t, uint16_t>::block(p1, p2);
  if (count == 7)
    return generic::BcmpSequence<uint32_t, uint16_t, uint8_t>::block(p1, p2);
  if (count == 8)
    return generic::Bcmp<uint64_t>::block(p1, p2);
  if (count <= 16)
    return generic::Bcmp<uint64_t>::head_tail(p1, p2, count);
#if defined(__AVX512BW__)
  return inline_bcmp_x86_avx512bw_gt16(p1, p2, count);
#elif defined(__AVX__)
  return inline_bcmp_x86_avx_gt16(p1, p2, count);
#elif defined(__SSE4_1__)
  return inline_bcmp_x86_sse41_gt16(p1, p2, count);
#else
  return inline_bcmp_generic_gt16(p1, p2, count);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_BCMP_H

//===-- Memcmp implementation for x86_64 ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCMP_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCMP_H

#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/string/memory_utils/op_generic.h"
#include "src/string/memory_utils/op_x86.h"
#include "src/string/memory_utils/utils.h" // MemcmpReturnType

namespace LIBC_NAMESPACE_DECL {

[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_generic_gt16(CPtr p1, CPtr p2, size_t count) {
  return generic::Memcmp<uint64_t>::loop_and_tail_align_above(384, p1, p2,
                                                              count);
}

#if defined(__SSE4_1__)
[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_x86_sse41_gt16(CPtr p1, CPtr p2, size_t count) {
  return generic::Memcmp<__m128i>::loop_and_tail_align_above(384, p1, p2,
                                                             count);
}
#endif // __SSE4_1__

#if defined(__AVX2__)
[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_x86_avx2_gt16(CPtr p1, CPtr p2, size_t count) {
  if (count <= 32)
    return generic::Memcmp<__m128i>::head_tail(p1, p2, count);
  if (count <= 64)
    return generic::Memcmp<__m256i>::head_tail(p1, p2, count);
  return generic::Memcmp<__m256i>::loop_and_tail_align_above(384, p1, p2,
                                                             count);
}
#endif // __AVX2__

#if defined(__AVX512BW__)
[[maybe_unused]] LIBC_INLINE MemcmpReturnType
inline_memcmp_x86_avx512bw_gt16(CPtr p1, CPtr p2, size_t count) {
  if (count <= 32)
    return generic::Memcmp<__m128i>::head_tail(p1, p2, count);
  if (count <= 64)
    return generic::Memcmp<__m256i>::head_tail(p1, p2, count);
  if (count <= 128)
    return generic::Memcmp<__m512i>::head_tail(p1, p2, count);
  return generic::Memcmp<__m512i>::loop_and_tail_align_above(384, p1, p2,
                                                             count);
}
#endif // __AVX512BW__

LIBC_INLINE MemcmpReturnType inline_memcmp_x86(CPtr p1, CPtr p2, size_t count) {
  if (count == 0)
    return MemcmpReturnType::zero();
  if (count == 1)
    return generic::Memcmp<uint8_t>::block(p1, p2);
  if (count == 2)
    return generic::Memcmp<uint16_t>::block(p1, p2);
  if (count == 3)
    return generic::MemcmpSequence<uint16_t, uint8_t>::block(p1, p2);
  if (count == 4)
    return generic::Memcmp<uint32_t>::block(p1, p2);
  if (count == 5)
    return generic::MemcmpSequence<uint32_t, uint8_t>::block(p1, p2);
  if (count == 6)
    return generic::MemcmpSequence<uint32_t, uint16_t>::block(p1, p2);
  if (count == 7)
    return generic::Memcmp<uint32_t>::head_tail(p1, p2, 7);
  if (count == 8)
    return generic::Memcmp<uint64_t>::block(p1, p2);
  if (count <= 16)
    return generic::Memcmp<uint64_t>::head_tail(p1, p2, count);
#if defined(__AVX512BW__)
  return inline_memcmp_x86_avx512bw_gt16(p1, p2, count);
#elif defined(__AVX2__)
  return inline_memcmp_x86_avx2_gt16(p1, p2, count);
#elif defined(__SSE4_1__)
  return inline_memcmp_x86_sse41_gt16(p1, p2, count);
#else
  return inline_memcmp_generic_gt16(p1, p2, count);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCMP_H

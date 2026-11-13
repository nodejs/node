//===-- Memcpy implementation for aarch64 -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMCPY_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMCPY_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/properties/cpu_features.h"
#include "src/string/memory_utils/generic/aligned_access.h"
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/utils.h"

#include <stddef.h> // size_t

#if defined(LIBC_TARGET_CPU_HAS_SVE)
#include <arm_sve.h>
#endif
namespace LIBC_NAMESPACE_DECL {
[[maybe_unused]] LIBC_INLINE void
inline_memcpy_aarch64(Ptr __restrict dst, CPtr __restrict src, size_t count) {
  // Always avoid emit any memory operation if count == 0.
  if (count == 0)
    return;
  // Use predicated load/store on SVE available targets to avoid branching in
  // small cases.
#ifdef LIBC_TARGET_CPU_HAS_SVE
  auto src_ptr = reinterpret_cast<const uint8_t *>(src);
  auto dst_ptr = reinterpret_cast<uint8_t *>(dst);
  if (count <= 16) {
    const svbool_t mask = svwhilelt_b8_u64(0, count);
    svst1_u8(mask, dst_ptr, svld1_u8(mask, src_ptr));
    return;
  }
  if (count <= 32) {
    const size_t vlen = svcntb();
    svbool_t m0 = svwhilelt_b8_u64(0, count);
    svbool_t m1 = svwhilelt_b8_u64(vlen, count);
    svst1_u8(m0, dst_ptr, svld1_u8(m0, src_ptr));
    svst1_u8(m1, dst_ptr + vlen, svld1_u8(m1, src_ptr + vlen));
    return;
  }
#else
  if (count == 1)
    return builtin::Memcpy<1>::block(dst, src);
  if (count == 2)
    return builtin::Memcpy<2>::block(dst, src);
  if (count == 3)
    return builtin::Memcpy<3>::block(dst, src);
  if (count == 4)
    return builtin::Memcpy<4>::block(dst, src);
  if (count < 8)
    return builtin::Memcpy<4>::head_tail(dst, src, count);
  if (count < 16)
    return builtin::Memcpy<8>::head_tail(dst, src, count);
  if (count < 32)
    return builtin::Memcpy<16>::head_tail(dst, src, count);
#endif
  if (count < 64)
    return builtin::Memcpy<32>::head_tail(dst, src, count);
  if (count < 128)
    return builtin::Memcpy<64>::head_tail(dst, src, count);
#if !defined(__ARM_FEATURE_UNALIGNED)
  // When unaligned access is disabled (-mstrict-align), align_to_next_boundary
  // below only aligns `src`. Because `dst` has type `cpp::byte*`, LLVM cannot
  // statically prove that `dst` is aligned to more than 1 byte. Under strict
  // alignment, this forces the compiler to lower the bulk copy loop to
  // single-byte load and store instructions to prevent unaligned access faults,
  // causing severe performance degradation.
  //
  // Instead, use inline_memcpy_aligned_access_64bit which explicitly aligns
  // `dst` to an 8-byte boundary so 64-bit word stores can be safely used.
  return inline_memcpy_aligned_access_64bit(dst, src, count);
#else
  // When hardware unaligned access is supported, aligning only `src` maximizes
  // streaming read throughput while hardware handles unaligned vector stores to
  // `dst` at full speed.
  builtin::Memcpy<16>::block(dst, src);
  align_to_next_boundary<16, Arg::Src>(dst, src, count);
  return builtin::Memcpy<64>::loop_and_tail(dst, src, count);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_AARCH64_INLINE_MEMCPY_H

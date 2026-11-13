//===-- Memcpy implementation for x86_64 ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCPY_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCPY_H

#include <stddef.h> // size_t

#include "hdr/stdint_proxy.h"                // SIZE_MAX
#include "src/__support/macros/attributes.h" // LIBC_INLINE_VAR
#include "src/__support/macros/is_defined.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/op_x86.h"
#include "src/string/memory_utils/utils.h"

#ifdef LLVM_LIBC_MEMCPY_X86_USE_ONLY_REPMOVSB
#error LLVM_LIBC_MEMCPY_X86_USE_ONLY_REPMOVSB is deprecated use LIBC_COPT_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE=0 instead.
#endif // LLVM_LIBC_MEMCPY_X86_USE_ONLY_REPMOVSB

#ifdef LLVM_LIBC_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE
#error LLVM_LIBC_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE is deprecated use LIBC_COPT_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE=0 instead.
#endif // LLVM_LIBC_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE

namespace LIBC_NAMESPACE_DECL {
namespace x86 {

LIBC_INLINE_VAR constexpr size_t K_ONE_CACHELINE = 64;
LIBC_INLINE_VAR constexpr size_t K_TWO_CACHELINES = 2 * K_ONE_CACHELINE;
LIBC_INLINE_VAR constexpr size_t K_THREE_CACHELINES = 3 * K_ONE_CACHELINE;

LIBC_INLINE_VAR constexpr bool K_USE_SOFTWARE_PREFETCHING =
    LLVM_LIBC_IS_DEFINED(LIBC_COPT_MEMCPY_X86_USE_SOFTWARE_PREFETCHING);

// Whether to use NTA stores and what threshold for switching to NTA
#if defined(LIBC_COPT_MEMCPY_X86_USE_NTA_STORES) && defined(__AVX__)
// Mostly based on empirical data. Theoretical justification:
// upper bound of L2 size is 1MB on most x86 machines.
LIBC_INLINE_VAR constexpr size_t K_NTA_THRESHOLD = 1 << 20;
#else
LIBC_INLINE_VAR constexpr size_t K_NTA_THRESHOLD = 0;
#endif

// Whether to use rep;movsb exclusively (0), not at all (SIZE_MAX), or only
// above a certain threshold. Defaults to "do not use rep;movsb".
#ifndef LIBC_COPT_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE
#define LIBC_COPT_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE SIZE_MAX
#endif
LIBC_INLINE_VAR constexpr size_t K_REP_MOVSB_THRESHOLD =
    LIBC_COPT_MEMCPY_X86_USE_REPMOVSB_FROM_SIZE;

// The code using __m256i is if constexpr unreachable, but not entirely #if'd
// out to avoid bitrot when compiling all the permutations.  However, the
// __m256i type doesn't exist at all without AVX and an unknown type name can't
// appear even in an if constexpr unreachable context.  So define an alias or
// stub type to use in the AVX-specific code below.
#ifdef __AVX__
using m256i = __m256i;
#else
struct alignas(32) m256i {
  uint64_t x[256 / 64];
};
#endif

} // namespace x86

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86_sse2_ge64(Ptr __restrict dst, CPtr __restrict src,
                            size_t count) {
  if (count <= 128)
    return builtin::Memcpy<64>::head_tail(dst, src, count);
  builtin::Memcpy<32>::block(dst, src);
  align_to_next_boundary<32, Arg::Dst>(dst, src, count);
  return builtin::Memcpy<32>::loop_and_tail(dst, src, count);
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86_avx_ge64(Ptr __restrict dst, CPtr __restrict src,
                           size_t count) {
  if (count <= 128)
    return builtin::Memcpy<64>::head_tail(dst, src, count);
  if (count < 256)
    return builtin::Memcpy<128>::head_tail(dst, src, count);
  builtin::Memcpy<32>::block(dst, src);
  align_to_next_boundary<32, Arg::Dst>(dst, src, count);
  return builtin::Memcpy<64>::loop_and_tail(dst, src, count);
}

[[maybe_unused]] LIBC_INLINE void inline_memcpy_prefetch(Ptr __restrict dst,
                                                         CPtr __restrict src,
                                                         size_t distance) {
  prefetch_to_local_cache(src + distance);
  prefetch_for_write(dst + distance);
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86_sse2_ge64_sw_prefetching(Ptr __restrict dst,
                                           CPtr __restrict src, size_t count) {
  using namespace LIBC_NAMESPACE::x86;
  inline_memcpy_prefetch(dst, src, K_ONE_CACHELINE);
  if (count <= 128)
    return builtin::Memcpy<64>::head_tail(dst, src, count);
  inline_memcpy_prefetch(dst, src, K_TWO_CACHELINES);
  // Aligning 'dst' on a 32B boundary.
  builtin::Memcpy<32>::block(dst, src);
  align_to_next_boundary<32, Arg::Dst>(dst, src, count);
  builtin::Memcpy<96>::block(dst, src);
  size_t offset = 96;
  // At this point:
  // - we copied between 96B and 128B,
  // - we prefetched cachelines at 'src + 64' and 'src + 128',
  // - 'dst' is 32B aligned,
  // - count >= 128.
  if (count < 352) {
    // Two cache lines at a time.
    while (offset + K_TWO_CACHELINES + 32 <= count) {
      inline_memcpy_prefetch(dst, src, offset + K_ONE_CACHELINE);
      inline_memcpy_prefetch(dst, src, offset + K_TWO_CACHELINES);
      // Copy one cache line at a time to prevent the use of `rep;movsb`.
      for (size_t i = 0; i < 2; ++i, offset += K_ONE_CACHELINE)
        builtin::Memcpy<K_ONE_CACHELINE>::block_offset(dst, src, offset);
    }
  } else {
    // Three cache lines at a time.
    while (offset + K_THREE_CACHELINES + 32 <= count) {
      inline_memcpy_prefetch(dst, src, offset + K_ONE_CACHELINE);
      inline_memcpy_prefetch(dst, src, offset + K_TWO_CACHELINES);
      inline_memcpy_prefetch(dst, src, offset + K_THREE_CACHELINES);
      // Copy one cache line at a time to prevent the use of `rep;movsb`.
      for (size_t i = 0; i < 3; ++i, offset += K_ONE_CACHELINE)
        builtin::Memcpy<K_ONE_CACHELINE>::block_offset(dst, src, offset);
    }
  }
  // We don't use 'loop_and_tail_offset' because it assumes at least one
  // iteration of the loop.
  while (offset + 32 <= count) {
    builtin::Memcpy<32>::block_offset(dst, src, offset);
    offset += 32;
  }
  return builtin::Memcpy<32>::tail(dst, src, count);
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86_avx_ge64_sw_prefetching(Ptr __restrict dst,
                                          CPtr __restrict src, size_t count) {
  using namespace LIBC_NAMESPACE::x86;
  inline_memcpy_prefetch(dst, src, K_ONE_CACHELINE);
  if (count <= 128)
    return builtin::Memcpy<64>::head_tail(dst, src, count);
  inline_memcpy_prefetch(dst, src, K_TWO_CACHELINES);
  inline_memcpy_prefetch(dst, src, K_THREE_CACHELINES);
  if (count < 256)
    return builtin::Memcpy<128>::head_tail(dst, src, count);
  // Aligning 'dst' on a 32B boundary.
  builtin::Memcpy<32>::block(dst, src);
  align_to_next_boundary<32, Arg::Dst>(dst, src, count);
  builtin::Memcpy<224>::block(dst, src);
  size_t offset = 224;
  // At this point:
  // - we copied between 224B and 256B,
  // - we prefetched cachelines at 'src + 64', 'src + 128', and 'src + 196'
  // - 'dst' is 32B aligned,
  // - count >= 128.
  // If we are using the Non-temporal stores, we don't need prefetching
  bool need_prefetch_run = true;
  if constexpr (x86::K_NTA_THRESHOLD != 0) {
    if (count >= x86::K_NTA_THRESHOLD) {
      while (offset + K_THREE_CACHELINES + 64 <= count) {
        for (size_t i = 0; i < 3; ++i, offset += K_ONE_CACHELINE) {
          generic::stream(dst + offset,
                          generic::load<x86::m256i>(src + offset));
          generic::stream(dst + offset + 32,
                          generic::load<x86::m256i>(src + offset + 32));
        }
      }
      generic::fence<x86::m256i>();
      need_prefetch_run = false;
    }
  }
  if (need_prefetch_run) {
    while (offset + K_THREE_CACHELINES + 64 <= count) {
      // Three cache lines at a time.
      inline_memcpy_prefetch(dst, src, offset + K_ONE_CACHELINE);
      inline_memcpy_prefetch(dst, src, offset + K_TWO_CACHELINES);
      inline_memcpy_prefetch(dst, src, offset + K_THREE_CACHELINES);
      // Copy one cache line at a time to prevent the use of `rep;movsb`.
      for (size_t i = 0; i < 3; ++i, offset += K_ONE_CACHELINE)
        builtin::Memcpy<K_ONE_CACHELINE>::block_offset(dst, src, offset);
    }
  }
  // We don't use 'loop_and_tail_offset' because it assumes at least one
  // iteration of the loop.
  while (offset + 64 <= count) {
    builtin::Memcpy<64>::block_offset(dst, src, offset);
    offset += 64;
  }
  return builtin::Memcpy<64>::tail(dst, src, count);
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86(Ptr __restrict dst, CPtr __restrict src, size_t count) {
  constexpr size_t VECTOR_SIZE = x86::K_AVX512_F ? 64
                                 : x86::K_AVX    ? 32
                                 : x86::K_SSE2   ? 16
                                                 : 8;
  if (count == 0)
    return;
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
  // If count is equal to a power of 2, we can handle it as head-tail
  // of both smaller size and larger size (head-tail are either
  // non-overlapping for smaller size, or completely collapsed
  // for larger size). It seems to be more profitable to do the copy
  // with the larger size, if it's natively supported (e.g. doing
  // 2 collapsed 32-byte moves for count=64 if AVX2 is supported).
  // But it's not profitable to use larger size if it's not natively
  // supported: we will both use more instructions and handle fewer
  // sizes in earlier branches.
  if (VECTOR_SIZE >= 16 ? count < 16 : count <= 16)
    return builtin::Memcpy<8>::head_tail(dst, src, count);
  if (VECTOR_SIZE >= 32 ? count < 32 : count <= 32)
    return builtin::Memcpy<16>::head_tail(dst, src, count);
  if (VECTOR_SIZE >= 64 ? count < 64 : count <= 64)
    return builtin::Memcpy<32>::head_tail(dst, src, count);
  if constexpr (x86::K_AVX) {
    if constexpr (x86::K_USE_SOFTWARE_PREFETCHING) {
      return inline_memcpy_x86_avx_ge64_sw_prefetching(dst, src, count);
    } else {
      return inline_memcpy_x86_avx_ge64(dst, src, count);
    }
  } else {
    if constexpr (x86::K_USE_SOFTWARE_PREFETCHING) {
      return inline_memcpy_x86_sse2_ge64_sw_prefetching(dst, src, count);
    } else {
      return inline_memcpy_x86_sse2_ge64(dst, src, count);
    }
  }
}

[[maybe_unused]] LIBC_INLINE void
inline_memcpy_x86_maybe_interpose_repmovsb(Ptr __restrict dst,
                                           CPtr __restrict src, size_t count) {
  if constexpr (x86::K_REP_MOVSB_THRESHOLD == 0) {
    return x86::Memcpy::repmovsb(dst, src, count);
  } else if constexpr (x86::K_REP_MOVSB_THRESHOLD == SIZE_MAX) {
    return inline_memcpy_x86(dst, src, count);
  } else {
    if (LIBC_UNLIKELY(count >= x86::K_REP_MOVSB_THRESHOLD))
      return x86::Memcpy::repmovsb(dst, src, count);
    else
      return inline_memcpy_x86(dst, src, count);
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_MEMCPY_H

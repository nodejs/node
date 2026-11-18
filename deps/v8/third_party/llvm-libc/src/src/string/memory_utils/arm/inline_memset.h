//===-- Memset implementation for arm ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The functions defined in this file give approximate code size. These sizes
// assume the following configuration options:
// - LIBC_CONF_KEEP_FRAME_POINTER = false
// - LIBC_CONF_ENABLE_STRONG_STACK_PROTECTOR = false
// - LIBC_ADD_NULL_CHECKS = false
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMSET_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMSET_H

#include "src/__support/CPP/type_traits.h"     // always_false
#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/optimization.h" // LIBC_LOOP_NOUNROLL
#include "src/string/memory_utils/arm/common.h" // LIBC_ATTR_LIKELY, LIBC_ATTR_UNLIKELY
#include "src/string/memory_utils/utils.h" // memcpy_inline, distance_to_align

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

namespace {

template <size_t bytes, AssumeAccess access>
LIBC_INLINE void set(void *dst, uint32_t value) {
  static_assert(bytes == 1 || bytes == 2 || bytes == 4);
  if constexpr (access == AssumeAccess::kAligned) {
    constexpr size_t alignment = bytes > kWordSize ? kWordSize : bytes;
    memcpy_inline<bytes>(assume_aligned<alignment>(dst), &value);
  } else if constexpr (access == AssumeAccess::kUnknown) {
    memcpy_inline<bytes>(dst, &value);
  } else {
    static_assert(cpp::always_false<decltype(access)>, "Invalid AssumeAccess");
  }
}

template <size_t bytes, AssumeAccess access = AssumeAccess::kUnknown>
LIBC_INLINE void set_block_and_bump_pointers(Ptr &dst, uint32_t value) {
  if constexpr (bytes <= kWordSize) {
    set<bytes, access>(dst, value);
  } else {
    static_assert(bytes % kWordSize == 0 && bytes >= kWordSize);
    LIBC_LOOP_UNROLL
    for (size_t offset = 0; offset < bytes; offset += kWordSize) {
      set<kWordSize, access>(dst + offset, value);
    }
  }
  // In the 1, 2, 4 byte set case, the compiler can fold pointer offsetting
  // into the store instructions.
  // e.g.,
  // strb  r3, [r0], #1
  dst += bytes;
}

template <size_t bytes, AssumeAccess access>
LIBC_INLINE void consume_by_block(Ptr &dst, uint32_t value, size_t &size) {
  LIBC_LOOP_NOUNROLL
  for (size_t i = 0; i < size / bytes; ++i)
    set_block_and_bump_pointers<bytes, access>(dst, value);
  size %= bytes;
}

[[maybe_unused]] LIBC_INLINE void
set_bytes_and_bump_pointers(Ptr &dst, uint32_t value, size_t size) {
  LIBC_LOOP_NOUNROLL
  for (size_t i = 0; i < size; ++i) {
    set<1, AssumeAccess::kUnknown>(dst++, value);
  }
}

} // namespace

// Implementation for Cortex-M0, M0+, M1. It compiles down to 140 bytes when
// used through `memset` that also needs to return the `dst` ptr. These cores do
// not allow unaligned stores so all accesses are aligned.
[[maybe_unused]] LIBC_INLINE void
inline_memset_arm_low_end(Ptr dst, uint8_t value, size_t size) {
  const uint32_t value32 = value * 0x01010101U; // splat value in each byte
  if (size >= 8)
    LIBC_ATTR_LIKELY {
      // Align `dst` to word boundary.
      if (const size_t offset = distance_to_align_up<kWordSize>(dst))
        LIBC_ATTR_UNLIKELY {
          set_bytes_and_bump_pointers(dst, value32, offset);
          size -= offset;
        }
      consume_by_block<64, AssumeAccess::kAligned>(dst, value32, size);
      consume_by_block<16, AssumeAccess::kAligned>(dst, value32, size);
      consume_by_block<4, AssumeAccess::kAligned>(dst, value32, size);
    }
  set_bytes_and_bump_pointers(dst, value32, size);
}

// Implementation for Cortex-M3, M4, M7, M23, M33, M35P, M52 with hardware
// support for unaligned loads and stores. It compiles down to 186 bytes when
// used through `memset` that also needs to return the `dst` ptr.
[[maybe_unused]] LIBC_INLINE void
inline_memset_arm_mid_end(Ptr dst, uint8_t value, size_t size) {
  const uint32_t value32 = value * 0x01010101U; // splat value in each byte
  if (misaligned(dst))
    LIBC_ATTR_UNLIKELY {
      if (size < 8)
        LIBC_ATTR_UNLIKELY {
          if (size & 1)
            set_block_and_bump_pointers<1>(dst, value32);
          if (size & 2)
            set_block_and_bump_pointers<2>(dst, value32);
          if (size & 4)
            set_block_and_bump_pointers<4>(dst, value32);
          return;
        }
      const size_t offset = distance_to_align_up<kWordSize>(dst);
      if (offset & 1)
        set_block_and_bump_pointers<1>(dst, value32);
      if (offset & 2)
        set_block_and_bump_pointers<2>(dst, value32);
      size -= offset;
    }
  // If we tell the compiler that the stores are aligned it will generate 8 x
  // STRD instructions. By not specifying alignment, the compiler conservatively
  // uses 16 x STR.W and is able to use the first one to prefetch the
  // destination in advance leading to better asymptotic performances.
  //   str      r12, [r3, #64]!   <- prefetch next cache line
  //   str.w    r12, [r3, #0x4]
  //   str.w    r12, [r3, #0x8]
  //   ...
  //   str.w    r12, [r3, #0x38]
  //   str.w    r12, [r3, #0x3c]
  consume_by_block<64, AssumeAccess::kUnknown>(dst, value32, size);
  // Prefetching does not matter anymore at this scale so using STRD yields
  // better results.
  consume_by_block<16, AssumeAccess::kAligned>(dst, value32, size);
  consume_by_block<4, AssumeAccess::kAligned>(dst, value32, size);
  if (size & 1)
    set_block_and_bump_pointers<1>(dst, value32);
  if (size & 2)
    LIBC_ATTR_UNLIKELY
  set_block_and_bump_pointers<2>(dst, value32);
}

[[maybe_unused]] LIBC_INLINE void
inline_memset_arm_dispatch(Ptr dst, uint8_t value, size_t size) {
#ifdef __ARM_FEATURE_UNALIGNED
  return inline_memset_arm_mid_end(dst, value, size);
#else
  return inline_memset_arm_low_end(dst, value, size);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMSET_H

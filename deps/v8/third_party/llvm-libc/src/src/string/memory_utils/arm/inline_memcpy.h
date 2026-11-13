//===-- Memcpy implementation for arm ---------------------------*- C++ -*-===//
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
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMCPY_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMCPY_H

#include "src/__support/CPP/type_traits.h"     // always_false
#include "src/__support/macros/attributes.h"   // LIBC_INLINE
#include "src/__support/macros/optimization.h" // LIBC_LOOP_NOUNROLL
#include "src/string/memory_utils/arm/common.h" // LIBC_ATTR_LIKELY, LIBC_ATTR_UNLIKELY
#include "src/string/memory_utils/utils.h" // memcpy_inline, distance_to_align

#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

namespace {

// Performs a copy of `bytes` byte from `src` to `dst`. This function has the
// semantics of `memcpy` where `src` and `dst` are `__restrict`. The compiler is
// free to use whatever instruction is best for the size and assumed access.
template <size_t bytes, AssumeAccess access>
LIBC_INLINE void copy(void *dst, const void *src) {
  if constexpr (access == AssumeAccess::kAligned) {
    constexpr size_t alignment = bytes > kWordSize ? kWordSize : bytes;
    memcpy_inline<bytes>(assume_aligned<alignment>(dst),
                         assume_aligned<alignment>(src));
  } else if constexpr (access == AssumeAccess::kUnknown) {
    memcpy_inline<bytes>(dst, src);
  } else {
    static_assert(cpp::always_false<decltype(access)>, "Invalid AssumeAccess");
  }
}

template <size_t bytes, BlockOp block_op = BlockOp::kFull,
          AssumeAccess access = AssumeAccess::kUnknown>
LIBC_INLINE void copy_block_and_bump_pointers(Ptr &dst, CPtr &src) {
  if constexpr (block_op == BlockOp::kFull) {
    copy<bytes, access>(dst, src);
  } else if constexpr (block_op == BlockOp::kByWord) {
    // We restrict loads/stores to 4 byte to prevent the use of load/store
    // multiple (LDM, STM) and load/store double (LDRD, STRD).
    static_assert((bytes % kWordSize == 0) && (bytes >= kWordSize));
    LIBC_LOOP_UNROLL
    for (size_t offset = 0; offset < bytes; offset += kWordSize) {
      copy<kWordSize, access>(dst + offset, src + offset);
    }
  } else {
    static_assert(cpp::always_false<decltype(block_op)>, "Invalid BlockOp");
  }
  // In the 1, 2, 4 byte copy case, the compiler can fold pointer offsetting
  // into the load/store instructions.
  // e.g.,
  // ldrb  r3, [r1], #1
  // strb  r3, [r0], #1
  dst += bytes;
  src += bytes;
}

template <size_t bytes, BlockOp block_op, AssumeAccess access>
LIBC_INLINE void consume_by_block(Ptr &dst, CPtr &src, size_t &size) {
  LIBC_LOOP_NOUNROLL
  for (size_t i = 0; i < size / bytes; ++i)
    copy_block_and_bump_pointers<bytes, block_op, access>(dst, src);
  size %= bytes;
}

[[maybe_unused]] LIBC_INLINE void
copy_bytes_and_bump_pointers(Ptr &dst, CPtr &src, size_t size) {
  LIBC_LOOP_NOUNROLL
  for (size_t i = 0; i < size; ++i)
    *dst++ = *src++;
}

} // namespace

// Implementation for Cortex-M0, M0+, M1 cores that do not allow for unaligned
// loads/stores. It compiles down to 208 bytes when used through `memcpy` that
// also needs to return the `dst` ptr.
// Note:
// - When `src` and `dst` are coaligned, we start by aligning them and perform
//   bulk copies. We let the compiler know the pointers are aligned so it can
//   use load/store multiple (LDM, STM). This significantly increase throughput
//   but it also requires more registers and push/pop instructions. This impacts
//   latency for small size copies.
// - When `src` and `dst` are misaligned, we align `dst` and recompose words
//   using multiple aligned loads. `load_aligned` takes care of endianness
//   issues.
[[maybe_unused]] LIBC_INLINE void inline_memcpy_arm_low_end(Ptr dst, CPtr src,
                                                            size_t size) {
  if (size >= 8) {
    if (const size_t offset = distance_to_align_up<kWordSize>(dst))
      LIBC_ATTR_UNLIKELY {
        copy_bytes_and_bump_pointers(dst, src, offset);
        size -= offset;
      }
    constexpr AssumeAccess kAligned = AssumeAccess::kAligned;
    const auto src_alignment = distance_to_align_down<kWordSize>(src);
    if (src_alignment == 0)
      LIBC_ATTR_LIKELY {
        // Both `src` and `dst` are now word-aligned.
        // We first copy by blocks of 64 bytes, the compiler will use 4
        // load/store multiple (LDM, STM), each of 4 words. This requires more
        // registers so additional push/pop are needed but the speedup is worth
        // it.
        consume_by_block<64, BlockOp::kFull, kAligned>(dst, src, size);
        // Then we use blocks of 4 word load/store.
        consume_by_block<16, BlockOp::kByWord, kAligned>(dst, src, size);
        // Then we use word by word copy.
        consume_by_block<4, BlockOp::kByWord, kAligned>(dst, src, size);
      }
    else {
      // `dst` is aligned but `src` is not.
      LIBC_LOOP_NOUNROLL
      while (size >= kWordSize) {
        // Recompose word from multiple loads depending on the alignment.
        const uint32_t value =
            src_alignment == 2
                ? load_aligned<uint32_t, uint16_t, uint16_t>(src)
                : load_aligned<uint32_t, uint8_t, uint16_t, uint8_t>(src);
        copy<kWordSize, kAligned>(dst, &value);
        dst += kWordSize;
        src += kWordSize;
        size -= kWordSize;
      }
    }
    // Up to 3 bytes may still need to be copied.
    // Handling them with the slow loop below.
  }
  copy_bytes_and_bump_pointers(dst, src, size);
}

// Implementation for Cortex-M3, M4, M7, M23, M33, M35P, M52 with hardware
// support for unaligned loads and stores. It compiles down to 272 bytes when
// used through `memcpy` that also needs to return the `dst` ptr.
[[maybe_unused]] LIBC_INLINE void inline_memcpy_arm_mid_end(Ptr dst, CPtr src,
                                                            size_t size) {
  if (misaligned(bitwise_or(src, dst)))
    LIBC_ATTR_UNLIKELY {
      if (size < 8)
        LIBC_ATTR_UNLIKELY {
          if (size & 1)
            copy_block_and_bump_pointers<1>(dst, src);
          if (size & 2)
            copy_block_and_bump_pointers<2>(dst, src);
          if (size & 4)
            copy_block_and_bump_pointers<4>(dst, src);
          return;
        }
      if (misaligned(src))
        LIBC_ATTR_UNLIKELY {
          const size_t offset = distance_to_align_up<kWordSize>(dst);
          if (offset & 1)
            copy_block_and_bump_pointers<1>(dst, src);
          if (offset & 2)
            copy_block_and_bump_pointers<2>(dst, src);
          size -= offset;
        }
    }
  // `dst` and `src` are not necessarily both aligned at that point but this
  // implementation assumes hardware support for unaligned loads and stores so
  // it is still fast to perform unrolled word by word copy. Note that wider
  // accesses through the use of load/store multiple (LDM, STM) and load/store
  // double (LDRD, STRD) instructions are generally not supported and can fault.
  // By forcing decomposition of 64 bytes copy into word by word copy, the
  // compiler uses a load to prefetch the next cache line:
  //   ldr  r3, [r1, #64]!  <- prefetch next cache line
  //   str  r3, [r0]
  //   ldr  r3, [r1, #0x4]
  //   str  r3, [r0, #0x4]
  //   ...
  //   ldr  r3, [r1, #0x3c]
  //   str  r3, [r0, #0x3c]
  // This is a bit detrimental for sizes between 64 and 256 (less than 10%
  // penalty) but the prefetch yields better throughput for larger copies.
  constexpr AssumeAccess kUnknown = AssumeAccess::kUnknown;
  consume_by_block<64, BlockOp::kByWord, kUnknown>(dst, src, size);
  consume_by_block<16, BlockOp::kByWord, kUnknown>(dst, src, size);
  consume_by_block<4, BlockOp::kByWord, kUnknown>(dst, src, size);
  if (size & 1)
    copy_block_and_bump_pointers<1>(dst, src);
  if (size & 2)
    copy_block_and_bump_pointers<2>(dst, src);
}

[[maybe_unused]] LIBC_INLINE void inline_memcpy_arm(Ptr dst, CPtr src,
                                                    size_t size) {
  // The compiler performs alias analysis and is able to prove that `dst` and
  // `src` do not alias by propagating the `__restrict` keyword from the
  // `memcpy` prototype. This allows the compiler to merge consecutive
  // load/store (LDR, STR) instructions generated in
  // `copy_block_and_bump_pointers` with `BlockOp::kByWord` into load/store
  // double (LDRD, STRD) instructions, this is undesirable so we prevent the
  // compiler from inferring `__restrict` with the following line.
  asm volatile("" : "+r"(dst), "+r"(src));
#ifdef __ARM_FEATURE_UNALIGNED
  return inline_memcpy_arm_mid_end(dst, src, size);
#else
  return inline_memcpy_arm_low_end(dst, src, size);
#endif
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_ARM_INLINE_MEMCPY_H

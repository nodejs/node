//===-- HashTable BitMasks --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_HASHTABLE_BITMASK_H
#define LLVM_LIBC_SRC___SUPPORT_HASHTABLE_BITMASK_H

#include "hdr/stdint_proxy.h" // uint8_t, uint64_t
#include "src/__support/CPP/bit.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/cpu_features.h"
#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// Implementations of the bitmask.
// The backend word type may vary depending on different microarchitectures.
// For example, with X86 SSE2, the bitmask is just the 16bit unsigned integer
// corresponding to lanes in a SIMD register.
//
// Notice that this implementation is simplified from traditional swisstable:
// since we do not support deletion, we only need to care about if the highest
// bit is set or not:
// =============================
// | Slot Status |   Bitmask   |
// =============================
// |  Available  | 0b1xxx'xxxx |
// |  Occupied   | 0b0xxx'xxxx |
// =============================
template <typename T, size_t WORD_STRIDE> struct BitMaskAdaptor {
  // A stride in the bitmask may use multiple bits.
  LIBC_INLINE_VAR constexpr static size_t STRIDE = WORD_STRIDE;

  T word;

  // Check if any bit is set inside the word.
  LIBC_INLINE constexpr bool any_bit_set() const { return word != 0; }

  // Count trailing zeros with respect to stride. (Assume the bitmask is none
  // zero.)
  LIBC_INLINE constexpr size_t lowest_set_bit_nonzero() const {
    return cpp::countr_zero<T>(word) / WORD_STRIDE;
  }
};

// Not all bitmasks are iterable --- only those who has only MSB set in each
// lane. Hence, we make the types nomially different to distinguish them.
template <class BitMask> struct IteratableBitMaskAdaptor : public BitMask {
  // Use the bitmask as an iterator. Update the state and return current lowest
  // set bit. To make the bitmask iterable, each stride must contain 0 or exact
  // 1 set bit.
  LIBC_INLINE void remove_lowest_bit() {
    // Remove the last set bit inside the word:
    //    word              = 011110100 (original value)
    //    word - 1          = 011110011 (invert all bits up to the last set bit)
    //    word & (word - 1) = 011110000 (value with the last bit cleared)
    this->word = this->word & (this->word - 1);
  }
  using value_type = size_t;
  using iterator = BitMask;
  using const_iterator = BitMask;
  LIBC_INLINE size_t operator*() const {
    return this->lowest_set_bit_nonzero();
  }
  LIBC_INLINE IteratableBitMaskAdaptor &operator++() {
    this->remove_lowest_bit();
    return *this;
  }
  LIBC_INLINE IteratableBitMaskAdaptor begin() { return *this; }
  LIBC_INLINE IteratableBitMaskAdaptor end() { return {BitMask{0}}; }
  LIBC_INLINE bool operator==(const IteratableBitMaskAdaptor &other) {
    return this->word == other.word;
  }
  LIBC_INLINE bool operator!=(const IteratableBitMaskAdaptor &other) {
    return this->word != other.word;
  }
};

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#if defined(LIBC_TARGET_CPU_HAS_SSE2) && defined(__LIBC_EXPLICIT_SIMD_OPT)
#include "sse2/bitmask_impl.inc"
#else
#include "generic/bitmask_impl.inc"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_HASHTABLE_BITMASK_H

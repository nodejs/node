//===-- HashTable BitMasks SSE2 Implementation ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <immintrin.h>

#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {
// With SSE2, every bitmask is iteratable as
// we use single bit to encode the data.

using BitMask = BitMaskAdaptor<uint16_t, 0x1u>;
using IteratableBitMask = IteratableBitMaskAdaptor<BitMask>;

struct Group {
  __m128i data;

  // Load a group of control words from an arbitary address.
  LIBC_INLINE static Group load(const void *addr) {
    return {_mm_loadu_si128(static_cast<const __m128i *>(addr))};
  }

  // Load a group of control words from an aligned address.
  LIBC_INLINE static Group load_aligned(const void *addr) {
    return {_mm_load_si128(static_cast<const __m128i *>(addr))};
  }

  // Find out the lanes equal to the given byte and return the bitmask
  // with corresponding bits set.
  LIBC_INLINE IteratableBitMask match_byte(uint8_t byte) const {
    auto cmp = _mm_cmpeq_epi8(data, _mm_set1_epi8(byte));
    auto bitmask = static_cast<uint16_t>(_mm_movemask_epi8(cmp));
    return {{bitmask}};
  }

  LIBC_INLINE BitMask mask_available() const {
    auto bitmask = static_cast<uint16_t>(_mm_movemask_epi8(data));
    return {bitmask};
  }

  LIBC_INLINE IteratableBitMask occupied() const {
    return {{static_cast<uint16_t>(~mask_available().word)}};
  }
};
} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

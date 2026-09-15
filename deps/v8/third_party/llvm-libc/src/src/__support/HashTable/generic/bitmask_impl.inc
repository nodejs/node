//===-- HashTable BitMasks Generic Implementation ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/bit.h"
#include "src/__support/common.h"
#include "src/__support/endian_internal.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

// GPU architectures are 64-bit but use 32-bit general purpose registers.
#ifdef LIBC_TARGET_ARCH_IS_GPU
using bitmask_t = uint32_t;
#else
using bitmask_t = uintptr_t;
#endif

// Helper function to spread a byte across the whole word.
// Accumutively, the procedure looks like:
//    byte                  = 0x00000000000000ff
//    byte | (byte << 8)    = 0x000000000000ffff
//    byte | (byte << 16)   = 0x00000000ffffffff
//    byte | (byte << 32)   = 0xffffffffffffffff
LIBC_INLINE constexpr bitmask_t repeat_byte(bitmask_t byte) {
  size_t shift_amount = 8;
  while (shift_amount < sizeof(bitmask_t) * 8) {
    byte |= byte << shift_amount;
    shift_amount <<= 1;
  }
  return byte;
}

using BitMask = BitMaskAdaptor<bitmask_t, 0x8ul>;
using IteratableBitMask = IteratableBitMaskAdaptor<BitMask>;

struct Group {
  LIBC_INLINE_VAR static constexpr bitmask_t MASK = repeat_byte(0x80ul);
  bitmask_t data;

  // Load a group of control words from an arbitary address.
  LIBC_INLINE static Group load(const void *addr) {
    char bytes[sizeof(bitmask_t)];

    for (size_t i = 0; i < sizeof(bitmask_t); ++i)
      bytes[i] = static_cast<const char *>(addr)[i];
    return Group{cpp::bit_cast<bitmask_t>(bytes)};
  }

  // Load a group of control words from an aligned address.
  LIBC_INLINE static Group load_aligned(const void *addr) {
    return *static_cast<const Group *>(addr);
  }

  // Find out the lanes equal to the given byte and return the bitmask
  // with corresponding bits set.
  LIBC_INLINE IteratableBitMask match_byte(uint8_t byte) const {
    // Given byte = 0x10, suppose the data is:
    //
    //       data = [ 0x10 | 0x10 | 0x00 | 0xF1 | ... ]
    //
    // First, we compare the byte using XOR operation:
    //
    //        [ 0x10 | 0x10 | 0x10 | 0x10 | ... ]   (0)
    //      ^ [ 0x10 | 0x10 | 0x00 | 0xF1 | ... ]   (1)
    //      = [ 0x00 | 0x00 | 0x10 | 0xE1 | ... ]   (2)
    //
    // Notice that the equal positions will now be 0x00, so if we substract 0x01
    // respective to every byte, it will need to carry the substraction to upper
    // bits (assume no carry from the hidden parts)
    //        [ 0x00 | 0x00 | 0x10 | 0xE1 | ... ]   (2)
    //      - [ 0x01 | 0x01 | 0x01 | 0x01 | ... ]   (3)
    //      = [ 0xFE | 0xFF | 0x0F | 0xE0 | ... ]   (4)
    //
    // But there may be some bytes whose highest bit is already set after the
    // xor operation. To rule out these positions, we AND them with the NOT
    // of the XOR result:
    //
    //        [ 0xFF | 0xFF | 0xEF | 0x1E | ... ]   (5, NOT (2))
    //      & [ 0xFE | 0xFF | 0x0F | 0xE0 | ... ]   (4)
    //      = [ 0xFE | 0xFF | 0x0F | 0x10 | ... ]   (6)
    //
    // To make the bitmask iteratable, only one bit can be set in each stride.
    // So we AND each byte with 0x80 and keep only the highest bit:
    //
    //        [ 0xFE | 0xFF | 0x0F | 0x10 | ... ]   (6)
    //      & [ 0x80 | 0x80 | 0x80 | 0x80 | ... ]   (7)
    //      = [ 0x80 | 0x80 | 0x00 | 0x00 | ... ]   (8)
    //
    // However, there are possitbilites for false positives. For example, if the
    // data is [ 0x10 | 0x11 | 0x10 | 0xF1 | ... ]. This only happens when there
    // is a key only differs from the searched by the lowest bit. The claims
    // are:
    //
    //  - This never happens for `EMPTY` and `DELETED`, only full entries.
    //  - The check for key equality will catch these.
    //  - This only happens if there is at least 1 true match.
    //  - The chance of this happening is very low (< 1% chance per byte).
    static constexpr bitmask_t ONES = repeat_byte(0x01ul);
    auto cmp = data ^ repeat_byte(static_cast<bitmask_t>(byte) & 0xFFul);
    auto result =
        LIBC_NAMESPACE::Endian::to_little_endian((cmp - ONES) & ~cmp & MASK);
    return {BitMask{result}};
  }

  // Find out the lanes equal to EMPTY or DELETE (highest bit set) and
  // return the bitmask with corresponding bits set.
  LIBC_INLINE BitMask mask_available() const {
    bitmask_t le_data = LIBC_NAMESPACE::Endian::to_little_endian(data);
    return {le_data & MASK};
  }

  LIBC_INLINE IteratableBitMask occupied() const {
    bitmask_t available = mask_available().word;
    return {BitMask{available ^ MASK}};
  }
};
} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

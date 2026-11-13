//===-- aarch64 implementation of memory function building blocks ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides aarch64 specific building blocks to compose memory
// functions.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_AARCH64_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_AARCH64_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/__support/macros/is_defined.h"
#include "src/__support/macros/properties/architectures.h"

#if defined(LIBC_TARGET_ARCH_IS_AARCH64)

#include "src/__support/CPP/type_traits.h" // cpp::always_false
#include "src/__support/common.h"
#include "src/string/memory_utils/op_generic.h"

#ifdef __ARM_NEON
#include <arm_neon.h>

namespace LIBC_NAMESPACE_DECL {
namespace aarch64 {

LIBC_INLINE_VAR constexpr bool kNeon = LLVM_LIBC_IS_DEFINED(__ARM_NEON);

namespace neon {

struct BzeroCacheLine {
  static constexpr size_t SIZE = 64;

  LIBC_INLINE static void block(Ptr dst, uint8_t) {
#if __SIZEOF_POINTER__ == 4
    asm("dc zva, %w[dst]" : : [dst] "r"(dst) : "memory");
#else
    asm("dc zva, %[dst]" : : [dst] "r"(dst) : "memory");
#endif
  }

  LIBC_INLINE static void loop_and_tail(Ptr dst, uint8_t value, size_t count) {
    size_t offset = 0;
    do {
      block(dst + offset, value);
      offset += SIZE;
    } while (offset < count - SIZE);
    // Unaligned store, we can't use 'dc zva' here.
    generic::Memset<generic_v512>::tail(dst, value, count);
  }
};

LIBC_INLINE bool hasZva() {
  uint64_t zva_val;
  asm("mrs %[zva_val], dczid_el0" : [zva_val] "=r"(zva_val));
  // DC ZVA is permitted if DZP, bit [4] is zero.
  // BS, bits [3:0] is log2 of the block count in words.
  // So the next line checks whether the instruction is permitted and block
  // count is 16 words (i.e. 64 bytes).
  return (zva_val & 0b11111) == 0b00100;
}

} // namespace neon

///////////////////////////////////////////////////////////////////////////////
// Bcmp
template <size_t Size> struct Bcmp {
  static constexpr size_t SIZE = Size;
  static constexpr size_t BlockSize = 32;

  LIBC_INLINE static const unsigned char *as_u8(CPtr ptr) {
    return reinterpret_cast<const unsigned char *>(ptr);
  }

  LIBC_INLINE static BcmpReturnType block(CPtr p1, CPtr p2) {
    if constexpr (Size == 16) {
      auto _p1 = as_u8(p1);
      auto _p2 = as_u8(p2);
      uint8x16_t a = vld1q_u8(_p1);
      uint8x16_t n = vld1q_u8(_p2);
      uint8x16_t an = veorq_u8(a, n);
      return vmaxvq_u32(vreinterpretq_u32_u8(an));
    } else if constexpr (Size == 32) {
      auto _p1 = as_u8(p1);
      auto _p2 = as_u8(p2);
      uint8x16_t a = vld1q_u8(_p1);
      uint8x16_t b = vld1q_u8(_p1 + 16);
      uint8x16_t n = vld1q_u8(_p2);
      uint8x16_t o = vld1q_u8(_p2 + 16);
      uint8x16_t an = veorq_u8(a, n);
      uint8x16_t bo = veorq_u8(b, o);
      // anbo = (a ^ n) | (b ^ o).  At least one byte is nonzero if there is
      // a difference between the two buffers.  We reduce this value down to 4
      // bytes using the UMAXV instruction to compute the max across the vector.
      uint8x16_t anbo = vorrq_u8(an, bo);
      return vmaxvq_u32(vreinterpretq_u32_u8(anbo));
    } else if constexpr ((Size % BlockSize) == 0) {
      for (size_t offset = 0; offset < Size; offset += BlockSize)
        if (auto value = Bcmp<BlockSize>::block(p1 + offset, p2 + offset))
          return value;
    } else {
      static_assert(cpp::always_false<decltype(Size)>, "SIZE not implemented");
    }
    return BcmpReturnType::zero();
  }

  LIBC_INLINE static BcmpReturnType tail(CPtr p1, CPtr p2, size_t count) {
    return block(p1 + count - SIZE, p2 + count - SIZE);
  }

  LIBC_INLINE static BcmpReturnType head_tail(CPtr p1, CPtr p2, size_t count) {
    if constexpr (Size == 16) {
      auto _p1 = as_u8(p1);
      auto _p2 = as_u8(p2);
      uint8x16_t a = vld1q_u8(_p1);
      uint8x16_t b = vld1q_u8(_p1 + count - 16);
      uint8x16_t n = vld1q_u8(_p2);
      uint8x16_t o = vld1q_u8(_p2 + count - 16);
      uint8x16_t an = veorq_u8(a, n);
      uint8x16_t bo = veorq_u8(b, o);
      // anbo = (a ^ n) | (b ^ o)
      uint8x16_t anbo = vorrq_u8(an, bo);
      return vmaxvq_u32(vreinterpretq_u32_u8(anbo));
    } else if constexpr (Size == 32) {
      auto _p1 = as_u8(p1);
      auto _p2 = as_u8(p2);
      uint8x16_t a = vld1q_u8(_p1);
      uint8x16_t b = vld1q_u8(_p1 + 16);
      uint8x16_t c = vld1q_u8(_p1 + count - 16);
      uint8x16_t d = vld1q_u8(_p1 + count - 32);
      uint8x16_t n = vld1q_u8(_p2);
      uint8x16_t o = vld1q_u8(_p2 + 16);
      uint8x16_t p = vld1q_u8(_p2 + count - 16);
      uint8x16_t q = vld1q_u8(_p2 + count - 32);
      uint8x16_t an = veorq_u8(a, n);
      uint8x16_t bo = veorq_u8(b, o);
      uint8x16_t cp = veorq_u8(c, p);
      uint8x16_t dq = veorq_u8(d, q);
      uint8x16_t anbo = vorrq_u8(an, bo);
      uint8x16_t cpdq = vorrq_u8(cp, dq);
      // abnocpdq = ((a ^ n) | (b ^ o)) | ((c ^ p) | (d ^ q)).  Reduce this to
      // a nonzero 32 bit value if a mismatch occurred.
      uint8x16_t abnocpdq = anbo | cpdq;
      return vmaxvq_u32(vreinterpretq_u32_u8(abnocpdq));
    } else {
      static_assert(cpp::always_false<decltype(Size)>, "SIZE not implemented");
    }
    return BcmpReturnType::zero();
  }

  LIBC_INLINE static BcmpReturnType loop_and_tail(CPtr p1, CPtr p2,
                                                  size_t count) {
    static_assert(Size > 1, "a loop of size 1 does not need tail");
    size_t offset = 0;
    do {
      if (auto value = block(p1 + offset, p2 + offset))
        return value;
      offset += SIZE;
    } while (offset < count - SIZE);
    return tail(p1, p2, count);
  }
};

} // namespace aarch64
} // namespace LIBC_NAMESPACE_DECL

#endif //__ARM_NEON

namespace LIBC_NAMESPACE_DECL {
namespace generic {

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint16_t
template <> struct cmp_is_expensive<uint16_t> : public cpp::false_type {};
template <> LIBC_INLINE bool eq<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint16_t>(p1, offset) == load<uint16_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint16_t>(p1, offset) ^ load<uint16_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint16_t>(CPtr p1, CPtr p2, size_t offset) {
  return static_cast<int32_t>(load_be<uint16_t>(p1, offset)) -
         static_cast<int32_t>(load_be<uint16_t>(p2, offset));
}

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint32_t
template <> struct cmp_is_expensive<uint32_t> : cpp::false_type {};
template <>
LIBC_INLINE uint32_t neq<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint32_t>(p1, offset) ^ load<uint32_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load_be<uint32_t>(p1, offset);
  const auto b = load_be<uint32_t>(p2, offset);
  return a > b ? 1 : a < b ? -1 : 0;
}

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint64_t
template <> struct cmp_is_expensive<uint64_t> : cpp::false_type {};
template <>
LIBC_INLINE uint32_t neq<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint64_t>(p1, offset) != load<uint64_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load_be<uint64_t>(p1, offset);
  const auto b = load_be<uint64_t>(p2, offset);
  if (a != b)
    return a > b ? 1 : -1;
  return MemcmpReturnType::zero();
}

#if defined(__ARM_NEON)

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint8x16_t
template <> struct is_vector<uint8x16_t> : cpp::true_type {};
template <> struct cmp_is_expensive<uint8x16_t> : cpp::false_type {};
template <>
LIBC_INLINE uint32_t neq<uint8x16_t>(CPtr p1, CPtr p2, size_t offset) {
  for (size_t i = 0; i < 2; ++i) {
    auto a = load<uint64_t>(p1, offset);
    auto b = load<uint64_t>(p2, offset);
    uint32_t cond = a != b;
    if (cond)
      return cond;
    offset += sizeof(uint64_t);
  }
  return 0;
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint8x16_t>(CPtr p1, CPtr p2, size_t offset) {
  for (size_t i = 0; i < 2; ++i) {
    auto a = load_be<uint64_t>(p1, offset);
    auto b = load_be<uint64_t>(p2, offset);
    if (a != b)
      return cmp_neq_uint64_t(a, b);
    offset += sizeof(uint64_t);
  }
  return MemcmpReturnType::zero();
}

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint8x16x2_t
template <> struct is_vector<uint8x16x2_t> : cpp::true_type {};
template <> struct cmp_is_expensive<uint8x16x2_t> : cpp::false_type {};
template <>
LIBC_INLINE MemcmpReturnType cmp<uint8x16x2_t>(CPtr p1, CPtr p2,
                                               size_t offset) {
  for (size_t i = 0; i < 4; ++i) {
    auto a = load_be<uint64_t>(p1, offset);
    auto b = load_be<uint64_t>(p2, offset);
    if (a != b)
      return cmp_neq_uint64_t(a, b);
    offset += sizeof(uint64_t);
  }
  return MemcmpReturnType::zero();
}

#endif // __ARM_NEON

} // namespace generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_ARCH_IS_AARCH64

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_AARCH64_H

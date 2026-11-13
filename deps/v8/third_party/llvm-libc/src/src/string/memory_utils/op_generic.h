//===-- Generic implementation of memory function building blocks ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides generic C++ building blocks.
// Depending on the requested size, the block operation uses unsigned integral
// types, vector types or an array of the type with the maximum size.
//
// The maximum size is passed as a template argument. For instance, on x86
// platforms that only supports integral types the maximum size would be 8
// (corresponding to uint64_t). On this platform if we request the size 32, this
// would be treated as a cpp::array<uint64_t, 4>.
//
// On the other hand, if the platform is x86 with support for AVX the maximum
// size is 32 and the operation can be handled with a single native operation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_GENERIC_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_GENERIC_H

#include "hdr/stdint_proxy.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/common.h"
#include "src/__support/endian_internal.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/__support/macros/optimization.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/macros/properties/types.h" // LIBC_TYPES_HAS_INT64
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/utils.h"

static_assert((UINTPTR_MAX == 4294967295U) ||
                  (UINTPTR_MAX == 18446744073709551615UL),
              "We currently only support 32- or 64-bit platforms");

namespace LIBC_NAMESPACE_DECL {

// Compiler types using the GNU vector attributes.  Clang and GCC support
// these, but MSVC does not.  However, on x86 MSVC does support the __m*i types
// instead.  When the compiler does support the types, it's not OK to actually
// use the ones that correspond to CPU features that are not enabled at compile
// time by the particular combination of compiler switches used--not even in
// templates and inlines that are ultimately never used because of other
// conditionals on when to use which generic_v* types.  For each vector type
// that is not available either because the target doesn't have them at all, or
// because the CPU features configured exclude that particular size, is
// replaced with an aligned byte array.

#if defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
using generic_v128 = __m128i;
#elif LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX >= 128
using generic_v128 = uint8_t __attribute__((__vector_size__(16)));
#else
struct alignas(16) generic_v128 : public cpp::array<uint8_t, 16> {};
#endif

#if defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
using generic_v256 = __m256i;
#elif LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX >= 256
using generic_v256 = uint8_t __attribute__((__vector_size__(32)));
#else
struct alignas(32) generic_v256 : public cpp::array<uint8_t, 32> {};
#endif

#if defined(LIBC_COMPILER_IS_MSVC) && defined(LIBC_TARGET_ARCH_IS_X86)
using generic_v512 = __m512i;
#elif LIBC_TARGET_CPU_GENERIC_VECTOR_SIZE_MAX >= 512
using generic_v512 = uint8_t __attribute__((__vector_size__(64)));
#else
struct alignas(64) generic_v512 : public cpp::array<uint8_t, 64> {};
#endif

namespace generic {

// We accept three types of values as elements for generic operations:
// - scalar : unsigned integral types,
// - vector : compiler types using the vector attributes or platform builtins,
// - array  : a cpp::array<T, N> where T is itself either a scalar or a vector.
// The following traits help discriminate between these cases.

template <typename T> struct is_scalar : cpp::false_type {};
template <> struct is_scalar<uint8_t> : cpp::true_type {};
template <> struct is_scalar<uint16_t> : cpp::true_type {};
template <> struct is_scalar<uint32_t> : cpp::true_type {};
#ifdef LIBC_TYPES_HAS_INT64
template <> struct is_scalar<uint64_t> : cpp::true_type {};
#endif // LIBC_TYPES_HAS_INT64
// Meant to match std::numeric_limits interface.
// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T> constexpr bool is_scalar_v = is_scalar<T>::value;

template <typename T> struct is_vector : cpp::false_type {};
template <> struct is_vector<generic_v128> : cpp::true_type {};
template <> struct is_vector<generic_v256> : cpp::true_type {};
template <> struct is_vector<generic_v512> : cpp::true_type {};
// Meant to match std::numeric_limits interface.
// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T> constexpr bool is_vector_v = is_vector<T>::value;

template <class T> struct is_array : cpp::false_type {};
template <class T, size_t N> struct is_array<cpp::array<T, N>> {
  // Meant to match std::numeric_limits interface.
  // NOLINTNEXTLINE(readability-identifier-naming)
  static constexpr bool value = is_scalar_v<T> || is_vector_v<T>;
};
// Meant to match std::numeric_limits interface.
// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T> constexpr bool is_array_v = is_array<T>::value;

// Meant to match std::numeric_limits interface.
// NOLINTBEGIN(readability-identifier-naming)
template <typename T>
constexpr bool is_element_type_v =
    is_scalar_v<T> || is_vector_v<T> || is_array_v<T>;
// NOLINTEND(readability-identifier-naming)

// Helper struct to retrieve the number of elements of an array.
template <class T> struct array_size {};
template <class T, size_t N>
struct array_size<cpp::array<T, N>> : cpp::integral_constant<size_t, N> {};
// Meant to match std::numeric_limits interface.
// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T> constexpr size_t array_size_v = array_size<T>::value;

// Generic operations for the above type categories.

template <typename T> T load(CPtr src) {
  static_assert(is_element_type_v<T>);
  if constexpr (is_scalar_v<T> || is_vector_v<T>) {
    return load_unaligned<T>(src);
  } else if constexpr (is_array_v<T>) {
    using value_type = typename T::value_type;
    T value;
    for (size_t i = 0; i < array_size_v<T>; ++i)
      value[i] = load<value_type>(src + (i * sizeof(value_type)));
    return value;
  }
}

template <typename T> void store(Ptr dst, T value) {
  static_assert(is_element_type_v<T>);
  if constexpr (is_scalar_v<T> || is_vector_v<T>) {
    store_unaligned<T>(dst, value);
  } else if constexpr (is_array_v<T>) {
    using value_type = typename T::value_type;
    for (size_t i = 0; i < array_size_v<T>; ++i)
      store<value_type>(dst + (i * sizeof(value_type)), value[i]);
  }
}

template <typename T> T splat(uint8_t value) {
  static_assert(is_scalar_v<T> || is_vector_v<T>);
  if constexpr (is_scalar_v<T>)
    return T(~0) / T(0xFF) * T(value);
  else if constexpr (is_vector_v<T>) {
    T out;
    // This for loop is optimized out for vector types.
    for (size_t i = 0; i < sizeof(T); ++i)
      out[i] = value;
    return out;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Memset
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Memset {
  static_assert(is_element_type_v<T>);
  static constexpr size_t SIZE = sizeof(T);

  LIBC_INLINE static void block(Ptr dst, uint8_t value) {
    if constexpr (is_scalar_v<T> || is_vector_v<T>) {
      // Avoid ambiguous call due to ADL
      generic::store<T>(dst, splat<T>(value));
    } else if constexpr (is_array_v<T>) {
      using value_type = typename T::value_type;
      const auto Splat = splat<value_type>(value);
      for (size_t i = 0; i < array_size_v<T>; ++i)
        store<value_type>(dst + (i * sizeof(value_type)), Splat);
    }
  }

  LIBC_INLINE static void tail(Ptr dst, uint8_t value, size_t count) {
    block(dst + count - SIZE, value);
  }

  LIBC_INLINE static void head_tail(Ptr dst, uint8_t value, size_t count) {
    block(dst, value);
    tail(dst, value, count);
  }

  LIBC_INLINE static void loop_and_tail_offset(Ptr dst, uint8_t value,
                                               size_t count, size_t offset) {
    static_assert(SIZE > 1, "a loop of size 1 does not need tail");
    do {
      block(dst + offset, value);
      offset += SIZE;
    } while (offset < count - SIZE);
    tail(dst, value, count);
  }

  LIBC_INLINE static void loop_and_tail(Ptr dst, uint8_t value, size_t count) {
    return loop_and_tail_offset(dst, value, count, 0);
  }
};

template <typename T, typename... TS> struct MemsetSequence {
  static constexpr size_t SIZE = (sizeof(T) + ... + sizeof(TS));
  LIBC_INLINE static void block(Ptr dst, uint8_t value) {
    Memset<T>::block(dst, value);
    if constexpr (sizeof...(TS) > 0)
      return MemsetSequence<TS...>::block(dst + sizeof(T), value);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Memmove
///////////////////////////////////////////////////////////////////////////////

template <typename T> struct Memmove {
  static_assert(is_element_type_v<T>);
  static constexpr size_t SIZE = sizeof(T);

  LIBC_INLINE static void block(Ptr dst, CPtr src) {
    store<T>(dst, load<T>(src));
  }

  LIBC_INLINE static void head_tail(Ptr dst, CPtr src, size_t count) {
    const size_t offset = count - SIZE;
    // The load and store operations can be performed in any order as long as
    // they are not interleaved. More investigations are needed to determine
    // the best order.
    const auto head = load<T>(src);
    const auto tail = load<T>(src + offset);
    store<T>(dst, head);
    store<T>(dst + offset, tail);
  }

  // Align forward suitable when dst < src. The alignment is performed with
  // an HeadTail operation of count ∈ [Alignment, 2 x Alignment].
  //
  // e.g. Moving two bytes forward, we make sure src is aligned.
  // [  |       |       |       |      ]
  // [____XXXXXXXXXXXXXXXXXXXXXXXXXXXX_]
  // [____LLLLLLLL_____________________]
  // [___________LLLLLLLA______________]
  // [_SSSSSSSS________________________]
  // [________SSSSSSSS_________________]
  //
  // e.g. Moving two bytes forward, we make sure dst is aligned.
  // [  |       |       |       |      ]
  // [____XXXXXXXXXXXXXXXXXXXXXXXXXXXX_]
  // [____LLLLLLLL_____________________]
  // [______LLLLLLLL___________________]
  // [_SSSSSSSS________________________]
  // [___SSSSSSSA______________________]
  template <Arg AlignOn>
  LIBC_INLINE static void align_forward(Ptr &dst, CPtr &src, size_t &count) {
    Ptr prev_dst = dst;
    CPtr prev_src = src;
    size_t prev_count = count;
    align_to_next_boundary<SIZE, AlignOn>(dst, src, count);
    adjust(SIZE, dst, src, count);
    head_tail(prev_dst, prev_src, prev_count - count);
  }

  // Align backward suitable when dst > src. The alignment is performed with
  // an HeadTail operation of count ∈ [Alignment, 2 x Alignment].
  //
  // e.g. Moving two bytes backward, we make sure src is aligned.
  // [  |       |       |       |      ]
  // [____XXXXXXXXXXXXXXXXXXXXXXXX_____]
  // [ _________________ALLLLLLL_______]
  // [ ___________________LLLLLLLL_____]
  // [____________________SSSSSSSS_____]
  // [______________________SSSSSSSS___]
  //
  // e.g. Moving two bytes backward, we make sure dst is aligned.
  // [  |       |       |       |      ]
  // [____XXXXXXXXXXXXXXXXXXXXXXXX_____]
  // [ _______________LLLLLLLL_________]
  // [ ___________________LLLLLLLL_____]
  // [__________________ASSSSSSS_______]
  // [______________________SSSSSSSS___]
  template <Arg AlignOn>
  LIBC_INLINE static void align_backward(Ptr &dst, CPtr &src, size_t &count) {
    Ptr headtail_dst = dst + count;
    CPtr headtail_src = src + count;
    size_t headtail_size = 0;
    align_to_next_boundary<SIZE, AlignOn>(headtail_dst, headtail_src,
                                          headtail_size);
    adjust(-2 * SIZE, headtail_dst, headtail_src, headtail_size);
    head_tail(headtail_dst, headtail_src, headtail_size);
    count -= headtail_size;
  }

  // Move forward suitable when dst < src. We load the tail bytes before
  // handling the loop.
  //
  // e.g. Moving two bytes
  // [   |       |       |       |       |]
  // [___XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX___]
  // [_________________________LLLLLLLL___]
  // [___LLLLLLLL_________________________]
  // [_SSSSSSSS___________________________]
  // [___________LLLLLLLL_________________]
  // [_________SSSSSSSS___________________]
  // [___________________LLLLLLLL_________]
  // [_________________SSSSSSSS___________]
  // [_______________________SSSSSSSS_____]
  LIBC_INLINE static void loop_and_tail_forward(Ptr dst, CPtr src,
                                                size_t count) {
    static_assert(SIZE > 1, "a loop of size 1 does not need tail");
    const size_t tail_offset = count - SIZE;
    const auto tail_value = load<T>(src + tail_offset);
    size_t offset = 0;
    LIBC_LOOP_NOUNROLL
    do {
      block(dst + offset, src + offset);
      offset += SIZE;
    } while (offset < count - SIZE);
    store<T>(dst + tail_offset, tail_value);
  }

  // Move backward suitable when dst > src. We load the head bytes before
  // handling the loop.
  //
  // e.g. Moving two bytes
  // [   |       |       |       |       |]
  // [___XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX___]
  // [___LLLLLLLL_________________________]
  // [_________________________LLLLLLLL___]
  // [___________________________SSSSSSSS_]
  // [_________________LLLLLLLL___________]
  // [___________________SSSSSSSS_________]
  // [_________LLLLLLLL___________________]
  // [___________SSSSSSSS_________________]
  // [_____SSSSSSSS_______________________]
  LIBC_INLINE static void loop_and_tail_backward(Ptr dst, CPtr src,
                                                 size_t count) {
    static_assert(SIZE > 1, "a loop of size 1 does not need tail");
    const auto head_value = load<T>(src);
    ptrdiff_t offset = count - SIZE;
    LIBC_LOOP_NOUNROLL
    do {
      block(dst + offset, src + offset);
      offset -= SIZE;
    } while (offset >= 0);
    store<T>(dst, head_value);
  }
};

///////////////////////////////////////////////////////////////////////////////
// Low level operations for Bcmp and Memcmp that operate on memory locations.
///////////////////////////////////////////////////////////////////////////////

// Same as load above but with an offset to the pointer.
// Making the offset explicit hints the compiler to use relevant addressing mode
// consistently.
template <typename T> LIBC_INLINE T load(CPtr ptr, size_t offset) {
  return load_unaligned<T>(ptr + offset);
}

// Same as above but also makes sure the loaded value is in big endian format.
// This is useful when implementing lexicograhic comparisons as big endian
// scalar comparison directly maps to lexicographic byte comparisons.
template <typename T> LIBC_INLINE T load_be(CPtr ptr, size_t offset) {
  return Endian::to_big_endian(load<T>(ptr, offset));
}

// Equality: returns true iff values at locations (p1 + offset) and (p2 +
// offset) compare equal.
template <typename T> LIBC_INLINE bool eq(CPtr p1, CPtr p2, size_t offset);

// Not equals: returns non-zero iff values at locations (p1 + offset) and (p2 +
// offset) differ.
template <typename T> LIBC_INLINE uint32_t neq(CPtr p1, CPtr p2, size_t offset);

// Lexicographic comparison:
// - returns 0 iff values at locations (p1 + offset) and (p2 + offset) compare
//   equal.
// - returns a negative value if value at location (p1 + offset) is
//   lexicographically less than value at (p2 + offset).
// - returns a positive value if value at location (p1 + offset) is
//   lexicographically greater than value at (p2 + offset).
template <typename T>
LIBC_INLINE MemcmpReturnType cmp(CPtr p1, CPtr p2, size_t offset);

// Lexicographic comparison of non-equal values:
// - returns a negative value if value at location (p1 + offset) is
//   lexicographically less than value at (p2 + offset).
// - returns a positive value if value at location (p1 + offset) is
//   lexicographically greater than value at (p2 + offset).
template <typename T>
LIBC_INLINE MemcmpReturnType cmp_neq(CPtr p1, CPtr p2, size_t offset);

///////////////////////////////////////////////////////////////////////////////
// Memcmp implementation
//
// When building memcmp, not all types are considered equals.
//
// For instance, the lexicographic comparison of two uint8_t can be implemented
// as a simple subtraction, but for wider operations the logic can be much more
// involving, especially on little endian platforms.
//
// For such wider types it is a good strategy to test for equality first and
// only do the expensive lexicographic comparison if necessary.
//
// Decomposing the algorithm like this for wider types allows us to have
// efficient implementation of higher order functions like 'head_tail' or
// 'loop_and_tail'.
///////////////////////////////////////////////////////////////////////////////

// Type traits to decide whether we can use 'cmp' directly or if we need to
// split the computation.
template <typename T> struct cmp_is_expensive;

template <typename T> struct Memcmp {
  static_assert(is_element_type_v<T>);
  static constexpr size_t SIZE = sizeof(T);

private:
  LIBC_INLINE static MemcmpReturnType block_offset(CPtr p1, CPtr p2,
                                                   size_t offset) {
    if constexpr (cmp_is_expensive<T>::value) {
      if (!eq<T>(p1, p2, offset))
        return cmp_neq<T>(p1, p2, offset);
      return MemcmpReturnType::zero();
    } else {
      return cmp<T>(p1, p2, offset);
    }
  }

public:
  LIBC_INLINE static MemcmpReturnType block(CPtr p1, CPtr p2) {
    return block_offset(p1, p2, 0);
  }

  LIBC_INLINE static MemcmpReturnType tail(CPtr p1, CPtr p2, size_t count) {
    return block_offset(p1, p2, count - SIZE);
  }

  LIBC_INLINE static MemcmpReturnType head_tail(CPtr p1, CPtr p2,
                                                size_t count) {
    if constexpr (cmp_is_expensive<T>::value) {
      if (!eq<T>(p1, p2, 0))
        return cmp_neq<T>(p1, p2, 0);
    } else {
      if (const auto value = cmp<T>(p1, p2, 0))
        return value;
    }
    return tail(p1, p2, count);
  }

  LIBC_INLINE static MemcmpReturnType loop_and_tail(CPtr p1, CPtr p2,
                                                    size_t count) {
    return loop_and_tail_offset(p1, p2, count, 0);
  }

  LIBC_INLINE static MemcmpReturnType
  loop_and_tail_offset(CPtr p1, CPtr p2, size_t count, size_t offset) {
    if constexpr (SIZE > 1) {
      const size_t limit = count - SIZE;
      LIBC_LOOP_NOUNROLL
      for (; offset < limit; offset += SIZE) {
        if constexpr (cmp_is_expensive<T>::value) {
          if (!eq<T>(p1, p2, offset))
            return cmp_neq<T>(p1, p2, offset);
        } else {
          if (const auto value = cmp<T>(p1, p2, offset))
            return value;
        }
      }
      return block_offset(p1, p2, limit); // tail
    } else {
      // No need for a tail operation when SIZE == 1.
      LIBC_LOOP_NOUNROLL
      for (; offset < count; offset += SIZE)
        if (auto value = cmp<T>(p1, p2, offset))
          return value;
      return MemcmpReturnType::zero();
    }
  }

  LIBC_INLINE static MemcmpReturnType
  loop_and_tail_align_above(size_t threshold, CPtr p1, CPtr p2, size_t count) {
    const AlignHelper<sizeof(T)> helper(p1);
    if (LIBC_UNLIKELY(count >= threshold) && helper.not_aligned()) {
      if (auto value = block(p1, p2))
        return value;
      adjust(helper.offset, p1, p2, count);
    }
    return loop_and_tail(p1, p2, count);
  }
};

template <typename T, typename... TS> struct MemcmpSequence {
  static constexpr size_t SIZE = (sizeof(T) + ... + sizeof(TS));
  LIBC_INLINE static MemcmpReturnType block(CPtr p1, CPtr p2) {
    // TODO: test suggestion in
    // https://reviews.llvm.org/D148717?id=515724#inline-1446890
    // once we have a proper way to check memory operation latency.
    if constexpr (cmp_is_expensive<T>::value) {
      if (!eq<T>(p1, p2, 0))
        return cmp_neq<T>(p1, p2, 0);
    } else {
      if (auto value = cmp<T>(p1, p2, 0))
        return value;
    }
    if constexpr (sizeof...(TS) > 0)
      return MemcmpSequence<TS...>::block(p1 + sizeof(T), p2 + sizeof(T));
    else
      return MemcmpReturnType::zero();
  }
};

///////////////////////////////////////////////////////////////////////////////
// Bcmp
///////////////////////////////////////////////////////////////////////////////
template <typename T> struct Bcmp {
  static_assert(is_element_type_v<T>);
  static constexpr size_t SIZE = sizeof(T);

  LIBC_INLINE static BcmpReturnType block(CPtr p1, CPtr p2) {
    return neq<T>(p1, p2, 0);
  }

  LIBC_INLINE static BcmpReturnType tail(CPtr p1, CPtr p2, size_t count) {
    const size_t tail_offset = count - SIZE;
    return neq<T>(p1, p2, tail_offset);
  }

  LIBC_INLINE static BcmpReturnType head_tail(CPtr p1, CPtr p2, size_t count) {
    if (const auto value = neq<T>(p1, p2, 0))
      return value;
    return tail(p1, p2, count);
  }

  LIBC_INLINE static BcmpReturnType loop_and_tail(CPtr p1, CPtr p2,
                                                  size_t count) {
    return loop_and_tail_offset(p1, p2, count, 0);
  }

  LIBC_INLINE static BcmpReturnType
  loop_and_tail_offset(CPtr p1, CPtr p2, size_t count, size_t offset) {
    if constexpr (SIZE > 1) {
      const size_t limit = count - SIZE;
      LIBC_LOOP_NOUNROLL
      for (; offset < limit; offset += SIZE)
        if (const auto value = neq<T>(p1, p2, offset))
          return value;
      return tail(p1, p2, count);
    } else {
      // No need for a tail operation when SIZE == 1.
      LIBC_LOOP_NOUNROLL
      for (; offset < count; offset += SIZE)
        if (const auto value = neq<T>(p1, p2, offset))
          return value;
      return BcmpReturnType::zero();
    }
  }

  LIBC_INLINE static BcmpReturnType
  loop_and_tail_align_above(size_t threshold, CPtr p1, CPtr p2, size_t count) {
    static_assert(SIZE > 1,
                  "No need to align when processing one byte at a time");
    const AlignHelper<sizeof(T)> helper(p1);
    if (LIBC_UNLIKELY(count >= threshold) && helper.not_aligned()) {
      if (auto value = block(p1, p2))
        return value;
      adjust(helper.offset, p1, p2, count);
    }
    return loop_and_tail(p1, p2, count);
  }
};

template <typename T, typename... TS> struct BcmpSequence {
  static constexpr size_t SIZE = (sizeof(T) + ... + sizeof(TS));
  LIBC_INLINE static BcmpReturnType block(CPtr p1, CPtr p2) {
    if (auto value = neq<T>(p1, p2, 0))
      return value;
    if constexpr (sizeof...(TS) > 0)
      return BcmpSequence<TS...>::block(p1 + sizeof(T), p2 + sizeof(T));
    else
      return BcmpReturnType::zero();
  }
};

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint8_t
template <> struct cmp_is_expensive<uint8_t> : public cpp::false_type {};
template <> LIBC_INLINE bool eq<uint8_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint8_t>(p1, offset) == load<uint8_t>(p2, offset);
}
template <> LIBC_INLINE uint32_t neq<uint8_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint8_t>(p1, offset) ^ load<uint8_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint8_t>(CPtr p1, CPtr p2, size_t offset) {
  return static_cast<int32_t>(load<uint8_t>(p1, offset)) -
         static_cast<int32_t>(load<uint8_t>(p2, offset));
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint8_t>(CPtr p1, CPtr p2, size_t offset);

} // namespace generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_GENERIC_H

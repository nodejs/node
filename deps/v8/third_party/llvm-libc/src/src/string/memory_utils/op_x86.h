//===-- x86 implementation of memory function building blocks -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides x86 specific building blocks to compose memory functions.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_X86_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_X86_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/__support/macros/is_defined.h"
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"

#if defined(LIBC_TARGET_ARCH_IS_X86)

#include "src/__support/common.h"
#include "src/string/memory_utils/op_builtin.h"
#include "src/string/memory_utils/op_generic.h"

#if defined(__AVX512BW__) || defined(__AVX512F__) || defined(__AVX2__) ||      \
    defined(__SSE2__)
#define LIBC_TARGET_HAS_X86_INTRIN 1
#include <immintrin.h>
#else
#define LIBC_TARGET_HAS_X86_INTRIN 0
#endif

// Define fake functions to prevent the compiler from failing on undefined
// functions in case the CPU extension is not present.
#if !defined(__AVX512BW__) && (defined(_MSC_VER) || defined(__SCE__))
#undef _mm512_cmpneq_epi8_mask
#define _mm512_cmpneq_epi8_mask(A, B) 0
#endif
#if !defined(__AVX2__) && (defined(_MSC_VER) || defined(__SCE__))
#undef _mm256_movemask_epi8
#define _mm256_movemask_epi8(A) 0
#endif
#if !defined(__SSE2__) && (defined(_MSC_VER) || defined(__SCE__))
#undef _mm_movemask_epi8
#define _mm_movemask_epi8(A) 0
#endif

namespace LIBC_NAMESPACE_DECL {
namespace x86 {

// A set of constants to check compile time features.
LIBC_INLINE_VAR constexpr bool K_SSE2 = LLVM_LIBC_IS_DEFINED(__SSE2__);
LIBC_INLINE_VAR constexpr bool K_SSE41 = LLVM_LIBC_IS_DEFINED(__SSE4_1__);
LIBC_INLINE_VAR constexpr bool K_AVX = LLVM_LIBC_IS_DEFINED(__AVX__);
LIBC_INLINE_VAR constexpr bool K_AVX2 = LLVM_LIBC_IS_DEFINED(__AVX2__);
LIBC_INLINE_VAR constexpr bool K_AVX512_F = LLVM_LIBC_IS_DEFINED(__AVX512F__);
LIBC_INLINE_VAR constexpr bool K_AVX512_BW = LLVM_LIBC_IS_DEFINED(__AVX512BW__);

///////////////////////////////////////////////////////////////////////////////
// Memcpy repmovsb implementation
struct Memcpy {
  LIBC_INLINE static void repmovsb(void *dst, const void *src, size_t count) {
#ifdef LIBC_COMPILER_IS_MSVC
    __movsb(static_cast<unsigned char *>(dst),
            static_cast<const unsigned char *>(src), count);
#else
    asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(count) : : "memory");
#endif // LIBC_COMPILER_IS_MSVC
  }
};

} // namespace x86
} // namespace LIBC_NAMESPACE_DECL

namespace LIBC_NAMESPACE_DECL {
namespace generic {

template <typename T> LIBC_INLINE void stream(Ptr dst, T value) {
#if __has_builtin(__builtin_nontemporal_store)
  __builtin_nontemporal_store(value, reinterpret_cast<T *>(dst));
#else
  // Falling back to regular stores is always safe
  store<T>(dst, value);
#endif
}

template <typename T> LIBC_INLINE void fence() {
  // This won't be reached when nothing fancy is enabled, but it can be
  // referenced in if constexpr context, so it's still defined, as a no-op.
#if LIBC_TARGET_HAS_X86_INTRIN
  _mm_sfence();
#endif
}

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
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint16_t>(CPtr p1, CPtr p2, size_t offset);

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint32_t
template <> struct cmp_is_expensive<uint32_t> : public cpp::false_type {};
template <> LIBC_INLINE bool eq<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint32_t>(p1, offset) == load<uint32_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint32_t>(p1, offset) ^ load<uint32_t>(p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint32_t>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load_be<uint32_t>(p1, offset);
  const auto b = load_be<uint32_t>(p2, offset);
  return cmp_uint32_t(a, b);
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint32_t>(CPtr p1, CPtr p2, size_t offset);

///////////////////////////////////////////////////////////////////////////////
// Specializations for uint64_t
template <> struct cmp_is_expensive<uint64_t> : public cpp::true_type {};
template <> LIBC_INLINE bool eq<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  return load<uint64_t>(p1, offset) == load<uint64_t>(p2, offset);
}
template <>
LIBC_INLINE uint32_t neq<uint64_t>(CPtr p1, CPtr p2, size_t offset) {
  return !eq<uint64_t>(p1, p2, offset);
}
template <>
LIBC_INLINE MemcmpReturnType cmp<uint64_t>(CPtr p1, CPtr p2, size_t offset);
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<uint64_t>(CPtr p1, CPtr p2,
                                               size_t offset) {
  const auto a = load_be<uint64_t>(p1, offset);
  const auto b = load_be<uint64_t>(p2, offset);
  return cmp_neq_uint64_t(a, b);
}

// SIMD types are defined with attributes. e.g., '__m128i' is defined as
// long long  __attribute__((__vector_size__(16), __aligned__(16)))
// When we use these SIMD types in template specialization GCC complains:
// "ignoring attributes on template argument ‘__m128i’ [-Wignored-attributes]"
// Therefore, we disable this warning in this file.
#ifndef LIBC_COMPILER_IS_MSVC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif // !LIBC_COMPILER_IS_MSVC

///////////////////////////////////////////////////////////////////////////////
// Specializations for __m128i
#if defined(__SSE4_1__)
template <> struct is_vector<__m128i> : cpp::true_type {};
template <> struct cmp_is_expensive<__m128i> : cpp::true_type {};
LIBC_INLINE __m128i bytewise_max(__m128i a, __m128i b) {
  return _mm_max_epu8(a, b);
}
LIBC_INLINE __m128i bytewise_reverse(__m128i value) {
  return _mm_shuffle_epi8(value, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, //
                                              8, 9, 10, 11, 12, 13, 14, 15));
}
LIBC_INLINE uint16_t big_endian_cmp_mask(__m128i max, __m128i value) {
  return static_cast<uint16_t>(
      _mm_movemask_epi8(bytewise_reverse(_mm_cmpeq_epi8(max, value))));
}
template <> LIBC_INLINE bool eq<__m128i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m128i>(p1, offset);
  const auto b = load<__m128i>(p2, offset);
  const auto xored = _mm_xor_si128(a, b);
  return _mm_testz_si128(xored, xored) == 1; // 1 iff xored == 0
}
template <> LIBC_INLINE uint32_t neq<__m128i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m128i>(p1, offset);
  const auto b = load<__m128i>(p2, offset);
  const auto xored = _mm_xor_si128(a, b);
  return _mm_testz_si128(xored, xored) == 0; // 0 iff xored != 0
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<__m128i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m128i>(p1, offset);
  const auto b = load<__m128i>(p2, offset);
  const auto vmax = bytewise_max(a, b);
  const auto le = big_endian_cmp_mask(vmax, b);
  const auto ge = big_endian_cmp_mask(vmax, a);
  static_assert(cpp::is_same_v<cpp::remove_cv_t<decltype(le)>, uint16_t>);
  return static_cast<int32_t>(ge) - static_cast<int32_t>(le);
}
#endif // __SSE4_1__

///////////////////////////////////////////////////////////////////////////////
// Specializations for __m256i
#if defined(__AVX__)
template <> struct is_vector<__m256i> : cpp::true_type {};
template <> struct cmp_is_expensive<__m256i> : cpp::true_type {};
template <> LIBC_INLINE bool eq<__m256i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m256i>(p1, offset);
  const auto b = load<__m256i>(p2, offset);
  const auto xored = _mm256_castps_si256(
      _mm256_xor_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b)));
  return _mm256_testz_si256(xored, xored) == 1; // 1 iff xored == 0
}
template <> LIBC_INLINE uint32_t neq<__m256i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m256i>(p1, offset);
  const auto b = load<__m256i>(p2, offset);
  const auto xored = _mm256_castps_si256(
      _mm256_xor_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b)));
  return _mm256_testz_si256(xored, xored) == 0; // 0 iff xored != 0
}
#endif // __AVX__

#if defined(__AVX2__)
LIBC_INLINE __m256i bytewise_max(__m256i a, __m256i b) {
  return _mm256_max_epu8(a, b);
}
LIBC_INLINE uint32_t big_endian_cmp_mask(__m256i max, __m256i value) {
  // Bytewise comparison of 'max' and 'value'.
  const __m256i little_endian_byte_mask = _mm256_cmpeq_epi8(max, value);
  // Because x86 is little endian, bytes in the vector must be reversed before
  // using movemask.
#if defined(__AVX512VBMI__) && defined(__AVX512VL__)
  // When AVX512BMI is available we can completely reverse the vector through
  // VPERMB __m256i _mm256_permutexvar_epi8( __m256i idx, __m256i a);
  const __m256i big_endian_byte_mask =
      _mm256_permutexvar_epi8(_mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7,         //
                                              8, 9, 10, 11, 12, 13, 14, 15,   //
                                              16, 17, 18, 19, 20, 21, 22, 23, //
                                              24, 25, 26, 27, 28, 29, 30, 31),
                              little_endian_byte_mask);
  // And turn the byte vector mask into an 'uint32_t' for direct scalar
  // comparison.
  return _mm256_movemask_epi8(big_endian_byte_mask);
#else
  // We can't byte-reverse '__m256i' in a single instruction with AVX2.
  // '_mm256_shuffle_epi8' can only shuffle within each 16-byte lane
  // leading to:
  // ymm = ymm[15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
  //           31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16]
  // So we first shuffle each 16-byte lane leading to half-reversed vector mask.
  const __m256i half_reversed = _mm256_shuffle_epi8(
      little_endian_byte_mask, _mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7,       //
                                               8, 9, 10, 11, 12, 13, 14, 15, //
                                               0, 1, 2, 3, 4, 5, 6, 7,       //
                                               8, 9, 10, 11, 12, 13, 14, 15));
  // Then we turn the vector into an uint32_t.
  const uint32_t half_reversed_scalar = _mm256_movemask_epi8(half_reversed);
  // And swap the lower and upper parts. This is optimized into a single `rorx`
  // instruction.
  return (half_reversed_scalar << 16) | (half_reversed_scalar >> 16);
#endif
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<__m256i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m256i>(p1, offset);
  const auto b = load<__m256i>(p2, offset);
  const auto vmax = bytewise_max(a, b);
  const auto le = big_endian_cmp_mask(vmax, b);
  const auto ge = big_endian_cmp_mask(vmax, a);
  static_assert(cpp::is_same_v<cpp::remove_cv_t<decltype(le)>, uint32_t>);
  return cmp_neq_uint64_t(ge, le);
}
#endif // __AVX2__

///////////////////////////////////////////////////////////////////////////////
// Specializations for __m512i
#if defined(__AVX512BW__)
template <> struct is_vector<__m512i> : cpp::true_type {};
template <> struct cmp_is_expensive<__m512i> : cpp::true_type {};
LIBC_INLINE __m512i bytewise_max(__m512i a, __m512i b) {
  return _mm512_max_epu8(a, b);
}
LIBC_INLINE uint64_t big_endian_cmp_mask(__m512i max, __m512i value) {
  // The AVX512BMI version is disabled due to bad codegen.
  // https://github.com/llvm/llvm-project/issues/77459
  // https://github.com/llvm/llvm-project/pull/77081
  // TODO: Re-enable when clang version meets the fixed version.
#if false && defined(__AVX512VBMI__)
  // When AVX512BMI is available we can completely reverse the vector through
  // VPERMB __m512i _mm512_permutexvar_epi8( __m512i idx, __m512i a);
  const auto indices = _mm512_set_epi8(0, 1, 2, 3, 4, 5, 6, 7,         //
                                       8, 9, 10, 11, 12, 13, 14, 15,   //
                                       16, 17, 18, 19, 20, 21, 22, 23, //
                                       24, 25, 26, 27, 28, 29, 30, 31, //
                                       32, 33, 34, 35, 36, 37, 38, 39, //
                                       40, 41, 42, 43, 44, 45, 46, 47, //
                                       48, 49, 50, 51, 52, 53, 54, 55, //
                                       56, 57, 58, 59, 60, 61, 62, 63);
  // Then we compute the mask for equal bytes.
  return _mm512_cmpeq_epi8_mask(_mm512_permutexvar_epi8(indices, max), //
                                _mm512_permutexvar_epi8(indices, value));
#else
  // We can't byte-reverse '__m512i' in a single instruction with __AVX512BW__.
  // '_mm512_shuffle_epi8' can only shuffle within each 16-byte lane.
  // So we only reverse groups of 8 bytes, these groups are necessarily within a
  // 16-byte lane.
  // zmm = | 16 bytes  | 16 bytes  | 16 bytes  | 16 bytes  |
  // zmm = | <8> | <8> | <8> | <8> | <8> | <8> | <8> | <8> |
  const __m512i indices = _mm512_set_epi8(8, 9, 10, 11, 12, 13, 14, 15, //
                                          0, 1, 2, 3, 4, 5, 6, 7,       //
                                          8, 9, 10, 11, 12, 13, 14, 15, //
                                          0, 1, 2, 3, 4, 5, 6, 7,       //
                                          8, 9, 10, 11, 12, 13, 14, 15, //
                                          0, 1, 2, 3, 4, 5, 6, 7,       //
                                          8, 9, 10, 11, 12, 13, 14, 15, //
                                          0, 1, 2, 3, 4, 5, 6, 7);
  // Then we compute the mask for equal bytes. In this mask the bits of each
  // byte are already reversed but the byte themselves should be reversed, this
  // is done by using a bswap instruction.
  return __builtin_bswap64(
      _mm512_cmpeq_epi8_mask(_mm512_shuffle_epi8(max, indices), //
                             _mm512_shuffle_epi8(value, indices)));

#endif
}
template <> LIBC_INLINE bool eq<__m512i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m512i>(p1, offset);
  const auto b = load<__m512i>(p2, offset);
  return _mm512_cmpneq_epi8_mask(a, b) == 0;
}
template <> LIBC_INLINE uint32_t neq<__m512i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m512i>(p1, offset);
  const auto b = load<__m512i>(p2, offset);
  const uint64_t xored = _mm512_cmpneq_epi8_mask(a, b);
  return static_cast<uint32_t>(xored >> 32) |
         static_cast<uint32_t>(xored & 0xFFFFFFFF);
}
template <>
LIBC_INLINE MemcmpReturnType cmp_neq<__m512i>(CPtr p1, CPtr p2, size_t offset) {
  const auto a = load<__m512i>(p1, offset);
  const auto b = load<__m512i>(p2, offset);
  const auto vmax = bytewise_max(a, b);
  const auto le = big_endian_cmp_mask(vmax, b);
  const auto ge = big_endian_cmp_mask(vmax, a);
  static_assert(cpp::is_same_v<cpp::remove_cv_t<decltype(le)>, uint64_t>);
  return cmp_neq_uint64_t(ge, le);
}
#endif // __AVX512BW__

#ifndef LIBC_COMPILER_IS_MSVC
#pragma GCC diagnostic pop
#endif // !LIBC_COMPILER_IS_MSVC

} // namespace generic
} // namespace LIBC_NAMESPACE_DECL

#endif // LIBC_TARGET_ARCH_IS_X86

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_OP_X86_H

//===-- Strlen implementation for x86_64 ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_STRLEN_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_STRLEN_H

#include "src/__support/CPP/bit.h" // countr_zero

#include <immintrin.h>
#include <stddef.h> // size_t

namespace LIBC_NAMESPACE_DECL {

namespace internal::arch_vector {

// Return a bit-mask with the nth bit set if the nth-byte in block_ptr matches
// character c.
template <typename Vector, typename Mask>
LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE static Mask
compare_and_mask(const Vector *block_ptr, char c);

template <typename Vector, typename Mask,
          Mask (*CompareAndMask)(const Vector *, char)>
LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE static size_t
string_length_vector(const char *src) {
  uintptr_t misalign_bytes = reinterpret_cast<uintptr_t>(src) % sizeof(Vector);

  const Vector *block_ptr =
      reinterpret_cast<const Vector *>(src - misalign_bytes);
  auto cmp = CompareAndMask(block_ptr, 0) >> misalign_bytes;
  if (cmp)
    return cpp::countr_zero(cmp);

  while (true) {
    block_ptr++;
    cmp = CompareAndMask(block_ptr, 0);
    if (cmp)
      return static_cast<size_t>(reinterpret_cast<uintptr_t>(block_ptr) -
                                 reinterpret_cast<uintptr_t>(src) +
                                 cpp::countr_zero(cmp));
  }
}

template <typename Mask>
LIBC_INLINE static void *
calculate_find_first_character_return(const unsigned char *src, Mask c_mask,
                                      size_t n_left) {
  size_t c_offset = cpp::countr_zero(c_mask);
  if (n_left < c_offset)
    return nullptr;
  return const_cast<unsigned char *>(src) + c_offset;
}

template <typename Vector, typename Mask,
          Mask (*CompareAndMask)(const Vector *, char)>
LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE static void *
find_first_character_vector(const unsigned char *s, unsigned char c, size_t n) {
  uintptr_t misalign_bytes = reinterpret_cast<uintptr_t>(s) % sizeof(Vector);

  const Vector *block_ptr =
      reinterpret_cast<const Vector *>(s - misalign_bytes);
  auto cmp_bytes = CompareAndMask(block_ptr, c) >> misalign_bytes;
  if (cmp_bytes)
    return calculate_find_first_character_return<Mask>(
        reinterpret_cast<const unsigned char *>(block_ptr) + misalign_bytes,
        cmp_bytes, n);

  for (size_t bytes_checked = sizeof(Vector) - misalign_bytes;
       bytes_checked < n; bytes_checked += sizeof(Vector)) {
    block_ptr++;
    cmp_bytes = CompareAndMask(block_ptr, c);
    if (cmp_bytes)
      return calculate_find_first_character_return<Mask>(
          reinterpret_cast<const unsigned char *>(block_ptr), cmp_bytes,
          n - bytes_checked);
  }
  return nullptr;
}

template <>
LIBC_INLINE uint32_t
compare_and_mask<__m128i, uint32_t>(const __m128i *block_ptr, char c) {
  __m128i b = _mm_load_si128(block_ptr);
  __m128i set = _mm_set1_epi8(c);
  __m128i cmp = _mm_cmpeq_epi8(b, set);
  return _mm_movemask_epi8(cmp);
}

namespace sse2 {
[[maybe_unused]] LIBC_INLINE size_t string_length(const char *src) {
  return string_length_vector<__m128i, uint32_t,
                              compare_and_mask<__m128i, uint32_t>>(src);
}

[[maybe_unused]] LIBC_INLINE void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
  return find_first_character_vector<__m128i, uint32_t,
                                     compare_and_mask<__m128i, uint32_t>>(s, c,
                                                                          n);
}

} // namespace sse2

#if defined(__AVX2__)
template <>
LIBC_INLINE uint32_t
compare_and_mask<__m256i, uint32_t>(const __m256i *block_ptr, char c) {
  __m256i b = _mm256_load_si256(block_ptr);
  __m256i set = _mm256_set1_epi16(c);
  __m256i cmp = _mm256_cmpeq_epi8(b, set);
  return _mm256_movemask_epi8(cmp);
}

namespace avx2 {
[[maybe_unused]] LIBC_INLINE size_t string_length(const char *src) {
  return string_length_vector<__m256i, uint32_t,
                              compare_and_mask<__m256i, uint32_t>>(src);
}

[[maybe_unused]] LIBC_INLINE void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
  return find_first_character_vector<__m256i, uint32_t,
                                     compare_and_mask<__m256i, uint32_t>>(s, c,
                                                                          n);
}
} // namespace avx2
#endif

#if defined(__AVX512F__)
template <>
LIBC_INLINE __mmask64
compare_and_mask<__m512i, __mmask64>(const __m512i *block_ptr, char c) {
  __m512i v = _mm512_load_si512(block_ptr);
  __m512i set = _mm512_set1_epi8(c);
  return _mm512_cmp_epu8_mask(set, v, _MM_CMPINT_EQ);
}

namespace avx512 {
[[maybe_unused]] LIBC_INLINE size_t string_length(const char *src) {
  return string_length_vector<__m512i, __mmask64,
                              compare_and_mask<__m512i, __mmask64>>(src);
}

[[maybe_unused]] LIBC_INLINE void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
  return find_first_character_vector<__m512i, __mmask64,
                                     compare_and_mask<__m512i, __mmask64>>(s, c,
                                                                           n);
}

} // namespace avx512
#endif

// We could directly use the various <function>_vector templates here, but this
// indirection allows comparing the various implementations elsewhere by name,
// without having to instantiate the templates by hand at those locations.

[[maybe_unused]] LIBC_INLINE size_t string_length(const char *src) {
#if defined(__AVX512F__)
  return avx512::string_length(src);
#elif defined(__AVX2__)
  return avx2::string_length(src);
#else
  return sse2::string_length(src);
#endif
}

[[maybe_unused]] LIBC_INLINE void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
#if defined(__AVX512F__)
  return avx512::find_first_character(s, c, n);
#elif defined(__AVX2__)
  return avx2::find_first_character(s, c, n);
#else
  return sse2::find_first_character(s, c, n);
#endif
}

} // namespace internal::arch_vector

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_X86_64_INLINE_STRLEN_H

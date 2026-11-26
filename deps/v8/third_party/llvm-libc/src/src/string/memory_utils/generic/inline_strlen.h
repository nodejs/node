//===-- Strlen for generic SIMD types -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_INLINE_STRLEN_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_INLINE_STRLEN_H

#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/simd.h"
#include "src/__support/common.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace clang_vector {

// Exploit the underlying integer representation to do a variable shift.
template <typename byte_ty>
LIBC_INLINE constexpr cpp::simd_mask<byte_ty> shift_mask(cpp::simd_mask<char> m,
                                                         size_t shift) {
  using bitmask_ty = cpp::simd_bitmask<char>;
  bitmask_ty r = cpp::to_bitmask(m) >> shift;
  return cpp::bit_cast<cpp::simd_mask<byte_ty>>(r);
}

LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE size_t string_length(const char *src) {
  constexpr cpp::simd<char> null_byte = cpp::splat('\0');

  size_t alignment = alignof(cpp::simd<char>);
  const cpp::simd<char> *aligned = reinterpret_cast<const cpp::simd<char> *>(
      __builtin_align_down(src, alignment));

  cpp::simd<char> chars = cpp::load<cpp::simd<char>>(aligned, /*aligned=*/true);
  cpp::simd_mask<char> mask = chars == null_byte;
  size_t offset = src - reinterpret_cast<const char *>(aligned);
  if (cpp::any_of(shift_mask<char>(mask, offset)))
    return cpp::find_first_set(shift_mask<char>(mask, offset));

  for (;;) {
    chars = cpp::load<cpp::simd<char>>(++aligned, /*aligned=*/true);
    mask = chars == null_byte;
    if (cpp::any_of(mask))
      return (reinterpret_cast<const char *>(aligned) - src) +
             cpp::find_first_set(mask);
  }
}

LIBC_INLINE static const void *calculate_find_first_character_return(
    const char *src, cpp::simd_mask<char> c_mask, size_t n_left) {
  size_t c_offset = cpp::find_first_set(c_mask);
  if (n_left <= c_offset)
    return nullptr;
  return src + c_offset;
}

LIBC_INLINE_VAR constexpr size_t VEC_SIZE = sizeof(cpp::simd<char>);
LIBC_INLINE_VAR constexpr size_t LOOP_UNROLL_FACTOR = 4;
LIBC_INLINE_VAR constexpr size_t LOOP_BLOCK_SIZE =
    LOOP_UNROLL_FACTOR * VEC_SIZE;

// A find_first_character implementation for inputs less than the loop block
// size. It loads VecN vectors with the end of the last vector aligning
// with the end of the buffer passed in.
template <size_t VecN>
LIBC_NO_SANITIZE_OOB_ACCESS LIBC_ALWAYS_INLINE static const void *
find_first_character_small(const unsigned char *s, cpp::simd<char> c,
                           size_t n) {
  static_assert(VecN >= 2 && VecN <= LOOP_UNROLL_FACTOR);

  const unsigned char *ptrs[VecN];
  cpp::simd_mask<char> masks[VecN];
  for (size_t i = 0; i < VecN - 1; ++i) {
    ptrs[i] = s + i * VEC_SIZE;
    masks[i] = cpp::load<cpp::simd<char>>(ptrs[i], /*aligned=*/false) == c;
  }
  ptrs[VecN - 1] = s + n - VEC_SIZE;
  masks[VecN - 1] =
      cpp::load<cpp::simd<char>>(ptrs[VecN - 1], /*aligned=*/false) == c;

  cpp::simd_mask<char> combined = masks[0] | masks[1];
  if constexpr (VecN == 3)
    combined = combined | masks[2];
  else if constexpr (VecN == 4)
    combined = combined | (masks[2] | masks[3]);

  if (cpp::any_of(combined)) {
    for (size_t i = 0; i < VecN - 1; ++i)
      if (cpp::any_of(masks[i]))
        return ptrs[i] + cpp::find_first_set(masks[i]);
    return ptrs[VecN - 1] + cpp::find_first_set(masks[VecN - 1]);
  }
  return nullptr;
}

// A find_first_character implementation for large inputs. The input is
// expected to be aligned to LOOP_BLOCK_SIZE. There is then a loop that
// does four VEC_SIZE loads per iteration.
LIBC_NO_SANITIZE_OOB_ACCESS LIBC_ALWAYS_INLINE static const void *
find_first_aligned_loop(const cpp::simd<char> *s, cpp::simd<char> c, size_t n) {
  LIBC_LOOP_NOUNROLL
  for (;;) {
    cpp::simd_mask<char> m0 =
        cpp::load<cpp::simd<char>>(s, /*aligned=*/true) == c;
    cpp::simd_mask<char> m1 =
        cpp::load<cpp::simd<char>>(s + 1, /*aligned=*/true) == c;
    cpp::simd_mask<char> m2 =
        cpp::load<cpp::simd<char>>(s + 2, /*aligned=*/true) == c;
    cpp::simd_mask<char> m3 =
        cpp::load<cpp::simd<char>>(s + 3, /*aligned=*/true) == c;
    if (LIBC_UNLIKELY(cpp::any_of((m0 | m1) | (m2 | m3)))) {
      size_t found_offset;
      if (cpp::any_of(m0))
        found_offset = cpp::find_first_set(m0);
      else if (cpp::any_of(m1))
        found_offset = VEC_SIZE + cpp::find_first_set(m1);
      else if (cpp::any_of(m2))
        found_offset = 2 * VEC_SIZE + cpp::find_first_set(m2);
      else
        found_offset = 3 * VEC_SIZE + cpp::find_first_set(m3);
      if (found_offset >= n)
        return nullptr;
      return reinterpret_cast<const char *>(s) + found_offset;
    }
    if (n <= LOOP_BLOCK_SIZE)
      return nullptr;
    s += LOOP_UNROLL_FACTOR;
    n -= LOOP_BLOCK_SIZE;
  }
}

LIBC_NO_SANITIZE_OOB_ACCESS LIBC_ALWAYS_INLINE static const void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
  // s doesn't have to be dereferenceable if there is nothing to search.
  if (n == 0)
    return nullptr;

  constexpr size_t min_page_size = 4096;
  static_assert(cpp::has_single_bit(min_page_size) &&
                    cpp::has_single_bit(LOOP_BLOCK_SIZE) &&
                    min_page_size % LOOP_BLOCK_SIZE == 0,
                "4x vector block must cleanly divide a page.");

  cpp::simd<char> c_byte = c;

  if (n <= LOOP_BLOCK_SIZE) {
    const uintptr_t page_offset =
        reinterpret_cast<uintptr_t>(s) & (min_page_size - 1);
    if (LIBC_LIKELY(page_offset <= min_page_size - LOOP_BLOCK_SIZE)) {
      if (n <= VEC_SIZE) {
        cpp::simd_mask<char> m0 =
            cpp::load<cpp::simd<char>>(s, /*aligned=*/false) == c_byte;
        return cpp::any_of(m0) ? calculate_find_first_character_return(
                                     reinterpret_cast<const char *>(s), m0, n)
                               : nullptr;
      }
      if (n <= 2 * VEC_SIZE)
        return find_first_character_small<2>(s, c_byte, n);
      if (n <= 3 * VEC_SIZE)
        return find_first_character_small<3>(s, c_byte, n);
      return find_first_character_small<4>(s, c_byte, n);
    }
  }

  const cpp::simd<char> *loop_aligned;
  if (LOOP_BLOCK_SIZE <= 64 && n > LOOP_BLOCK_SIZE) {
    // Align s down to LOOP_BLOCK_SIZE; all 4 vectors lie on the same page.
    const cpp::simd<char> *block = reinterpret_cast<const cpp::simd<char> *>(
        __builtin_align_down(s, LOOP_BLOCK_SIZE));
    size_t offset =
        static_cast<size_t>(s - reinterpret_cast<const unsigned char *>(block));
    uint64_t combined = 0;
    for (size_t i = 0; i < LOOP_UNROLL_FACTOR; ++i) {
      cpp::simd_mask<char> m =
          cpp::load<cpp::simd<char>>(block + i, /*aligned=*/true) == c_byte;
      combined |= static_cast<uint64_t>(cpp::to_bitmask(m)) << (i * VEC_SIZE);
    }
    combined >>= offset;
    if (LIBC_UNLIKELY(combined != 0))
      return s + cpp::countr_zero(combined);
    n -= LOOP_BLOCK_SIZE - offset;
    loop_aligned = block + LOOP_UNROLL_FACTOR;
  } else {
    // Fallback for page-crossing small inputs or wide vectors (LOOP_BLOCK_SIZE
    // > 64): step aligned vectors until done or LOOP_BLOCK_SIZE-aligned.
    size_t alignment = alignof(cpp::simd<char>);
    const cpp::simd<char> *aligned = reinterpret_cast<const cpp::simd<char> *>(
        __builtin_align_down(s, alignment));
    size_t offset = static_cast<size_t>(
        s - reinterpret_cast<const unsigned char *>(aligned));
    cpp::simd_mask<char> cmp_v = shift_mask<unsigned char>(
        cpp::load<cpp::simd<char>>(aligned, /*aligned=*/true) == c_byte,
        offset);
    if (cpp::any_of(cmp_v))
      return calculate_find_first_character_return(
          reinterpret_cast<const char *>(s), cmp_v, n);
    if (n <= VEC_SIZE - offset)
      return nullptr;
    n -= VEC_SIZE - offset;
    ++aligned;

    while ((reinterpret_cast<uintptr_t>(aligned) & (LOOP_BLOCK_SIZE - 1)) !=
           0) {
      cpp::simd_mask<char> m =
          cpp::load<cpp::simd<char>>(aligned, /*aligned=*/true) == c_byte;
      if (cpp::any_of(m))
        return calculate_find_first_character_return(
            reinterpret_cast<const char *>(aligned), m, n);
      if (n <= VEC_SIZE)
        return nullptr;
      n -= VEC_SIZE;
      ++aligned;
    }
    loop_aligned = aligned;
  }
  return find_first_aligned_loop(loop_aligned, c_byte, n);
}

} // namespace clang_vector

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_INLINE_STRLEN_H

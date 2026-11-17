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

namespace LIBC_NAMESPACE_DECL {
namespace clang_vector {

// Exploit the underlying integer representation to do a variable shift.
template <typename byte_ty>
LIBC_INLINE constexpr cpp::simd_mask<byte_ty> shift_mask(cpp::simd_mask<char> m,
                                                         size_t shift) {
  using bitmask_ty = cpp::internal::get_as_integer_type_t<cpp::simd_mask<char>>;
  bitmask_ty r = cpp::bit_cast<bitmask_ty>(m) >> shift;
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

LIBC_INLINE static void *calculate_find_first_character_return(
    const char *src, cpp::simd_mask<char> c_mask, size_t n_left) {
  size_t c_offset = cpp::find_first_set(c_mask);
  if (n_left < c_offset)
    return nullptr;
  return const_cast<char *>(src) + c_offset;
}

LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE static void *
find_first_character(const unsigned char *s, unsigned char c, size_t n) {
  using Vector = cpp::simd<char>;
  using Mask = cpp::simd_mask<char>;
  Vector c_byte = c;

  size_t alignment = alignof(Vector);
  const Vector *aligned =
      reinterpret_cast<const Vector *>(__builtin_align_down(s, alignment));

  Vector chars = cpp::load<Vector>(aligned, /*aligned=*/true);
  Mask cmp_v = chars == c_byte;
  size_t offset = s - reinterpret_cast<const unsigned char *>(aligned);

  cmp_v = shift_mask<unsigned char>(cmp_v, offset);
  if (cpp::any_of(cmp_v))
    return calculate_find_first_character_return(
        reinterpret_cast<const char *>(s), cmp_v, n);

  for (size_t bytes_checked = sizeof(Vector) - offset; bytes_checked < n;
       bytes_checked += sizeof(Vector)) {
    aligned++;
    chars = cpp::load<Vector>(aligned, /*aligned=*/true);
    cmp_v = chars == c_byte;
    if (cpp::any_of(cmp_v))
      return calculate_find_first_character_return(
          reinterpret_cast<const char *>(aligned), cmp_v, n - bytes_checked);
  }
  return nullptr;
}

} // namespace clang_vector

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_GENERIC_INLINE_STRLEN_H

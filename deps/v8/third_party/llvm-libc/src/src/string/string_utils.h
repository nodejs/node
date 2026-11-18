//===-- String utils --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Standalone string utility functions. Utilities requiring memory allocations
// should be placed in allocating_string_utils.h instead.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_STRING_UTILS_H
#define LLVM_LIBC_SRC_STRING_STRING_UTILS_H

#include "hdr/types/size_t.h"
#include "src/__support/CPP/bitset.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_length.h"

namespace LIBC_NAMESPACE_DECL {
namespace internal {

LIBC_INLINE int strcoll(const char *left, const char *right) {
  for (; *left && *left == *right; ++left, ++right)
    ;
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

// Returns the maximum length span that contains only characters not found in
// 'segment'. If no characters are found, returns the length of 'src'.
LIBC_INLINE size_t complementary_span(const char *src, const char *segment) {
  const char *initial = src;
  cpp::bitset<256> bitset;

  for (; *segment; ++segment)
    bitset.set(*reinterpret_cast<const unsigned char *>(segment));
  for (; *src && !bitset.test(*reinterpret_cast<const unsigned char *>(src));
       ++src)
    ;
  return static_cast<size_t>(src - initial);
}

// Given the similarities between strtok and strtok_r, we can implement both
// using a utility function. On the first call, 'src' is scanned for the
// first character not found in 'delimiter_string'. Once found, it scans until
// the first character in the 'delimiter_string' or the null terminator is
// found. We define this span as a token. The end of the token is appended with
// a null terminator, and the token is returned. The point where the last token
// is found is then stored within 'context' for subsequent calls. Subsequent
// calls will use 'context' when a nullptr is passed in for 'src'. Once the null
// terminating character is reached, returns a nullptr.
template <bool SkipDelim = true>
LIBC_INLINE char *string_token(char *__restrict src,
                               const char *__restrict delimiter_string,
                               char **__restrict context) {
  // Return nullptr immediately if both src AND context are nullptr
  if (LIBC_UNLIKELY(src == nullptr && ((src = *context) == nullptr)))
    return nullptr;

  static_assert(CHAR_BIT == 8, "bitset of 256 assumes char is 8 bits");
  cpp::bitset<256> delims;
  for (; *delimiter_string != '\0'; ++delimiter_string)
    delims.set(*reinterpret_cast<const unsigned char *>(delimiter_string));

  unsigned char *tok_start = reinterpret_cast<unsigned char *>(src);
  if constexpr (SkipDelim)
    while (*tok_start != '\0' && delims.test(*tok_start))
      ++tok_start;
  if (*tok_start == '\0' && SkipDelim) {
    *context = nullptr;
    return nullptr;
  }

  unsigned char *tok_end = tok_start;
  while (*tok_end != '\0' && !delims.test(*tok_end))
    ++tok_end;

  if (*tok_end == '\0') {
    *context = nullptr;
  } else {
    *tok_end = '\0';
    *context = reinterpret_cast<char *>(tok_end + 1);
  }
  return reinterpret_cast<char *>(tok_start);
}

LIBC_INLINE size_t strlcpy(char *__restrict dst, const char *__restrict src,
                           size_t size) {
  size_t len = internal::string_length(src);
  if (!size)
    return len;
  size_t n = len < size - 1 ? len : size - 1;
  inline_memcpy(dst, src, n);
  dst[n] = '\0';
  return len;
}

template <bool ReturnNull = true>
LIBC_INLINE constexpr static char *strchr_implementation(const char *src,
                                                         int c) {
  char ch = static_cast<char>(c);
  for (; *src && *src != ch; ++src)
    ;
  char *ret = ReturnNull ? nullptr : const_cast<char *>(src);
  return *src == ch ? const_cast<char *>(src) : ret;
}

LIBC_INLINE constexpr static char *strrchr_implementation(const char *src,
                                                          int c) {
  char ch = static_cast<char>(c);
  char *last_occurrence = nullptr;
  while (true) {
    if (*src == ch)
      last_occurrence = const_cast<char *>(src);
    if (!*src)
      return last_occurrence;
    ++src;
  }
}

// Returns the first occurrence of 'ch' within the first 'n' characters of
// 'src'. If 'ch' is not found, returns nullptr.
LIBC_INLINE void *find_first_character(const unsigned char *src,
                                       unsigned char ch, size_t max_strlen) {
  return find_first_character_impl(src, ch, max_strlen);
}

LIBC_INLINE size_t strnlen(const char *s, size_t max_len) {
  const void *temp = internal::find_first_character(
      reinterpret_cast<const unsigned char *>(s), '\0', max_len);
  return temp ? reinterpret_cast<const char *>(temp) - s : max_len;
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif //  LLVM_LIBC_SRC_STRING_STRING_UTILS_H

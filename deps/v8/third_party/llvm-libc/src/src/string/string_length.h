//===-- String Length -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Basic implementation and dispatch mechanism for performance-sensitive string-
// related code.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_STRING_LENGTH_H
#define LLVM_LIBC_SRC_STRING_STRING_LENGTH_H

#include "hdr/limits_macros.h"
#include "hdr/stdint_proxy.h" // uintptr_t
#include "hdr/types/size_t.h"
#include "src/__support/CPP/type_traits.h" // cpp::is_same_v

#if LIBC_HAS_VECTOR_TYPE
#include "src/string/memory_utils/generic/inline_strlen.h"
#endif
#if defined(LIBC_TARGET_ARCH_IS_X86)
#include "src/string/memory_utils/x86_64/inline_strlen.h"
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64) &&                                  \
    (defined(LIBC_TARGET_CPU_HAS_SVE) || defined(__ARM_NEON))
#include "src/string/memory_utils/aarch64/inline_strlen.h"
#endif

// Set sensible defaults
#ifndef LIBC_COPT_STRING_LENGTH_IMPL
#define LIBC_COPT_STRING_LENGTH_IMPL element
#endif
#ifndef LIBC_COPT_FIND_FIRST_CHARACTER_IMPL
#define LIBC_COPT_FIND_FIRST_CHARACTER_IMPL element
#endif

namespace LIBC_NAMESPACE_DECL {
namespace internal {

#if !LIBC_HAS_VECTOR_TYPE
// Forward any clang vector impls to architecture specific ones
namespace arch_vector {}
namespace clang_vector = arch_vector;
#endif

namespace element {
// Element-by-element (usually a byte, but wider for wchar) implementations of
// functions that search for data.  Slow, but easy to understand and analyze.

// Returns the length of a string, denoted by the first occurrence
// of a null terminator.
LIBC_INLINE size_t string_length(const char *src) {
  size_t length;
  for (length = 0; *src; ++src, ++length)
    ;
  return length;
}

template <typename T> LIBC_INLINE size_t string_length_element(const T *src) {
  size_t length;
  for (length = 0; *src; ++src, ++length)
    ;
  return length;
}

LIBC_INLINE void *find_first_character(const unsigned char *src,
                                       unsigned char ch, size_t n) {
  for (; n && *src != ch; --n, ++src)
    ;
  return n ? const_cast<unsigned char *>(src) : nullptr;
}
} // namespace element

namespace word {
// Non-vector, implementations of functions that search for data by reading from
// memory word-by-word.

template <typename Word> LIBC_INLINE constexpr Word repeat_byte(Word byte) {
  static_assert(CHAR_BIT == 8, "repeat_byte assumes a byte is 8 bits.");
  constexpr size_t BITS_IN_BYTE = CHAR_BIT;
  constexpr size_t BYTE_MASK = 0xff;
  Word result = 0;
  byte = byte & BYTE_MASK;
  for (size_t i = 0; i < sizeof(Word); ++i)
    result = (result << BITS_IN_BYTE) | byte;
  return result;
}

// The goal of this function is to take in a block of arbitrary size and return
// if it has any bytes equal to zero without branching. This is done by
// transforming the block such that zero bytes become non-zero and non-zero
// bytes become zero.
// The first transformation relies on the properties of carrying in arithmetic
// subtraction. Specifically, if 0x01 is subtracted from a byte that is 0x00,
// then the result for that byte must be equal to 0xff (or 0xfe if the next byte
// needs a carry as well).
// The next transformation is a simple mask. All zero bytes will have the high
// bit set after the subtraction, so each byte is masked with 0x80. This narrows
// the set of bytes that result in a non-zero value to only zero bytes and bytes
// with the high bit and any other bit set.
// The final transformation masks the result of the previous transformations
// with the inverse of the original byte. This means that any byte that had the
// high bit set will no longer have it set, narrowing the list of bytes which
// result in non-zero values to just the zero byte.
template <typename Word> LIBC_INLINE constexpr bool has_zeroes(Word block) {
  constexpr unsigned int LOW_BITS = repeat_byte<Word>(0x01);
  constexpr Word HIGH_BITS = repeat_byte<Word>(0x80);
  Word subtracted = block - LOW_BITS;
  Word inverted = ~block;
  return (subtracted & inverted & HIGH_BITS) != 0;
}

// Unsigned int is the default size for most processors, and on x86-64 it
// performs better than larger sizes when the src pointer can't be assumed to
// be aligned to a word boundary, so it's the size we use for reading the
// string a block at a time.

LIBC_INLINE size_t string_length(const char *src) {
  using Word = unsigned int;
  const char *char_ptr = src;
  // Step 1: read 1 byte at a time to align to block size
  for (; reinterpret_cast<uintptr_t>(char_ptr) % sizeof(Word) != 0;
       ++char_ptr) {
    if (*char_ptr == '\0')
      return static_cast<size_t>(char_ptr - src);
  }
  // Step 2: read blocks
  for (const Word *block_ptr = reinterpret_cast<const Word *>(char_ptr);
       !has_zeroes<Word>(*block_ptr); ++block_ptr) {
    char_ptr = reinterpret_cast<const char *>(block_ptr);
  }
  // Step 3: find the zero in the block
  for (; *char_ptr != '\0'; ++char_ptr) {
    ;
  }
  return static_cast<size_t>(char_ptr - src);
}

LIBC_NO_SANITIZE_OOB_ACCESS LIBC_INLINE void *
find_first_character(const unsigned char *src, unsigned char ch,
                     size_t max_strlen = cpp::numeric_limits<size_t>::max()) {
  using Word = unsigned int;
  const unsigned char *char_ptr = src;
  size_t cur = 0;

  // If the maximum size of the string is small, the overhead of aligning to a
  // word boundary and generating a bitmask of the appropriate size may be
  // greater than the gains from reading larger chunks. Based on some testing,
  // the crossover point between when it's faster to just read bytewise and read
  // blocks is somewhere between 16 and 32, so 4 times the size of the block
  // should be in that range.
  if (max_strlen < (sizeof(Word) * 4)) {
    return element::find_first_character(src, ch, max_strlen);
  }
  size_t n = max_strlen;
  // Step 1: read 1 byte at a time to align to block size
  for (; cur < n && reinterpret_cast<uintptr_t>(char_ptr) % sizeof(Word) != 0;
       ++cur, ++char_ptr) {
    if (*char_ptr == ch)
      return const_cast<unsigned char *>(char_ptr);
  }

  const Word ch_mask = repeat_byte<Word>(ch);

  // Step 2: read blocks
  const Word *block_ptr = reinterpret_cast<const Word *>(char_ptr);
  for (; cur < n && !has_zeroes<Word>((*block_ptr) ^ ch_mask);
       cur += sizeof(Word), ++block_ptr)
    ;
  char_ptr = reinterpret_cast<const unsigned char *>(block_ptr);

  // Step 3: find the match in the block
  for (; cur < n && *char_ptr != ch; ++cur, ++char_ptr) {
    ;
  }

  if (cur >= n || *char_ptr != ch)
    return static_cast<void *>(nullptr);

  return const_cast<unsigned char *>(char_ptr);
}

} // namespace word

// Dispatch mechanism for implementations of performance-sensitive
// functions. Always measure, but generally from lower- to higher-performance
// order:
//
// 1. element - read char-by-char or wchar-by-wchar
// 3. word - read word-by-word
// 3. clang_vector - read using clang's internal vector types
// 4. arch_vector - hand-coded per architecture. Possibly in asm, or with
// intrinsics.
//
// The called implementation is chosen at build-time by setting
// LIBC_CONF_{FUNC}_IMPL in config.json
static constexpr auto &string_length_impl =
    LIBC_COPT_STRING_LENGTH_IMPL::string_length;
static constexpr auto &find_first_character_impl =
    LIBC_COPT_FIND_FIRST_CHARACTER_IMPL::find_first_character;

template <typename T> LIBC_INLINE size_t string_length(const T *src) {
  if constexpr (cpp::is_same_v<T, char>)
    return string_length_impl(src);
  return element::string_length_element<T>(src);
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif //  LLVM_LIBC_SRC_STRING_STRING_LENGTH_H

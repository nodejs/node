//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines POSIX character class bitset tables and lookup functions for regex.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_CHARCLASS_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_CHARCLASS_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bitset.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

struct PosixClassEntry {
  static constexpr size_t MAX_NAME_LEN = 7;

  cpp::array<char, MAX_NAME_LEN> name{};
  uint8_t len = 0;
  ByteSet set{};

  template <size_t N>
  LIBC_INLINE constexpr PosixClassEntry(const char (&s)[N], const ByteSet &bs)
      : len(static_cast<uint8_t>(N - 1)), set(bs) {
    static_assert(N - 1 <= MAX_NAME_LEN, "POSIX class name exceeds buffer");
    // Evaluated only at compile time; inline_memcpy is not constexpr.
    for (size_t i = 0; i < N - 1; ++i)
      name[i] = s[i];
  }
};

template <typename Pred>
LIBC_INLINE constexpr ByteSet make_class_set(Pred pred) {
  ByteSet bs;
  for (size_t c = 0; c < 256; ++c)
    if (pred(static_cast<char>(c)))
      bs.set(c);
  return bs;
}

LIBC_INLINE_VAR constexpr ByteSet DIGIT_CLASS_SET =
    make_class_set([](char c) { return internal::isdigit(c); });

LIBC_INLINE_VAR constexpr ByteSet SPACE_CLASS_SET =
    make_class_set([](char c) { return internal::isspace(c); });

LIBC_INLINE_VAR constexpr ByteSet WORD_CLASS_SET =
    make_class_set([](char c) { return internal::isalnum(c) || c == '_'; });

LIBC_INLINE_VAR constexpr cpp::array<PosixClassEntry, 12> POSIX_CLASSES = {{
    {"alnum", make_class_set([](char c) { return internal::isalnum(c); })},
    {"alpha", make_class_set([](char c) { return internal::isalpha(c); })},
    {"blank", make_class_set([](char c) { return c == ' ' || c == '\t'; })},
    {"cntrl", make_class_set([](char c) {
       const unsigned ch = static_cast<unsigned char>(c);
       return ch < 0x20 || ch == 0x7F;
     })},
    {"digit", DIGIT_CLASS_SET},
    {"graph", make_class_set([](char c) { return internal::isgraph(c); })},
    {"lower", make_class_set([](char c) { return internal::islower(c); })},
    {"print", make_class_set([](char c) {
       const unsigned ch = static_cast<unsigned char>(c);
       return (ch - ' ') < 95;
     })},
    {"punct", make_class_set([](char c) {
       return !internal::isalnum(c) && internal::isgraph(c);
     })},
    {"space", SPACE_CLASS_SET},
    {"upper", make_class_set([](char c) { return internal::isupper(c); })},
    {"xdigit", make_class_set([](char c) {
       return internal::isdigit(c) || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F');
     })},
}};

/// Unions the named POSIX character class into \p out.
/// Returns false if \p name is not a recognized POSIX class name.
[[nodiscard]] LIBC_INLINE constexpr bool add_posix_class(cpp::string_view name,
                                                         ByteSet &out) {
  for (const auto &entry : POSIX_CLASSES) {
    if (cpp::string_view(entry.name.data(), entry.len) == name) {
      out |= entry.set;
      return true;
    }
  }
  return false;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_CHARCLASS_H

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

#include "src/__support/CPP/array.h"
#include "src/__support/CPP/bitset.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

struct PosixClassEntry {
  cpp::string_view name;
  ByteSet set;
};

template <typename Pred>
LIBC_INLINE constexpr ByteSet make_class_set(Pred pred) {
  ByteSet bs;
  for (size_t c = 0; c < 256; ++c)
    if (pred(static_cast<char>(c)))
      bs.set(c);
  return bs;
}

LIBC_INLINE constexpr ByteSet digit_class_set() {
  return make_class_set([](char c) { return internal::isdigit(c); });
}

LIBC_INLINE constexpr ByteSet space_class_set() {
  return make_class_set([](char c) { return internal::isspace(c); });
}

LIBC_INLINE constexpr ByteSet word_class_set() {
  return make_class_set(
      [](char c) { return internal::isalnum(c) || c == '_'; });
}

LIBC_INLINE constexpr cpp::array<PosixClassEntry, 12> POSIX_CLASSES = {{
    {"alnum", make_class_set([](char c) { return internal::isalnum(c); })},
    {"alpha", make_class_set([](char c) { return internal::isalpha(c); })},
    {"blank", make_class_set([](char c) { return c == ' ' || c == '\t'; })},
    {"cntrl", make_class_set([](char c) {
       const unsigned ch = static_cast<unsigned char>(c);
       return ch < 0x20 || ch == 0x7F;
     })},
    {"digit", digit_class_set()},
    {"graph", make_class_set([](char c) { return internal::isgraph(c); })},
    {"lower", make_class_set([](char c) { return internal::islower(c); })},
    {"print", make_class_set([](char c) {
       const unsigned ch = static_cast<unsigned char>(c);
       return (ch - ' ') < 95;
     })},
    {"punct", make_class_set([](char c) {
       return !internal::isalnum(c) && internal::isgraph(c);
     })},
    {"space", space_class_set()},
    {"upper", make_class_set([](char c) { return internal::isupper(c); })},
    {"xdigit", make_class_set([](char c) {
       return internal::isdigit(c) || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F');
     })},
}};

LIBC_INLINE constexpr cpp::optional<ByteSet>
lookup_posix_class(cpp::string_view name) {
  for (const auto &entry : POSIX_CLASSES)
    if (entry.name == name)
      return entry.set;
  return cpp::nullopt;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_CHARCLASS_H

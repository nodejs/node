//===-- Implementation of wctype_t dependent functions -----------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WCTYPE_IMPL_H
#define LLVM_LIBC_SRC___SUPPORT_WCTYPE_IMPL_H

#include "hdr/types/wchar_t.h"
#include "hdr/types/wctype_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"
#include "src/__support/wctype_utils.h" // For isalnum, isalpha, etc.

namespace LIBC_NAMESPACE_DECL {
namespace internal {

struct wctype_mapping {
  cpp::string_view name;
  wctype_t desc;
};

LIBC_INLINE constexpr wctype_t WCTYPE_INVALID = 0;
LIBC_INLINE constexpr wctype_t WCTYPE_ALNUM = 1;
LIBC_INLINE constexpr wctype_t WCTYPE_ALPHA = 2;
LIBC_INLINE constexpr wctype_t WCTYPE_BLANK = 3;
LIBC_INLINE constexpr wctype_t WCTYPE_CNTRL = 4;
LIBC_INLINE constexpr wctype_t WCTYPE_DIGIT = 5;
LIBC_INLINE constexpr wctype_t WCTYPE_GRAPH = 6;
LIBC_INLINE constexpr wctype_t WCTYPE_LOWER = 7;
LIBC_INLINE constexpr wctype_t WCTYPE_PRINT = 8;
LIBC_INLINE constexpr wctype_t WCTYPE_PUNCT = 9;
LIBC_INLINE constexpr wctype_t WCTYPE_SPACE = 10;
LIBC_INLINE constexpr wctype_t WCTYPE_UPPER = 11;
LIBC_INLINE constexpr wctype_t WCTYPE_XDIGIT = 12;

LIBC_INLINE constexpr cpp::array<wctype_mapping, 12> mappings = {{
    {"alnum", WCTYPE_ALNUM},
    {"alpha", WCTYPE_ALPHA},
    {"blank", WCTYPE_BLANK},
    {"cntrl", WCTYPE_CNTRL},
    {"digit", WCTYPE_DIGIT},
    {"graph", WCTYPE_GRAPH},
    {"lower", WCTYPE_LOWER},
    {"print", WCTYPE_PRINT},
    {"punct", WCTYPE_PUNCT},
    {"space", WCTYPE_SPACE},
    {"upper", WCTYPE_UPPER},
    {"xdigit", WCTYPE_XDIGIT},
}};

LIBC_INLINE constexpr wctype_t wctype(const char *property) {
  if (!property)
    return WCTYPE_INVALID;

  cpp::string_view prop(property);

  for (const auto &wc : mappings) {
    if (wc.name == prop) {
      return wc.desc;
    }
  }
  return WCTYPE_INVALID;
}

LIBC_INLINE constexpr int iswctype(wchar_t c, wctype_t desc) {
  switch (desc) {
  case WCTYPE_ALNUM:
    return isalnum(c);
  case WCTYPE_ALPHA:
    return isalpha(c);
  case WCTYPE_BLANK:
    return isblank(c);
  case WCTYPE_CNTRL:
    return iscntrl(c);
  case WCTYPE_DIGIT:
    return isdigit(c);
  case WCTYPE_GRAPH:
    return isgraph(c);
  case WCTYPE_LOWER:
    return islower(c);
  case WCTYPE_PRINT:
    return isprint(c);
  case WCTYPE_PUNCT:
    return ispunct(c);
  case WCTYPE_SPACE:
    return isspace(c);
  case WCTYPE_UPPER:
    return isupper(c);
  case WCTYPE_XDIGIT:
    return isxdigit(c);
  default:
    return 0;
  }
}

} // namespace internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WCTYPE_IMPL_H

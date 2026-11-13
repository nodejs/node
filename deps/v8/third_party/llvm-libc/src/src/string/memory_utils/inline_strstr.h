//===-- str{,case}str implementation ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_STRSTR_H
#define LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_STRSTR_H

#include "src/__support/macros/attributes.h" // LIBC_INLINE
#include "src/__support/macros/config.h"     // LIBC_NAMESPACE_DECL
#include "src/string/memory_utils/inline_memmem.h"
#include "src/string/string_utils.h"
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {

template <typename Comp>
LIBC_INLINE constexpr char *inline_strstr(const char *haystack,
                                          const char *needle, Comp &&comp) {
  void *result = inline_memmem(
      static_cast<const void *>(haystack), internal::string_length(haystack),
      static_cast<const void *>(needle), internal::string_length(needle), comp);
  return static_cast<char *>(result);
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC_STRING_MEMORY_UTILS_INLINE_STRSTR_H

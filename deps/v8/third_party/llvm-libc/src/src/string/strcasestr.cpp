//===-- Implementation of strcasestr --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strcasestr.h"

#include "src/__support/common.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_strstr.h"

namespace LIBC_NAMESPACE_DECL {

// TODO: This is a simple brute force implementation. This can be
// improved upon using well known string matching algorithms.
LLVM_LIBC_FUNCTION(char *, strcasestr,
                   (const char *haystack, const char *needle)) {
  auto case_cmp = [](char a, char b) {
    return static_cast<int>(LIBC_NAMESPACE::internal::tolower(a)) -
           static_cast<int>(LIBC_NAMESPACE::internal::tolower(b));
  };

  LIBC_CRASH_ON_NULLPTR(haystack);
  LIBC_CRASH_ON_NULLPTR(needle);
  return inline_strstr(haystack, needle, case_cmp);
}

} // namespace LIBC_NAMESPACE_DECL

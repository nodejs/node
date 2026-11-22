//===-- Implementation of strncpy -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strncpy.h"

#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include <stddef.h> // For size_t.

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strncpy,
                   (char *__restrict dest, const char *__restrict src,
                    size_t n)) {
  if (n) {
    LIBC_CRASH_ON_NULLPTR(dest);
    LIBC_CRASH_ON_NULLPTR(src);
  }
  size_t i = 0;
  // Copy up until \0 is found.
  for (; i < n && src[i] != '\0'; ++i)
    dest[i] = src[i];
  // When n>strlen(src), n-strlen(src) \0 are appended.
  for (; i < n; ++i)
    dest[i] = '\0';
  return dest;
}

} // namespace LIBC_NAMESPACE_DECL

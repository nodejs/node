//===-- Implementation of strncat -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/string/strncat.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/string_utils.h"

#include "src/__support/common.h"

namespace LIBC_NAMESPACE_DECL {

LLVM_LIBC_FUNCTION(char *, strncat,
                   (char *__restrict dest, const char *__restrict src,
                    size_t count)) {
  if (count) {
    LIBC_CRASH_ON_NULLPTR(dest);
    LIBC_CRASH_ON_NULLPTR(src);
  }
  size_t dest_length = internal::string_length(dest);
  size_t i;
  for (i = 0; i < count && src[i] != '\0'; ++i)
    dest[dest_length + i] = src[i];

  dest[dest_length + i] = '\0';
  return dest;
}

} // namespace LIBC_NAMESPACE_DECL
